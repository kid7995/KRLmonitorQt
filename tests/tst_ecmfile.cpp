#include <QtTest/QtTest>

#include <QDir>
#include <QFile>

#include "ecmfile.h"

namespace {

DataGroup makeGroup() {
    DataGroup g;
    g.name = QStringLiteral("件0001 粗磨 S1-2");
    const QVector<int> pn{-1, -1, -11, 1, 1, -11, 2, -12, -2, -12};
    const QVector<int> es{1000, 1005, 980, 1000, 1010, 1020, 1030, 1040, 2000, 2000};
    for (int i = 0; i < pn.size(); ++i) {
        g.times.append(0.1 * i);
        g.pos.append(1000 + i * 3);
        g.force.append(-i);
        g.passNo.append(pn[i]);
        g.esPos.append(es[i]);
    }
    g.passes = computePasses(g.times, g.passNo);
    rebuildSeamStats(g);
    if (SeamStat* s = g.seam.find(1)) {
        s->hasValues = true;
        s->weldHum = 1852;
        s->baseH = 1214;
        s->misalignUm = -120;
        s->rounds = 12;
    }
    g.hasEs = true;
    g.incomplete = true;
    return g;
}

QString tmpPath(const QString& tag) {
    return QDir::tempPath() + QStringLiteral("/krlmonitor_test_%1.ecmb").arg(tag);
}

void compareGroups(const DataGroup& a, const DataGroup& b) {
    QCOMPARE(b.name, a.name);
    QCOMPARE(b.times.size(), a.times.size());
    for (int i = 0; i < a.times.size(); ++i) {
        QVERIFY2(qAbs(a.times[i] - b.times[i]) < 1e-4, "时间列精度丢失超过 0.1ms");
    }
    QCOMPARE(b.pos, a.pos);
    QCOMPARE(b.force, a.force);
    QCOMPARE(b.passNo, a.passNo);
    QCOMPARE(b.esPos, a.esPos);
    QCOMPARE(b.hasEs, a.hasEs);
    QCOMPARE(b.incomplete, a.incomplete);
    QCOMPARE(b.live, false);

    QCOMPARE(b.seam.segments.size(), a.seam.segments.size());
    for (int i = 0; i < a.seam.segments.size(); ++i) {
        QCOMPARE(int(b.seam.segments[i].kind), int(a.seam.segments[i].kind));
        QCOMPARE(int(b.seam.segments[i].craft), int(a.seam.segments[i].craft));
        QCOMPARE(b.seam.segments[i].seam, a.seam.segments[i].seam);
        QCOMPARE(b.seam.segments[i].passNo, a.seam.segments[i].passNo);
        QCOMPARE(b.seam.segments[i].retry, a.seam.segments[i].retry);
        QCOMPARE(b.seam.segments[i].i0, a.seam.segments[i].i0);
        QCOMPARE(b.seam.segments[i].i1, a.seam.segments[i].i1);
    }

    // 道次表由段表推导，必须与原来的 passes 等价
    QCOMPARE(b.passes.size(), a.passes.size());
    for (int i = 0; i < a.passes.size(); ++i) {
        QCOMPARE(b.passes[i].no, a.passes[i].no);
        QCOMPARE(b.passes[i].seam, a.passes[i].seam);
        QCOMPARE(b.passes[i].retry, a.passes[i].retry);
        QCOMPARE(b.passes[i].i0, a.passes[i].i0);
        QCOMPARE(b.passes[i].i1, a.passes[i].i1);
    }

    const SeamStat* sa = a.seam.find(1);
    const SeamStat* sb = b.seam.find(1);
    QVERIFY(sa);
    QVERIFY(sb);
    QVERIFY(sb->hasValues);
    QCOMPARE(sb->weldHum, sa->weldHum);
    QCOMPARE(sb->baseH, sa->baseH);
    QCOMPARE(sb->misalignUm, sa->misalignUm);
    QCOMPARE(sb->rounds, sa->rounds);
    QCOMPARE(sb->detectEs, sa->detectEs);
    QCOMPARE(sb->normalN, sa->normalN);
    QCOMPARE(sb->retryN, sa->retryN);
    QCOMPARE(int(sb->craft), int(sa->craft));
}

}  // namespace

class EcmFileTest : public QObject {
    Q_OBJECT

private slots:
    void roundTripsAllOptionCombos();
    void peekReadsMetaWithoutDecoding();
    void rejectsForeignAndCorruptFiles();
};

void EcmFileTest::roundTripsAllOptionCombos() {
    const DataGroup src = makeGroup();

    struct Combo { bool compress; bool obfuscate; const char* tag; };
    const Combo combos[] = {
        {true,  true,  "c1o1"},
        {true,  false, "c1o0"},
        {false, true,  "c0o1"},
        {false, false, "c0o0"},
    };

    qint64 plainSize = 0;
    qint64 compressedSize = 0;
    for (const Combo& c : combos) {
        const QString path = tmpPath(QString::fromLatin1(c.tag));
        QFile::remove(path);

        EcmFileMeta meta;
        meta.groupName = src.name;
        meta.partLabel = QStringLiteral("件0001");
        meta.partId = 1;
        EcmSaveOptions opt;
        opt.compress = c.compress;
        opt.obfuscate = c.obfuscate;

        QString err;
        QVERIFY2(saveEcm(path, src, meta, opt, err), qPrintable(err));
        QVERIFY(QFile::exists(path));
        const qint64 sz = QFile(path).size();
        if (!c.compress && !c.obfuscate) {
            plainSize = sz;
        }
        if (c.compress && !c.obfuscate) {
            compressedSize = sz;
        }

        DataGroup back;
        EcmFileMeta backMeta;
        QVERIFY2(loadEcm(path, back, backMeta, err), qPrintable(err));
        compareGroups(src, back);
        QCOMPARE(backMeta.partLabel, QStringLiteral("件0001"));
        QCOMPARE(backMeta.partId, 1);
        QVERIFY(backMeta.incomplete);
        QFile::remove(path);
    }
    // 压缩必须真的省体积（这是需求 5 的核心诉求）
    QVERIFY(plainSize > 0);
    QVERIFY(compressedSize > 0);
    QVERIFY2(compressedSize < plainSize, "压缩后应小于未压缩体积");
}

void EcmFileTest::peekReadsMetaWithoutDecoding() {
    const DataGroup src = makeGroup();
    const QString path = tmpPath(QStringLiteral("peek"));
    QFile::remove(path);

    EcmFileMeta meta;
    meta.groupName = src.name;
    meta.partLabel = QStringLiteral("件0009");
    meta.partId = 9;
    EcmSaveOptions opt;
    QString err;
    QVERIFY2(saveEcm(path, src, meta, opt, err), qPrintable(err));

    EcmFileMeta peeked;
    QVERIFY2(peekEcm(path, peeked, err), qPrintable(err));
    QCOMPARE(peeked.groupName, src.name);
    QCOMPARE(peeked.partLabel, QStringLiteral("件0009"));
    QCOMPARE(peeked.partId, 9);
    QFile::remove(path);
}

void EcmFileTest::rejectsForeignAndCorruptFiles() {
    const QString bad = tmpPath(QStringLiteral("bad"));
    {
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("this is not an ecmb file, just plain text\n");
    }
    DataGroup g;
    EcmFileMeta meta;
    QString err;
    QVERIFY(!loadEcm(bad, g, meta, err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!peekEcm(bad, meta, err));
    QFile::remove(bad);

    // 篡改 payload 的 1 个字节：CRC 必须拦下来
    const DataGroup src = makeGroup();
    const QString path = tmpPath(QStringLiteral("corrupt"));
    QFile::remove(path);
    EcmSaveOptions opt;
    QVERIFY(saveEcm(path, src, EcmFileMeta(), opt, err));

    QByteArray bytes;
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        bytes = f.readAll();
    }
    QVERIFY(bytes.size() > 64);
    bytes[bytes.size() - 8] = char(quint8(bytes[bytes.size() - 8]) ^ 0x5A);
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(bytes);
    }
    err.clear();
    QVERIFY(!loadEcm(path, g, meta, err));
    QFile::remove(path);
}

QTEST_GUILESS_MAIN(EcmFileTest)
#include "tst_ecmfile.moc"
