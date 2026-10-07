// Copyright (c) 2025. Andrew Kevin Bailey
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
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "AboutDialog.h"

#include <QClipboard>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QtGlobal>

namespace {

QString appNameAndVersion() {
  return QStringLiteral("%1 %2")
      .arg(QGuiApplication::applicationDisplayName(),
           QCoreApplication::applicationVersion());
}

QString copyrightText() {
  return QStringLiteral("Copyright (C) 2026 Andrew Kevin Bailey");
}

QLabel *makeValueLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setTextFormat(Qt::PlainText);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  label->setWordWrap(true);
  return label;
}

void addInfoRow(QGridLayout *layout, const int row, const QString &name,
                const QString &value, QWidget *parent) {
  auto *nameLabel = new QLabel(name, parent);
  nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  layout->addWidget(nameLabel, row, 0);
  layout->addWidget(makeValueLabel(value, parent), row, 1);
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("About %1")
                     .arg(QGuiApplication::applicationDisplayName()));
  setObjectName(QStringLiteral("AboutDialog"));
  setModal(true);
  setMinimumSize(460, 280);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(8);

  auto *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(12);

  auto *iconLabel = new QLabel(this);
  iconLabel->setPixmap(QIcon(QStringLiteral(":/icons/optime_ntp.svg")).pixmap(48, 48));
  iconLabel->setFixedSize(52, 52);
  iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  headerLayout->addWidget(iconLabel);

  auto *summaryLayout = new QVBoxLayout();
  summaryLayout->setSpacing(4);
  summaryLayout->addWidget(makeValueLabel(appNameAndVersion(), this));
  summaryLayout->addWidget(makeValueLabel(copyrightText(), this));

  const QString homepage = QString::fromUtf8(OPTIME_HOMEPAGE_URL).toHtmlEscaped();
  auto *homepageLabel = new QLabel(
      QStringLiteral("Homepage: <a href=\"%1\" style=\"color: #60a5fa;\">%1</a>")
          .arg(homepage), this);
  homepageLabel->setOpenExternalLinks(true);
  homepageLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  homepageLabel->setWordWrap(true);
  summaryLayout->addWidget(homepageLabel);

  headerLayout->addLayout(summaryLayout, 1);
  mainLayout->addLayout(headerLayout);

  auto *buildGroup = new QGroupBox(QStringLiteral("Build information"), this);
  buildGroup->setObjectName(QStringLiteral("AboutBuildInformation"));
  buildGroup->setStyleSheet(QStringLiteral(R"QSS(
    QGroupBox#AboutBuildInformation {
      border: 1px solid #2a3040;
      border-radius: 4px;
      margin-top: 8px;
      padding-top: 8px;
    }
    QGroupBox#AboutBuildInformation::title {
      subcontrol-origin: margin;
      subcontrol-position: top left;
      left: 8px;
      padding: 0 4px;
    }
  )QSS"));
  auto *buildLayout = new QGridLayout(buildGroup);
  buildLayout->setContentsMargins(8, 8, 8, 8);
  buildLayout->setHorizontalSpacing(16);
  buildLayout->setVerticalSpacing(6);
  buildLayout->setColumnStretch(1, 1);

  int row = 0;
  addInfoRow(buildLayout, row++, QStringLiteral("Compiled for:"),
             QString::fromUtf8(OPTIME_COMPILED_FOR), buildGroup);
  addInfoRow(buildLayout, row++, QStringLiteral("Compiled on:"),
             QString::fromUtf8(OPTIME_COMPILED_ON), buildGroup);
  addInfoRow(buildLayout, row++, QStringLiteral("Build date:"),
             QString::fromUtf8(OPTIME_BUILD_DATE), buildGroup);
  addInfoRow(buildLayout, row++, QStringLiteral("Compiled with:"),
             QString::fromUtf8(OPTIME_COMPILED_WITH), buildGroup);
  addInfoRow(buildLayout, row, QStringLiteral("Qt version:"),
             QString::fromLatin1(qVersion()), buildGroup);
  mainLayout->addWidget(buildGroup);

  auto *buttonLayout = new QHBoxLayout();
  auto *copyButton = new QPushButton(QStringLiteral("Copy to clipboard"), this);
  auto *okButton = new QPushButton(QStringLiteral("OK"), this);
  okButton->setDefault(true);
  buttonLayout->addWidget(copyButton);
  buttonLayout->addStretch(1);
  buttonLayout->addWidget(okButton);
  mainLayout->addLayout(buttonLayout);

  connect(copyButton, &QPushButton::clicked, this, [] {
    QGuiApplication::clipboard()->setText(AboutDialog::clipboardText());
  });
  connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

  // Keep the reference dialog's compact size, but allow longer platform values
  // and larger system fonts to increase it instead of clipping the content.
  resize(minimumWidth(),
         qMax(minimumHeight(), mainLayout->totalHeightForWidth(minimumWidth())));
}

QString AboutDialog::clipboardText() {
  return QStringLiteral(
             "%1\n"
             "%2\n"
             "Homepage: %3\n"
             "Compiled for: %4\n"
             "Compiled on: %5\n"
             "Build date: %6\n"
             "Compiled with: %7\n"
             "Qt version used at run-time: %8")
      .arg(appNameAndVersion(), copyrightText(),
           QString::fromUtf8(OPTIME_HOMEPAGE_URL),
           QString::fromUtf8(OPTIME_COMPILED_FOR),
           QString::fromUtf8(OPTIME_COMPILED_ON),
           QString::fromUtf8(OPTIME_BUILD_DATE),
           QString::fromUtf8(OPTIME_COMPILED_WITH),
           QString::fromLatin1(qVersion()));
}
