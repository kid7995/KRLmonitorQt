#include "csvio.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <algorithm>

namespace {

enum ColKind { ColTime, ColPos, ColForce, ColPass, ColEs, ColUnknown };

// 去掉单位后缀（mm / N / µm / % …），并把千分位/小数逗号归一化
QString cleanNumber(QString s) {
    s = s.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    while (!s.isEmpty()) {
        const QChar c = s.at(s.size() - 1);
        if (c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('+')) {
            break;
        }
        s.chop(1);
    }
    s = s.trimmed();
    if (s.contains(QLatin1Char(','))) {
        if (s.contains(QLatin1Char('.'))) {
            s.remove(QLatin1Char(','));               // 1,234.5 → 1234.5
        } else if (s.count(QLatin1Char(',')) == 1) {
            s.replace(QLatin1Char(','), QLatin1Char('.'));  // 1234,5 → 1234.5
        } else {
            s.remove(QLatin1Char(','));               // 1,234,567 → 1234567
        }
    }
    return s;
}

bool toDouble(const QString& raw, double* out) {
    const QString s = cleanNumber(raw);
    if (s.isEmpty()) {
        return false;
    }
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (!ok) {
        return false;
    }
    *out = d;
    return true;
}

bool toInt(const QString& raw, int* out) {
    double d = 0.0;
    if (!toDouble(raw, &d)) {
        return false;
    }
    *out = static_cast<int>(d);     // 与旧实现一致：截断
    return true;
}

// UTF-8 优先；出现替换字符（说明不是 UTF-8）时按本机编码（中文 Windows = GBK）重读
QString decodeBytes(const QByteArray& raw) {
    QString s = QString::fromUtf8(raw);
    if (s.contains(QChar(0xFFFD))) {
        s = QString::fromLocal8Bit(raw);
    }
    if (s.startsWith(QChar(0xFEFF))) {
        s.remove(0, 1);
    }
    return s;
}

// 支持引号包裹（含 "" 转义）的简易 CSV 行拆分
QStringList splitLine(const QString& line, QChar sep) {
    QStringList out;
    QString cur;
    bool inQuote = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('"')) {
            if (inQuote && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                cur += QLatin1Char('"');
                ++i;
            } else {
                inQuote = !inQuote;
            }
        } else if (c == sep && !inQuote) {
            out << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur;
    return out;
}

QChar detectDelimiter(const QString& line) {
    const int c = line.count(QLatin1Char(','));
    const int s = line.count(QLatin1Char(';'));
    const int t = line.count(QLatin1Char('\t'));
    if (s > c && s >= t) {
        return QLatin1Char(';');
    }
    if (t > c && t > s) {
        return QLatin1Char('\t');
    }
    return QLatin1Char(',');
}

QString normalizedHeader(const QString& raw) {
    QString k = raw.trimmed().toLower();
    k.remove(QLatin1Char(' '));
    k.remove(QLatin1Char('\t'));
    k.remove(QLatin1Char('_'));
    k.remove(QLatin1Char('-'));
    return k;
}

ColKind classifyHeader(const QString& raw) {
    const QString k = normalizedHeader(raw);
    static const QStringList timeKeys{QStringLiteral("time"), QStringLiteral("times"), QStringLiteral("t"),
                                      QStringLiteral("sec"), QStringLiteral("seconds"),
                                      QStringLiteral("时间"), QStringLiteral("时刻")};
    static const QStringList posKeys{QStringLiteral("posr"), QStringLiteral("pos"),
                                     QStringLiteral("position"), QStringLiteral("位置")};
    static const QStringList forceKeys{QStringLiteral("forcer"), QStringLiteral("force"),
                                       QStringLiteral("力"), QStringLiteral("力值")};
    static const QStringList passKeys{QStringLiteral("passno"), QStringLiteral("pass"),
                                      QStringLiteral("道次")};
    static const QStringList esKeys{QStringLiteral("es"), QStringLiteral("espos"),
                                    QStringLiteral("setpos"), QStringLiteral("设定位置")};
    if (timeKeys.contains(k)) return ColTime;
    if (posKeys.contains(k)) return ColPos;
    if (forceKeys.contains(k)) return ColForce;
    if (passKeys.contains(k)) return ColPass;
    if (esKeys.contains(k)) return ColEs;
    return ColUnknown;      // 包含 Seam 等可选列
}

}  // namespace

bool loadCsv(const QString& path, CsvData& out, QString& err, QString* warn) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return false;
    }
    const QByteArray raw = f.readAll();
    f.close();
    if (raw.isEmpty()) {
        err = QStringLiteral("空文件");
        return false;
    }

    const QString text = decodeBytes(raw);
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& l : lines) {
        if (l.endsWith(QLatin1Char('\r'))) {
            l.chop(1);
        }
    }

    // 第一条非空、非注释行：用于嗅探分隔符与判断有没有表头
    int firstLine = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const QString t = lines[i].trimmed();
        if (!t.isEmpty() && !t.startsWith(QLatin1Char('#'))) {
            firstLine = i;
            break;
        }
    }
    if (firstLine < 0) {
        err = QStringLiteral("空文件");
        return false;
    }

    const QChar sep = detectDelimiter(lines[firstLine]);
    const QStringList firstCols = splitLine(lines[firstLine], sep);

    int iTime = -1, iPos = -1, iForce = -1, iPass = -1, iEs = -1;
    int startRow = firstLine;
    bool hasHeader = false;
    for (int c = 0; c < firstCols.size(); ++c) {
        const ColKind kind = classifyHeader(firstCols[c]);
        if (kind == ColUnknown) {
            continue;
        }
        hasHeader = true;
        switch (kind) {
        case ColTime:  iTime = c;  break;
        case ColPos:   iPos = c;   break;
        case ColForce: iForce = c; break;
        case ColPass:  iPass = c;  break;
        case ColEs:    iEs = c;    break;
        default: break;
        }
    }

    if (hasHeader) {
        startRow = firstLine + 1;
    } else {
        // 无表头：按列数推断（3 列 = 时间/位置/力；5 列 = 再加道次/设定位置）
        if (firstCols.size() >= 5) {
            iTime = 0; iPos = 1; iForce = 2; iPass = 3; iEs = 4;
        } else if (firstCols.size() >= 3) {
            iTime = 0; iPos = 1; iForce = 2;
        }
        startRow = firstLine;
    }

    if (iTime < 0 || iPos < 0 || iForce < 0) {
        err = QStringLiteral("缺少必要列 time_s/PosR/ForceR");
        return false;
    }

    int skipped = 0;
    int firstSkippedLine = 0;
    for (int ln = startRow; ln < lines.size(); ++ln) {
        const QString line = lines[ln];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const QStringList row = splitLine(line, sep);
        const int need = qMax(iTime, qMax(iPos, iForce));
        if (row.size() <= need) {
            ++skipped;
            if (!firstSkippedLine) firstSkippedLine = ln + 1;
            continue;
        }
        double t = 0.0;
        int p = 0, fo = 0, pn = 0, es = 0;
        if (!toDouble(row[iTime], &t) || !toInt(row[iPos], &p) || !toInt(row[iForce], &fo)) {
            ++skipped;
            if (!firstSkippedLine) firstSkippedLine = ln + 1;
            continue;
        }
        if (iPass >= 0 && iPass < row.size()) {
            toInt(row[iPass], &pn);     // 解析失败按 0
        }
        if (iEs >= 0 && iEs < row.size()) {
            toInt(row[iEs], &es);
        }
        out.times.append(t);
        out.pos.append(p);
        out.force.append(fo);
        out.passNo.append(pn);
        out.esPos.append(es);
    }

    if (out.times.isEmpty()) {
        err = QStringLiteral("未找到有效数据行（检查分隔符/表头/编码）");
        return false;
    }

    // 时间非单调：稳定排序，保证绘图与悬停二分查找不出错
    bool monotonic = true;
    for (int i = 1; i < out.times.size(); ++i) {
        if (out.times[i] < out.times[i - 1]) {
            monotonic = false;
            break;
        }
    }
    if (!monotonic) {
        QVector<int> idx(out.times.size());
        for (int i = 0; i < idx.size(); ++i) {
            idx[i] = i;
        }
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return out.times[a] < out.times[b];
        });
        CsvData sorted;
        for (int i : idx) {
            sorted.times.append(out.times[i]);
            sorted.pos.append(out.pos[i]);
            sorted.force.append(out.force[i]);
            sorted.passNo.append(out.passNo[i]);
            sorted.esPos.append(out.esPos[i]);
        }
        out = sorted;
    }

    if (warn && skipped > 0) {
        *warn = QStringLiteral("已跳过 %1 行无效数据（首个在第 %2 行）").arg(skipped).arg(firstSkippedLine);
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
    ts << "time_s,PosR,ForceR,PassNo,EsPos,Seam\n";
    int seam = 0;
    for (int i = 0; i < g.times.size(); ++i) {
        // Seam 列由 PassNo 的负数标记推导（见 src/station.h），供下游工具/Excel 直接使用；
        // 载入时不依赖该列（旧文件没有也能读）。
        const PassMark m = decodePassNo(g.passNo[i]);
        if (m.seam > 0) {
            seam = m.seam;
        }
        ts << QString::number(g.times[i], 'f', 6) << ','
           << g.pos[i] << ','
           << g.force[i] << ','
           << g.passNo[i] << ','
           << g.esPos[i] << ','
           << seam << '\n';
    }
    return true;
}
