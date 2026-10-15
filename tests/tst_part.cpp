#include <QtTest/QtTest>

#include "part.h"

namespace {

QDateTime at(int minute) {
    return QDateTime(QDate(2026, 10, 14), QTime(10, 0)).addSecs(minute * 60);
}

}  // namespace

class PartTest : public QObject {
    Q_OBJECT

private slots:
    void perCycleMakesOnePartPerCycle();
    void craftRestartSplitsWhenRoughComesBack();
    void seamWrapSplitsOnRewindInSameCraft();
    void idleTimeoutStartsFresh();
    void manualSplitAlwaysWorks();
    void archiveNameFromTemplate();
};

void PartTest::perCycleMakesOnePartPerCycle() {
    PartSplitter sp;
    sp.configure(PartRule::PerCycle, 30);
    sp.reset(at(0));

    QVERIFY(sp.onCycleStart(at(0)));          // 第一件
    QCOMPARE(sp.current().label, QStringLiteral("件0001"));
    sp.onSeamSeen(1, Craft::Rough, at(1));
    sp.onCycleEnd(at(5));
    QCOMPARE(sp.current().cycles, 1);

    QVERIFY(sp.onCycleStart(at(6)));          // 换件
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));
    QCOMPARE(sp.current().cycles, 1);
}

void PartTest::craftRestartSplitsWhenRoughComesBack() {
    PartSplitter sp;
    sp.configure(PartRule::CraftRestart, 30);
    sp.reset(at(0));

    // 一件 = 粗磨(1..4) + 粗磨(5..8) + 细磨(1..4) + 细磨(5..8)，四个周期都必须归同一件
    sp.onCycleStart(at(0));
    for (int k = 1; k <= 4; ++k) QVERIFY(!sp.onSeamSeen(k, Craft::Rough, at(k)));
    sp.onCycleEnd(at(5));
    sp.onCycleStart(at(6));
    for (int k = 5; k <= 8; ++k) QVERIFY(!sp.onSeamSeen(k, Craft::Rough, at(6 + k)));
    sp.onCycleEnd(at(15));
    sp.onCycleStart(at(16));
    for (int k = 1; k <= 4; ++k) QVERIFY(!sp.onSeamSeen(k, Craft::Fine, at(16 + k)));
    sp.onCycleEnd(at(21));
    sp.onCycleStart(at(22));
    for (int k = 5; k <= 8; ++k) QVERIFY(!sp.onSeamSeen(k, Craft::Fine, at(22 + k)));
    sp.onCycleEnd(at(31));

    QCOMPARE(sp.current().label, QStringLiteral("件0001"));
    QCOMPARE(sp.current().cycles, 4);

    // 下一件又从粗磨开始 -> 切新件
    sp.onCycleStart(at(60));
    QVERIFY(sp.onSeamSeen(1, Craft::Rough, at(61)));
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));
}

void PartTest::seamWrapSplitsOnRewindInSameCraft() {
    PartSplitter sp;
    sp.configure(PartRule::SeamWrap, 30);
    sp.reset(at(0));

    sp.onCycleStart(at(0));
    for (int k = 1; k <= 4; ++k) {
        QVERIFY(!sp.onSeamSeen(k, Craft::Rough, at(k)));
    }
    sp.onCycleEnd(at(5));

    // 同工序内回绕到 1 -> 新件
    sp.onCycleStart(at(6));
    QVERIFY(sp.onSeamSeen(1, Craft::Rough, at(7)));
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));

    // 工序切换（粗磨 -> 细磨）不算新件
    sp.onCycleEnd(at(20));
    sp.onCycleStart(at(21));
    QVERIFY(!sp.onSeamSeen(1, Craft::Fine, at(22)));
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));
    QCOMPARE(sp.current().cycles, 2);
}

void PartTest::idleTimeoutStartsFresh() {
    PartSplitter sp;
    sp.configure(PartRule::PerCycle, 10);
    sp.reset(at(0));

    sp.onCycleStart(at(0));
    sp.onCycleEnd(at(1));
    QCOMPARE(sp.current().label, QStringLiteral("件0001"));

    QVERIFY(!sp.onIdle(at(5)));               // 未超时
    QVERIFY(sp.onIdle(at(40)));               // 超时 -> 下一件从新时间点开始
    QVERIFY(!sp.started());

    sp.onCycleStart(at(41));
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));
}

void PartTest::manualSplitAlwaysWorks() {
    PartSplitter sp;
    sp.configure(PartRule::CraftRestart, 30);
    sp.reset(at(0));

    sp.onCycleStart(at(0));
    QCOMPARE(sp.current().label, QStringLiteral("件0001"));
    QVERIFY(sp.startNewPart(at(2)));
    QCOMPARE(sp.current().label, QStringLiteral("件0002"));

    // 跨重启续号
    sp.setStartSeq(100);
    sp.startNewPart(at(3));
    QCOMPARE(sp.current().label, QStringLiteral("件0101"));
}

void PartTest::archiveNameFromTemplate() {
    PartInfo p;
    p.id = 7;
    p.label = QStringLiteral("件0007");
    const QDateTime t(QDate(2026, 10, 14), QTime(16, 52, 33));

    QCOMPARE(buildArchiveName(QString(), t, p, Craft::Rough, 1, 4),
             QStringLiteral("2026-10-14_16-52_件0007_粗磨_S1-4"));
    QCOMPARE(buildArchiveName(QString(), t, p, Craft::Fine, 5, 8),
             QStringLiteral("2026-10-14_16-52_件0007_细磨_S5-8"));
    QCOMPARE(buildArchiveName(QStringLiteral("{date}/{part}"), t, p, Craft::Unknown, 3, 3),
             QStringLiteral("2026-10-14_件0007"));
    // 模板里没写的占位符不会自动追加；未识别的占位符整段清掉
    QCOMPARE(buildArchiveName(QStringLiteral("{time}-{bogus}"), t, p, Craft::Rough, 2, 2),
             QStringLiteral("2026-10-14_16-52-"));
}

QTEST_GUILESS_MAIN(PartTest)
#include "tst_part.moc"
