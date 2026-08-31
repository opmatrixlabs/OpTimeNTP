// Copyright (c) 2026. Andrew Kevin Bailey
// This code, firmware, and software is released under the MIT License (http://opensource.org/licenses/MIT).
//
// The MIT License (MIT)
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or significant portions of
// the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// ReSharper disable CppUnusedIncludeDirective
#include "NtpClient.h"

#include <QHostInfo>
#include <QNetworkDatagram>
#include <QTimer>

#include <chrono>

#include "NtpProtocol.h"

NtpClient::NtpClient(QObject *parent) : QObject(parent) {
  m_ipv4Available = m_ipv4Socket.bind(QHostAddress::AnyIPv4, 0);
  m_ipv6Available = m_ipv6Socket.bind(QHostAddress::AnyIPv6, 0);

  connect(&m_ipv4Socket, &QUdpSocket::readyRead, this,
          [this] { readPendingDatagrams(&m_ipv4Socket); });
  connect(&m_ipv6Socket, &QUdpSocket::readyRead, this,
          [this] { readPendingDatagrams(&m_ipv6Socket); });
}

void NtpClient::queryServer(const int serverId, const QString &host,
                            const int timeoutMillis) {
  const QString normalizedHost = host.trimmed();
  if (normalizedHost.isEmpty()) {
    emit queryFailed(serverId, QueryStatus::DnsError,
                     QStringLiteral("Server address is empty."));
    return;
  }
  if (m_activeRequestByServer.contains(serverId)) {
    return;
  }

  const quint64 requestId = m_nextRequestId++;
  PendingQuery query;
  query.requestId = requestId;
  query.serverId = serverId;
  query.host = normalizedHost;
  query.timeoutMillis = timeoutMillis;
  m_pendingQueries.insert(requestId, query);
  m_activeRequestByServer.insert(serverId, requestId);

  emit statusChanged(serverId, QueryStatus::Resolving,
                     QStringLiteral("Resolving %1").arg(normalizedHost));

  QTimer::singleShot(timeoutMillis, this, [this, requestId] {
    if (m_pendingQueries.contains(requestId)) {
      finishWithFailure(requestId, QueryStatus::Timeout,
                        QStringLiteral("No NTP response before the timeout."));
    }
  });

  QHostAddress literalAddress;
  if (literalAddress.setAddress(normalizedHost)) {
    sendRequest(requestId, literalAddress);
    return;
  }
  resolveHost(requestId);
}

bool NtpClient::isBusy(const int serverId) const {
  return m_activeRequestByServer.contains(serverId);
}

void NtpClient::cancelAll() {
  m_pendingQueries.clear();
  m_requestByOriginToken.clear();
  m_activeRequestByServer.clear();
}

void NtpClient::resolveHost(const quint64 requestId) {
  const auto iterator = m_pendingQueries.constFind(requestId);
  if (iterator == m_pendingQueries.cend()) {
    return;
  }
  const QString host = iterator->host;

  QHostInfo::lookupHost(host, this, [this, requestId](const QHostInfo &hostInfo) {
    if (!m_pendingQueries.contains(requestId)) {
      return;
    }
    if (hostInfo.error() != QHostInfo::NoError) {
      finishWithFailure(requestId, QueryStatus::DnsError,
                        QStringLiteral("DNS lookup failed: %1")
                            .arg(hostInfo.errorString()));
      return;
    }

    QHostAddress selectedAddress;
    for (const QHostAddress &address : hostInfo.addresses()) {
      if (address.protocol() == QAbstractSocket::IPv4Protocol &&
          m_ipv4Available) {
        selectedAddress = address;
        break;
      }
    }
    if (selectedAddress.isNull()) {
      for (const QHostAddress &address : hostInfo.addresses()) {
        if (address.protocol() == QAbstractSocket::IPv6Protocol &&
            m_ipv6Available) {
          selectedAddress = address;
          break;
        }
      }
    }

    if (selectedAddress.isNull()) {
      finishWithFailure(
          requestId, QueryStatus::NetworkError,
          QStringLiteral("The host has no address supported by an available UDP socket."));
      return;
    }
    sendRequest(requestId, selectedAddress);
  });
}

void NtpClient::sendRequest(const quint64 requestId,
                            const QHostAddress &address) {
  auto iterator = m_pendingQueries.find(requestId);
  if (iterator == m_pendingQueries.end()) {
    return;
  }

  QUdpSocket *socket = socketForAddress(address);
  if (socket == nullptr) {
    finishWithFailure(requestId, QueryStatus::NetworkError,
                      QStringLiteral("No UDP socket is available for %1.")
                          .arg(address.toString()));
    return;
  }

  qint64 originUnixMicros = currentUnixMicros();
  QByteArray packet;
  QByteArray originToken;
  do {
    packet = NtpProtocol::createClientRequest(originUnixMicros);
    originToken = NtpProtocol::transmitToken(packet);
    ++originUnixMicros;
  } while (m_requestByOriginToken.contains(originToken));
  --originUnixMicros;

  const qint64 written = socket->writeDatagram(
      packet, address, NtpProtocol::kDefaultPort);
  if (written != packet.size()) {
    finishWithFailure(requestId, QueryStatus::NetworkError,
                      QStringLiteral("UDP send failed: %1")
                          .arg(socket->errorString()));
    return;
  }

  iterator->address = address;
  iterator->originUnixMicros = originUnixMicros;
  iterator->originToken = originToken;
  m_requestByOriginToken.insert(originToken, requestId);
  emit statusChanged(iterator->serverId, QueryStatus::Querying,
                     QStringLiteral("Waiting for %1:123")
                         .arg(address.toString()));
}

void NtpClient::readPendingDatagrams(QUdpSocket *socket) {
  while (socket->hasPendingDatagrams()) {
    const QNetworkDatagram networkDatagram = socket->receiveDatagram();
    const qint64 destinationUnixMicros = currentUnixMicros();
    const QByteArray datagram = networkDatagram.data();
    if (datagram.size() < NtpProtocol::kPacketSize ||
        networkDatagram.senderPort() != NtpProtocol::kDefaultPort) {
      continue;
    }

    const QByteArray originToken = datagram.mid(24, 8);
    const auto requestIterator = m_requestByOriginToken.constFind(originToken);
    if (requestIterator == m_requestByOriginToken.cend()) {
      continue;
    }
    const quint64 requestId = requestIterator.value();
    const auto pendingIterator = m_pendingQueries.constFind(requestId);
    if (pendingIterator == m_pendingQueries.cend()) {
      continue;
    }
    const PendingQuery &pending = pendingIterator.value();
    if (networkDatagram.senderAddress() != pending.address) {
      continue;
    }

    QString parseError;
    const auto parsed = NtpProtocol::parseServerResponse(
        datagram, pending.originToken, pending.originUnixMicros,
        destinationUnixMicros, &parseError);
    if (!parsed.has_value()) {
      finishWithFailure(requestId, QueryStatus::InvalidResponse, parseError);
      continue;
    }

    NtpSample sample;
    sample.resolvedAddress = pending.address.toString();
    sample.referenceId = parsed->referenceId;
    sample.originUnixMicros = pending.originUnixMicros;
    sample.receiveUnixMicros = parsed->receiveUnixMicros;
    sample.transmitUnixMicros = parsed->transmitUnixMicros;
    sample.destinationUnixMicros = destinationUnixMicros;
    sample.adjustedDestinationUnixMicros = static_cast<qint64>(
        static_cast<double>(destinationUnixMicros) + parsed->offsetMicros);
    sample.offsetMicros = parsed->offsetMicros;
    sample.roundTripMicros = parsed->roundTripMicros;
    sample.rootDelayMillis = parsed->rootDelayMillis;
    sample.rootDispersionMillis = parsed->rootDispersionMillis;
    sample.precisionSeconds = parsed->precisionSeconds;
    sample.pollIntervalSeconds = parsed->pollIntervalSeconds;
    sample.precisionExponent = parsed->precisionExponent;
    sample.pollExponent = parsed->pollExponent;
    sample.leapIndicator = parsed->leapIndicator;
    sample.version = parsed->version;
    sample.stratum = parsed->stratum;
    sample.synchronized = parsed->synchronized;

    const int serverId = pending.serverId;
    removeRequest(requestId);
    emit querySucceeded(serverId, sample);
  }
}

void NtpClient::finishWithFailure(const quint64 requestId,
                                  const QueryStatus status,
                                  const QString &detail) {
  const auto iterator = m_pendingQueries.constFind(requestId);
  if (iterator == m_pendingQueries.cend()) {
    return;
  }
  const int serverId = iterator->serverId;
  removeRequest(requestId);
  emit queryFailed(serverId, status, detail);
}

void NtpClient::removeRequest(const quint64 requestId) {
  const auto iterator = m_pendingQueries.find(requestId);
  if (iterator == m_pendingQueries.end()) {
    return;
  }
  if (!iterator->originToken.isEmpty()) {
    m_requestByOriginToken.remove(iterator->originToken);
  }
  if (m_activeRequestByServer.value(iterator->serverId) == requestId) {
    m_activeRequestByServer.remove(iterator->serverId);
  }
  m_pendingQueries.erase(iterator);
}

QUdpSocket *NtpClient::socketForAddress(const QHostAddress &address) {
  if (address.protocol() == QAbstractSocket::IPv4Protocol && m_ipv4Available) {
    return &m_ipv4Socket;
  }
  if (address.protocol() == QAbstractSocket::IPv6Protocol && m_ipv6Available) {
    return &m_ipv6Socket;
  }
  return nullptr;
}

qint64 NtpClient::currentUnixMicros() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}
