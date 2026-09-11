#include "model.h"

QVector<Pass> computePasses(const QVector<double>& times, const QVector<int>& passNo) {
    QVector<Pass> passes;
    int cur = 0;
    int start = 0;
    int n = passNo.size();

    for (int i = 0; i < n; ++i) {
        int p = passNo[i];
        if (p != cur) {
            if (cur > 0) {
                Pass seg;
                seg.no = cur;
                seg.t0 = times[start];
                seg.t1 = times[i - 1];
                seg.i0 = start;
                seg.i1 = i - 1;
                passes.append(seg);
            }
            cur = p;
            start = i;
        }
    }
    if (cur > 0 && n > 0) {
        Pass seg;
        seg.no = cur;
        seg.t0 = times[start];
        seg.t1 = times[n - 1];
        seg.i0 = start;
        seg.i1 = n - 1;
        passes.append(seg);
    }
    return passes;
}
