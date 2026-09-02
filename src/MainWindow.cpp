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
#include "MainWindow.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "FormatUtils.h"
#include "ServerCard.h"
#include "ServerDialog.h"
#include "ServerListYaml.h"

namespace {

constexpr auto kLastServerListDirectoryKey =
    "files/lastServerListDirectory";

QString serverListDirectory() {
  QSettings settings;
  QString directory =
      settings.value(QString::fromLatin1(kLastServerListDirectoryKey))
          .toString();
  if (directory.isEmpty() || !QDir(directory).exists()) {
    directory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  }
  if (directory.isEmpty() || !QDir(directory).exists()) {
    directory = QDir::homePath();
  }
  return directory;
}

void rememberServerListDirectory(const QString &fileName) {
  QSettings settings;
  settings.setValue(QString::fromLatin1(kLastServerListDirectoryKey),
                    QFileInfo(fileName).absolutePath());
}

QToolButton *toolbarButton(QWidget *parent, const QString &text,
                           const QIcon &icon, const QString &toolTip) {
  auto *button = new QToolButton(parent);
  button->setText(text);
  button->setIcon(icon);
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  button->setToolTip(toolTip);
  button->setAutoRaise(false);
  return button;
}

QLabel *legend(QWidget *parent, const QString &text, const QString &color) {
  auto *label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("Legend"));
  label->setStyleSheet(QStringLiteral("color: %1;").arg(color));
  return label;
}

QTableWidget *comparisonTable(QWidget *parent, const QStringList &headers,
                              const int lastColumnWidth) {
  auto *table = new QTableWidget(parent);
  table->setColumnCount(static_cast<int>(headers.size()));
  table->setHorizontalHeaderLabels(headers);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setShowGrid(false);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->setVisible(false);
  auto *header = table->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::ResizeToContents);
  header->setStretchLastSection(false);
  const int lastColumn = table->columnCount() - 1;
  header->setSectionResizeMode(lastColumn, QHeaderView::Fixed);
  table->setColumnWidth(lastColumn, lastColumnWidth);
  table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  return table;
}

QTableWidgetItem *tableItem(const QString &text, const QString &color = {}) {
  auto *item = new QTableWidgetItem(text);
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  font.setPointSize(9);
  const int resolvedPixelSize = QFontInfo(font).pixelSize();
  if (resolvedPixelSize > 0) {
    font.setPixelSize(resolvedPixelSize + 2);
  }
  item->setFont(font);
  if (!color.isEmpty()) {
    item->setForeground(QColor(color));
  }
  return item;
}

void sizeTableToRows(QTableWidget *table) {
  constexpr int rowHeight = 31;
  table->verticalHeader()->setDefaultSectionSize(rowHeight);
  table->setFixedHeight(table->horizontalHeader()->height() +
                        table->rowCount() * rowHeight + 3);
}

QString displayFrequency(const NtpSample &sample) {
  if (!sample.frequencyPpm.has_value()) {
    return QStringLiteral("Collecting");
  }
  return QStringLiteral("%1%2 ppm")
      .arg(*sample.frequencyPpm >= 0.0 ? QStringLiteral("+") : QString())
      .arg(*sample.frequencyPpm, 0, 'f', 3);
}

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ntpClient(this), m_systemTimeSetter(this) {
  setupUi();
  setupMenus();
  applyStyle();
  loadSettings();
  rebuildCards();
  updateViews();
  updateClock();

  connect(&m_clockTimer, &QTimer::timeout, this, &MainWindow::updateClock);
  m_clockTimer.start(100);
  connect(&m_pollTimer, &QTimer::timeout, this, &MainWindow::queryAllServers);

  connect(&m_ntpClient, &NtpClient::statusChanged, this,
          &MainWindow::handleStatusChanged);
  connect(&m_ntpClient, &NtpClient::querySucceeded, this,
          &MainWindow::handleQuerySucceeded);
  connect(&m_ntpClient, &NtpClient::queryFailed, this,
          &MainWindow::handleQueryFailed);
  connect(&m_systemTimeSetter, &SystemTimeSetter::finished, this,
          &MainWindow::handleSystemTimeSetFinished);

  applyPollingState(false);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  saveSettings();
  QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  const int availableWidth =
      m_cardScrollArea == nullptr ? width() : m_cardScrollArea->viewport()->width();
  const int desiredColumns = availableWidth >= 1120 ? 2 : 1;
  if (desiredColumns != m_cardColumns) {
    m_cardColumns = desiredColumns;
    layoutCards();
  }
}

void MainWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  QTimer::singleShot(0, this, [this] {
    const int availableWidth = m_cardScrollArea == nullptr
                                   ? width()
                                   : m_cardScrollArea->viewport()->width();
    m_cardColumns = availableWidth >= 1120 ? 2 : 1;
    layoutCards();
  });
  if (!m_initialQueryScheduled) {
    m_initialQueryScheduled = true;
    QTimer::singleShot(250, this, &MainWindow::queryAllServers);
  }
}

void MainWindow::queryAllServers() {
  if (clockWorkflowActive()) {
    return;
  }
  QList<int> serverIds;
  for (const ServerState &server : std::as_const(m_servers)) {
    if (!m_ntpClient.isBusy(server.config.id)) {
      serverIds.append(server.config.id);
    }
  }
  queryServerIds(serverIds);
}

void MainWindow::loadServerList() {
  if (clockWorkflowActive()) {
    QMessageBox::information(
        this, QStringLiteral("Clock update in progress"),
        QStringLiteral("Wait for the system clock update to finish before "
                       "loading a server list."));
    return;
  }

  const QString fileName = QFileDialog::getOpenFileName(
      this, QStringLiteral("Load NTP server list"), serverListDirectory(),
      QStringLiteral("YAML files (*.yaml *.yml);;All files (*)"));
  if (fileName.isEmpty()) {
    return;
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMessageBox::warning(
        this, QStringLiteral("Cannot load server list"),
        QStringLiteral("Could not open the file:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), file.errorString()));
    return;
  }
  const QByteArray yaml = file.read(ServerListYaml::kMaximumFileSize + 1);
  if (file.error() != QFileDevice::NoError) {
    QMessageBox::warning(
        this, QStringLiteral("Cannot load server list"),
        QStringLiteral("Could not read the file:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), file.errorString()));
    return;
  }

  QString parseError;
  const std::optional<QVector<ServerConfig>> loadedServers =
      ServerListYaml::parse(yaml, &parseError);
  if (!loadedServers.has_value()) {
    QMessageBox::warning(
        this, QStringLiteral("Invalid server list"),
        QStringLiteral("The server list could not be loaded:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), parseError));
    return;
  }

  QMessageBox confirmation(
      QMessageBox::Question, QStringLiteral("Load NTP server list"),
      QStringLiteral("Load %1 server(s) from %2?")
          .arg(loadedServers->size())
          .arg(QFileInfo(fileName).fileName()),
      QMessageBox::Yes | QMessageBox::No, this);
  confirmation.setTextFormat(Qt::PlainText);
  confirmation.setInformativeText(
      QStringLiteral("This replaces the %1 currently configured server(s).")
          .arg(m_servers.size()));
  confirmation.setDefaultButton(QMessageBox::No);
  if (confirmation.exec() != QMessageBox::Yes) {
    return;
  }

  m_pollTimer.stop();
  m_ntpClient.cancelAll();
  m_servers.clear();
  m_servers.reserve(loadedServers->size());
  for (ServerConfig config : *loadedServers) {
    ServerState state;
    config.id = m_nextServerId++;
    state.config = std::move(config);
    m_servers.append(std::move(state));
  }
  m_selectedServerId = m_servers.front().config.id;

  rememberServerListDirectory(fileName);
  saveSettings();
  rebuildCards();
  applyPollingState(false);
  updateViews();
  statusBar()->showMessage(
      QStringLiteral("Loaded %1 NTP server(s) from %2.")
          .arg(m_servers.size())
          .arg(QFileInfo(fileName).fileName()),
      5000);
  queryAllServers();
}

void MainWindow::saveServerList() {
  QVector<ServerConfig> servers;
  servers.reserve(m_servers.size());
  for (const ServerState &state : std::as_const(m_servers)) {
    servers.append(state.config);
  }

  QString yamlError;
  const std::optional<QByteArray> yaml =
      ServerListYaml::serialize(servers, &yamlError);
  if (!yaml.has_value()) {
    QMessageBox::warning(this, QStringLiteral("Cannot save server list"),
                         yamlError);
    return;
  }

  const QString suggestedFile =
      QDir(serverListDirectory())
          .filePath(QStringLiteral("optime-ntp-servers.yaml"));
  QString fileName = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save NTP server list"), suggestedFile,
      QStringLiteral("YAML files (*.yaml *.yml);;All files (*)"));
  if (fileName.isEmpty()) {
    return;
  }
  if (QFileInfo(fileName).suffix().isEmpty()) {
    fileName.append(QStringLiteral(".yaml"));
  }

  QSaveFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(
        this, QStringLiteral("Cannot save server list"),
        QStringLiteral("Could not create the file:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), file.errorString()));
    return;
  }
  if (file.write(*yaml) != yaml->size()) {
    const QString error = file.errorString();
    file.cancelWriting();
    QMessageBox::warning(
        this, QStringLiteral("Cannot save server list"),
        QStringLiteral("Could not write the file:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), error));
    return;
  }
  if (!file.commit()) {
    QMessageBox::warning(
        this, QStringLiteral("Cannot save server list"),
        QStringLiteral("Could not finish writing the file:\n%1\n\n%2")
            .arg(QDir::toNativeSeparators(fileName), file.errorString()));
    return;
  }

  rememberServerListDirectory(fileName);
  statusBar()->showMessage(
      QStringLiteral("Saved %1 NTP server(s) to %2.")
          .arg(servers.size())
          .arg(QFileInfo(fileName).fileName()),
      5000);
}

void MainWindow::addServer() {
  if (clockWorkflowActive()) {
    return;
  }
  if (m_servers.size() >= kMaximumNtpServers) {
    QMessageBox::information(this, QStringLiteral("Server limit reached"),
                             QStringLiteral("OpTime NTP supports up to 10 servers."));
    return;
  }

  ServerDialog dialog(QStringLiteral("Add NTP server"), this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  if (serverHostExists(dialog.serverHost())) {
    QMessageBox::warning(this, QStringLiteral("Duplicate server"),
                         QStringLiteral("That server address is already configured."));
    return;
  }

  ServerState state;
  state.config.id = m_nextServerId++;
  state.config.label = dialog.serverLabel();
  state.config.host = dialog.serverHost();
  m_servers.append(state);
  m_selectedServerId = state.config.id;
  saveSettings();
  rebuildCards();
  updateViews();
  queryServerIds({state.config.id});
}

void MainWindow::editSelectedServer() {
  if (clockWorkflowActive()) {
    return;
  }
  ServerState *state = findState(m_selectedServerId);
  if (state == nullptr) {
    QMessageBox::information(this, QStringLiteral("Select a server"),
                             QStringLiteral("Select a server card or table row first."));
    return;
  }

  ServerDialog dialog(QStringLiteral("Edit NTP server"), this);
  dialog.setServer(state->config);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  if (serverHostExists(dialog.serverHost(), state->config.id)) {
    QMessageBox::warning(this, QStringLiteral("Duplicate server"),
                         QStringLiteral("That server address is already configured."));
    return;
  }

  const bool addressChanged =
      state->config.host.compare(dialog.serverHost(), Qt::CaseInsensitive) != 0;
  state->config.label = dialog.serverLabel();
  state->config.host = dialog.serverHost();
  if (addressChanged) {
    state->status = QueryStatus::Idle;
    state->statusMessage.clear();
    state->sample.reset();
    state->history.clear();
    state->reachability = 0;
  }
  saveSettings();
  rebuildCards();
  updateViews();
  if (addressChanged) {
    queryServerIds({state->config.id});
  }
}

void MainWindow::removeSelectedServer() {
  if (clockWorkflowActive()) {
    return;
  }
  ServerState *state = findState(m_selectedServerId);
  if (state == nullptr) {
    QMessageBox::information(this, QStringLiteral("Select a server"),
                             QStringLiteral("Select a server card or table row first."));
    return;
  }
  if (m_servers.size() == 1) {
    QMessageBox::information(this, QStringLiteral("One server is required"),
                             QStringLiteral("Add another server before removing this one."));
    return;
  }

  const QMessageBox::StandardButton answer = QMessageBox::question(
      this, QStringLiteral("Remove NTP server"),
      QStringLiteral("Remove %1 (%2)?")
          .arg(state->config.label, state->config.host),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer != QMessageBox::Yes) {
    return;
  }

  const int row = selectedRow();
  m_servers.removeAt(row);
  const int nextRow =
      std::min(row, static_cast<int>(m_servers.size()) - 1);
  m_selectedServerId = m_servers[nextRow].config.id;
  saveSettings();
  rebuildCards();
  updateViews();
}

void MainWindow::updateClock() {
  const QDateTime local = QDateTime::currentDateTime();
  m_localClockLabel->setText(
      QStringLiteral("%1 %2")
          .arg(local.toString(QStringLiteral("ddd, yyyy-MM-dd HH:mm:ss.zzz")),
               local.timeZoneAbbreviation()));

  const int offsetSeconds = local.offsetFromUtc();
  const int absoluteOffsetMinutes = std::abs(offsetSeconds) / 60;
  m_clockDetailLabel->setText(
      QStringLiteral("LOCAL MACHINE  |  UTC%1%2:%3  |  Unix %4")
          .arg(offsetSeconds < 0 ? QStringLiteral("-") : QStringLiteral("+"))
          .arg(absoluteOffsetMinutes / 60, 2, 10, QLatin1Char('0'))
          .arg(absoluteOffsetMinutes % 60, 2, 10, QLatin1Char('0'))
          .arg(local.toSecsSinceEpoch()));

  ++m_clockTicks;
  if (m_clockTicks % 10 == 0) {
    updateCards();
  }
}

void MainWindow::setupUi() {
  setWindowTitle(QStringLiteral("OpTime NTP"));
  resize(1500, 900);
  setMinimumSize(1280, 720);

  auto *central = new QWidget(this);
  central->setObjectName(QStringLiteral("ApplicationRoot"));
  auto *root = new QVBoxLayout(central);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  setCentralWidget(central);

  auto *topBar = new QFrame(central);
  topBar->setObjectName(QStringLiteral("TopBar"));
  auto *topLayout = new QHBoxLayout(topBar);
  topLayout->setContentsMargins(15, 8, 15, 8);
  topLayout->setSpacing(8);

  m_pollingButton = toolbarButton(
      topBar, QStringLiteral("Polling Active"),
      QIcon(QStringLiteral(":/icons/play.svg")),
      QStringLiteral("Start or stop automatic NTP queries"));
  m_pollingButton->setCheckable(true);
  topLayout->addWidget(m_pollingButton);

  m_queryButton = toolbarButton(
      topBar, QStringLiteral("Query Now"),
      style()->standardIcon(QStyle::SP_BrowserReload),
      QStringLiteral("Query every configured NTP server now"));
  topLayout->addWidget(m_queryButton);

  m_addButton = toolbarButton(
      topBar, QStringLiteral("Add"),
      QIcon(QStringLiteral(":/icons/plus.svg")),
      QStringLiteral("Add an NTP server"));
  topLayout->addWidget(m_addButton);
  m_editButton = toolbarButton(
      topBar, QStringLiteral("Edit"),
      QIcon(QStringLiteral(":/icons/edit.svg")),
      QStringLiteral("Edit the selected NTP server"));
  topLayout->addWidget(m_editButton);
  m_removeButton = toolbarButton(
      topBar, QStringLiteral("Remove"),
      QIcon(QStringLiteral(":/icons/minus.svg")),
      QStringLiteral("Remove the selected NTP server"));
  topLayout->addWidget(m_removeButton);

  auto *intervalLabel = new QLabel(QStringLiteral("Interval"), topBar);
  intervalLabel->setObjectName(QStringLiteral("ToolbarLabel"));
  topLayout->addWidget(intervalLabel);
  m_intervalCombo = new QComboBox(topBar);
  for (const int seconds : {2, 5, 10, 30, 60}) {
    m_intervalCombo->addItem(QStringLiteral("%1 s").arg(seconds), seconds * 1000);
  }
  m_intervalCombo->setToolTip(QStringLiteral("Automatic polling interval"));
  topLayout->addWidget(m_intervalCombo);

  topLayout->addStretch();

  auto *clock = new QVBoxLayout;
  clock->setSpacing(1);
  m_localClockLabel = new QLabel(topBar);
  m_localClockLabel->setObjectName(QStringLiteral("LocalClock"));
  m_localClockLabel->setAlignment(Qt::AlignRight);
  m_clockDetailLabel = new QLabel(topBar);
  m_clockDetailLabel->setObjectName(QStringLiteral("ClockDetail"));
  m_clockDetailLabel->setAlignment(Qt::AlignRight);
  clock->addWidget(m_localClockLabel);
  clock->addWidget(m_clockDetailLabel);
  topLayout->addLayout(clock);
  root->addWidget(topBar);

  m_tabs = new QTabWidget(central);
  m_tabs->setObjectName(QStringLiteral("MainTabs"));
  m_tabs->setDocumentMode(true);
  m_tabs->tabBar()->setDrawBase(false);

  m_cardScrollArea = new QScrollArea(m_tabs);
  m_cardScrollArea->setObjectName(QStringLiteral("CardScrollArea"));
  m_cardScrollArea->viewport()->setObjectName(QStringLiteral("CardViewport"));
  m_cardScrollArea->setWidgetResizable(true);
  m_cardScrollArea->setFrameShape(QFrame::NoFrame);
  m_cardHost = new QWidget(m_cardScrollArea);
  m_cardHost->setObjectName(QStringLiteral("CardHost"));
  m_cardGrid = new QGridLayout(m_cardHost);
  m_cardGrid->setContentsMargins(14, 14, 14, 14);
  m_cardGrid->setHorizontalSpacing(12);
  m_cardGrid->setVerticalSpacing(12);
  m_cardGrid->setAlignment(Qt::AlignTop);
  m_cardScrollArea->setWidget(m_cardHost);
  m_tabs->addTab(m_cardScrollArea, QStringLiteral("Server Detail Cards"));

  auto *comparisonScroll = new QScrollArea(m_tabs);
  comparisonScroll->setObjectName(QStringLiteral("ComparisonScrollArea"));
  comparisonScroll->viewport()->setObjectName(
      QStringLiteral("ComparisonViewport"));
  comparisonScroll->setWidgetResizable(true);
  comparisonScroll->setFrameShape(QFrame::NoFrame);
  auto *comparisonHost = new QWidget(comparisonScroll);
  comparisonHost->setObjectName(QStringLiteral("ComparisonHost"));
  auto *comparisonLayout = new QVBoxLayout(comparisonHost);
  comparisonLayout->setContentsMargins(14, 14, 14, 14);
  comparisonLayout->setSpacing(10);

  m_offsetTableTitle = new QLabel(QStringLiteral("OFFSET COMPARISON"), comparisonHost);
  m_offsetTableTitle->setObjectName(QStringLiteral("SectionTitle"));
  comparisonLayout->addWidget(m_offsetTableTitle);
  m_offsetTable = comparisonTable(
      comparisonHost,
      {QStringLiteral("Server"), QStringLiteral("Status"),
       QStringLiteral("Server time"), QStringLiteral("Offset"),
       QStringLiteral("Precise offset"), QStringLiteral("Delta vs ref"),
       QStringLiteral("RTT"), QStringLiteral("Jitter"),
       QStringLiteral("Stratum"), QStringLiteral("Leap")},
      100);
  comparisonLayout->addWidget(m_offsetTable);

  auto *detailTitle = new QLabel(QStringLiteral("PRECISION AND SYNCHRONIZATION DETAIL"),
                                 comparisonHost);
  detailTitle->setObjectName(QStringLiteral("SectionTitle"));
  comparisonLayout->addWidget(detailTitle);
  m_detailTable = comparisonTable(
      comparisonHost,
      {QStringLiteral("Server"), QStringLiteral("Precision"),
       QStringLiteral("Root delay"), QStringLiteral("Root dispersion"),
       QStringLiteral("Server poll"), QStringLiteral("Reachability"),
       QStringLiteral("Reference ID"), QStringLiteral("Frequency estimate")},
      200);
  comparisonLayout->addWidget(m_detailTable);

  auto *leapTitle = new QLabel(QStringLiteral("LEAP SECOND INDICATORS (RFC 5905)"),
                               comparisonHost);
  leapTitle->setObjectName(QStringLiteral("SectionTitle"));
  comparisonLayout->addWidget(leapTitle);
  m_leapTable = comparisonTable(
      comparisonHost,
      {QStringLiteral("Server"), QStringLiteral("LI bits"),
       QStringLiteral("Meaning"), QStringLiteral("Last successful query")},
      320);
  comparisonLayout->addWidget(m_leapTable);
  comparisonLayout->addStretch();
  comparisonScroll->setWidget(comparisonHost);
  m_tabs->addTab(comparisonScroll, QStringLiteral("Comparison Table"));
  root->addWidget(m_tabs, 1);

  auto *applicationStatus = new QStatusBar(this);
  applicationStatus->setSizeGripEnabled(true);
  m_serversStatusLabel = new QLabel(applicationStatus);
  m_bestSourceLabel = new QLabel(applicationStatus);
  m_averageRttLabel = new QLabel(applicationStatus);
  m_protocolLabel = new QLabel(applicationStatus);
  m_stratumOneLabel =
      legend(applicationStatus, QStringLiteral("Stratum 1: 0"),
             QStringLiteral("#10b981"));
  m_stratumTwoLabel =
      legend(applicationStatus, QStringLiteral("Stratum 2: 0"),
             QStringLiteral("#60a5fa"));
  m_stratumThreePlusLabel =
      legend(applicationStatus, QStringLiteral("Stratum 3+: 0"),
             QStringLiteral("#94a3b8"));
  applicationStatus->addWidget(m_serversStatusLabel);
  applicationStatus->addWidget(m_bestSourceLabel);
  applicationStatus->addWidget(m_averageRttLabel);
  applicationStatus->addPermanentWidget(m_stratumOneLabel);
  applicationStatus->addPermanentWidget(m_stratumTwoLabel);
  applicationStatus->addPermanentWidget(m_stratumThreePlusLabel);
  applicationStatus->addPermanentWidget(m_protocolLabel);
  setStatusBar(applicationStatus);

  connect(m_pollingButton, &QToolButton::toggled, this, [this](const bool checked) {
    m_pollingActive = checked;
    applyPollingState(checked);
    saveSettings();
  });
  connect(m_queryButton, &QToolButton::clicked, this,
          &MainWindow::queryAllServers);
  connect(m_addButton, &QToolButton::clicked, this, &MainWindow::addServer);
  connect(m_editButton, &QToolButton::clicked, this,
          &MainWindow::editSelectedServer);
  connect(m_removeButton, &QToolButton::clicked, this,
          &MainWindow::removeSelectedServer);
  connect(m_intervalCombo, &QComboBox::currentIndexChanged, this,
          [this](const int index) {
            m_pollIntervalMillis = m_intervalCombo->itemData(index).toInt();
            m_pollTimer.setInterval(m_pollIntervalMillis);
            saveSettings();
          });

  auto connectTableSelection = [this](const QTableWidget *table) {
    connect(table, &QTableWidget::cellClicked, this,
            [this](const int row, int) {
              if (row >= 0 && row < m_servers.size()) {
                selectServer(m_servers[row].config.id);
              }
            });
  };
  connectTableSelection(m_offsetTable);
  connectTableSelection(m_detailTable);
  connectTableSelection(m_leapTable);
}

void MainWindow::setupMenus() {
  QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  QAction *loadAction = fileMenu->addAction(
      style()->standardIcon(QStyle::SP_DialogOpenButton),
      QStringLiteral("&Load Server List..."));
  loadAction->setShortcut(QKeySequence::Open);
  QAction *saveAction = fileMenu->addAction(
      style()->standardIcon(QStyle::SP_DialogSaveButton),
      QStringLiteral("&Save Server List As..."));
  saveAction->setShortcut(QKeySequence::SaveAs);
  fileMenu->addSeparator();
  QAction *exitAction = fileMenu->addAction(QStringLiteral("E&xit"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(loadAction, &QAction::triggered, this, &MainWindow::loadServerList);
  connect(saveAction, &QAction::triggered, this, &MainWindow::saveServerList);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  QMenu *serverMenu = menuBar()->addMenu(QStringLiteral("&Servers"));
  QAction *addAction = serverMenu->addAction(QStringLiteral("&Add Server..."));
  addAction->setShortcut(QKeySequence::New);
  m_editAction = serverMenu->addAction(QStringLiteral("&Edit Selected..."));
  m_removeAction = serverMenu->addAction(QStringLiteral("&Remove Selected"));
  serverMenu->addSeparator();
  QAction *queryAction = serverMenu->addAction(QStringLiteral("&Query Now"));
  queryAction->setShortcut(QKeySequence(Qt::Key_F5));
  connect(addAction, &QAction::triggered, this, &MainWindow::addServer);
  connect(m_editAction, &QAction::triggered, this,
          &MainWindow::editSelectedServer);
  connect(m_removeAction, &QAction::triggered, this,
          &MainWindow::removeSelectedServer);
  connect(queryAction, &QAction::triggered, this,
          &MainWindow::queryAllServers);

  QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
  QAction *aboutAction = helpMenu->addAction(QStringLiteral("&About OpTime NTP"));
  connect(aboutAction, &QAction::triggered, this, [this] {
    QMessageBox::about(
        this, QStringLiteral("About OpTime NTP"),
        QStringLiteral("<b>OpTime NTP %1</b><br><br>"
                       "Compares the local computer clock with up to ten NTPv4 "
                       "servers using the four-timestamp offset calculation.<br>"
                       "<br>Copyright (C) 2026 Andrew Kevin Bailey<br><br>"
                       "Built with C++20 and Qt 6.10.3. Application source is MIT "
                       "licensed. NTP client design incorporates ideas from the "
                       "MIT-licensed elrinor/qntp project. YAML support uses the "
                       "MIT-licensed yaml-cpp library.")
            .arg(QCoreApplication::applicationVersion()));
  });
}

void MainWindow::applyStyle() {
  auto *application =
      qobject_cast<QApplication *>(QCoreApplication::instance());
  if (application == nullptr) {
    return;
  }

  QPalette darkPalette = QApplication::palette();
  darkPalette.setColor(QPalette::Window, QColor(QStringLiteral("#070b0f")));
  darkPalette.setColor(QPalette::WindowText,
                       QColor(QStringLiteral("#c8d3e0")));
  darkPalette.setColor(QPalette::Base, QColor(QStringLiteral("#0d1117")));
  darkPalette.setColor(QPalette::AlternateBase,
                       QColor(QStringLiteral("#0a0f15")));
  darkPalette.setColor(QPalette::Text, QColor(QStringLiteral("#c8d3e0")));
  darkPalette.setColor(QPalette::Button, QColor(QStringLiteral("#111827")));
  darkPalette.setColor(QPalette::ButtonText,
                       QColor(QStringLiteral("#c8d3e0")));
  darkPalette.setColor(QPalette::ToolTipBase,
                       QColor(QStringLiteral("#374151")));
  darkPalette.setColor(QPalette::ToolTipText,
                       QColor(QStringLiteral("#e2e8f0")));
  darkPalette.setColor(QPalette::Highlight,
                       QColor(QStringLiteral("#0d2846")));
  darkPalette.setColor(QPalette::HighlightedText,
                       QColor(QStringLiteral("#e2e8f0")));
  QApplication::setPalette(darkPalette);

  application->setStyleSheet(QStringLiteral(R"QSS(
    * {
      font-family: "Segoe UI", "Inter", sans-serif;
      color: #c8d3e0;
    }
    QMainWindow, #ApplicationRoot, #MainTabs,
    #CardScrollArea, #CardViewport, #CardHost,
    #ComparisonScrollArea, #ComparisonViewport, #ComparisonHost {
      background: #070b0f;
    }
    QMenuBar, QMenu {
      background: #0a0d12;
      color: #94a3b8;
      border-color: #1b2430;
    }
    QMenuBar::item:selected, QMenu::item:selected {
      background: #111827;
      color: #e2e8f0;
    }
    QToolTip {
      background: #374151;
      color: #e2e8f0;
      border: 1px solid #4b5563;
      padding: 4px 6px;
    }
    #TopBar {
      background: #0a0d12;
      border-bottom: 1px solid #161b22;
    }
    #ToolbarLabel {
      color: #526075;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 12px;
    }
    #Legend {
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 12px;
      padding: 0 5px;
    }
    #LocalClock {
      color: #60a5fa;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 14px;
    }
    #ClockDetail {
      color: #526075;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 11px;
    }
    QToolButton, QPushButton, QComboBox {
      background: #111827;
      border: 1px solid #2a3040;
      border-radius: 4px;
      padding: 5px 9px;
      color: #94a3b8;
      font-size: 12px;
    }
    QToolButton:hover, QPushButton:hover, QComboBox:hover {
      border-color: #3b82f6;
      color: #e2e8f0;
    }
    QToolButton:checked {
      background: #0d1f35;
      border-color: #1e3a5f;
      color: #60a5fa;
    }
    QTabWidget::pane { background: #070b0f; border: none; }
    QTabBar { background: #0a0d12; }
    QTabBar::tab {
      background: #0a0d12;
      border: none;
      border-bottom: 2px solid transparent;
      color: #526075;
      padding: 9px 17px;
      font-size: 14px;
      font-weight: 600;
    }
    QTabBar::tab:selected {
      color: #60a5fa;
      border-bottom-color: #3b82f6;
    }
    #ServerCard {
      background: #0d1117;
      border: 1px solid #1b2430;
      border-radius: 6px;
    }
    #ServerCard[selected="true"] { border-color: #3b82f6; }
    #ServerIndex {
      background: #0d1f35;
      border: 1px solid #1e3a5f;
      border-radius: 4px;
      color: #60a5fa;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-weight: 700;
      font-size: 14px;
    }
    #ServerName { color: #e2e8f0; font-size: 18px; font-weight: 600; }
    #ServerHost, #MetricSub {
      color: #526075;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 13px;
    }
    #ServerStatus {
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 12px;
      font-weight: 600;
    }
    #SetTimeButton {
      background: transparent;
      border: none;
      color: #60a5fa;
      font-size: 13px;
      padding: 4px 6px;
    }
    #SetTimeButton:hover { background: #0d1f35; color: #bfdbfe; }
    #SetTimeButton:pressed { background: #102a46; }
    #SetTimeButton:disabled { background: transparent; color: #3b4657; }
    #TimeBand { background: #080c11; border-radius: 3px; }
    #MetricCaption {
      color: #526075;
      font-size: 12px;
      font-weight: 600;
    }
    #MetricValue, #TimeValue, #TimeValueAccent, #PreciseOffset, #OffsetValue {
      font-family: "Consolas", "JetBrains Mono", monospace;
    }
    #MetricValue { color: #a8b4c5; font-size: 14px; }
    #TimeValue { color: #94a3b8; font-size: 14px; }
    #TimeValueAccent { color: #60a5fa; font-size: 14px; }
    #OffsetValue { color: #06b6d4; font-size: 20px; font-weight: 600; }
    #PreciseOffset { color: #64748b; font-size: 13px; }
    #SectionTitle {
      color: #64748b;
      font-size: 14px;
      font-weight: 700;
    }
    QTableWidget {
      background: #0d1117;
      alternate-background-color: #0a0f15;
      border: 1px solid #1b2430;
      border-radius: 4px;
      selection-background-color: #0d2846;
      selection-color: #e2e8f0;
    }
    QHeaderView::section {
      background: #0a0d12;
      color: #526075;
      border: none;
      border-bottom: 1px solid #1b2430;
      padding: 6px 9px;
      font-size: 14px;
      font-weight: 700;
    }
    QStatusBar {
      background: #080b0f;
      border-top: 1px solid #161b22;
      font-size: 12px;
    }
    QStatusBar QLabel {
      color: #64748b;
      font-family: "Consolas", "JetBrains Mono", monospace;
      font-size: 12px;
      padding: 0 8px;
    }
    QScrollBar:vertical { background: #080b0f; width: 10px; }
    QScrollBar::handle:vertical { background: #273244; min-height: 30px; border-radius: 4px; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    QDialog, QMessageBox { background: #0d1117; }
    #DialogDescription { color: #94a3b8; }
    QLineEdit {
      background: #080c11;
      border: 1px solid #2a3040;
      border-radius: 4px;
      padding: 6px 8px;
      color: #e2e8f0;
    }
    QLineEdit:focus { border-color: #3b82f6; }
  )QSS"));
}

void MainWindow::loadSettings() {
  QSettings settings;
  const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }
  m_pollingActive = settings.value(QStringLiteral("polling/enabled"), true).toBool();
  m_pollIntervalMillis =
      settings.value(QStringLiteral("polling/intervalMillis"), 10'000).toInt();
  if (!QList<int>{2000, 5000, 10'000, 30'000, 60'000}.contains(
          m_pollIntervalMillis)) {
    m_pollIntervalMillis = 10'000;
  }

  const int count = settings.beginReadArray(QStringLiteral("servers"));
  for (int index = 0; index < count && index < kMaximumNtpServers; ++index) {
    settings.setArrayIndex(index);
    ServerState state;
    state.config.id = settings.value(QStringLiteral("id"), m_nextServerId).toInt();
    state.config.label = settings.value(QStringLiteral("label")).toString();
    state.config.host = settings.value(QStringLiteral("host")).toString();
    if (!state.config.host.trimmed().isEmpty()) {
      if (state.config.label.trimmed().isEmpty()) {
        state.config.label = state.config.host;
      }
      m_servers.append(state);
      m_nextServerId = std::max(m_nextServerId, state.config.id + 1);
    }
  }
  settings.endArray();

  if (m_servers.isEmpty()) {
    constexpr std::array<std::pair<const char *, const char *>, 4> defaults = {{
        {"NTP Pool", "pool.ntp.org"},
        {"Cloudflare", "time.cloudflare.com"},
        {"Google Time", "time.google.com"},
        {"Windows Time", "time.windows.com"},
    }};
    for (const auto &[label, host] : defaults) {
      ServerState state;
      state.config.id = m_nextServerId++;
      state.config.label = QString::fromLatin1(label);
      state.config.host = QString::fromLatin1(host);
      m_servers.append(state);
    }
  }

  m_selectedServerId = m_servers.front().config.id;
  m_pollingButton->blockSignals(true);
  m_pollingButton->setChecked(m_pollingActive);
  m_pollingButton->blockSignals(false);
  const int intervalIndex = m_intervalCombo->findData(m_pollIntervalMillis);
  m_intervalCombo->blockSignals(true);
  m_intervalCombo->setCurrentIndex(std::max(0, intervalIndex));
  m_intervalCombo->blockSignals(false);
}

void MainWindow::saveSettings() const {
  QSettings settings;
  settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("polling/enabled"), m_pollingActive);
  settings.setValue(QStringLiteral("polling/intervalMillis"),
                    m_pollIntervalMillis);
  const int serverCount = static_cast<int>(m_servers.size());
  settings.beginWriteArray(QStringLiteral("servers"), serverCount);
  for (int index = 0; index < serverCount; ++index) {
    settings.setArrayIndex(index);
    settings.setValue(QStringLiteral("id"), m_servers[index].config.id);
    settings.setValue(QStringLiteral("label"), m_servers[index].config.label);
    settings.setValue(QStringLiteral("host"), m_servers[index].config.host);
  }
  settings.endArray();
}

void MainWindow::applyPollingState(const bool queryImmediately) {
  m_pollingButton->blockSignals(true);
  m_pollingButton->setChecked(m_pollingActive);
  m_pollingButton->setText(m_pollingActive ? QStringLiteral("Polling Active")
                                           : QStringLiteral("Start Polling"));
  m_pollingButton->setIcon(
      m_pollingActive ? QIcon(QStringLiteral(":/icons/play.svg"))
                      : QIcon(QStringLiteral(":/icons/pause.svg")));
  m_pollingButton->blockSignals(false);

  m_pollTimer.setInterval(m_pollIntervalMillis);
  if (m_pollingActive) {
    m_pollTimer.start();
    if (queryImmediately) {
      queryAllServers();
    }
  } else {
    m_pollTimer.stop();
  }
}

void MainWindow::rebuildCards() {
  while (QLayoutItem *item = m_cardGrid->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }
  m_cards.clear();

  for (const ServerState &state : std::as_const(m_servers)) {
    auto *card = new ServerCard(state.config.id, m_cardHost);
    connect(card, &ServerCard::selected, this, &MainWindow::selectServer);
    connect(card, &ServerCard::setLocalTimeRequested, this,
            &MainWindow::requestLocalTimeSet);
    m_cards.insert(state.config.id, card);
  }
  layoutCards();
}

void MainWindow::layoutCards() {
  while (QLayoutItem *item = m_cardGrid->takeAt(0)) {
    delete item;
  }
  for (int index = 0; index < m_servers.size(); ++index) {
    ServerCard *card = m_cards.value(m_servers[index].config.id);
    if (card != nullptr) {
      m_cardGrid->addWidget(card, index / m_cardColumns,
                            index % m_cardColumns);
    }
  }
  for (int column = 0; column < 2; ++column) {
    m_cardGrid->setColumnStretch(column, column < m_cardColumns ? 1 : 0);
  }
}

void MainWindow::updateViews() {
  updateCards();
  updateComparisonTables();
  updateStatusBar();
  const bool controlsEnabled = !clockWorkflowActive();
  const bool hasSelection = findState(m_selectedServerId) != nullptr;
  m_pollingButton->setEnabled(controlsEnabled);
  m_queryButton->setEnabled(controlsEnabled);
  m_intervalCombo->setEnabled(controlsEnabled);
  m_editButton->setEnabled(controlsEnabled && hasSelection);
  m_removeButton->setEnabled(controlsEnabled && hasSelection &&
                             m_servers.size() > 1);
  m_editAction->setEnabled(controlsEnabled && hasSelection);
  m_removeAction->setEnabled(controlsEnabled && hasSelection &&
                             m_servers.size() > 1);
  m_addButton->setEnabled(controlsEnabled &&
                          m_servers.size() < kMaximumNtpServers);
}

void MainWindow::updateCards() {
  for (int index = 0; index < m_servers.size(); ++index) {
    ServerCard *card = m_cards.value(m_servers[index].config.id);
    if (card != nullptr) {
      card->setState(m_servers[index], index);
      card->setSelected(m_servers[index].config.id == m_selectedServerId);
      card->setTimeActionEnabled(!clockWorkflowActive());
    }
  }
}

void MainWindow::updateComparisonTables() {
  const ServerState *reference = nullptr;
  for (const ServerState &state : std::as_const(m_servers)) {
    if (state.status == QueryStatus::Synchronized && state.sample.has_value() &&
        (reference == nullptr ||
         std::abs(state.sample->offsetMicros) <
             std::abs(reference->sample->offsetMicros))) {
      reference = &state;
    }
  }
  m_offsetTableTitle->setText(
      reference == nullptr
          ? QStringLiteral("OFFSET COMPARISON")
          : QStringLiteral("OFFSET COMPARISON | RELATIVE TO %1")
                .arg(reference->config.label.toUpper()));

  const int serverCount = static_cast<int>(m_servers.size());
  const QList<QTableWidget *> tables = {m_offsetTable, m_detailTable, m_leapTable};
  for (QTableWidget *table : tables) {
    table->blockSignals(true);
    table->clearContents();
    table->setRowCount(serverCount);
  }

  for (int row = 0; row < serverCount; ++row) {
    const ServerState &state = m_servers[row];
    const QString statusColor = FormatUtils::statusColor(state.status);
    m_offsetTable->setItem(row, 0, tableItem(state.config.label));
    m_offsetTable->setItem(
        row, 1, tableItem(FormatUtils::status(state.status), statusColor));
    m_detailTable->setItem(row, 0, tableItem(state.config.label));
    m_leapTable->setItem(row, 0, tableItem(state.config.label));

    if (!state.sample.has_value()) {
      for (int column = 2; column < m_offsetTable->columnCount(); ++column) {
        m_offsetTable->setItem(row, column, tableItem(QStringLiteral("--")));
      }
      for (int column = 1; column < m_detailTable->columnCount(); ++column) {
        m_detailTable->setItem(row, column, tableItem(QStringLiteral("--")));
      }
      for (int column = 1; column < m_leapTable->columnCount(); ++column) {
        m_leapTable->setItem(row, column, tableItem(QStringLiteral("--")));
      }
      continue;
    }

    const NtpSample &sample = *state.sample;
    const QString offsetColor = std::abs(sample.offsetMicros) >= 10'000.0
                                    ? QStringLiteral("#f59e0b")
                                    : QStringLiteral("#06b6d4");
    m_offsetTable->setItem(
        row, 2, tableItem(FormatUtils::utcTimestamp(sample.transmitUnixMicros)));
    m_offsetTable->setItem(
        row, 3, tableItem(FormatUtils::offsetHundredths(sample.offsetMicros),
                          offsetColor));
    m_offsetTable->setItem(
        row, 4,
        tableItem(FormatUtils::preciseOffset(sample.offsetMicros), offsetColor));
    QString delta = QStringLiteral("--");
    if (reference != nullptr && reference != &state) {
      delta = FormatUtils::preciseOffset(
          sample.offsetMicros - reference->sample->offsetMicros);
    }
    m_offsetTable->setItem(row, 5, tableItem(delta));
    m_offsetTable->setItem(
        row, 6, tableItem(FormatUtils::milliseconds(sample.roundTripMicros)));
    m_offsetTable->setItem(
        row, 7,
        tableItem(state.history.size() < 2
                      ? QStringLiteral("Collecting")
                      : QStringLiteral("%1 ms").arg(sample.jitterMillis, 0, 'f', 3)));
    m_offsetTable->setItem(row, 8, tableItem(QString::number(sample.stratum)));
    m_offsetTable->setItem(
        row, 9, tableItem(FormatUtils::leapBits(sample.leapIndicator),
                          sample.leapIndicator == 0 ? QStringLiteral("#10b981")
                                                    : QStringLiteral("#f59e0b")));

    m_detailTable->setItem(
        row, 1,
        tableItem(FormatUtils::precision(sample.precisionExponent,
                                         sample.precisionSeconds)));
    m_detailTable->setItem(
        row, 2,
        tableItem(QStringLiteral("%1 ms").arg(sample.rootDelayMillis, 0, 'f', 3)));
    m_detailTable->setItem(
        row, 3,
        tableItem(QStringLiteral("%1 ms")
                      .arg(sample.rootDispersionMillis, 0, 'f', 3)));
    m_detailTable->setItem(
        row, 4,
        tableItem(QStringLiteral("%1 s").arg(sample.pollIntervalSeconds, 0, 'f', 0)));
    m_detailTable->setItem(
        row, 5, tableItem(FormatUtils::reachability(state.reachability)));
    m_detailTable->setItem(row, 6, tableItem(sample.referenceId));
    m_detailTable->setItem(row, 7, tableItem(displayFrequency(sample)));

    m_leapTable->setItem(row, 1,
                         tableItem(FormatUtils::leapBits(sample.leapIndicator)));
    m_leapTable->setItem(
        row, 2, tableItem(FormatUtils::leapIndicator(sample.leapIndicator)));
    m_leapTable->setItem(
        row, 3,
        tableItem(FormatUtils::localTimestamp(sample.destinationUnixMicros)));
  }

  for (QTableWidget *table : tables) {
    sizeTableToRows(table);
    table->blockSignals(false);
  }
  const int row = selectedRow();
  if (row >= 0) {
    for (QTableWidget *table : tables) {
      table->selectRow(row);
    }
  }
}

void MainWindow::updateStatusBar() {
  QVector<const ServerState *> synchronizedServers;
  int stratumOneCount = 0;
  int stratumTwoCount = 0;
  int stratumThreePlusCount = 0;
  for (const ServerState &state : std::as_const(m_servers)) {
    if (state.sample.has_value()) {
      if (state.sample->stratum == 1) {
        ++stratumOneCount;
      } else if (state.sample->stratum == 2) {
        ++stratumTwoCount;
      } else if (state.sample->stratum >= 3) {
        ++stratumThreePlusCount;
      }
    }
    if (state.status == QueryStatus::Synchronized && state.sample.has_value()) {
      synchronizedServers.append(&state);
    }
  }
  m_stratumOneLabel->setText(
      QStringLiteral("Stratum 1: %1").arg(stratumOneCount));
  m_stratumTwoLabel->setText(
      QStringLiteral("Stratum 2: %1").arg(stratumTwoCount));
  m_stratumThreePlusLabel->setText(
      QStringLiteral("Stratum 3+: %1").arg(stratumThreePlusCount));
  m_serversStatusLabel->setText(
      QStringLiteral("Servers: %1/%2 synchronized")
          .arg(synchronizedServers.size())
          .arg(m_servers.size()));

  if (synchronizedServers.isEmpty()) {
    m_bestSourceLabel->setText(QStringLiteral("Best source: --"));
    m_averageRttLabel->setText(QStringLiteral("Average RTT: --"));
  } else {
    const ServerState *best = *std::ranges::min_element(
        synchronizedServers,
        [](const ServerState *left, const ServerState *right) {
          return std::abs(left->sample->offsetMicros) <
                 std::abs(right->sample->offsetMicros);
        });
    m_bestSourceLabel->setText(
        QStringLiteral("Best source: %1 (%2)")
            .arg(best->config.label,
                 FormatUtils::preciseOffset(best->sample->offsetMicros)));
    const double totalRtt = std::accumulate(
        synchronizedServers.cbegin(), synchronizedServers.cend(), 0.0,
        [](const double total, const ServerState *state) {
          return total + state->sample->roundTripMicros;
        });
    m_averageRttLabel->setText(
        QStringLiteral("Average RTT: %1")
            .arg(FormatUtils::milliseconds(
                totalRtt /
                    static_cast<double>(synchronizedServers.size()),
                2)));
  }
  m_protocolLabel->setText(
      QStringLiteral("RFC 5905 | NTPv4 | Query #%1").arg(m_queryCount));
}

void MainWindow::selectServer(const int serverId) {
  if (findState(serverId) == nullptr) {
    return;
  }
  m_selectedServerId = serverId;
  updateViews();
}

void MainWindow::requestLocalTimeSet(const int serverId) {
  ServerState *source = findState(serverId);
  if (source == nullptr || clockWorkflowActive()) {
    return;
  }

  m_ntpClient.cancelAll();
  for (ServerState &state : m_servers) {
    state.requestActive = false;
    if (state.status == QueryStatus::Resolving ||
        state.status == QueryStatus::Querying) {
      state.status = state.sample.has_value()
                         ? (state.sample->synchronized
                                ? QueryStatus::Synchronized
                                : QueryStatus::Unsynchronized)
                         : QueryStatus::Idle;
      state.statusMessage = state.sample.has_value()
                                ? QStringLiteral("Displaying the previous result.")
                                : QStringLiteral("Query canceled.");
    }
  }

  m_pollTimer.stop();
  m_pendingTimeSetServerId = serverId;
  m_timeSetSourceLabel.clear();
  statusBar()->showMessage(
      QStringLiteral("Refreshing %1 before setting the local machine time...")
          .arg(source->config.label));
  updateViews();
  queryServerIds({serverId});
}

void MainWindow::beginSystemTimeSet(const ServerConfig &source,
                                    const NtpSample &sample) {
  const auto correctionMicros =
      static_cast<qint64>(std::llround(sample.offsetMicros));
  const qint64 estimatedTargetMicros =
      QDateTime::currentMSecsSinceEpoch() * 1'000LL + correctionMicros;

  QMessageBox confirmation(this);
  confirmation.setIcon(QMessageBox::Warning);
  confirmation.setWindowTitle(QStringLiteral("Set local machine time"));
  confirmation.setText(
      QStringLiteral("Use the fresh response from %1 to change this machine's clock?")
          .arg(source.label));
  confirmation.setInformativeText(
      QStringLiteral("Server: %1\nMeasured correction: %2\nEstimated local time: %3\n\n"
                     "The operating system may request administrator authorization.")
          .arg(source.host, FormatUtils::preciseOffset(sample.offsetMicros),
               FormatUtils::localTimestamp(estimatedTargetMicros)));
  QPushButton *setTimeButton = confirmation.addButton(
      QStringLiteral("Set Time"), QMessageBox::AcceptRole);
  confirmation.addButton(QMessageBox::Cancel);
  confirmation.setDefaultButton(QMessageBox::Cancel);
  confirmation.exec();

  if (confirmation.clickedButton() != setTimeButton) {
    m_pendingTimeSetServerId = -1;
    restorePollingAfterClockWorkflow();
    return;
  }

  m_timeSetSourceLabel =
      QStringLiteral("%1 (%2)").arg(source.label, source.host);
  statusBar()->showMessage(
      QStringLiteral("Waiting for operating-system authorization..."));
  updateViews();
  m_pendingTimeSetServerId = -1;
  m_systemTimeSetter.requestSystemTimeChange(
      correctionMicros, static_cast<quintptr>(winId()));
}

void MainWindow::handleSystemTimeSetFinished(const bool success,
                                             const QString &message) {
  m_pendingTimeSetServerId = -1;
  if (success) {
    m_ntpClient.cancelAll();
    for (ServerState &state : m_servers) {
      state.status = QueryStatus::Idle;
      state.statusMessage =
          QStringLiteral("System clock changed; awaiting a fresh result.");
      state.sample.reset();
      state.history.clear();
      state.reachability = 0;
      state.requestActive = false;
    }
    updateClock();
  }

  const QString sourceDetail =
      m_timeSetSourceLabel.isEmpty()
          ? QString()
          : QStringLiteral("\n\nTime source: %1").arg(m_timeSetSourceLabel);
  m_timeSetSourceLabel.clear();

  if (success) {
    QMessageBox::information(this, QStringLiteral("Local time updated"),
                             message + sourceDetail);
  } else {
    QMessageBox::warning(this, QStringLiteral("Local time unchanged"),
                         message + sourceDetail);
  }

  restorePollingAfterClockWorkflow();
  if (success) {
    QTimer::singleShot(250, this, &MainWindow::queryAllServers);
  }
}

void MainWindow::restorePollingAfterClockWorkflow() {
  statusBar()->clearMessage();
  applyPollingState(false);
  updateViews();
}

bool MainWindow::clockWorkflowActive() const {
  return m_pendingTimeSetServerId >= 0 || m_systemTimeSetter.isBusy();
}

void MainWindow::queryServerIds(const QList<int> &serverIds) {
  if (serverIds.isEmpty()) {
    return;
  }
  ++m_queryCount;
  for (const int serverId : serverIds) {
    ServerState *state = findState(serverId);
    if (state != nullptr && !m_ntpClient.isBusy(serverId)) {
      m_ntpClient.queryServer(serverId, state->config.host);
    }
  }
  updateStatusBar();
}

void MainWindow::handleStatusChanged(const int serverId,
                                     const QueryStatus status,
                                     const QString &detail) {
  ServerState *state = findState(serverId);
  if (state == nullptr) {
    return;
  }
  if (status == QueryStatus::Resolving && !state->requestActive) {
    state->reachability = static_cast<quint8>(state->reachability << 1U);
    state->requestActive = true;
  }
  state->status = status;
  state->statusMessage = detail;
  updateViews();
}

void MainWindow::handleQuerySucceeded(const int serverId, NtpSample sample) {
  ServerState *state = findState(serverId);
  if (state == nullptr) {
    return;
  }
  state->requestActive = false;
  state->reachability = static_cast<quint8>(state->reachability | 0x01U);
  updateSampleStatistics(*state, sample);
  state->sample = sample;
  state->status = sample.synchronized ? QueryStatus::Synchronized
                                      : QueryStatus::Unsynchronized;
  state->statusMessage = sample.synchronized
                             ? QStringLiteral("Valid NTPv%1 response from %2")
                                   .arg(sample.version)
                                   .arg(sample.resolvedAddress)
                             : QStringLiteral("Server reports an unsynchronized clock.");
  updateViews();

  if (m_pendingTimeSetServerId != serverId) {
    return;
  }
  if (!sample.synchronized) {
    m_pendingTimeSetServerId = -1;
    QMessageBox::warning(
        this, QStringLiteral("Local time unchanged"),
        QStringLiteral("%1 reported an unsynchronized clock. The local machine time was not changed.")
            .arg(state->config.label));
    restorePollingAfterClockWorkflow();
    return;
  }

  beginSystemTimeSet(state->config, sample);
}

void MainWindow::handleQueryFailed(const int serverId, const QueryStatus status,
                                   const QString &detail) {
  ServerState *state = findState(serverId);
  if (state == nullptr) {
    return;
  }
  state->requestActive = false;
  state->status = status;
  state->statusMessage = detail;
  updateViews();

  if (m_pendingTimeSetServerId == serverId) {
    const QString sourceLabel = state->config.label;
    m_pendingTimeSetServerId = -1;
    QMessageBox::warning(
        this, QStringLiteral("Local time unchanged"),
        QStringLiteral("The fresh query to %1 failed. The local machine time was not changed.\n\n%2")
            .arg(sourceLabel, detail));
    restorePollingAfterClockWorkflow();
  }
}

void MainWindow::updateSampleStatistics(ServerState &state, NtpSample &sample) {
  state.history.append({sample.destinationUnixMicros, sample.offsetMicros});
  while (state.history.size() > 8) {
    state.history.removeFirst();
  }

  if (state.history.size() >= 2) {
    const auto observationCount = static_cast<double>(state.history.size());
    const double mean = std::accumulate(
                            state.history.cbegin(), state.history.cend(), 0.0,
                            [](const double total,
                               const OffsetObservation &observation) {
                              return total + observation.offsetMicros;
                            }) /
                        observationCount;
    const double variance = std::accumulate(
                                state.history.cbegin(), state.history.cend(), 0.0,
                                [mean](const double total,
                                       const OffsetObservation &observation) {
                                  const double difference =
                                      observation.offsetMicros - mean;
                                  return total + difference * difference;
                                }) /
                            observationCount;
    sample.jitterMillis = std::sqrt(variance) / 1000.0;

    const OffsetObservation &first = state.history.front();
    const OffsetObservation &last = state.history.back();
    const qint64 elapsedMicros =
        last.observedAtUnixMicros - first.observedAtUnixMicros;
    if (elapsedMicros >= 60'000'000) {
      sample.frequencyPpm =
          (last.offsetMicros - first.offsetMicros) /
          static_cast<double>(elapsedMicros) * 1'000'000.0;
    }
  }
}

ServerState *MainWindow::findState(const int serverId) {
  const auto iterator = std::ranges::find_if(
      m_servers, [serverId](const ServerState &state) {
        return state.config.id == serverId;
      });
  return iterator == m_servers.end() ? nullptr : &*iterator;
}

const ServerState *MainWindow::findState(const int serverId) const {
  const auto iterator = std::ranges::find_if(
      m_servers, [serverId](const ServerState &state) {
        return state.config.id == serverId;
      });
  return iterator == m_servers.cend() ? nullptr : &*iterator;
}

int MainWindow::selectedRow() const {
  for (int row = 0; row < m_servers.size(); ++row) {
    if (m_servers[row].config.id == m_selectedServerId) {
      return row;
    }
  }
  return -1;
}

bool MainWindow::serverHostExists(const QString &host,
                                  const int exceptServerId) const {
  const QString normalized = host.trimmed();
  return std::ranges::any_of(
      m_servers,
      [&normalized, exceptServerId](const ServerState &state) {
        return state.config.id != exceptServerId &&
               state.config.host.compare(normalized, Qt::CaseInsensitive) == 0;
      });
}
