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
#include <QMetaType>
#include <QString>
#include <QVector>

#include <optional>

inline constexpr int kMaximumNtpServers = 10;

enum class QueryStatus {
  Idle,
  Resolving,
  Querying,
  Synchronized,
  Unsynchronized,
  Timeout,
  DnsError,
  NetworkError,
  InvalidResponse,
};

struct NtpSample {
  QString resolvedAddress;
  QString referenceId;
  qint64 originUnixMicros = 0;
  qint64 receiveUnixMicros = 0;
  qint64 transmitUnixMicros = 0;
  qint64 destinationUnixMicros = 0;
  qint64 adjustedDestinationUnixMicros = 0;
  double offsetMicros = 0.0;
  double roundTripMicros = 0.0;
  double rootDelayMillis = 0.0;
  double rootDispersionMillis = 0.0;
  double precisionSeconds = 0.0;
  double pollIntervalSeconds = 0.0;
  double jitterMillis = 0.0;
  std::optional<double> frequencyPpm;
  int precisionExponent = 0;
  int pollExponent = 0;
  int leapIndicator = 3;
  int version = 0;
  int stratum = 0;
  bool synchronized = false;
};

struct ServerConfig {
  int id = 0;
  QString label;
  QString host;
};

struct OffsetObservation {
  qint64 observedAtUnixMicros = 0;
  double offsetMicros = 0.0;
};

struct ServerState {
  ServerConfig config;
  QueryStatus status = QueryStatus::Idle;
  QString statusMessage;
  std::optional<NtpSample> sample;
  QVector<OffsetObservation> history;
  quint8 reachability = 0;
  bool requestActive = false;
};

Q_DECLARE_METATYPE(NtpSample)
Q_DECLARE_METATYPE(QueryStatus)
