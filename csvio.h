#pragma once

#include <QString>
#include <QVector>

#include "model.h"

// 波形 CSV 读写。列格式：time_s, PosR, ForceR, PassNo, EsPos
// 其中 PassNo / EsPos 是后加的列，旧文件只有前三列，读取时补 0。
struct CsvData {
    QVector<double> times;
    QVector<int> pos;
    QVector<int> force;
    QVector<int> passNo;
    QVector<int> esPos;
};

bool loadCsv(const QString& path, CsvData& out, QString& err);
bool saveCsv(const QString& path, const DataGroup& g, QString& err);
