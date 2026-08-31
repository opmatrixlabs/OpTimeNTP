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
#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace NtpProtocol {

constexpr int kPacketSize = 48;
constexpr quint16 kDefaultPort = 123;

struct WireTimestamp {
  quint32 seconds = 0;
  quint32 fraction = 0;
};

struct ParsedResponse {
  QString referenceId;
  qint64 receiveUnixMicros = 0;
  qint64 transmitUnixMicros = 0;
  double offsetMicros = 0.0;
  double roundTripMicros = 0.0;
  double rootDelayMillis = 0.0;
  double rootDispersionMillis = 0.0;
  double precisionSeconds = 0.0;
  double pollIntervalSeconds = 0.0;
  int precisionExponent = 0;
  int pollExponent = 0;
  int leapIndicator = 3;
  int version = 0;
  int stratum = 0;
  bool synchronized = false;
};

WireTimestamp fromUnixMicros(qint64 unixMicros);
qint64 toUnixMicros(const WireTimestamp &timestamp, qint64 referenceUnixMicros);
QByteArray encodeTimestamp(const WireTimestamp &timestamp);
WireTimestamp decodeTimestamp(const QByteArray &bytes, int offset);

QByteArray createClientRequest(qint64 originUnixMicros);
QByteArray transmitToken(const QByteArray &request);

std::optional<ParsedResponse> parseServerResponse(
    const QByteArray &datagram,
    const QByteArray &expectedOriginToken,
    qint64 originUnixMicros,
    qint64 destinationUnixMicros,
    QString *errorMessage = nullptr);

}  // namespace NtpProtocol
