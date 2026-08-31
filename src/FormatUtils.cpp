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
#include "FormatUtils.h"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>

#include <cmath>

namespace {

QDateTime dateTimeFromMicros(const qint64 unixMicros, const QTimeZone &zone) {
  return QDateTime::fromMSecsSinceEpoch(unixMicros / 1000, zone);
}

}  // namespace

namespace FormatUtils {

QString utcTimestamp(const qint64 unixMicros) {
  return dateTimeFromMicros(unixMicros, QTimeZone::UTC)
      .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz 'UTC'"));
}

QString localTimestamp(const qint64 unixMicros) {
  const QDateTime local = dateTimeFromMicros(unixMicros, QTimeZone::LocalTime);
  return QStringLiteral("%1 %2")
      .arg(local.toString(QStringLiteral("ddd, yyyy-MM-dd HH:mm:ss.zzz")),
           local.timeZoneAbbreviation());
}

QString offsetHundredths(const double offsetMicros) {
  const QChar sign = offsetMicros < 0.0 ? QLatin1Char('-') : QLatin1Char('+');
  return QStringLiteral("%1%2 s")
      .arg(sign)
      .arg(std::abs(offsetMicros) / 1'000'000.0, 0, 'f', 2);
}

QString preciseOffset(const double offsetMicros) {
  const QChar sign = offsetMicros < 0.0 ? QLatin1Char('-') : QLatin1Char('+');
  const double absoluteMicros = std::abs(offsetMicros);
  if (absoluteMicros >= 1'000'000.0) {
    return QStringLiteral("%1%2 s")
        .arg(sign)
        .arg(absoluteMicros / 1'000'000.0, 0, 'f', 6);
  }
  if (absoluteMicros >= 1'000.0) {
    return QStringLiteral("%1%2 ms")
        .arg(sign)
        .arg(absoluteMicros / 1'000.0, 0, 'f', 3);
  }
  return QStringLiteral("%1%2 us")
      .arg(sign)
      .arg(absoluteMicros, 0, 'f', 1);
}

QString milliseconds(const double micros, const int decimals) {
  return QStringLiteral("%1 ms").arg(micros / 1000.0, 0, 'f', decimals);
}

QString precision(const int exponent, const double precisionSeconds) {
  if (precisionSeconds < 0.001) {
    return QStringLiteral("2^%1 (%2 us)")
        .arg(exponent)
        .arg(precisionSeconds * 1'000'000.0, 0, 'f', 2);
  }
  return QStringLiteral("2^%1 (%2 ms)")
      .arg(exponent)
      .arg(precisionSeconds * 1'000.0, 0, 'f', 3);
}

QString leapIndicator(const int indicator) {
  switch (indicator) {
    case 0:
      return QStringLiteral("00 | No warning");
    case 1:
      return QStringLiteral("01 | 61-second minute");
    case 2:
      return QStringLiteral("10 | 59-second minute");
    default:
      return QStringLiteral("11 | Unsynchronized");
  }
}

QString leapBits(const int indicator) {
  return QString::number(indicator & 0x03, 2).rightJustified(2, QLatin1Char('0'));
}

QString status(const QueryStatus queryStatus) {
  switch (queryStatus) {
    case QueryStatus::Idle:
      return QStringLiteral("Idle");
    case QueryStatus::Resolving:
      return QStringLiteral("Resolving");
    case QueryStatus::Querying:
      return QStringLiteral("Querying");
    case QueryStatus::Synchronized:
      return QStringLiteral("Synchronized");
    case QueryStatus::Unsynchronized:
      return QStringLiteral("Unsynchronized");
    case QueryStatus::Timeout:
      return QStringLiteral("Timeout");
    case QueryStatus::DnsError:
      return QStringLiteral("DNS error");
    case QueryStatus::NetworkError:
      return QStringLiteral("Network error");
    case QueryStatus::InvalidResponse:
      return QStringLiteral("Invalid response");
  }
  return QStringLiteral("Unknown");
}

QString statusColor(const QueryStatus queryStatus) {
  switch (queryStatus) {
    case QueryStatus::Synchronized:
      return QStringLiteral("#10b981");
    case QueryStatus::Resolving:
    case QueryStatus::Querying:
      return QStringLiteral("#60a5fa");
    case QueryStatus::Unsynchronized:
    case QueryStatus::Timeout:
      return QStringLiteral("#f59e0b");
    case QueryStatus::DnsError:
    case QueryStatus::NetworkError:
    case QueryStatus::InvalidResponse:
      return QStringLiteral("#ef4444");
    case QueryStatus::Idle:
      return QStringLiteral("#64748b");
  }
  return QStringLiteral("#64748b");
}

QString reachability(const quint8 value) {
  return QStringLiteral("0%1 (%2)")
      .arg(QString::number(value, 8).rightJustified(3, QLatin1Char('0')))
      .arg(value);
}

}  // namespace FormatUtils
