#pragma once

#include <QVector>
#include <QHash>
#include <QString>
#include <algorithm>

#include "config.h"

// 抽稀结果：一对同下标的 t / y 数组
struct XY {
    QVector<double> t;
    QVector<double> y;
};

// 单个抽稀缓存条目
struct DecEntry {
    int n = -1;
    QVector<double> t;
    QVector<double> y;
};

// 一个数据组（或监视缓冲）的抽稀缓存，键为 "pos"/"force"/"es"
struct DecCache {
    QHash<QString, DecEntry> e;
    void clear() { e.clear(); }
};

// min/max 抽稀：把 N 点压到约 target 点，保留每个区间极大/极小值
XY decimate(const QVector<double>& t, const QVector<double>& y, int target = DISPLAY_MAX_PTS);

// 带缓存的抽稀：全量结果缓存 + 尾部增量拼接。
// 数据被裁剪后调用方必须 clear() 缓存（下标全变了）。
template <typename YVec>
XY cachedXY(DecCache& cache, const QString& key,
            const QVector<double>& t, const YVec& y) {
    int n = t.size();
    DecEntry& c = cache.e[key];
    // 缓存失效：首次、数据被裁剪（n 变小）、或新增点数超过快路径上限
    if (c.n < 0 || n < c.n || n - c.n > DEC_APPEND_MAX) {
        QVector<double> yd(y.size());
        for (int i = 0; i < y.size(); ++i) {
            yd[i] = double(y[i]);
        }
        XY r = decimate(t, yd);
        c.n = n;
        c.t = r.t;
        c.y = r.y;
        return r;
    }
    if (n == c.n) {
        return XY{c.t, c.y};
    }
    // 尾部增量拼接（不更新缓存，等价于 Python 的 concatenate 快路径）
    int tail = n - c.n;
    QVector<double> rt = c.t;
    QVector<double> ry = c.y;
    rt.reserve(rt.size() + tail);
    ry.reserve(ry.size() + tail);
    for (int i = c.n; i < n; ++i) {
        rt.append(t[i]);
        ry.append(double(y[i]));
    }
    return XY{rt, ry};
}
