#include <QtTest/QtTest>

#include "protocol.h"

class ProtocolTest : public QObject {
    Q_OBJECT

private slots:
    void parsesValidFrame();
    void parsesSignedForce();
    void parsesDetectionFields();
    void absentDetectionFieldsAreFlagged();
    void rejectsFrameWithoutRequiredFields();
    void ignoresMalformedOptionalFields();
};

void ProtocolTest::parsesValidFrame() {
    Sample s;
    const QString frame = QStringLiteral(
        "<ECM><PosR>123</PosR><ForceR>-7</ForceR>"
        "<RecordOn>1</RecordOn><PassNo>3</PassNo><EsPos>120</EsPos></ECM>");

    QVERIFY2(parsePacket(frame, s), qPrintable(frame));
    QCOMPARE(s.pos, 123);
    QCOMPARE(s.force, -7);
    QCOMPARE(s.recordOn, true);
    QCOMPARE(s.passNo, 3);
    QCOMPARE(s.esPos, 120);
}

void ProtocolTest::parsesSignedForce() {
    Sample s;
    const QString frame = QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>65529</ForceR></ECM>");

    QVERIFY(parsePacket(frame, s));
    QCOMPARE(s.force, -7);
    QCOMPARE(s.recordOn, false);
    QCOMPARE(s.passNo, 0);
}

void ProtocolTest::rejectsFrameWithoutRequiredFields() {
    Sample s;
    QVERIFY(!parsePacket(QStringLiteral("<ECM><PosR>10</PosR></ECM>"), s));
    QVERIFY(!parsePacket(QStringLiteral("<ECM><ForceR>10</ForceR></ECM>"), s));
}

void ProtocolTest::ignoresMalformedOptionalFields() {
    Sample s;
    QVERIFY(parsePacket(QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>20</ForceR>"
        "<RecordOn>yes</RecordOn><PassNo>abc</PassNo><EsPos>def</EsPos></ECM>"), s));
    QCOMPARE(s.recordOn, false);
    QCOMPARE(s.passNo, 0);
    QCOMPARE(s.esPos, 0);
}

QTEST_GUILESS_MAIN(ProtocolTest)
#include "tst_protocol.moc"
void ProtocolTest::parsesDetectionFields() {
    Sample s;
    const QString frame = QStringLiteral(
        "<ECM><PosR>1000</PosR><ForceR>-7</ForceR><RecordOn>1</RecordOn>"
        "<PassNo>-1</PassNo><EsPos>1029</EsPos>"
        "<WeldH>1852</WeldH><BaseH>1214</BaseH>"
        "<Misalign>-120</Misalign><Rounds>12</Rounds></ECM>");

    QVERIFY2(parsePacket(frame, s), qPrintable(frame));
    QCOMPARE(s.weldHum, 1852);      // µm
    QCOMPARE(s.baseH, 1214);
    QCOMPARE(s.misalignUm, -120);   // 可负
    QCOMPARE(s.rounds, 12);
    QVERIFY(s.hasDetect);
}

void ProtocolTest::absentDetectionFieldsAreFlagged() {
    // 旧程序 / 未升级的 EKI 配置：4 个检测字段缺失时不得把 0 当成有效值
    Sample s;
    QVERIFY(parsePacket(QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>20</ForceR><PassNo>-1</PassNo></ECM>"), s));
    QVERIFY(!s.hasDetect);
    QCOMPARE(s.weldHum, 0);
    QCOMPARE(s.baseH, 0);
    QCOMPARE(s.misalignUm, 0);
    QCOMPARE(s.rounds, 0);
}

