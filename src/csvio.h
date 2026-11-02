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

// 读取 CSV。warn 非空时，成功但存在"跳过行/字段缺失"等非致命情况会写入提示。
bool loadCsv(const QString& path, CsvData& out, QString& err, QString* warn = nullptr);
bool saveCsv(const QString& path, const DataGroup& g, QString& err);
