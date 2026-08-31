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
#include "ServerListYaml.h"

#include <QSet>

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

#include "ServerValidation.h"

namespace {

constexpr auto kFormatName = "OpTimeNTP";
constexpr int kFormatVersion = 1;

void setError(QString *errorMessage, const QString &message) {
  if (errorMessage != nullptr) {
    *errorMessage = message;
  }
}

std::string utf8String(const QString &value) {
  const QByteArray utf8 = value.toUtf8();
  return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

QString scalarText(const YAML::Node &node) {
  const auto value = node.as<std::string>();
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString validateServers(const QVector<ServerConfig> &servers) {
  if (servers.isEmpty()) {
    return QStringLiteral("A server list must contain at least one server.");
  }
  if (servers.size() > kMaximumNtpServers) {
    return QStringLiteral("A server list cannot contain more than 10 servers.");
  }

  QSet<QString> normalizedHosts;
  for (int index = 0; index < servers.size(); ++index) {
    const ServerConfig &server = servers[index];
    const QString host = server.host.trimmed();
    const QString label = server.label.trimmed().isEmpty()
                              ? host
                              : server.label.trimmed();

    if (const QString error = ServerValidation::hostError(host);
        !error.isEmpty()) {
      return QStringLiteral("Server entry %1: %2").arg(index + 1).arg(error);
    }
    if (const QString error = ServerValidation::labelError(label);
        !error.isEmpty()) {
      return QStringLiteral("Server entry %1: %2").arg(index + 1).arg(error);
    }

    const QString normalizedHost = host.toCaseFolded();
    if (normalizedHosts.contains(normalizedHost)) {
      return QStringLiteral("Server entry %1 duplicates the address '%2'.")
          .arg(index + 1)
          .arg(host);
    }
    normalizedHosts.insert(normalizedHost);
  }
  return {};
}

}  // namespace

namespace ServerListYaml {

std::optional<QByteArray> serialize(const QVector<ServerConfig> &servers,
                                    QString *errorMessage) {
  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
  if (const QString error = validateServers(servers); !error.isEmpty()) {
    setError(errorMessage, error);
    return std::nullopt;
  }

  YAML::Emitter output;
  output << YAML::BeginMap << YAML::Key << "format" << YAML::Value
         << kFormatName << YAML::Key << "version" << YAML::Value
         << kFormatVersion << YAML::Key << "servers" << YAML::Value
         << YAML::BeginSeq;
  for (const ServerConfig &server : servers) {
    const QString host = server.host.trimmed();
    const QString label = server.label.trimmed().isEmpty()
                              ? host
                              : server.label.trimmed();
    output << YAML::BeginMap << YAML::Key << "label" << YAML::Value
           << utf8String(label) << YAML::Key << "host" << YAML::Value
           << utf8String(host) << YAML::EndMap;
  }
  output << YAML::EndSeq << YAML::EndMap;

  if (!output.good()) {
    setError(errorMessage,
             QStringLiteral("The YAML document could not be generated: %1")
                 .arg(QString::fromUtf8(output.GetLastError().c_str())));
    return std::nullopt;
  }

  QByteArray yaml(output.c_str(), static_cast<qsizetype>(output.size()));
  yaml.append('\n');
  return yaml;
}

std::optional<QVector<ServerConfig>> parse(const QByteArray &yaml,
                                           QString *errorMessage) {
  if (errorMessage != nullptr) {
    errorMessage->clear();
  }
  if (yaml.size() > kMaximumFileSize) {
    setError(errorMessage,
             QStringLiteral("The YAML file exceeds the 1 MiB size limit."));
    return std::nullopt;
  }
  if (yaml.trimmed().isEmpty()) {
    setError(errorMessage,
             QStringLiteral("The file does not contain a server list."));
    return std::nullopt;
  }

  try {
    const std::string input(yaml.constData(),
                            static_cast<std::size_t>(yaml.size()));
    const std::vector<YAML::Node> documents = YAML::LoadAll(input);
    if (documents.size() != 1) {
      setError(
          errorMessage,
          QStringLiteral("The file must contain exactly one YAML document."));
      return std::nullopt;
    }

    const YAML::Node &root = documents.front();
    if (!root || !root.IsMap()) {
      setError(errorMessage,
               QStringLiteral("The YAML document root must be a mapping."));
      return std::nullopt;
    }

    const YAML::Node formatNode = root["format"];
    if (!formatNode || !formatNode.IsScalar() ||
        scalarText(formatNode) != QString::fromLatin1(kFormatName)) {
      setError(errorMessage,
               QStringLiteral("The file is not an OpTimeNTP server list."));
      return std::nullopt;
    }

    const YAML::Node versionNode = root["version"];
    if (!versionNode || !versionNode.IsScalar() ||
        versionNode.as<int>() != kFormatVersion) {
      setError(errorMessage,
               QStringLiteral("The server-list format version is not supported."));
      return std::nullopt;
    }

    const YAML::Node serversNode = root["servers"];
    if (!serversNode || !serversNode.IsSequence()) {
      setError(
          errorMessage,
          QStringLiteral("The 'servers' field must be a YAML sequence."));
      return std::nullopt;
    }
    if (serversNode.size() == 0 ||
        serversNode.size() > static_cast<std::size_t>(kMaximumNtpServers)) {
      setError(errorMessage,
               QStringLiteral(
                   "The server list must contain between 1 and 10 servers."));
      return std::nullopt;
    }

    QVector<ServerConfig> servers;
    servers.reserve(static_cast<qsizetype>(serversNode.size()));
    for (std::size_t index = 0; index < serversNode.size(); ++index) {
      const YAML::Node entry = serversNode[index];
      if (!entry || !entry.IsMap()) {
        setError(errorMessage,
                 QStringLiteral("Server entry %1 must be a mapping.")
                     .arg(index + 1));
        return std::nullopt;
      }

      const YAML::Node hostNode = entry["host"];
      if (!hostNode || !hostNode.IsScalar()) {
        setError(
            errorMessage,
            QStringLiteral("Server entry %1 requires a scalar 'host' field.")
                .arg(index + 1));
        return std::nullopt;
      }

      ServerConfig server;
      server.host = scalarText(hostNode).trimmed();
      const YAML::Node labelNode = entry["label"];
      if (labelNode && !labelNode.IsNull()) {
        if (!labelNode.IsScalar()) {
          setError(
              errorMessage,
              QStringLiteral("Server entry %1 has a non-scalar 'label' field.")
                  .arg(index + 1));
          return std::nullopt;
        }
        server.label = scalarText(labelNode).trimmed();
      }
      if (server.label.isEmpty()) {
        server.label = server.host;
      }
      servers.append(server);
    }

    if (const QString error = validateServers(servers); !error.isEmpty()) {
      setError(errorMessage, error);
      return std::nullopt;
    }
    return servers;
  } catch (const YAML::Exception &exception) {
    setError(errorMessage,
             QStringLiteral("Invalid YAML: %1")
                 .arg(QString::fromUtf8(exception.what())));
    return std::nullopt;
  }
}

}  // namespace ServerListYaml
