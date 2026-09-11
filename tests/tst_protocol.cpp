#include <QtTest/QtTest>

#include "protocol.h"

class ProtocolTest : public QObject {
    Q_OBJECT

private slots:
    void parsesValidFrame();
    void parsesSignedForce();
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