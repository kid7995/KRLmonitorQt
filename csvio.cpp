#include "csvio.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>

static int safeInt(const QString& s) {
    QString t = s.trimmed();
    if (t.isEmpty()) {
        return 0;
    }
    bool ok = false;
    double d = t.toDouble(&ok);
    return ok ? (int)d : 0;
}

bool loadCsv(const QString& path, CsvData& out, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err = QStringLiteral("无法打开文件");
        return false;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");

    QString header = ts.readLine();
    if (header.isNull()) {
        err = QStringLiteral("空文件");
        return false;
    }
    // 去除可能的 UTF-8 BOM
    if (header.startsWith(QChar(0xFEFF))) {
        header.remove(0, 1);
    }

    QStringList cols = header.split(',');
    int iTime = -1, iPos = -1, iForce = -1, iPass = -1, iEs = -1;
    for (int i = 0; i < cols.size(); ++i) {
        QString c = cols[i].trimmed();
        if (c == QLatin1String("time_s"))       iTime = i;
        else if (c == QLatin1String("PosR"))    iPos = i;
        else if (c == QLatin1String("ForceR"))  iForce = i;
        else if (c == QLatin1String("PassNo"))  iPass = i;
        else if (c == QLatin1String("EsPos"))   iEs = i;
    }
    if (iTime < 0 || iPos < 0 || iForce < 0) {
        err = QStringLiteral("缺少必要列 time_s/PosR/ForceR");
        return false;
    }

    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }
        QStringList row = line.split(',');
        int need = iTime;
        need = qMax(need, iPos);
        need = qMax(need, iForce);
        if (row.size() <= need) {
            continue;
        }
        out.times.append(row[iTime].trimmed().toDouble());
        out.pos.append(safeInt(row[iPos]));
        out.force.append(safeInt(row[iForce]));
        out.passNo.append(iPass >= 0 && iPass < row.size() ? safeInt(row[iPass]) : 0);
        out.esPos.append(iEs >= 0 && iEs < row.size() ? safeInt(row[iEs]) : 0);
    }
    return true;
}

bool saveCsv(const QString& path, const DataGroup& g, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        err = QStringLiteral("无法写入文件");
        return false;
    }
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    ts << "time_s,PosR,ForceR,PassNo,EsPos\n";
    for (int i = 0; i < g.times.size(); ++i) {
        ts << QString::number(g.times[i], 'f', 6) << ','
           << g.pos[i] << ','
           << g.force[i] << ','
           << g.passNo[i] << ','
           << g.esPos[i] << '\n';
    }
    return true;
}
