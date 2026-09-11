#include <QtTest/QtTest>
#include <algorithm>

#include "decimate.h"

class DecimateTest : public QObject {
    Q_OBJECT

private slots:
    void passesThroughSmallSeries();
    void preservesMinAndMax();
    void cachedAppendAndInvalidationWork();
};

void DecimateTest::passesThroughSmallSeries() {
    const QVector<double> t{0.0, 0.1, 0.2};
    const QVector<double> y{1.0, 2.0, 3.0};

    const XY result = decimate(t, y, 10);
    QCOMPARE(result.t, t);
    QCOMPARE(result.y, y);
}

void DecimateTest::preservesMinAndMax() {
    const QVector<double> t{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const QVector<double> y{0, -5, 2, 4, 9, 1, 3, 4, 5, 6};

    const XY result = decimate(t, y, 4);
    QCOMPARE(result.t.size(), 4);
    QCOMPARE(result.y.size(), 4);
    QVERIFY(std::find(result.y.begin(), result.y.end(), -5.0) != result.y.end());
    QVERIFY(std::find(result.y.begin(), result.y.end(), 9.0) != result.y.end());

    for (int i = 1; i < result.t.size(); ++i) {
        QVERIFY(result.t[i] >= result.t[i - 1]);
    }
}

void DecimateTest::cachedAppendAndInvalidationWork() {
    DecCache cache;
    QVector<double> t{0.0, 1.0, 2.0};
    QVector<int> y{1, 2, 3};

    XY first = cachedXY(cache, QStringLiteral("pos"), t, y);
    QCOMPARE(first.t.size(), 3);
    QCOMPARE(cache.e.value(QStringLiteral("pos")).n, 3);

    t.append(3.0);
    t.append(4.0);
    y.append(4);
    y.append(5);

    XY appended = cachedXY(cache, QStringLiteral("pos"), t, y);
    QCOMPARE(appended.t.size(), 5);
    QCOMPARE(appended.t.last(), 4.0);
    QCOMPARE(appended.y.last(), 5.0);
    QCOMPARE(cache.e.value(QStringLiteral("pos")).n, 3);

    t.resize(2);
    y.resize(2);
    XY invalidated = cachedXY(cache, QStringLiteral("pos"), t, y);
    QCOMPARE(invalidated.t.size(), 2);
    QCOMPARE(cache.e.value(QStringLiteral("pos")).n, 2);
}

QTEST_GUILESS_MAIN(DecimateTest)
#include "tst_decimate.moc"