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
#include "ServerCard.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <array>
#include <algorithm>
#include <cmath>

#include "FormatUtils.h"

class OffsetBar final : public QWidget {
 public:
  explicit OffsetBar(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(12);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  }

  void setOffset(const double offsetMicros) {
    m_offsetMicros = offsetMicros;
    update();
  }

 protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0.0, height() / 2.0 - 1.0, width(), 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#161b22")));
    painter.drawRoundedRect(track, 1.0, 1.0);

    constexpr double halfRangeMicros = 1'000'000.0;
    const double normalized =
        std::clamp(m_offsetMicros / halfRangeMicros, -1.0, 1.0);
    const double center = width() / 2.0;
    const double markerX = center + normalized * (width() / 2.0 - 3.0);
    const double absoluteOffsetMicros = std::abs(m_offsetMicros);
    QString markerColor;
    if (absoluteOffsetMicros <= 100'000.0) {
      markerColor = QStringLiteral("#10b981");
    } else if (absoluteOffsetMicros <= 500'000.0) {
      markerColor = QStringLiteral("#f59e0b");
    } else if (absoluteOffsetMicros <= 1'000'000.0) {
      markerColor = QStringLiteral("#991b1b");
    } else {
      markerColor = QStringLiteral("#ef4444");
    }
    painter.setBrush(QColor(markerColor));
    painter.drawRoundedRect(QRectF(markerX - 2.0, 1.0, 4.0, height() - 2.0),
                            2.0, 2.0);
  }

 private:
  double m_offsetMicros = 0.0;
};

namespace {

QLabel *captionLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("MetricCaption"));
  return label;
}

QWidget *metricContainer(QWidget *parent, QLabel **value, QLabel **sub,
                         const QString &caption) {
  auto *container = new QWidget(parent);
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);
  layout->addWidget(captionLabel(caption, container));

  *value = new QLabel(QStringLiteral("--"), container);
  (*value)->setObjectName(QStringLiteral("MetricValue"));
  (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(*value);

  *sub = new QLabel(container);
  (*sub)->setObjectName(QStringLiteral("MetricSub"));
  (*sub)->setVisible(false);
  layout->addWidget(*sub);
  return container;
}

QString elapsedText(const qint64 destinationUnixMicros) {
  if (destinationUnixMicros <= 0) {
    return QStringLiteral("Never");
  }
  const qint64 elapsedSeconds = std::max<qint64>(
      0, QDateTime::currentMSecsSinceEpoch() / 1000 -
             destinationUnixMicros / 1'000'000);
  if (elapsedSeconds < 60) {
    return QStringLiteral("%1 s ago").arg(elapsedSeconds);
  }
  return QStringLiteral("%1 min ago").arg(elapsedSeconds / 60);
}

}  // namespace

ServerCard::ServerCard(const int serverId, QWidget *parent)
    : QFrame(parent), m_serverId(serverId) {
  setObjectName(QStringLiteral("ServerCard"));
  setProperty("selected", false);
  setCursor(Qt::PointingHandCursor);
  setMinimumWidth(520);
  setFixedHeight(390);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(16, 12, 16, 12);
  root->setSpacing(8);

  auto *header = new QHBoxLayout;
  header->setSpacing(9);
  m_indexLabel = new QLabel(QStringLiteral("1"), this);
  m_indexLabel->setObjectName(QStringLiteral("ServerIndex"));
  m_indexLabel->setAlignment(Qt::AlignCenter);
  m_indexLabel->setFixedSize(29, 29);
  header->addWidget(m_indexLabel);

  auto *identity = new QVBoxLayout;
  identity->setSpacing(2);
  m_nameLabel = new QLabel(this);
  m_nameLabel->setObjectName(QStringLiteral("ServerName"));
  m_hostLabel = new QLabel(this);
  m_hostLabel->setObjectName(QStringLiteral("ServerHost"));
  identity->addWidget(m_nameLabel);
  identity->addWidget(m_hostLabel);
  header->addLayout(identity);
  header->addStretch();

  m_setTimeButton = new QPushButton(
      QIcon(QStringLiteral(":/icons/clock.svg")),
      QStringLiteral("Set time to this server"), this);
  m_setTimeButton->setObjectName(QStringLiteral("SetTimeButton"));
  m_setTimeButton->setFlat(true);
  m_setTimeButton->setEnabled(false);
  m_setTimeButton->setCursor(Qt::PointingHandCursor);
  m_setTimeButton->setToolTip(QStringLiteral(
      "Query this server again and set the local machine clock"));
  connect(m_setTimeButton, &QPushButton::clicked, this,
          [this] { emit setLocalTimeRequested(m_serverId); });
  header->addWidget(m_setTimeButton, 0, Qt::AlignTop);

  m_statusLabel = new QLabel(QStringLiteral("IDLE"), this);
  m_statusLabel->setObjectName(QStringLiteral("ServerStatus"));
  header->addWidget(m_statusLabel, 0, Qt::AlignTop);
  root->addLayout(header);

  auto *timeBand = new QFrame(this);
  timeBand->setObjectName(QStringLiteral("TimeBand"));
  auto *timeLayout = new QGridLayout(timeBand);
  timeLayout->setContentsMargins(11, 9, 11, 9);
  timeLayout->setHorizontalSpacing(16);
  timeLayout->setVerticalSpacing(2);
  timeLayout->addWidget(captionLabel(QStringLiteral("SERVER TRANSMIT TIME"), timeBand),
                        0, 0);
  timeLayout->addWidget(captionLabel(QStringLiteral("CORRECTED LOCAL TIME"), timeBand),
                        0, 1);
  m_serverTimeLabel = new QLabel(QStringLiteral("--"), timeBand);
  m_serverTimeLabel->setObjectName(QStringLiteral("TimeValue"));
  m_adjustedTimeLabel = new QLabel(QStringLiteral("--"), timeBand);
  m_adjustedTimeLabel->setObjectName(QStringLiteral("TimeValueAccent"));
  timeLayout->addWidget(m_serverTimeLabel, 1, 0);
  timeLayout->addWidget(m_adjustedTimeLabel, 1, 1);
  root->addWidget(timeBand);

  auto *offsetHeader = new QHBoxLayout;
  offsetHeader->addWidget(captionLabel(QStringLiteral("CLOCK OFFSET"), this));
  offsetHeader->addStretch();
  m_preciseOffsetLabel = new QLabel(this);
  m_preciseOffsetLabel->setObjectName(QStringLiteral("PreciseOffset"));
  offsetHeader->addWidget(m_preciseOffsetLabel);
  m_offsetLabel = new QLabel(QStringLiteral("--"), this);
  m_offsetLabel->setObjectName(QStringLiteral("OffsetValue"));
  offsetHeader->addWidget(m_offsetLabel);
  root->addLayout(offsetHeader);
  m_offsetBar = new OffsetBar(this);
  root->addWidget(m_offsetBar);

  auto addMetricRow = [this, root](const std::array<QString, 3> &captions,
                                    const std::array<MetricLabels *, 3> &metrics) {
    auto *row = new QGridLayout;
    row->setHorizontalSpacing(12);
    row->setVerticalSpacing(0);
    for (int column = 0; column < 3; ++column) {
      *metrics[column] = createMetric(captions[column], this);
      row->addWidget(metrics[column]->value->parentWidget(), 0, column);
      row->setColumnStretch(column, 1);
    }
    root->addLayout(row);
  };

  addMetricRow({QStringLiteral("ROUND-TRIP"), QStringLiteral("JITTER"),
                QStringLiteral("FREQUENCY")},
               {&m_roundTrip, &m_jitter, &m_frequency});
  addMetricRow({QStringLiteral("PRECISION"), QStringLiteral("ROOT DELAY"),
                QStringLiteral("ROOT DISPERSION")},
               {&m_precision, &m_rootDelay, &m_rootDispersion});
  addMetricRow({QStringLiteral("LEAP INDICATOR"), QStringLiteral("REFERENCE ID"),
                QStringLiteral("STRATUM")},
               {&m_leap, &m_referenceId, &m_stratum});
  addMetricRow({QStringLiteral("POLL INTERVAL"), QStringLiteral("REACHABILITY"),
                QStringLiteral("LAST UPDATED")},
               {&m_pollInterval, &m_reachability, &m_lastUpdated});
}

void ServerCard::setState(const ServerState &state,
                          const int displayIndex) const {
  m_indexLabel->setText(QString::number(displayIndex + 1));
  m_nameLabel->setText(state.config.label);
  m_hostLabel->setText(state.config.host);
  m_statusLabel->setText(FormatUtils::status(state.status).toUpper());
  const QString statusColor = FormatUtils::statusColor(state.status);
  m_statusLabel->setStyleSheet(
      QStringLiteral("color: %1; border: 1px solid %1; padding: 3px 7px; "
                     "border-radius: 3px;")
          .arg(statusColor));
  m_statusLabel->setToolTip(state.statusMessage);

  setMetric(m_reachability, FormatUtils::reachability(state.reachability));

  if (!state.sample.has_value()) {
    m_serverTimeLabel->setText(QStringLiteral("--"));
    m_adjustedTimeLabel->setText(QStringLiteral("--"));
    m_offsetLabel->setText(QStringLiteral("--"));
    m_preciseOffsetLabel->clear();
    m_offsetBar->setOffset(0.0);
    setMetric(m_roundTrip, QStringLiteral("--"));
    setMetric(m_jitter, QStringLiteral("--"));
    setMetric(m_frequency, QStringLiteral("--"));
    setMetric(m_precision, QStringLiteral("--"));
    setMetric(m_rootDelay, QStringLiteral("--"));
    setMetric(m_rootDispersion, QStringLiteral("--"));
    setMetric(m_leap, QStringLiteral("--"));
    setMetric(m_referenceId, QStringLiteral("--"));
    setMetric(m_stratum, QStringLiteral("--"));
    setMetric(m_pollInterval, QStringLiteral("--"));
    setMetric(m_lastUpdated, QStringLiteral("Never"));
    return;
  }

  const NtpSample &sample = *state.sample;
  m_serverTimeLabel->setText(FormatUtils::utcTimestamp(sample.transmitUnixMicros));
  m_adjustedTimeLabel->setText(
      FormatUtils::utcTimestamp(sample.adjustedDestinationUnixMicros));
  m_offsetLabel->setText(FormatUtils::offsetHundredths(sample.offsetMicros));
  m_preciseOffsetLabel->setText(
      QStringLiteral("(%1)").arg(FormatUtils::preciseOffset(sample.offsetMicros)));
  m_offsetBar->setOffset(sample.offsetMicros);

  const QString warning = QStringLiteral("#f59e0b");
  setMetric(m_roundTrip, FormatUtils::milliseconds(sample.roundTripMicros), {},
            QStringLiteral("#a78bfa"));
  setMetric(m_jitter,
            state.history.size() < 2
                ? QStringLiteral("Collecting")
                : QStringLiteral("%1 ms").arg(sample.jitterMillis, 0, 'f', 3),
            {}, sample.jitterMillis > 1.0 ? warning : QString());
  setMetric(m_frequency,
            sample.frequencyPpm.has_value()
                ? QStringLiteral("%1%2 ppm")
                      .arg(*sample.frequencyPpm >= 0.0 ? QStringLiteral("+")
                                                       : QString())
                      .arg(*sample.frequencyPpm, 0, 'f', 3)
                : QStringLiteral("Collecting"));
  setMetric(m_precision,
            QStringLiteral("2^%1").arg(sample.precisionExponent),
            FormatUtils::precision(sample.precisionExponent,
                                   sample.precisionSeconds));
  setMetric(m_rootDelay,
            QStringLiteral("%1 ms").arg(sample.rootDelayMillis, 0, 'f', 3));
  setMetric(m_rootDispersion,
            QStringLiteral("%1 ms")
                .arg(sample.rootDispersionMillis, 0, 'f', 3));
  setMetric(m_leap, FormatUtils::leapIndicator(sample.leapIndicator), {},
            sample.leapIndicator == 0
                ? QStringLiteral("#10b981")
                : sample.leapIndicator == 3 ? QStringLiteral("#ef4444") : warning);
  setMetric(m_referenceId,
            sample.referenceId.isEmpty() ? QStringLiteral("--")
                                         : sample.referenceId,
            sample.resolvedAddress);
  const QString stratumColor =
      sample.stratum == 1
          ? QStringLiteral("#10b981")
          : sample.stratum == 2 ? QStringLiteral("#60a5fa")
                                : QStringLiteral("#94a3b8");
  setMetric(m_stratum, QString::number(sample.stratum), {}, stratumColor);
  setMetric(m_pollInterval,
            QStringLiteral("%1 s").arg(sample.pollIntervalSeconds, 0, 'f', 0),
            QStringLiteral("Server preference"));
  setMetric(m_lastUpdated, elapsedText(sample.destinationUnixMicros));
}

void ServerCard::setSelected(const bool selected) {
  setProperty("selected", selected);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void ServerCard::setTimeActionEnabled(const bool enabled) const {
  m_setTimeButton->setEnabled(enabled);
}

int ServerCard::serverId() const { return m_serverId; }

void ServerCard::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit selected(m_serverId);
  }
  QFrame::mousePressEvent(event);
}

ServerCard::MetricLabels ServerCard::createMetric(const QString &caption,
                                                   QWidget *parent,
                                                   const QString &objectName) {
  MetricLabels metric;
  QWidget *container =
      metricContainer(parent, &metric.value, &metric.sub, caption);
  if (!objectName.isEmpty()) {
    container->setObjectName(objectName);
  }
  return metric;
}

void ServerCard::setMetric(const MetricLabels &metric, const QString &value,
                           const QString &sub, const QString &color) {
  metric.value->setText(value);
  metric.value->setStyleSheet(color.isEmpty()
                                  ? QString()
                                  : QStringLiteral("color: %1;").arg(color));
  metric.sub->setText(sub);
  metric.sub->setVisible(!sub.isEmpty());
}
