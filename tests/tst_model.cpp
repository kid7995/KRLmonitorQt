#include <QtTest/QtTest>

#include "model.h"

class ModelTest : public QObject {
    Q_OBJECT

private slots:
    void computesPassesAndSkipsZeroSegments();
    void keepsOpenPassWhenInputEndsInsidePass();
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

QTEST_GUILESS_MAIN(ModelTest)
#include "tst_model.moc"