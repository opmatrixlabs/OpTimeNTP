#include <QDateTime>
#include <QTest>
#include <QtEndian>

#include <algorithm>
#include <cmath>

#include "FormatUtils.h"
#include "NtpProtocol.h"
#include "ServerListYaml.h"

namespace {

void writeUnsigned32(QByteArray &bytes, const int offset, const quint32 value) {
  qToBigEndian<quint32>(
      value, reinterpret_cast<uchar *>(bytes.data() + offset));
}

void writeTimestamp(QByteArray &bytes, const int offset,
                    const qint64 unixMicros) {
  const QByteArray encoded =
      NtpProtocol::encodeTimestamp(NtpProtocol::fromUnixMicros(unixMicros));
  std::ranges::copy(encoded, bytes.begin() + offset);
}

QByteArray validResponse(const QByteArray &request, const qint64 receiveMicros,
                         const qint64 transmitMicros) {
  QByteArray response(NtpProtocol::kPacketSize, '\0');
  response[0] = static_cast<char>((4U << 3U) | 4U);
  response[1] = 2;
  response[2] = 6;
  response[3] = static_cast<char>(-20);
  writeUnsigned32(response, 4, 0x00018000U);
  writeUnsigned32(response, 8, 0x00004000U);
  response.replace(12, 4, QByteArray("GPS\0", 4));
  response.replace(24, 8, NtpProtocol::transmitToken(request));
  writeTimestamp(response, 32, receiveMicros);
  writeTimestamp(response, 40, transmitMicros);
  return response;
}

class NtpProtocolTests final : public QObject {
  Q_OBJECT

 private slots:
  static void timestampRoundTripBeforeEraRollover() {
    constexpr qint64 value = 1'788'120'123'456'789LL;
    const auto wire = NtpProtocol::fromUnixMicros(value);
    const qint64 decoded = NtpProtocol::toUnixMicros(wire, value);
    QVERIFY(std::abs(decoded - value) <= 1);
  }

  static void timestampRoundTripAfterEraRollover() {
    const qint64 value =
        QDateTime(QDate(2040, 6, 1), QTime(12, 30), QTimeZone::UTC)
            .toMSecsSinceEpoch() *
            1000;
    const auto wire = NtpProtocol::fromUnixMicros(value);
    const qint64 decoded = NtpProtocol::toUnixMicros(wire, value);
    QVERIFY(std::abs(decoded - value) <= 1);
  }

  static void clientRequestUsesNtpV4ClientMode() {
    const QByteArray request = NtpProtocol::createClientRequest(1'700'000'000'000'000LL);
    QCOMPARE(request.size(), NtpProtocol::kPacketSize);
    QCOMPARE(static_cast<quint8>(request[0]), static_cast<quint8>(0x23));
    QCOMPARE(static_cast<qint8>(request[2]), static_cast<qint8>(6));
    QCOMPARE(static_cast<qint8>(request[3]), static_cast<qint8>(-20));
    QCOMPARE(NtpProtocol::transmitToken(request), request.mid(40, 8));
    QVERIFY(NtpProtocol::transmitToken(request) != QByteArray(8, '\0'));
  }

  static void parsesFourTimestampOffsetAndDelay() {
    constexpr qint64 origin = 1'700'000'000'000'000LL;
    constexpr qint64 receive = origin + 120'000;
    constexpr qint64 transmit = origin + 140'000;
    constexpr qint64 destination = origin + 240'000;
    const QByteArray request = NtpProtocol::createClientRequest(origin);
    const QByteArray response = validResponse(request, receive, transmit);

    QString error;
    const auto parsed = NtpProtocol::parseServerResponse(
        response, NtpProtocol::transmitToken(request), origin, destination,
        &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QVERIFY(std::abs(parsed->offsetMicros - 10'000.0) <= 1.0);
    QVERIFY(std::abs(parsed->roundTripMicros - 220'000.0) <= 1.0);
    QCOMPARE(parsed->version, 4);
    QCOMPARE(parsed->stratum, 2);
    QCOMPARE(parsed->leapIndicator, 0);
    QVERIFY(parsed->synchronized);
    QCOMPARE(parsed->pollExponent, 6);
    QCOMPARE(parsed->precisionExponent, -20);
    QVERIFY(std::abs(parsed->rootDelayMillis - 1500.0) < 0.001);
    QVERIFY(std::abs(parsed->rootDispersionMillis - 250.0) < 0.001);
  }

  static void rejectsMismatchedOriginTimestamp() {
    constexpr qint64 origin = 1'700'000'000'000'000LL;
    const QByteArray request = NtpProtocol::createClientRequest(origin);
    const QByteArray response = validResponse(request, origin + 10'000,
                                              origin + 20'000);
    QString error;
    const auto parsed = NtpProtocol::parseServerResponse(
        response, QByteArray(8, '\x7f'), origin, origin + 30'000, &error);
    QVERIFY(!parsed.has_value());
    QVERIFY(error.contains(QStringLiteral("does not match")));
  }

  static void formatsRequiredSignedHundredths() {
    QCOMPARE(FormatUtils::offsetHundredths(134'000.0), QStringLiteral("+0.13 s"));
    QCOMPARE(FormatUtils::offsetHundredths(-1'426'000.0),
             QStringLiteral("-1.43 s"));
    QCOMPARE(FormatUtils::offsetHundredths(0.0), QStringLiteral("+0.00 s"));
  }

  static void formatsPreciseOffsetsWithoutUnicodeUnits() {
    QCOMPARE(FormatUtils::preciseOffset(208.0), QStringLiteral("+208.0 us"));
    QCOMPARE(FormatUtils::preciseOffset(-1'860.0), QStringLiteral("-1.860 ms"));
  }

  static void yamlServerListRoundTrips() {
    const QVector<ServerConfig> original = {
        {42, QStringLiteral("Primary: lab #1"),
         QStringLiteral("time1.example.test")},
        {87, QStringLiteral("Backup pool"),
         QStringLiteral("time2.example.test")},
    };

    QString error;
    const std::optional<QByteArray> yaml =
        ServerListYaml::serialize(original, &error);
    QVERIFY2(yaml.has_value(), qPrintable(error));
    QVERIFY(yaml->contains("format: OpTimeNTP"));
    QVERIFY(yaml->contains("version: 1"));
    QVERIFY(!yaml->contains("id:"));

    const std::optional<QVector<ServerConfig>> parsed =
        ServerListYaml::parse(*yaml, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->size(), 2);
    QCOMPARE(parsed->at(0).id, 0);
    QCOMPARE(parsed->at(0).label, original.at(0).label);
    QCOMPARE(parsed->at(0).host, original.at(0).host);
    QCOMPARE(parsed->at(1).label, original.at(1).label);
    QCOMPARE(parsed->at(1).host, original.at(1).host);
  }

  static void yamlServerListDefaultsMissingLabelToHost() {
    const QByteArray yaml = R"YAML(format: OpTimeNTP
version: 1
servers:
  - host: pool.ntp.org
)YAML";

    QString error;
    const std::optional<QVector<ServerConfig>> parsed =
        ServerListYaml::parse(yaml, &error);
    QVERIFY2(parsed.has_value(), qPrintable(error));
    QCOMPARE(parsed->at(0).label, QStringLiteral("pool.ntp.org"));
  }

  static void yamlServerListRejectsDuplicateHostsCaseInsensitively() {
    const QByteArray yaml = R"YAML(format: OpTimeNTP
version: 1
servers:
  - label: First
    host: TIME.EXAMPLE.TEST
  - label: Second
    host: time.example.test
)YAML";

    QString error;
    const auto parsed = ServerListYaml::parse(yaml, &error);
    QVERIFY(!parsed.has_value());
    QVERIFY(error.contains(QStringLiteral("duplicates")));
  }

  static void yamlServerListRejectsInvalidSchemaAndSyntax() {
    QString error;
    const auto wrongVersion = ServerListYaml::parse(
        QByteArray("format: OpTimeNTP\nversion: 2\nservers: []\n"), &error);
    QVERIFY(!wrongVersion.has_value());
    QVERIFY(error.contains(QStringLiteral("version")));

    const auto malformed =
        ServerListYaml::parse(QByteArray("format: [unterminated\n"), &error);
    QVERIFY(!malformed.has_value());
    QVERIFY(error.startsWith(QStringLiteral("Invalid YAML:")));
  }

  static void yamlServerListEnforcesTenServerLimit() {
    QVector<ServerConfig> servers;
    for (int index = 0; index < 11; ++index) {
      servers.append({index + 1, QStringLiteral("Server %1").arg(index + 1),
                      QStringLiteral("time%1.example.test").arg(index + 1)});
    }

    QString error;
    const auto yaml = ServerListYaml::serialize(servers, &error);
    QVERIFY(!yaml.has_value());
    QVERIFY(error.contains(QStringLiteral("more than 10")));
  }
};

}  // namespace

QTEST_APPLESS_MAIN(NtpProtocolTests)

#include "NtpProtocolTests.moc"
