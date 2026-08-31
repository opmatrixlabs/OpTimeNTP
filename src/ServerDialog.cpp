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
#include "ServerDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include "ServerValidation.h"

ServerDialog::ServerDialog(const QString &title, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(title);
  setModal(true);
  setMinimumWidth(430);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(18, 18, 18, 18);
  root->setSpacing(14);

  auto *description = new QLabel(
      QStringLiteral("Enter an NTP hostname or IP address. Requests use UDP port 123."),
      this);
  description->setWordWrap(true);
  description->setObjectName(QStringLiteral("DialogDescription"));
  root->addWidget(description);

  auto *form = new QFormLayout;
  form->setHorizontalSpacing(14);
  form->setVerticalSpacing(10);
  m_labelEdit = new QLineEdit(this);
  m_labelEdit->setMaxLength(ServerValidation::kMaximumLabelLength);
  m_labelEdit->setPlaceholderText(QStringLiteral("Cloudflare"));
  m_hostEdit = new QLineEdit(this);
  m_hostEdit->setMaxLength(ServerValidation::kMaximumHostLength);
  m_hostEdit->setPlaceholderText(QStringLiteral("time.cloudflare.com"));
  form->addRow(QStringLiteral("Display name"), m_labelEdit);
  form->addRow(QStringLiteral("Server address"), m_hostEdit);
  root->addLayout(form);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &ServerDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);
}

void ServerDialog::setServer(const ServerConfig &server) const {
  m_labelEdit->setText(server.label);
  m_hostEdit->setText(server.host);
}

QString ServerDialog::serverLabel() const { return m_labelEdit->text().trimmed(); }

QString ServerDialog::serverHost() const { return m_hostEdit->text().trimmed(); }

void ServerDialog::accept() {
  const QString host = serverHost();
  if (const QString error = ServerValidation::hostError(host);
      !error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Invalid server address"), error);
    m_hostEdit->setFocus();
    return;
  }
  const QString label = m_labelEdit->text().trimmed();
  if (const QString error = ServerValidation::labelError(label);
      !error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Invalid display name"), error);
    m_labelEdit->setFocus();
    return;
  }
  if (label.isEmpty()) {
    m_labelEdit->setText(host);
  }
  QDialog::accept();
}
