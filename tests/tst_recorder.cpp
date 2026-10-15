#include <QtTest/QtTest>
#include <QQueue>
#include <QVector>

#include "recorder.h"
#include "ecmfile.h"

#include <QDir>
#include <QFile>
namespace {
Sample makeSample(double ts, bool recordOn, int pos, int force, int passNo, int esPos) {
    Sample s;
    s.ts = ts;
    s.recordOn = recordOn;
    s.pos = pos;
    s.force = force;
    s.passNo = passNo;
    s.esPos = esPos;
    return s;
}

int createGroup(QVector<DataGroup*>& groups, const QString& name) {
    auto* g = new DataGroup();
    g->name = name;
    g->live = true;
    groups.append(g);
    return groups.size() - 1;
}

Sample makeDetectSample(double ts, int passNo, int esPos,
                        int weldHum, int baseH, int misalignUm, int rounds) {
    Sample s = makeSample(ts, true, 1000, 0, passNo, esPos);
    s.hasDetect = true;
    s.weldHum = weldHum;
    s.baseH = baseH;
    s.misalignUm = misalignUm;
    s.rounds = rounds;
    return s;
}
}

class RecorderTest : public QObject {
    Q_OBJECT

private slots:
    void recordsPassesAndSeals();
    void autoPauseLeavesLaterSamplesQueued();
    void stopsRecordingAtGroupLimit();
    void latchesDetectionValuesWhileRecording();
    void twoCyclesBecomeTwoGroupsAndRoundTrip();
    void forceSealKeepsNextCycleSeparate();
};

void RecorderTest::recordsPassesAndSeals() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100, 10, 4);

    QQueue<Sample> queue;
    queue.enqueue(makeSample(0.0, false, 5, -5, 0, 0));
    queue.enqueue(makeSample(1.0, true, 10, -1, 1, 100));
    queue.enqueue(makeSample(2.0, true, 11, -2, 1, 101));
    queue.enqueue(makeSample(3.0, true, 12, -3, 2, 102));
    queue.enqueue(makeSample(4.0, false, 13, -4, 0, 0));

    const TickResult result = recorder.process(queue, false);
    QCOMPARE(groups.size(), 1);

    DataGroup* g = groups.first();
    QCOMPARE(g->name, QStringLiteral("打磨#1"));
    QCOMPARE(g->times, QVector<double>({0.0, 1.0, 2.0}));
    QCOMPARE(g->pos, QVector<int>({10, 11, 12}));
    QCOMPARE(g->force, QVector<int>({-1, -2, -3}));
    QCOMPARE(g->passNo, QVector<int>({1, 1, 2}));
    QCOMPARE(g->esPos, QVector<int>({100, 101, 102}));
    QCOMPARE(g->passes.size(), 2);
    QCOMPARE(g->passes[0].no, 1);
    QCOMPARE(g->passes[0].i0, 0);
    QCOMPARE(g->passes[0].i1, 1);
    QCOMPARE(g->passes[1].no, 2);
    QCOMPARE(g->passes[1].i0, 2);
    QCOMPARE(g->passes[1].i1, 2);
    QCOMPARE(g->live, false);
    QCOMPARE(g->curPass, 0);

    QVERIFY(result.sealed);
    QCOMPARE(result.sealedName, QStringLiteral("打磨#1"));
    QCOMPARE(result.sealedPasses, 2);
    QCOMPARE(recorder.recGroupIdx, -1);
    QVERIFY(recorder.monTimes.isEmpty());
}

void RecorderTest::autoPauseLeavesLaterSamplesQueued() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100, 10, 4);

    QQueue<Sample> queue;
    queue.enqueue(makeSample(0.0, false, 0, 0, 0, 0));
    queue.enqueue(makeSample(1.0, true, 1, 1, 1, 1));
    queue.enqueue(makeSample(2.0, true, 2, 2, 1, 2));
    queue.enqueue(makeSample(3.0, false, 3, 3, 0, 3));
    queue.enqueue(makeSample(4.0, true, 4, 4, 1, 4));
    queue.enqueue(makeSample(5.0, true, 5, 5, 1, 5));

    const TickResult result = recorder.process(queue, true);
    QVERIFY(result.sealed);
    QCOMPARE(groups.size(), 1);
    QCOMPARE(queue.size(), 2);
    QCOMPARE(queue.head().ts, 4.0);
}

void RecorderTest::stopsRecordingAtGroupLimit() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100, 10, 1);

    QQueue<Sample> first;
    first.enqueue(makeSample(0.0, false, 0, 0, 0, 0));
    first.enqueue(makeSample(1.0, true, 1, 1, 1, 1));
    first.enqueue(makeSample(2.0, false, 2, 2, 0, 2));
    recorder.process(first, false);
    QCOMPARE(groups.size(), 1);

    QQueue<Sample> second;
    second.enqueue(makeSample(3.0, true, 3, 3, 1, 3));
    const TickResult result = recorder.process(second, false);
    QVERIFY(result.limitHit);
    QCOMPARE(result.started, -1);
    QCOMPARE(groups.size(), 1);
    QCOMPARE(recorder.recGroupIdx, -1);
}

QTEST_GUILESS_MAIN(RecorderTest)
#include "tst_recorder.moc"
void RecorderTest::latchesDetectionValuesWhileRecording() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100, 10, 4);

    QQueue<Sample> queue;
    queue.enqueue(makeSample(0.0, false, 0, 0, 0, 0));
    // 检测段：KRL 上报 焊缝高度 1852µm / 基材 1214 / 错边 -120µm / 计划 12 轮
    queue.enqueue(makeDetectSample(1.0, -1, 1029, 1852, 1214, -120, 12));
    queue.enqueue(makeDetectSample(1.1, -1, 1029, 1852, 1214, -120, 12));
    // 打磨阶段：不再带检测字段，快照必须保持不被清零
    queue.enqueue(makeSample(2.0, true, 1000, -5, -11, 1029));
    queue.enqueue(makeSample(3.0, true, 1000, -5, 1, 1030));
    queue.enqueue(makeSample(4.0, false, 1000, -5, 0, 0));

    const TickResult result = recorder.process(queue, false);
    QVERIFY(result.sealed);
    QCOMPARE(groups.size(), 1);

    const DataGroup* g = groups.first();
    const SeamStat* s = g->seam.find(1);
    QVERIFY(s);
    QVERIFY(s->hasValues);
    QCOMPARE(s->weldHum, 1852);
    QCOMPARE(s->baseH, 1214);
    QCOMPARE(s->misalignUm, -120);
    QCOMPARE(s->rounds, 12);
    QCOMPARE(s->detectEs, 1029);
    QCOMPARE(s->craft, Craft::Rough);
    QCOMPARE(s->normalN, 1);
}

// 连续生产：两个 RecordOn 周期必须变成两个独立的数据组，各自带自己的焊缝统计；
// 落盘再读回后，检测结果快照与段表都必须一致（这就是"换料件能识别 + 自动存"的数据层验证）。
void RecorderTest::twoCyclesBecomeTwoGroupsAndRoundTrip() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100000, 10000, 64);

    QQueue<Sample> queue;
    queue.enqueue(makeSample(0.0, false, 0, 0, 0, 0));            // 首帧：只确定边沿起点

    // ── 第一件：焊缝1 检测 + 两道打磨 ──
    // 道次序列与 KRL 侧一致：每个 FOR 迭代写一次 PassNo=j，段末写 -11
    queue.enqueue(makeDetectSample(1.0, -1, 1029, 1852, 1214, -120, 12));
    queue.enqueue(makeSample(2.0, true, 1000, -5, -11, 1029));
    queue.enqueue(makeSample(3.0, true, 1000, -5, 1, 1030));
    queue.enqueue(makeSample(4.0, true, 1000, -5, -11, 1030));
    queue.enqueue(makeSample(5.0, true, 1000, -5, 2, 1060));
    queue.enqueue(makeSample(6.0, true, 1000, -5, -11, 1060));

    // ── 第二件：焊缝2 检测 + 一道打磨（同一连接上继续）──
    queue.enqueue(makeSample(7.0, false, 0, 0, 0, 0));            // 封第一组
    queue.enqueue(makeDetectSample(8.0, -2, 2050, 2100, 2500, 350, 8));
    queue.enqueue(makeSample(9.0, true, 2000, -3, -12, 2050));
    queue.enqueue(makeSample(10.0, true, 2000, -3, 1, 2051));
    queue.enqueue(makeSample(11.0, true, 2000, -3, -12, 2051));
    queue.enqueue(makeSample(12.0, false, 0, 0, 0, 0));           // 封第二组

    TickResult r = recorder.process(queue, false);
    QVERIFY(r.sealed);
    QCOMPARE(groups.size(), 2);

    // 两组各自独立：焊缝号不同、检测值不同、相互不串
    const DataGroup* g1 = groups[0];
    const DataGroup* g2 = groups[1];
    QCOMPARE(g1->seam.seams.size(), 1);
    QCOMPARE(g2->seam.seams.size(), 1);
    const SeamStat* s1 = g1->seam.find(1);
    const SeamStat* s2 = g2->seam.find(2);
    QVERIFY(s1);
    QVERIFY(s2);
    QVERIFY(!g1->seam.find(2));
    QVERIFY(!g2->seam.find(1));
    QCOMPARE(s1->weldHum, 1852);
    QCOMPARE(s2->weldHum, 2100);
    QCOMPARE(s1->normalN, 2);
    QCOMPARE(s2->normalN, 1);
    QCOMPARE(s1->detectEs, 1029);
    QCOMPARE(s2->detectEs, 2050);
    QVERIFY(!g1->incomplete);
    QVERIFY(!g2->incomplete);

    // 落盘 → 读回：段表、道次、检测快照都要一致
    const QString p1 = QDir::tempPath() + QStringLiteral("/krlmonitor_it_1.ecmb");
    const QString p2 = QDir::tempPath() + QStringLiteral("/krlmonitor_it_2.ecmb");
    QFile::remove(p1);
    QFile::remove(p2);
    EcmSaveOptions opt;
    QString err;
    QVERIFY2(saveEcm(p1, *g1, EcmFileMeta(), opt, err), qPrintable(err));
    QVERIFY2(saveEcm(p2, *g2, EcmFileMeta(), opt, err), qPrintable(err));

    DataGroup b1;
    DataGroup b2;
    EcmFileMeta m1;
    EcmFileMeta m2;
    QVERIFY2(loadEcm(p1, b1, m1, err), qPrintable(err));
    QVERIFY2(loadEcm(p2, b2, m2, err), qPrintable(err));

    QCOMPARE(b1.seam.segments.size(), g1->seam.segments.size());
    QCOMPARE(b1.passes.size(), g1->passes.size());
    const SeamStat* r1 = b1.seam.find(1);
    QVERIFY(r1);
    QVERIFY(r1->hasValues);
    QCOMPARE(r1->weldHum, 1852);
    QCOMPARE(r1->misalignUm, -120);
    QCOMPARE(r1->rounds, 12);
    const SeamStat* r2 = b2.seam.find(2);
    QVERIFY(r2);
    QCOMPARE(r2->weldHum, 2100);
    QCOMPARE(r2->baseH, 2500);

    QFile::remove(p1);
    QFile::remove(p2);
}

// 断线兜底：强制封存后，下一个上升沿必须新建一组，绝不能把两件并成一组
void RecorderTest::forceSealKeepsNextCycleSeparate() {
    QVector<DataGroup*> groups;
    Recorder recorder(&groups, [&groups](const QString& name) {
        return createGroup(groups, name);
    }, 100000, 10000, 64);

    QQueue<Sample> queue;
    queue.enqueue(makeSample(0.0, false, 0, 0, 0, 0));
    queue.enqueue(makeDetectSample(1.0, -1, 1029, 1000, 100, 0, 5));
    queue.enqueue(makeSample(2.0, true, 1000, -5, 1, 1030));
    recorder.process(queue, false);
    QCOMPARE(groups.size(), 1);
    QCOMPARE(recorder.recGroupIdx, 0);

    // 模拟网线掉了：此时 RecordOn 还是 TRUE，没有下降沿
    TickResult sealed;
    QVERIFY(recorder.forceSeal(sealed));
    QVERIFY(sealed.sealed);
    QCOMPARE(recorder.recGroupIdx, -1);
    QVERIFY(groups[0]->incomplete);          // 必须标记"可能不完整"
    QVERIFY(!groups[0]->live);

    // 重连后第一帧的 RecordOn 已经是 TRUE（第二件正在跑）
    QQueue<Sample> resumed;
    resumed.enqueue(makeSample(3.0, true, 2000, -3, -2, 2050));
    resumed.enqueue(makeSample(4.0, true, 2000, -3, 1, 2051));
    resumed.enqueue(makeSample(5.0, false, 0, 0, 0, 0));
    recorder.process(resumed, false);

    QCOMPARE(groups.size(), 2);              // 第二件是独立的一组，没有并进第一组
    QCOMPARE(groups[1]->times.size(), 2);
    QVERIFY(groups[1]->seam.find(2));
    QVERIFY(!groups[1]->seam.find(1));
}

