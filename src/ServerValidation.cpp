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
#include "ServerValidation.h"

#include <QRegularExpression>

namespace ServerValidation {

QString labelError(const QString &label) {
  if (label.size() > kMaximumLabelLength) {
    return QStringLiteral("The display name must be 128 characters or fewer.");
  }

  static const QRegularExpression controlCharacters(
      QStringLiteral("[\\x{0000}-\\x{001f}\\x{007f}]"));
  if (label.contains(controlCharacters)) {
    return QStringLiteral("The display name cannot contain control characters.");
  }
  return {};
}

QString hostError(const QString &host) {
  if (host.isEmpty()) {
    return QStringLiteral("Enter an NTP hostname or IP address.");
  }
  if (host.size() > kMaximumHostLength) {
    return QStringLiteral("The server address must be 255 characters or fewer.");
  }

  static const QRegularExpression whitespaceOrControl(
      QStringLiteral("[\\s\\x{0000}-\\x{001f}\\x{007f}]"));
  if (host.contains(whitespaceOrControl) ||
      host.contains(QStringLiteral("://"))) {
    return QStringLiteral(
        "Enter a hostname or IP address without a URL scheme or spaces.");
  }
  return {};
}

}  // namespace ServerValidation
