// Copyright (c) 2026. Andrew Kevin Bailey
// This code, firmware, and software is released under the MIT License (http://opensource.org/licenses/MIT).

#pragma once

#include <optional>

#include "NtpTypes.h"

namespace TimingStatistics {

inline constexpr int kJitterSampleCount = 8;
inline constexpr int kMinimumDriftSampleCount = 8;
inline constexpr qint64 kMinimumDriftSpanMicros = 5LL * 60 * 1'000'000;
inline constexpr qint64 kDriftHistoryWindowMicros = 30LL * 60 * 1'000'000;

void appendObservation(QVector<OffsetObservation> &history,
                       const OffsetObservation &observation);
double jitterMillis(const QVector<OffsetObservation> &history);
std::optional<double> localDriftMillisPerHour(
    const QVector<OffsetObservation> &history);

}  // namespace TimingStatistics
