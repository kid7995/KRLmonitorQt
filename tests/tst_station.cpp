#include <QtTest/QtTest>
#include <QVector>
#include <QStringList>

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
    void decodesFineCraftRanges();
    void decodesGrindSubPhases();
    void negativeMarkersDoNotCreatePasses();
    void tracksSeamsAcrossDetectAndGrind();
    void stickyRetryNumbering();
    void legacyStreamWithoutMarkers();
    void buildsSegmentTable();
    void grindingCycleSplitsBySeam();
    void detectSnapshotSurvivesRebuild();
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
void StationTest::decodesFineCraftRanges() {
    // 细磨复用同一套编码，整体 +100（工序位）；粗磨 = Rough，细磨 = Fine
    QCOMPARE(decodePassNo(-11).craft, Craft::Rough);
    QCOMPARE(decodePassNo(1).craft, Craft::Unknown);   // 正数道次不自带工序

    for (int k = 1; k <= 8; ++k) {
        const PassMark d = decodePassNo(-(100 + k));
        QCOMPARE(d.phase, SeamPhase::Detect);
        QCOMPARE(d.craft, Craft::Fine);
        QCOMPARE(d.seam, k);

        const PassMark g = decodePassNo(-(110 + k));
        QCOMPARE(g.phase, SeamPhase::GrindGap);
        QCOMPARE(g.craft, Craft::Fine);
        QCOMPARE(g.seam, k);

        const PassMark m = decodePassNo(-(120 + k));
        QCOMPARE(m.phase, SeamPhase::Transfer);
        QCOMPARE(m.craft, Craft::Fine);
        QCOMPARE(m.seam, k);
    }

    QCOMPARE(decodePassNo(-99).phase, SeamPhase::Finish);
    QCOMPARE(decodePassNo(-99).craft, Craft::Rough);
    QCOMPARE(decodePassNo(-199).phase, SeamPhase::Finish);
    QCOMPARE(decodePassNo(-199).craft, Craft::Fine);

    // 未定义区间仍必须落回 None
    QCOMPARE(decodePassNo(-100).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-109).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-110).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-119).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-120).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-129).phase, SeamPhase::None);
    QCOMPARE(decodePassNo(-198).phase, SeamPhase::None);
}

void StationTest::buildsSegmentTable() {
    // 焊缝1：检测 -1 → 间隙 -11 → 道1 → 间隙 -11 → 道2 → 间隙 -11 → 补充道 12 → 间隙 -11
    const QVector<int> pn{-1, -1, -11, 1, 1, -11, 2, -11, 12, -11};
    const QVector<int> es{1000, 1005, 980, 1000, 1000, 1000, 1030, 1030, 1090, 1090};
    const DataGroup g = makeGroup(pn, es);

    const QVector<Segment>& seg = g.seam.segments;
    QCOMPARE(seg.size(), 8);

    // 段 0：检测（负标记段，带工序与焊缝号）
    QCOMPARE(seg[0].kind, SeamPhase::Detect);
    QCOMPARE(seg[0].craft, Craft::Rough);
    QCOMPARE(seg[0].seam, 1);
    QCOMPARE(seg[0].passNo, 0);
    QCOMPARE(seg[0].i0, 0);
    QCOMPARE(seg[0].i1, 1);

    // 段 1：打磨间隙
    QCOMPARE(seg[1].kind, SeamPhase::GrindGap);
    QCOMPARE(seg[1].i0, 2);
    QCOMPARE(seg[1].i1, 2);

    // 段 2：第 1 道（kind == Grind 表示"打磨道次"）
    QCOMPARE(seg[2].kind, SeamPhase::Grind);
    QCOMPARE(seg[2].passNo, 1);
    QCOMPARE(seg[2].seam, 1);
    QCOMPARE(seg[2].craft, Craft::Rough);
    QVERIFY(!seg[2].retry);
    QCOMPARE(seg[2].i0, 3);
    QCOMPARE(seg[2].i1, 4);

    QCOMPARE(seg[4].kind, SeamPhase::Grind);
    QCOMPARE(seg[4].passNo, 2);
    QCOMPARE(seg[4].i0, 6);
    QCOMPARE(seg[4].i1, 6);

    // 段 6：补充打磨道（编号跳增）必须被标记
    QCOMPARE(seg[6].kind, SeamPhase::Grind);
    QCOMPARE(seg[6].passNo, 12);
    QCOMPARE(seg[6].seam, 1);
    QVERIFY(seg[6].retry);
    QCOMPARE(seg[6].i0, 8);
    QCOMPARE(seg[6].i1, 8);

    // 最后一段（打磨间隙）必须被收尾闭合，否则图上会缺一段
    QCOMPARE(seg[7].kind, SeamPhase::GrindGap);
    QCOMPARE(seg[7].passNo, 0);
    QCOMPARE(seg[7].i0, 9);
    QCOMPARE(seg[7].i1, 9);

    // 覆盖全部样点，且段间无缝衔接
    for (int i = 1; i < seg.size(); ++i) {
        QCOMPARE(seg[i].i0, seg[i - 1].i1 + 1);
    }
    QCOMPARE(seg.last().i1, pn.size() - 1);
}

void StationTest::detectSnapshotSurvivesRebuild() {
    const QVector<int> pn{-1, -1, -11, 1};
    const QVector<int> es{1000, 1005, 980, 1000};
    DataGroup g = makeGroup(pn, es);

    const SeamStat* before = g.seam.find(1);
    QVERIFY(before);
    QVERIFY(!before->hasValues);        // 离线重建不带检测字段

    // 实时路径：检测段喂入检测结果（焊缝高度 1852µm / 基材 1214 / 错边 -120µm / 12 轮）
    DetectVals dv;
    dv.has = true;
    dv.weldHum = 1852;
    dv.baseH = 1214;
    dv.misalignUm = -120;
    dv.rounds = 12;
    g.seam.feed(1.5, -1, 1000, 0, 1005, 1, dv);

    // 重建（模拟裁剪 / 重新统计）后快照必须保留
    rebuildSeamStats(g);
    const SeamStat* after = g.seam.find(1);
    QVERIFY(after);
    QVERIFY(after->hasValues);
    QCOMPARE(after->weldHum, 1852);
    QCOMPARE(after->baseH, 1214);
    QCOMPARE(after->misalignUm, -120);
    QCOMPARE(after->rounds, 12);
    QCOMPARE(after->craft, Craft::Rough);
}

// 2026-10-15 新增：打磨阶段的细分子阶段
//     -(40+k) 走到焊缝 k 的打磨预备位（KRL 下到 Pk4，Z=+40 抬高位）
//     -(50+k) 打磨后抬起（从打磨位回到 Pk4）
// 目的：把"一次完整打磨"拆成 空走 → 预备位 → 打磨 → 抬起，检测阶段保持整体不变。
void StationTest::decodesGrindSubPhases() {
    for (int k = 1; k <= 8; ++k) {
        const PassMark prep = decodePassNo(-(40 + k));
        QCOMPARE(prep.phase, SeamPhase::Prepare);
        QCOMPARE(prep.seam, k);
        QCOMPARE(prep.craft, Craft::Rough);

        const PassMark lift = decodePassNo(-(50 + k));
        QCOMPARE(lift.phase, SeamPhase::Lift);
        QCOMPARE(lift.seam, k);
        QCOMPARE(lift.craft, Craft::Rough);

        // 细磨同一套规则，整体 +100
        const PassMark prepFine = decodePassNo(-(140 + k));
        QCOMPARE(prepFine.phase, SeamPhase::Prepare);
        QCOMPARE(prepFine.seam, k);
        QCOMPARE(prepFine.craft, Craft::Fine);

        const PassMark liftFine = decodePassNo(-(150 + k));
        QCOMPARE(liftFine.phase, SeamPhase::Lift);
        QCOMPARE(liftFine.seam, k);
        QCOMPARE(liftFine.craft, Craft::Fine);
    }

    // 区间之间（-39/-40/-49/-50/-59 等）必须落回"未标记"，不能误判成任何动作
    const QVector<int> undef{-39, -40, -49, -50, -59, -139, -140, -149, -150, -159};
    for (int v : undef) {
        QCOMPARE(decodePassNo(v).phase, SeamPhase::None);
    }

    // 界面上要能直接读出来的动作名
    QCOMPARE(seamPhaseLabel(SeamPhase::Prepare), QStringLiteral("预备位"));
    QCOMPARE(seamPhaseLabel(SeamPhase::Lift), QStringLiteral("抬起"));
    QCOMPARE(seamPhaseLabel(SeamPhase::Transfer), QStringLiteral("空走"));
}

// 改完 KRL 之后，打磨阶段的 PassNo 序列长这样（焊缝 1 → 焊缝 2）：
//   -21 空走到焊缝1 / -41 预备位 / 1,2 两道 / -11 道间 / 12 补充道 / -51 抬起 /
//   -22 空走到焊缝2 / -42 预备位 / 1 / -52 抬起
// 必须切成 12 段、段段都归属到具体焊缝（seam 不能为 0），
// 否则"点选某一条焊缝"就会串台或漏显示。
void StationTest::grindingCycleSplitsBySeam() {
    const QVector<int> pn{-21, -21, -41, -41, 1, 1, -11, 2, 2, -11, 12, 12,
                          -51, -51, -22, -22, -42, 1, -52};
    const DataGroup g = makeGroup(pn, QVector<int>(pn.size(), 1000));

    QStringList ids;
    for (const Segment& s : g.seam.segments) {
        ids << QStringLiteral("%1/%2").arg(int(s.kind)).arg(s.seam);
        QVERIFY2(s.seam >= 1, "打磨阶段的每一段都必须归属到某条焊缝");
    }
    QCOMPARE(ids.join(QLatin1Char('|')),
             QStringLiteral("4/1|6/1|2/1|3/1|2/1|3/1|2/1|7/1|4/2|6/2|2/2|7/2"));

    // 道次挂在正确的焊缝上，补充打磨仍按"编号跳增"识别
    QCOMPARE(g.passes.size(), 4);
    QCOMPARE(g.passes[0].seam, 1);
    QCOMPARE(g.passes[1].seam, 1);
    QCOMPARE(g.passes[2].seam, 1);
    QCOMPARE(g.passes[3].seam, 2);
    QVERIFY(g.passes[2].retry);
    QVERIFY(!g.passes[3].retry);

    const SeamStat* s1 = g.seam.find(1);
    const SeamStat* s2 = g.seam.find(2);
    QVERIFY(s1);
    QVERIFY(s2);
    QCOMPARE(s1->normalN, 2);
    QCOMPARE(s1->retryN, 1);
    QCOMPARE(s2->normalN, 1);
    QCOMPARE(s2->retryN, 0);
}
