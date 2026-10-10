#include <QtTest/QtTest>
#include <QVector>

#include "model.h"
#include "station.h"

namespace {

// 构造一段数据（times 由调用方给）
DataGroup makeGroup(const QVector<int>& passNo, const QVector<int>& esPos,
                    QVector<Pass>* outPasses = nullptr) {
    DataGroup g;
    for (int i = 0; i < passNo.size(); ++i) {
        g.times.append(double(i));
        g.pos.append(1000 + i);
        g.force.append(-i);
        g.passNo.append(passNo[i]);
        g.esPos.append(esPos.value(i, 0));
    }
    g.passes = computePasses(g.times, g.passNo);
    rebuildSeamStats(g);
    if (outPasses) *outPasses = g.passes;
    return g;
}

}  // namespace

class StationTest : public QObject {
    Q_OBJECT

private slots:
    void decodesMarkerRanges();
    void negativeMarkersDoNotCreatePasses();
    void tracksSeamsAcrossDetectAndGrind();
    void stickyRetryNumbering();
    void legacyStreamWithoutMarkers();
};

void StationTest::decodesMarkerRanges() {
    QCOMPARE(int(decodePassNo(7).phase), int(SeamPhase::Grind));
    QCOMPARE(decodePassNo(0).phase, SeamPhase::None);

    for (int k = 1; k <= 8; ++k) {
        const PassMark d = decodePassNo(-k);
        QCOMPARE(d.phase, SeamPhase::Detect);
        QCOMPARE(d.seam, k);

        const PassMark g = decodePassNo(-(10 + k));
        QCOMPARE(g.phase, SeamPhase::GrindGap);
        QCOMPARE(g.seam, k);

        const PassMark m = decodePassNo(-(20 + k));
        QCOMPARE(m.phase, SeamPhase::Transfer);
        QCOMPARE(m.seam, k);
    }

    QCOMPARE(decodePassNo(-99).phase, SeamPhase::Finish);
    // 未定义区间必须落回 None，避免误判
    QCOMPARE(decodePassNo(-9).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-10).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-19).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-20).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-29).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-100).phase, SeamPhase::None);
}

void StationTest::negativeMarkersDoNotCreatePasses() {
    const QVector<int> pn{-1, -11, 1, 1, -11, 2, -12, -2};
    const QVector<double> t{0, 1, 2, 3, 4, 5, 6, 7};
    const QVector<Pass> passes = computePasses(t, pn);

    QCOMPARE(passes.size(), 2);
    QCOMPARE(passes[0].no, 1);
    QCOMPARE(passes[0].i0, 2);
    QCOMPARE(passes[0].i1, 3);
    QCOMPARE(passes[1].no, 2);
    QCOMPARE(passes[1].i0, 5);
    QCOMPARE(passes[1].i1, 5);
}

void StationTest::tracksSeamsAcrossDetectAndGrind() {
    // 焊缝1：检测 -1 → 打磨 1,2,3 → 非打磨段 -11 → 补充打磨 12,13
    // 焊缝2：检测 -2 → 打磨 1,2 → 非打磨段 -12
    const QVector<int> pn{-1, -1, -11, 1, 1, -11, 2, 3, -11, 12, 13,
                          -2, -12, 1, 2, -12};
    const QVector<int> es{1000, 1005, 980, 1000, 1000, 1000, 1030, 1060, 1060, 1090, 1090,
                          2000, 1990, 2010, 2040, 2040};

    QVector<Pass> passes;
    const DataGroup g = makeGroup(pn, es, &passes);

    QCOMPARE(g.seam.seams.size(), 2);

    const SeamStat* s1 = g.seam.find(1);
    QVERIFY(s1);
    QVERIFY(s1->detected);
    QCOMPARE(s1->detectEs, 1005);      // 检测段最后收到的 EsPos = 该焊缝基准高度
    QCOMPARE(s1->pts, 11);
    QCOMPARE(s1->normalN, 3);          // 第 1/2/3 遍
    QCOMPARE(s1->retryN, 2);           // 补充打磨 12、13
    QCOMPARE(s1->curPass, 13);
    QVERIFY(s1->retryNow);

    const SeamStat* s2 = g.seam.find(2);
    QVERIFY(s2);
    QVERIFY(s2->detected);
    QCOMPARE(s2->detectEs, 2000);
    QCOMPARE(s2->pts, 5);
    QCOMPARE(s2->normalN, 2);
    QCOMPARE(s2->retryN, 0);
    QCOMPARE(s2->curPass, 2);
    QVERIFY(!s2->retryNow);

    // 道次必须落到正确的焊缝，且补充打磨被标注
    QCOMPARE(passes.size(), 7);
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(passes[i].seam, 1);
    }
    QCOMPARE(passes[3].no, 12);
    QVERIFY(passes[3].retry);
    QVERIFY(passes[4].retry);
    for (int i = 5; i < 7; ++i) {
        QCOMPARE(passes[i].seam, 2);
        QVERIFY(!passes[i].retry);
    }
}

void StationTest::stickyRetryNumbering() {
    // 补充打磨编号连续时（12,13,14）仍必须全部算作补充打磨
    const QVector<int> pn{-1, 1, 2, 12, 13, 14};
    const QVector<int> es{500, 500, 530, 560, 560, 560};
    const DataGroup g = makeGroup(pn, es);

    const SeamStat* s = g.seam.find(1);
    QVERIFY(s);
    QCOMPARE(s->normalN, 2);
    QCOMPARE(s->retryN, 3);
}

void StationTest::legacyStreamWithoutMarkers() {
    // 旧协议（PassNo 全为非负）不得产生焊缝统计，道次仍照常切分
    const QVector<int> pn{0, 1, 1, 0, 2, 2};
    const QVector<int> es{0, 100, 100, 0, 200, 200};
    QVector<Pass> passes;
    const DataGroup g = makeGroup(pn, es, &passes);

    QVERIFY(g.seam.seams.isEmpty());
    QCOMPARE(passes.size(), 2);
    QCOMPARE(passes[0].seam, 0);
    QCOMPARE(passes[1].seam, 0);
    QVERIFY(!passes[0].retry);
}

QTEST_GUILESS_MAIN(StationTest)
#include "tst_station.moc"