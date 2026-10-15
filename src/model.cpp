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
    segments.clear();
    curSeam = 0;
    curPhase = SeamPhase::None;
    curCraft = Craft::Unknown;
    m_segKey = PassMark();
    m_segOpen = false;
    m_segT0 = 0.0;
    m_segI0 = -1;
    m_segPassNo = 0;
    m_segRetry = false;
    m_lastPassNo = 0;
    m_lastPassRetry = false;
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

void SeamTracker::feed(double rt, int passNo, int pos, int force, int es, int idx,
                       const DetectVals& dv) {
    const PassMark m = decodePassNo(passNo);
    if (m.phase != SeamPhase::None) {
        curPhase = m.phase;
        if (m.craft != Craft::Unknown) {
            curCraft = m.craft;     // 粗磨/细磨由负标记的百位决定，正数道次沿用最近一次
        }
        if (m.seam > 0) {
            curSeam = m.seam;
        }
    }

    // 段表与焊缝统计无关：任何编码都能切段（旧协议也有段，只是 seam/craft 未知）
    const bool retry = (passNo > 0 && passNo == m_lastPassNo) ? m_lastPassRetry : false;
    feedSegment(m, passNo, retry, rt, idx);

    if (curSeam <= 0) {
        return;             // 旧协议（无负数标记）：不做焊缝统计，只保留道次与段
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
    if (curCraft != Craft::Unknown) {
        s.craft = curCraft;
    }
    if (curPhase == SeamPhase::Detect) {
        s.detected = true;
        s.detectEs = es;    // 检测段的 EsPos = 该焊缝基准高度（KRL 已补 MonRef = ESpos）
        // 检测结果（焊缝高度/基材高度/错边量/计划轮次）在检测段内持续刷新，段末自然定格
        if (dv.has) {
            s.hasValues = true;
            s.weldHum = dv.weldHum;
            s.baseH = dv.baseH;
            s.misalignUm = dv.misalignUm;
            s.rounds = dv.rounds;
        }
    }
    refreshLive();
}

// 段边界：把"当前段"收进 segments
void SeamTracker::closeSegment(double rt, int iEnd) {
    if (!m_segOpen || m_segI0 < 0 || iEnd < m_segI0) {
        m_segOpen = false;
        return;
    }
    Segment seg;
    seg.kind = m_segKey.phase;
    seg.craft = m_segKey.craft;
    seg.seam = m_segKey.seam;
    seg.passNo = m_segPassNo;
    seg.retry = m_segRetry;
    seg.t0 = m_segT0;
    seg.t1 = rt;
    seg.i0 = m_segI0;
    seg.i1 = iEnd;
    segments.append(seg);
    m_segOpen = false;
}

// 按编码（段类型/工序/焊缝/道次）切段：编码一变就收一段、开一段
void SeamTracker::feedSegment(const PassMark& m, int passNo, bool retry, double rt, int idx) {
    PassMark key;
    key.phase = m.phase;                                   // 正数与 0 都是 None（非"负标记段"）
    key.craft = (m.craft != Craft::Unknown) ? m.craft : curCraft;
    key.seam = (m.seam > 0) ? m.seam : curSeam;
    const int segPass = (passNo > 0) ? passNo : 0;

    const bool same = m_segOpen && m_segKey.phase == key.phase && m_segKey.craft == key.craft
                      && m_segKey.seam == key.seam && m_segPassNo == segPass;
    if (same) {
        return;
    }
    closeSegment(rt, idx - 1);
    m_segKey = key;
    m_segPassNo = segPass;
    m_segRetry = (segPass > 0) ? retry : false;
    m_segT0 = rt;
    m_segI0 = idx;
    m_segOpen = true;
}

// 收尾：整组数据结束时调用（离线重建 / 封组），把最后一段闭合
void SeamTracker::finishSegments(double rt, int iEnd) {
    closeSegment(rt, iEnd);
    m_segOpen = false;
    m_segI0 = -1;
}

void SeamTracker::passOpened(Pass& p) {
    p.phase = SeamPhase::Grind;
    m_lastPassNo = p.no;
    m_lastPassRetry = false;
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
    m_lastPassRetry = p.retry;
    refreshLive();
}

void rebuildSeamStats(DataGroup& g) {
    // 检测结果快照是"按焊缝的结果"，重建统计（裁剪 / CSV 载入）时不能丢
    QVector<SeamStat> keep;
    for (const SeamStat& s : g.seam.seams) {
        if (s.hasValues || s.detected || s.detectEs != 0) {
            keep.append(s);
        }
    }

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
    if (n > 0) {
        g.seam.finishSegments(g.times[n - 1], n - 1);
    }

    // 回填检测快照
    for (const SeamStat& k : keep) {
        SeamStat* d = g.seam.find(k.seam);
        if (!d) {
            continue;
        }
        if (k.hasValues) {
            d->hasValues = true;
            d->weldHum = k.weldHum;
            d->baseH = k.baseH;
            d->misalignUm = k.misalignUm;
            d->rounds = k.rounds;
        }
        if (k.detected) {
            d->detected = true;
            if (k.detectEs != 0) {
                d->detectEs = k.detectEs;
            }
        }
        if (k.craft != Craft::Unknown) {
            d->craft = k.craft;
        }
    }
    g.seam.refreshLive();
}
