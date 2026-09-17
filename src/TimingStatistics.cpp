// Copyright (c) 2026. Andrew Kevin Bailey
// This code, firmware, and software is released under the MIT License (http://opensource.org/licenses/MIT).

#include "TimingStatistics.h"

#include <algorithm>
#include <cmath>

namespace TimingStatistics {

void appendObservation(QVector<OffsetObservation> &history,
                       const OffsetObservation &observation) {
  if (!history.isEmpty() && observation.observedAtUnixMicros <=
                                history.back().observedAtUnixMicros) {
    history.clear();
  }

  history.append(observation);
  const qint64 cutoff =
      observation.observedAtUnixMicros - kDriftHistoryWindowMicros;
  while (!history.isEmpty() && history.front().observedAtUnixMicros < cutoff) {
    history.removeFirst();
  }
}

double jitterMillis(const QVector<OffsetObservation> &history) {
  if (history.size() < 2) {
    return 0.0;
  }

  const qsizetype sampleCount =
      std::min<qsizetype>(history.size(), kJitterSampleCount);
  const qsizetype firstIndex = history.size() - sampleCount;
  double meanOffsetMicros = 0.0;
  for (qsizetype index = firstIndex; index < history.size(); ++index) {
    meanOffsetMicros += history[index].offsetMicros;
  }
  meanOffsetMicros /= static_cast<double>(sampleCount);

  double varianceMicrosSquared = 0.0;
  for (qsizetype index = firstIndex; index < history.size(); ++index) {
    const double difference = history[index].offsetMicros - meanOffsetMicros;
    varianceMicrosSquared += difference * difference;
  }
  varianceMicrosSquared /= static_cast<double>(sampleCount);
  return std::sqrt(varianceMicrosSquared) / 1000.0;
}

std::optional<double> localDriftMillisPerHour(
    const QVector<OffsetObservation> &history) {
  if (history.size() < kMinimumDriftSampleCount) {
    return std::nullopt;
  }

  const qint64 firstTime = history.front().observedAtUnixMicros;
  const qint64 elapsedMicros =
      history.back().observedAtUnixMicros - firstTime;
  if (elapsedMicros < kMinimumDriftSpanMicros) {
    return std::nullopt;
  }

  constexpr double microsPerHour = 3'600'000'000.0;
  constexpr double microsPerMillisecond = 1000.0;
  const double sampleCount = static_cast<double>(history.size());
  double meanTimeHours = 0.0;
  double meanOffsetMillis = 0.0;
  for (const OffsetObservation &observation : history) {
    meanTimeHours +=
        static_cast<double>(observation.observedAtUnixMicros - firstTime) /
        microsPerHour;
    meanOffsetMillis += observation.offsetMicros / microsPerMillisecond;
  }
  meanTimeHours /= sampleCount;
  meanOffsetMillis /= sampleCount;

  double covariance = 0.0;
  double timeVariance = 0.0;
  for (const OffsetObservation &observation : history) {
    const double timeHours =
        static_cast<double>(observation.observedAtUnixMicros - firstTime) /
        microsPerHour;
    const double offsetMillis =
        observation.offsetMicros / microsPerMillisecond;
    const double centeredTime = timeHours - meanTimeHours;
    covariance += centeredTime * (offsetMillis - meanOffsetMillis);
    timeVariance += centeredTime * centeredTime;
  }

  if (timeVariance <= 0.0) {
    return std::nullopt;
  }
  return covariance / timeVariance;
}

}  // namespace TimingStatistics
