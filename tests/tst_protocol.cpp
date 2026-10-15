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
    void treatsUnsetDetectionAsAbsent();
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

// KRL 侧把"尚未检测"的检测结果初始化为 -1
// （$config.dat: DECL INT ECM_WeldH = -1 …，检测段测量完成后才写成真实值）。
// 上位机必须把"全 -1"当成"没有值"，否则界面会显示 -0.001 mm / -1 轮这种假数据。
void ProtocolTest::treatsUnsetDetectionAsAbsent() {
    Sample s;
    QVERIFY(parsePacket(QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>20</ForceR><PassNo>-1</PassNo><EsPos>1028</EsPos>"
        "<WeldH>-1</WeldH><BaseH>-1</BaseH><Misalign>-1</Misalign><Rounds>-1</Rounds></ECM>"), s));
    QVERIFY(!s.hasDetect);          // 还没测过 → 界面显示"—"
    QCOMPARE(s.esPos, 1028);        // 但检测段基准（EsPos）依然有效

    // 只要有任意一个是真实值就算有效检测：Rounds = 0（计划 0 轮）是合法值，不是"未赋值"
    Sample z;
    QVERIFY(parsePacket(QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>20</ForceR>"
        "<WeldH>-1</WeldH><BaseH>-1</BaseH><Misalign>-1</Misalign><Rounds>0</Rounds></ECM>"), z));
    QVERIFY(z.hasDetect);
    QCOMPARE(z.rounds, 0);

    // 正常检测帧不受影响（µm 语义不变，-1 只作哨兵）
    Sample v;
    QVERIFY(parsePacket(QStringLiteral(
        "<ECM><PosR>10</PosR><ForceR>20</ForceR>"
        "<WeldH>1852</WeldH><BaseH>1214</BaseH><Misalign>-120</Misalign><Rounds>12</Rounds></ECM>"), v));
    QVERIFY(v.hasDetect);
    QCOMPARE(v.weldHum, 1852);
    QCOMPARE(v.baseH, 1214);
    QCOMPARE(v.misalignUm, -120);
    QCOMPARE(v.rounds, 12);
}

