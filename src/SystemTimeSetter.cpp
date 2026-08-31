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
#include "SystemTimeSetter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <cerrno>
#include <limits>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/time.h>
#endif

namespace {

constexpr auto kHelperArgument = "--apply-system-time-offset-us";
constexpr int kInvalidArgumentsExitCode = 2;
constexpr int kSetTimeFailedExitCode = 3;

#if defined(_WIN32)

int applySystemTimeOffset(const qint64 offsetMicros,
                          bool *authorizationRequired = nullptr) {
  if (authorizationRequired != nullptr) {
    *authorizationRequired = false;
  }
  if (offsetMicros > std::numeric_limits<qint64>::max() / 10 ||
      offsetMicros < std::numeric_limits<qint64>::min() / 10) {
    return kInvalidArgumentsExitCode;
  }

  FILETIME currentFileTime{};
  GetSystemTimePreciseAsFileTime(&currentFileTime);
  const quint64 currentTicks =
      (static_cast<quint64>(currentFileTime.dwHighDateTime) << 32U) |
      currentFileTime.dwLowDateTime;
  const qint64 deltaTicks = offsetMicros * 10;

  quint64 targetTicks = currentTicks;
  if (deltaTicks >= 0) {
    const auto positiveDelta = static_cast<quint64>(deltaTicks);
    if (positiveDelta > std::numeric_limits<quint64>::max() - currentTicks) {
      return kInvalidArgumentsExitCode;
    }
    targetTicks += positiveDelta;
  } else {
    const quint64 magnitude =
        static_cast<quint64>(-(deltaTicks + 1)) + 1U;
    if (magnitude > currentTicks) {
      return kInvalidArgumentsExitCode;
    }
    targetTicks -= magnitude;
  }

  FILETIME targetFileTime{};
  targetFileTime.dwLowDateTime = static_cast<DWORD>(targetTicks & 0xffffffffU);
  targetFileTime.dwHighDateTime = static_cast<DWORD>(targetTicks >> 32U);
  SYSTEMTIME targetSystemTime{};
  if (!FileTimeToSystemTime(&targetFileTime, &targetSystemTime)) {
    return kSetTimeFailedExitCode;
  }
  if (!SetSystemTime(&targetSystemTime)) {
    const DWORD error = GetLastError();
    if (authorizationRequired != nullptr) {
      *authorizationRequired = error == ERROR_ACCESS_DENIED ||
                               error == ERROR_PRIVILEGE_NOT_HELD;
    }
    return kSetTimeFailedExitCode;
  }
  return 0;
}

#else

int applySystemTimeOffset(const qint64 offsetMicros,
                          bool *authorizationRequired = nullptr) {
  if (authorizationRequired != nullptr) {
    *authorizationRequired = false;
  }
  timeval current{};
  if (gettimeofday(&current, nullptr) != 0) {
    return kSetTimeFailedExitCode;
  }

  const qint64 currentMicros =
      static_cast<qint64>(current.tv_sec) * 1'000'000LL + current.tv_usec;
  if ((offsetMicros > 0 &&
       currentMicros > std::numeric_limits<qint64>::max() - offsetMicros) ||
      (offsetMicros < 0 &&
       currentMicros < std::numeric_limits<qint64>::min() - offsetMicros)) {
    return kInvalidArgumentsExitCode;
  }

  const qint64 targetMicros = currentMicros + offsetMicros;
  timeval target{};
  target.tv_sec = static_cast<time_t>(targetMicros / 1'000'000LL);
  target.tv_usec = static_cast<suseconds_t>(targetMicros % 1'000'000LL);
  if (target.tv_usec < 0) {
    --target.tv_sec;
    target.tv_usec += 1'000'000;
  }
  if (settimeofday(&target, nullptr) == 0) {
    return 0;
  }
  if (authorizationRequired != nullptr) {
    *authorizationRequired = errno == EPERM || errno == EACCES;
  }
  return kSetTimeFailedExitCode;
}

#if defined(__APPLE__)
QString appleScriptLiteral(QString value) {
  value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return value;
}
#endif

#endif

}  // namespace

SystemTimeSetter::SystemTimeSetter(QObject *parent) : QObject(parent) {
#if defined(_WIN32)
  m_processTimer = new QTimer(this);
  m_processTimer->setInterval(100);
  connect(m_processTimer, &QTimer::timeout, this,
          &SystemTimeSetter::pollElevatedProcess);
#else
  m_process = new QProcess(this);
  m_process->setProcessChannelMode(QProcess::SeparateChannels);
  connect(m_process, &QProcess::finished, this,
          [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
            handleProcessFinished(exitCode, static_cast<int>(exitStatus));
          });
  connect(m_process, &QProcess::errorOccurred, this,
          [this](const QProcess::ProcessError error) {
            if (m_busy && error == QProcess::FailedToStart) {
              complete(false,
                       QStringLiteral("The authorization helper could not be started."));
            }
          });
#endif
}

SystemTimeSetter::~SystemTimeSetter() {
#if defined(_WIN32)
  if (m_nativeProcessHandle != 0) {
    CloseHandle(reinterpret_cast<HANDLE>(m_nativeProcessHandle));
    m_nativeProcessHandle = 0;
  }
#endif
}

int SystemTimeSetter::runHelperIfRequested(const int argc, char *argv[],
                                           bool *handled) {
  if (handled != nullptr) {
    *handled = false;
  }
  if (argc < 2 || QByteArray(argv[1]) != kHelperArgument) {
    return 0;
  }
  if (handled != nullptr) {
    *handled = true;
  }
  if (argc != 3) {
    return kInvalidArgumentsExitCode;
  }

  bool parsed = false;
  const qint64 offsetMicros = QByteArray(argv[2]).toLongLong(&parsed);
  return parsed ? applySystemTimeOffset(offsetMicros)
                : kInvalidArgumentsExitCode;
}

void SystemTimeSetter::requestSystemTimeChange(
    const qint64 offsetMicros, const quintptr parentWindowId) {
  if (m_busy) {
    emit finished(false,
                  QStringLiteral("A system clock update is already in progress."));
    return;
  }
  m_busy = true;

  bool authorizationRequired = false;
  const int directResult =
      applySystemTimeOffset(offsetMicros, &authorizationRequired);
  if (directResult == 0) {
    complete(true, QStringLiteral("The local machine clock was updated."));
    return;
  }
  if (!authorizationRequired) {
    complete(false,
             directResult == kInvalidArgumentsExitCode
                 ? QStringLiteral("The clock helper rejected the correction value.")
                 : QStringLiteral("The operating system rejected the system clock update."));
    return;
  }

  const QString executable = QCoreApplication::applicationFilePath();
  const QString offset = QString::number(offsetMicros);
  Q_UNUSED(parentWindowId);

#if defined(_WIN32)
  const QString parameters =
      QStringLiteral("%1 %2").arg(QString::fromLatin1(kHelperArgument), offset);
  const QString workingDirectory = QFileInfo(executable).absolutePath();

  SHELLEXECUTEINFOW executeInfo{};
  executeInfo.cbSize = sizeof(executeInfo);
  executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  executeInfo.hwnd = reinterpret_cast<HWND>(parentWindowId);
  executeInfo.lpVerb = L"runas";
  executeInfo.lpFile = reinterpret_cast<LPCWSTR>(executable.utf16());
  executeInfo.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
  executeInfo.lpDirectory =
      reinterpret_cast<LPCWSTR>(workingDirectory.utf16());
  executeInfo.nShow = SW_HIDE;

  if (!ShellExecuteExW(&executeInfo)) {
    const DWORD error = GetLastError();
    complete(false, error == ERROR_CANCELLED
                        ? QStringLiteral("Administrator authorization was canceled.")
                        : QStringLiteral("Windows could not start the elevated clock helper (error %1).")
                              .arg(error));
    return;
  }
  if (executeInfo.hProcess == nullptr) {
    complete(false,
             QStringLiteral("Windows did not return an elevated process handle."));
    return;
  }
  m_nativeProcessHandle = reinterpret_cast<quintptr>(executeInfo.hProcess);
  m_processTimer->start();
#elif defined(__APPLE__)
  if (executable.contains(QLatin1Char('\n')) ||
      executable.contains(QLatin1Char('\r'))) {
    complete(false, QStringLiteral("The application path cannot be authorized safely."));
    return;
  }
  const QString script = QStringLiteral(
      "do shell script ((quoted form of \"%1\") & \" %2 \" & "
      "(quoted form of \"%3\")) with administrator privileges")
                             .arg(appleScriptLiteral(executable),
                                  QString::fromLatin1(kHelperArgument), offset);
  m_process->start(QStringLiteral("/usr/bin/osascript"),
                   {QStringLiteral("-e"), script});
#else
  const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
  if (pkexec.isEmpty()) {
    complete(false,
             QStringLiteral("pkexec is not installed; PolicyKit authorization is required to set the system clock."));
    return;
  }
  m_process->start(pkexec,
                   {executable, QString::fromLatin1(kHelperArgument), offset});
#endif
}

bool SystemTimeSetter::isBusy() const { return m_busy; }

void SystemTimeSetter::complete(const bool success, const QString &message) {
#if defined(_WIN32)
  m_processTimer->stop();
#endif
  m_busy = false;
  emit finished(success, message);
}

#if defined(_WIN32)

void SystemTimeSetter::pollElevatedProcess() {
  if (m_nativeProcessHandle == 0) {
    complete(false, QStringLiteral("The elevated clock helper was lost."));
    return;
  }

  const auto process = reinterpret_cast<HANDLE>(m_nativeProcessHandle);
  DWORD exitCode = STILL_ACTIVE;
  if (!GetExitCodeProcess(process, &exitCode)) {
    CloseHandle(process);
    m_nativeProcessHandle = 0;
    complete(false,
             QStringLiteral("Windows could not read the clock helper result."));
    return;
  }
  if (exitCode == STILL_ACTIVE) {
    return;
  }

  CloseHandle(process);
  m_nativeProcessHandle = 0;
  if (exitCode == 0) {
    complete(true, QStringLiteral("The local machine clock was updated."));
  } else if (exitCode == kInvalidArgumentsExitCode) {
    complete(false, QStringLiteral("The clock helper rejected the correction value."));
  } else {
    complete(false,
             QStringLiteral("Windows rejected the system clock update."));
  }
}

#else

void SystemTimeSetter::handleProcessFinished(const int exitCode,
                                             const int exitStatus) {
  if (!m_busy) {
    return;
  }
  const QString errorOutput =
      QString::fromLocal8Bit(m_process->readAllStandardError()).trimmed();
  if (exitStatus == static_cast<int>(QProcess::NormalExit) && exitCode == 0) {
    complete(true, QStringLiteral("The local machine clock was updated."));
    return;
  }
  if (exitCode == 126 || errorOutput.contains(QStringLiteral("canceled"),
                                               Qt::CaseInsensitive) ||
      errorOutput.contains(QStringLiteral("cancelled"),
                           Qt::CaseInsensitive) ||
      errorOutput.contains(QStringLiteral("(-128)"))) {
    complete(false, QStringLiteral("Administrator authorization was canceled."));
    return;
  }

  complete(false,
           errorOutput.isEmpty()
               ? QStringLiteral("The operating system rejected the system clock update.")
               : QStringLiteral("The system clock update failed: %1")
                     .arg(errorOutput));
}

#endif
