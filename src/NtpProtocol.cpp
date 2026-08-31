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
#include "NtpProtocol.h"

#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr qint64 kMicrosPerSecond = 1'000'000;
constexpr qint64 kNtpUnixEpochDeltaSeconds = 2'208'988'800LL;
constexpr qint64 kNtpEraSeconds = 1LL << 32;

quint32 readUnsigned32(const QByteArray &bytes, const int offset) {
  return qFromBigEndian<quint32>(
      reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

int readUnsigned8(const QByteArray &bytes, const int offset) {
  return static_cast<unsigned char>(bytes[offset]);
}

int readSigned8(const QByteArray &bytes, const int offset) {
  const int value = readUnsigned8(bytes, offset);
  return value < 0x80 ? value : value - 0x100;
}

qint32 readSigned32(const QByteArray &bytes, const int offset) {
  return static_cast<qint32>(readUnsigned32(bytes, offset));
}

void writeUnsigned32(QByteArray &bytes, const int offset, const quint32 value) {
  qToBigEndian<quint32>(
      value, reinterpret_cast<uchar *>(bytes.data() + offset));
}

qint64 floorDiv(const qint64 numerator, const qint64 denominator) {
  qint64 quotient = numerator / denominator;
  const qint64 remainder = numerator % denominator;
  if (remainder < 0) {
    --quotient;
  }
  return quotient;
}

QString referenceIdString(const QByteArray &bytes, const int stratum) {
  const QByteArray id = bytes.mid(12, 4);
  const bool printable = std::ranges::all_of(id, [](const char value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte >= 32 && byte <= 126;
  });

  if (printable || stratum <= 1) {
    return QString::fromLatin1(id).trimmed();
  }

  return QStringLiteral("%1.%2.%3.%4")
      .arg(static_cast<unsigned char>(id[0]))
      .arg(static_cast<unsigned char>(id[1]))
      .arg(static_cast<unsigned char>(id[2]))
      .arg(static_cast<unsigned char>(id[3]));
}

void setError(QString *errorMessage, const QString &message) {
  if (errorMessage != nullptr) {
    *errorMessage = message;
  }
}

}  // namespace

namespace NtpProtocol {

WireTimestamp fromUnixMicros(const qint64 unixMicros) {
  const qint64 unixSeconds = floorDiv(unixMicros, kMicrosPerSecond);
  const qint64 fractionalMicros = unixMicros - unixSeconds * kMicrosPerSecond;
  const qint64 ntpSeconds = unixSeconds + kNtpUnixEpochDeltaSeconds;

  WireTimestamp result;
  result.seconds = static_cast<quint32>(ntpSeconds & 0xffffffffLL);
  result.fraction = static_cast<quint32>(
      (static_cast<quint64>(fractionalMicros) << 32U) /
      static_cast<quint64>(kMicrosPerSecond));
  return result;
}

qint64 toUnixMicros(const WireTimestamp &timestamp,
                    const qint64 referenceUnixMicros) {
  const qint64 referenceUnixSeconds =
      floorDiv(referenceUnixMicros, kMicrosPerSecond);
  const qint64 referenceNtpSeconds =
      referenceUnixSeconds + kNtpUnixEpochDeltaSeconds;
  const qint64 referenceEra = floorDiv(referenceNtpSeconds, kNtpEraSeconds);

  const std::array<qint64, 3> candidates = {
      (referenceEra - 1) * kNtpEraSeconds + timestamp.seconds,
      referenceEra * kNtpEraSeconds + timestamp.seconds,
      (referenceEra + 1) * kNtpEraSeconds + timestamp.seconds,
  };

  const qint64 fullNtpSeconds = *std::ranges::min_element(
      candidates,
      [referenceNtpSeconds](const qint64 left, const qint64 right) {
        return std::abs(left - referenceNtpSeconds) <
               std::abs(right - referenceNtpSeconds);
      });

  const auto fractionalMicros = static_cast<qint64>(
      (static_cast<quint64>(timestamp.fraction) * kMicrosPerSecond +
       (1ULL << 31U)) >>
      32U);
  return (fullNtpSeconds - kNtpUnixEpochDeltaSeconds) * kMicrosPerSecond +
         fractionalMicros;
}

QByteArray encodeTimestamp(const WireTimestamp &timestamp) {
  QByteArray bytes(8, '\0');
  writeUnsigned32(bytes, 0, timestamp.seconds);
  writeUnsigned32(bytes, 4, timestamp.fraction);
  return bytes;
}

WireTimestamp decodeTimestamp(const QByteArray &bytes, const int offset) {
  if (offset < 0 || offset + 8 > bytes.size()) {
    return {};
  }
  return {readUnsigned32(bytes, offset), readUnsigned32(bytes, offset + 4)};
}

QByteArray createClientRequest(const qint64 originUnixMicros) {
  QByteArray packet(kPacketSize, '\0');
  packet[0] = static_cast<char>((4U << 3U) | 3U);
  packet[2] = static_cast<char>(6);    // Suggested 64-second poll interval.
  packet[3] = static_cast<char>(-20);  // Approximately one-microsecond precision.
  const QByteArray timestamp = encodeTimestamp(fromUnixMicros(originUnixMicros));
  std::ranges::copy(timestamp, packet.begin() + 40);
  return packet;
}

QByteArray transmitToken(const QByteArray &request) {
  if (request.size() < kPacketSize) {
    return {};
  }
  return request.mid(40, 8);
}

std::optional<ParsedResponse> parseServerResponse(
    const QByteArray &datagram,
    const QByteArray &expectedOriginToken,
    const qint64 originUnixMicros,
    const qint64 destinationUnixMicros,
    QString *errorMessage) {
  if (datagram.size() < kPacketSize) {
    setError(errorMessage, QStringLiteral("NTP response is shorter than 48 bytes."));
    return std::nullopt;
  }
  if (expectedOriginToken.size() != 8 ||
      datagram.mid(24, 8) != expectedOriginToken) {
    setError(errorMessage,
             QStringLiteral("NTP response does not match the request timestamp."));
    return std::nullopt;
  }

  const int flags = readUnsigned8(datagram, 0);
  const int leapIndicator = (flags >> 6) & 0x03;
  const int version = (flags >> 3) & 0x07;
  const int mode = flags & 0x07;
  const int stratum = readUnsigned8(datagram, 1);

  if (version < 3 || version > 4) {
    setError(errorMessage, QStringLiteral("NTP response uses an unsupported version."));
    return std::nullopt;
  }
  if (mode != 4) {
    setError(errorMessage, QStringLiteral("NTP response is not in server mode."));
    return std::nullopt;
  }
  if (stratum == 0) {
    const QString kissCode = referenceIdString(datagram, stratum);
    setError(errorMessage,
             QStringLiteral("NTP server rejected the request (%1).")
                 .arg(kissCode.isEmpty() ? QStringLiteral("Kiss-o'-Death")
                                         : kissCode));
    return std::nullopt;
  }
  if (stratum > 16) {
    setError(errorMessage, QStringLiteral("NTP response contains an invalid stratum."));
    return std::nullopt;
  }

  const WireTimestamp receiveTimestamp = decodeTimestamp(datagram, 32);
  const WireTimestamp transmitTimestamp = decodeTimestamp(datagram, 40);
  if ((receiveTimestamp.seconds == 0 && receiveTimestamp.fraction == 0) ||
      (transmitTimestamp.seconds == 0 && transmitTimestamp.fraction == 0)) {
    setError(errorMessage, QStringLiteral("NTP response contains a zero timestamp."));
    return std::nullopt;
  }

  ParsedResponse response;
  response.receiveUnixMicros =
      toUnixMicros(receiveTimestamp, destinationUnixMicros);
  response.transmitUnixMicros =
      toUnixMicros(transmitTimestamp, destinationUnixMicros);
  response.offsetMicros =
      (static_cast<double>(response.receiveUnixMicros - originUnixMicros) +
       static_cast<double>(response.transmitUnixMicros -
                           destinationUnixMicros)) /
      2.0;
  response.roundTripMicros =
      static_cast<double>(destinationUnixMicros - originUnixMicros) -
      static_cast<double>(response.transmitUnixMicros -
                          response.receiveUnixMicros);
  response.rootDelayMillis =
      static_cast<double>(readSigned32(datagram, 4)) * 1000.0 / 65536.0;
  response.rootDispersionMillis =
      static_cast<double>(readUnsigned32(datagram, 8)) * 1000.0 / 65536.0;
  response.pollExponent = readSigned8(datagram, 2);
  response.precisionExponent = readSigned8(datagram, 3);
  response.pollIntervalSeconds = std::ldexp(1.0, response.pollExponent);
  response.precisionSeconds = std::ldexp(1.0, response.precisionExponent);
  response.referenceId = referenceIdString(datagram, stratum);
  response.leapIndicator = leapIndicator;
  response.version = version;
  response.stratum = stratum;
  response.synchronized = leapIndicator != 3 && stratum >= 1 && stratum <= 15;
  return response;
}

}  // namespace NtpProtocol
