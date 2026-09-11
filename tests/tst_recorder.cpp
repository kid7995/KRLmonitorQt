#include <QtTest/QtTest>
#include <QQueue>
#include <QVector>

#include "recorder.h"

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
}

class RecorderTest : public QObject {
    Q_OBJECT

private slots:
    void recordsPassesAndSeals();
    void autoPauseLeavesLaterSamplesQueued();
    void stopsRecordingAtGroupLimit();
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