#include "model.h"

#include "config.h"

QVector<Pass> computePasses(const QVector<double>& times, const QVector<int>& passNo) {
    QVector<Pass> passes;
    int cur = 0;            // 只有正数 PassNo 才是打磨道次；0 与负数都是"非打磨段"
    int start = 0;
    const int n = passNo.size();

    for (int i = 0; i < n; ++i) {
        const int p = passNo[i];
        if (p != cur) {
            if (cur > 0) {
                Pass seg;
                seg.no = cur;
                seg.phase = SeamPhase::Grind;
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
        seg.phase = SeamPhase::Grind;
        seg.t0 = times[start];
        seg.t1 = times[n - 1];
        seg.i0 = start;
        seg.i1 = n - 1;
        passes.append(seg);
    }
    return passes;
}

// ── SeamTracker：焊缝（工位）统计 ────────────────────────────────

void SeamTracker::reset() {
    seams.clear();
    curSeam = 0;
    curPhase = SeamPhase::None;
}

SeamStat& SeamTracker::ensure(int seam) {
    for (int i = 0; i < seams.size(); ++i) {
        if (seams[i].seam == seam) {
            return seams[i];
        }
    }
    SeamStat s;
    s.seam = seam;
    seams.append(s);
    return seams.last();
}

SeamStat* SeamTracker::find(int seam) {
    for (int i = 0; i < seams.size(); ++i) {
        if (seams[i].seam == seam) {
            return &seams[i];
        }
    }
    return nullptr;
}

const SeamStat* SeamTracker::find(int seam) const {
    for (int i = 0; i < seams.size(); ++i) {
        if (seams[i].seam == seam) {
            return &seams[i];
        }
    }
    return nullptr;
}

void SeamTracker::refreshLive() {
    for (SeamStat& s : seams) {
        s.live = (curSeam > 0 && s.seam == curSeam);
    }
}

void SeamTracker::clearLive() {
    for (SeamStat& s : seams) {
        s.live = false;
    }
}

void SeamTracker::feed(double rt, int passNo, int pos, int force, int es, int idx) {
    const PassMark m = decodePassNo(passNo);
    if (m.phase != SeamPhase::None) {
        curPhase = m.phase;
        if (m.seam > 0) {
            curSeam = m.seam;
        }
    }
    if (curSeam <= 0) {
        return;             // 旧协议（无负数标记）：不做焊缝统计，只保留道次
    }

    SeamStat& s = ensure(curSeam);
    if (s.pts == 0) {
        s.t0 = rt;
        s.t1 = rt;
        s.i0 = idx;
    }
    ++s.pts;
    s.i1 = idx;
    if (rt > s.t1) {
        s.t1 = rt;
    }
    s.lastPos = pos;
    s.lastForce = force;
    s.lastEs = es;
    if (curPhase == SeamPhase::Detect) {
        s.detected = true;
        s.detectEs = es;    // 检测段的 EsPos = 该焊缝基准高度（KRL 已补 MonRef = ESpos）
    }
    refreshLive();
}

void SeamTracker::passOpened(Pass& p) {
    p.phase = SeamPhase::Grind;
    if (p.seam <= 0) {
        p.seam = curSeam;
    }
    if (p.seam <= 0) {
        return;             // 旧协议：只留道次号
    }
    SeamStat& s = ensure(p.seam);
    // 正常道次：编号从 1 连续递增；补充打磨：编号跳增（KRL 用 TIMES+1+Retry+10 偏移），
    // 且一旦进入补充打磨，后续编号虽连续也仍属补充打磨（保持到本焊缝结束）。
    const bool consecutive = (p.no == s.curPass + 1);
    if (!s.retryNow && consecutive) {
        ++s.normalN;
    } else {
        p.retry = true;
        ++s.retryN;
        s.retryNow = true;
    }
    s.curPass = p.no;
    refreshLive();
}

void rebuildSeamStats(DataGroup& g) {
    g.seam.reset();
    const int n = g.times.size();
    int pi = 0;
    for (int i = 0; i < n; ++i) {
        while (pi < g.passes.size() && g.passes[pi].i0 < i) {
            ++pi;
        }
        if (pi < g.passes.size() && g.passes[pi].i0 == i) {
            Pass& p = g.passes[pi];
            p.seam = g.seam.curSeam;
            g.seam.passOpened(p);
        }
        g.seam.feed(g.times[i], g.passNo[i], g.pos[i], g.force[i], g.esPos[i], i);
    }
    g.seam.refreshLive();
}