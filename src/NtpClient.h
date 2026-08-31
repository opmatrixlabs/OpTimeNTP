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

#pragma once

// ReSharper disable CppUnusedIncludeDirective
#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>

#include "NtpTypes.h"

class NtpClient final : public QObject {
  Q_OBJECT

 public:
  explicit NtpClient(QObject *parent = nullptr);

  void queryServer(int serverId, const QString &host, int timeoutMillis = 3500);
  void cancelAll();
  [[nodiscard]] bool isBusy(int serverId) const;

 signals:
  void statusChanged(int serverId, QueryStatus status, const QString &detail);
  void querySucceeded(int serverId, const NtpSample &sample);
  void queryFailed(int serverId, QueryStatus status, const QString &detail);

 private:
  struct PendingQuery {
    quint64 requestId = 0;
    int serverId = 0;
    QString host;
    QHostAddress address;
    qint64 originUnixMicros = 0;
    QByteArray originToken;
    int timeoutMillis = 3500;
  };

  void resolveHost(quint64 requestId);
  void sendRequest(quint64 requestId, const QHostAddress &address);
  void readPendingDatagrams(QUdpSocket *socket);
  void finishWithFailure(quint64 requestId, QueryStatus status,
                         const QString &detail);
  void removeRequest(quint64 requestId);
  [[nodiscard]] QUdpSocket *socketForAddress(const QHostAddress &address);
  [[nodiscard]] static qint64 currentUnixMicros();

  QUdpSocket m_ipv4Socket;
  QUdpSocket m_ipv6Socket;
  bool m_ipv4Available = false;
  bool m_ipv6Available = false;
  quint64 m_nextRequestId = 1;
  QHash<quint64, PendingQuery> m_pendingQueries;
  QHash<QByteArray, quint64> m_requestByOriginToken;
  QHash<int, quint64> m_activeRequestByServer;
};
