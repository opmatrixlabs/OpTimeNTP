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
#include <QObject>
#include <QString>
#include <QtGlobal>

class QProcess;
class QTimer;

class SystemTimeSetter final : public QObject {
  Q_OBJECT

 public:
  explicit SystemTimeSetter(QObject *parent = nullptr);
  ~SystemTimeSetter() override;

  static int runHelperIfRequested(int argc, char *argv[], bool *handled);

  void requestSystemTimeChange(qint64 offsetMicros,
                               quintptr parentWindowId = 0);
  [[nodiscard]] bool isBusy() const;

 signals:
  void finished(bool success, const QString &message);

 private:
  void complete(bool success, const QString &message);

#if defined(_WIN32)
  void pollElevatedProcess();

  QTimer *m_processTimer = nullptr;
  quintptr m_nativeProcessHandle = 0;
#else
  void handleProcessFinished(int exitCode, int exitStatus);

  QProcess *m_process = nullptr;
#endif

  bool m_busy = false;
};
