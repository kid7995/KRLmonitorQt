#pragma once

#include <QString>
#include <QVector>

#include "decimate.h"

// 道次区间
struct Pass {
    int no = 0;
    double t0 = 0.0;
    double t1 = 0.0;
    int i0 = 0;
    int i1 = 0;
};

// 从逐点 PassNo 数组切出道次区间列表（PassNo == 0 的道次间返回段不建区间，跳过）
QVector<Pass> computePasses(const QVector<double>& times, const QVector<int>& passNo);

// 一组位移/力控波形（纯数据容器，无 GUI 依赖）
class DataGroup {
public:
    QString name;
    QVector<double> times;   // 相对时间（秒）
    QVector<int> pos;
    QVector<int> force;
    QVector<int> passNo;
    QVector<int> esPos;
    QVector<Pass> passes;

    bool hasEs = false;      // 是否含非零 ESpos（旧格式 CSV 补 0 → false）
    bool showPos = true;
    bool showForce = true;
    int paletteIdx = 0;
    bool live = false;
    int curPass = 0;         // 记录中：当前未闭合道次编号（0 = 无）

    DecCache decCache;       // 抽稀缓存（数据被裁剪后须 clear）

    int len() const { return times.size(); }
};
