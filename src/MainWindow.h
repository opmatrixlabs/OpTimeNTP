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
#include <QMainWindow>
#include <QTimer>
#include <QVector>

#include "NtpClient.h"
#include "NtpTypes.h"
#include "SystemTimeSetter.h"

class QAction;
class QCloseEvent;
class QComboBox;
class QGridLayout;
class QLabel;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QTabWidget;
class QTableWidget;
class QToolButton;
class ServerCard;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget *parent = nullptr);

 protected:
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

 private slots:
  void queryAllServers();
  void loadServerList();
  void saveServerList();
  void addServer();
  void editSelectedServer();
  void removeSelectedServer();
  void updateClock();

 private:
  void setupUi();
  void setupMenus();
  static void applyStyle();
  void loadSettings();
  void saveSettings() const;
  void applyPollingState(bool queryImmediately);
  void rebuildCards();
  void layoutCards();
  void updateViews();
  void updateCards();
  void updateComparisonTables();
  void updateStatusBar();
  void selectServer(int serverId);
  void requestLocalTimeSet(int serverId);
  void beginSystemTimeSet(const ServerConfig &source,
                          const NtpSample &sample);
  void handleSystemTimeSetFinished(bool success, const QString &message);
  void restorePollingAfterClockWorkflow();
  [[nodiscard]] bool clockWorkflowActive() const;
  void queryServerIds(const QList<int> &serverIds);
  void handleStatusChanged(int serverId, QueryStatus status,
                           const QString &detail);
  void handleQuerySucceeded(int serverId, NtpSample sample);
  void handleQueryFailed(int serverId, QueryStatus status,
                         const QString &detail);
  static void updateSampleStatistics(ServerState &state, NtpSample &sample);
  [[nodiscard]] ServerState *findState(int serverId);
  [[nodiscard]] const ServerState *findState(int serverId) const;
  [[nodiscard]] int selectedRow() const;
  [[nodiscard]] bool serverHostExists(const QString &host,
                                      int exceptServerId = -1) const;

  NtpClient m_ntpClient;
  SystemTimeSetter m_systemTimeSetter;
  QVector<ServerState> m_servers;
  QHash<int, ServerCard *> m_cards;
  int m_nextServerId = 1;
  int m_selectedServerId = -1;
  int m_cardColumns = 2;
  int m_queryCount = 0;
  int m_clockTicks = 0;
  int m_pendingTimeSetServerId = -1;
  int m_pollIntervalMillis = 10'000;
  bool m_pollingActive = true;
  bool m_initialQueryScheduled = false;
  QString m_timeSetSourceLabel;

  QTimer m_clockTimer;
  QTimer m_pollTimer;
  QToolButton *m_pollingButton = nullptr;
  QToolButton *m_queryButton = nullptr;
  QToolButton *m_addButton = nullptr;
  QToolButton *m_editButton = nullptr;
  QToolButton *m_removeButton = nullptr;
  QComboBox *m_intervalCombo = nullptr;
  QLabel *m_localClockLabel = nullptr;
  QLabel *m_clockDetailLabel = nullptr;
  QTabWidget *m_tabs = nullptr;
  QScrollArea *m_cardScrollArea = nullptr;
  QWidget *m_cardHost = nullptr;
  QGridLayout *m_cardGrid = nullptr;
  QLabel *m_offsetTableTitle = nullptr;
  QTableWidget *m_offsetTable = nullptr;
  QTableWidget *m_detailTable = nullptr;
  QTableWidget *m_leapTable = nullptr;
  QLabel *m_serversStatusLabel = nullptr;
  QLabel *m_bestSourceLabel = nullptr;
  QLabel *m_averageRttLabel = nullptr;
  QLabel *m_stratumOneLabel = nullptr;
  QLabel *m_stratumTwoLabel = nullptr;
  QLabel *m_stratumThreePlusLabel = nullptr;
  QLabel *m_protocolLabel = nullptr;
  QAction *m_editAction = nullptr;
  QAction *m_removeAction = nullptr;
};
