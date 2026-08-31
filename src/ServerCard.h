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
#include <QFrame>

#include "NtpTypes.h"

class QLabel;
class OffsetBar;
class QPushButton;

class ServerCard final : public QFrame {
  Q_OBJECT

 public:
  explicit ServerCard(int serverId, QWidget *parent = nullptr);

  void setState(const ServerState &state, int displayIndex) const;
  void setSelected(bool selected);
  void setTimeActionEnabled(bool enabled) const;
  [[nodiscard]] int serverId() const;

 signals:
  void selected(int serverId);
  void setLocalTimeRequested(int serverId);

 protected:
  void mousePressEvent(QMouseEvent *event) override;

 private:
  struct MetricLabels {
    QLabel *value = nullptr;
    QLabel *sub = nullptr;
  };

  static MetricLabels createMetric(const QString &caption, QWidget *parent,
                                   const QString &objectName = {});
  static void setMetric(const MetricLabels &metric, const QString &value,
                        const QString &sub = {}, const QString &color = {});

  int m_serverId = 0;
  QLabel *m_indexLabel = nullptr;
  QLabel *m_nameLabel = nullptr;
  QLabel *m_hostLabel = nullptr;
  QLabel *m_statusLabel = nullptr;
  QPushButton *m_setTimeButton = nullptr;
  QLabel *m_serverTimeLabel = nullptr;
  QLabel *m_adjustedTimeLabel = nullptr;
  QLabel *m_offsetLabel = nullptr;
  QLabel *m_preciseOffsetLabel = nullptr;
  OffsetBar *m_offsetBar = nullptr;
  MetricLabels m_roundTrip;
  MetricLabels m_jitter;
  MetricLabels m_frequency;
  MetricLabels m_precision;
  MetricLabels m_rootDelay;
  MetricLabels m_rootDispersion;
  MetricLabels m_leap;
  MetricLabels m_referenceId;
  MetricLabels m_stratum;
  MetricLabels m_pollInterval;
  MetricLabels m_reachability;
  MetricLabels m_lastUpdated;
};
