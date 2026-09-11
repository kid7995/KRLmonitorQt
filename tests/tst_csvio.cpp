#include <QtTest/QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "csvio.h"

namespace {
bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return f.write(bytes) == bytes.size();
}
}

class CsvIoTest : public QObject {
    Q_OBJECT

private slots:
    void roundTripPreservesColumns();
    void loadsLegacyThreeColumnFormat();
    void rejectsMissingRequiredColumns();
};

void CsvIoTest::roundTripPreservesColumns() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("wave.csv"));

    DataGroup source;
    source.times = QVector<double>{0.0, 0.125, 0.25};
    source.pos = QVector<int>{10, -20, 30};
    source.force = QVector<int>{-1, -32768, 32767};
    source.passNo = QVector<int>{0, 2, 2};
    source.esPos = QVector<int>{0, 100, 101};

    QString err;
    QVERIFY2(saveCsv(path, source, err), qPrintable(err));

    CsvData loaded;
    QVERIFY2(loadCsv(path, loaded, err), qPrintable(err));
    QCOMPARE(loaded.times, source.times);
    QCOMPARE(loaded.pos, source.pos);
    QCOMPARE(loaded.force, source.force);
    QCOMPARE(loaded.passNo, source.passNo);
    QCOMPARE(loaded.esPos, source.esPos);
}

void CsvIoTest::loadsLegacyThreeColumnFormat() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("legacy.csv"));
    QVERIFY(writeBytes(path,
        "time_s,PosR,ForceR\n"
        "0,10,-20\n"
        "0.5,11,-21\n"));

    CsvData loaded;
    QString err;
    QVERIFY2(loadCsv(path, loaded, err), qPrintable(err));
    QCOMPARE(loaded.times, QVector<double>({0.0, 0.5}));
    QCOMPARE(loaded.pos, QVector<int>({10, 11}));
    QCOMPARE(loaded.force, QVector<int>({-20, -21}));
    QCOMPARE(loaded.passNo, QVector<int>({0, 0}));
    QCOMPARE(loaded.esPos, QVector<int>({0, 0}));
}

void CsvIoTest::rejectsMissingRequiredColumns() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("invalid.csv"));
    QVERIFY(writeBytes(path, "time_s,PosR\n0,10\n"));

    CsvData loaded;
    QString err;
    QVERIFY(!loadCsv(path, loaded, err));
    QVERIFY(!err.isEmpty());
}

QTEST_GUILESS_MAIN(CsvIoTest)
#include "tst_csvio.moc"