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
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>

#include "MainWindow.h"
#include "NtpTypes.h"
#include "SystemTimeSetter.h"

#ifndef OPTIME_DISPLAY_NAME
#define OPTIME_DISPLAY_NAME "OpTime NTP"
#endif

#ifndef OPTIME_ORGANIZATION_NAME
#define OPTIME_ORGANIZATION_NAME "OpTime"
#endif

#ifndef OPTIME_APP_ID
#define OPTIME_APP_ID "io.github.akevinbailey.optimentp"
#endif

#ifndef OPTIME_VERSION
#define OPTIME_VERSION "0.1.0"
#endif

int main(int argc, char *argv[]) {
  bool helperHandled = false;
  const int helperResult =
      SystemTimeSetter::runHelperIfRequested(argc, argv, &helperHandled);
  if (helperHandled) {
    return helperResult;
  }

  QApplication application(argc, argv);

  QCoreApplication::setOrganizationName(QStringLiteral(OPTIME_ORGANIZATION_NAME));
  QCoreApplication::setOrganizationDomain(QStringLiteral("io.github.akevinbailey"));
  QCoreApplication::setApplicationName(QStringLiteral("OpTimeNTP"));
  QCoreApplication::setApplicationVersion(QStringLiteral(OPTIME_VERSION));
  QGuiApplication::setApplicationDisplayName(QStringLiteral(OPTIME_DISPLAY_NAME));
  QGuiApplication::setDesktopFileName(QStringLiteral(OPTIME_APP_ID));

  qRegisterMetaType<NtpSample>();
  qRegisterMetaType<QueryStatus>();

  const QIcon applicationIcon(QStringLiteral(":/icons/optime_ntp.svg"));
  QGuiApplication::setWindowIcon(applicationIcon);

  MainWindow window;
  window.setWindowIcon(applicationIcon);
  window.show();
  return QCoreApplication::exec();
}
