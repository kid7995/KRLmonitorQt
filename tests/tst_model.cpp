#include <QtTest/QtTest>

#include "model.h"

class ModelTest : public QObject {
    Q_OBJECT

private slots:
    void computesPassesAndSkipsZeroSegments();
    void keepsOpenPassWhenInputEndsInsidePass();
    void marksLegacyRetriesByNumberJump();
};

void ModelTest::computesPassesAndSkipsZeroSegments() {
    const QVector<double> times{0.0, 1.0, 2.0, 3.0, 4.0, 5.0};
    const QVector<int> passNo{0, 1, 1, 0, 2, 2};

    const QVector<Pass> passes = computePasses(times, passNo);
    QCOMPARE(passes.size(), 2);

    QCOMPARE(passes[0].no, 1);
    QCOMPARE(passes[0].t0, 1.0);
    QCOMPARE(passes[0].t1, 2.0);
    QCOMPARE(passes[0].i0, 1);
    QCOMPARE(passes[0].i1, 2);

    QCOMPARE(passes[1].no, 2);
    QCOMPARE(passes[1].t0, 4.0);
    QCOMPARE(passes[1].t1, 5.0);
    QCOMPARE(passes[1].i0, 4);
    QCOMPARE(passes[1].i1, 5);
}

void ModelTest::keepsOpenPassWhenInputEndsInsidePass() {
    const QVector<double> times{0.0, 0.5, 1.0};
    const QVector<int> passNo{0, 4, 4};

    const QVector<Pass> passes = computePasses(times, passNo);
    QCOMPARE(passes.size(), 1);
    QCOMPARE(passes[0].no, 4);
    QCOMPARE(passes[0].t0, 0.5);
    QCOMPARE(passes[0].t1, 1.0);
    QCOMPARE(passes[0].i0, 1);
    QCOMPARE(passes[0].i1, 2);
}

// 老版程序没有焊缝号：补充打磨只能靠"编号跳增"判断，编号回落视为新一段
void ModelTest::marksLegacyRetriesByNumberJump() {
    const QVector<double> times{0, 1, 2, 3, 4, 5, 6, 7};
    const QVector<int> passNo{1, 1, 2, 2, 12, 12, 13, 1};

    const QVector<Pass> passes = computePasses(times, passNo);
    QCOMPARE(passes.size(), 5);
    QVERIFY(!passes[0].retry);   // #1
    QVERIFY(!passes[1].retry);   // #2
    QVERIFY(passes[2].retry);    // #12 跳增 → 补
    QVERIFY(passes[3].retry);    // #13 连续但保持补
    QVERIFY(!passes[4].retry);   // 回落到 #1 → 新一段
}

QTEST_GUILESS_MAIN(ModelTest)
#include "tst_model.moc"
