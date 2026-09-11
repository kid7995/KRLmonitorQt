#include "decimate.h"

XY decimate(const QVector<double>& t, const QVector<double>& y, int target) {
    int n = t.size();
    if (n <= target) {
        return XY{t, y};
    }

    int nb = std::max(1, target / 2);  // 每个桶产出 2 个点
    int per = n / nb;
    int m = per * nb;                  // 能整除的部分

    QVector<int> idx;
    idx.reserve(nb * 2 + (n - m));
    for (int b = 0; b < nb; ++b) {
        int base = b * per;
        double vmin = y[base];
        double vmax = y[base];
        int imin = base;
        int imax = base;
        for (int k = 1; k < per; ++k) {
            double v = y[base + k];
            if (v < vmin) { vmin = v; imin = base + k; }
            if (v > vmax) { vmax = v; imax = base + k; }
        }
        // 每桶按时间顺序输出 min/max 两点（与 numpy argmin/argmax 后排序一致）
        if (imin <= imax) { idx.append(imin); idx.append(imax); }
        else              { idx.append(imax); idx.append(imin); }
    }
    // 尾部零头原样保留
    for (int k = m; k < n; ++k) {
        idx.append(k);
    }

    QVector<double> rt(idx.size());
    QVector<double> ry(idx.size());
    for (int i = 0; i < idx.size(); ++i) {
        rt[i] = t[idx[i]];
        ry[i] = y[idx[i]];
    }
    return XY{rt, ry};
}
