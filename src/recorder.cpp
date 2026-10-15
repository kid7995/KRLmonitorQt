#include "recorder.h"

Recorder::Recorder(QVector<DataGroup*>* groups, std::function<int(const QString&)> createGroup,
                   int maxPoints, int trimChunk, int maxGroups)
    : m_groups(groups), m_createGroup(createGroup),
      m_maxPoints(maxPoints), m_trimChunk(trimChunk), m_maxGroups(maxGroups) {}

void Recorder::clearMonitor() {
    monTimes.clear();
    monPos.clear();
    monForce.clear();
    monDec.clear();
    monStartValid = false;
    monStartTime = 0.0;
}

void Recorder::arm() {
    m_prevRecordOn.reset();
    recGroupIdx = -1;
    m_recStartTime = 0.0;
    monSeam = 0;
    monPhase = SeamPhase::None;
    monCraft = Craft::Unknown;
}

void Recorder::reset() {
    arm();
    m_recordSeq = 0;
    clearMonitor();
}

void Recorder::markPrevUnknown() {
    m_prevRecordOn.reset();
}

bool Recorder::forceSeal(TickResult& r) {
    if (recGroupIdx < 0 || recGroupIdx >= m_groups->size()) {
        m_prevRecordOn = false;
        return false;
    }
    DataGroup* g = (*m_groups)[recGroupIdx];
    onFallingEdge(r);
    if (g) {
        g->incomplete = true;      // 断线/异常封存：数据可能不完整，界面必须标出来
    }
    m_prevRecordOn = false;        // 重连后的第一个上升沿一定新建组
    return r.sealed;
}

bool Recorder::noteGroupRemoved(int idx) {
    if (recGroupIdx == idx) {
        recGroupIdx = -1;
        m_prevRecordOn.reset();  // 不产生假边沿
        return true;
    }
    if (recGroupIdx >= 0 && recGroupIdx > idx) {
        recGroupIdx--;
    }
    return false;
}

TickResult Recorder::process(QQueue<Sample>& queue, bool autoPause) {
    TickResult r;
    while (!queue.isEmpty()) {
        Sample s = queue.dequeue();
        double ts = s.ts;
        int pos = s.pos;
        int force = s.force;
        bool ro = s.recordOn;
        int pn = s.passNo;
        int es = s.esPos;

        // 首帧或恢复后 prevRecordOn 未知：直接采纳当前值，不产生假边沿
        if (!m_prevRecordOn.has_value()) {
            m_prevRecordOn = ro;
        }

        if (ro && !m_prevRecordOn.value()) {
            onRisingEdge(ts, r);
        }

        if (ro && recGroupIdx >= 0) {
            append(s, r);
        } else if (recGroupIdx < 0) {
            // 灰色预览：只跟踪阶段/焊缝，供状态栏显示，不做统计
            const PassMark mk = decodePassNo(pn);
            if (mk.phase != SeamPhase::None) {
                monPhase = mk.phase;
                if (mk.seam > 0) {
                    monSeam = mk.seam;
                }
            }
            appendMonitor(ts, pos, force, r);
            r.hasLastVal = true;
            r.lastPos = pos;
            r.lastForce = force;
            r.lastEs = es;
            r.lastPass = pn;
            r.lastSeam = monSeam;
            r.lastPhase = monPhase;
            const Craft mc = decodePassNo(pn).craft;
            if (mc != Craft::Unknown) {
                monCraft = mc;      // 与焊缝号同理：只在负标记帧刷新
            }
            r.lastCraft = monCraft;
        }

        if (!ro && m_prevRecordOn.value()) {
            onFallingEdge(r);
        }

        m_prevRecordOn = ro;

        if (r.sealed && autoPause) {
            break;  // 冻结：本帧不再处理后续采样
        }
    }
    return r;
}

void Recorder::onRisingEdge(double ts, TickResult& r) {
    if (m_groups->size() >= m_maxGroups) {
        r.limitHit = true;
        recGroupIdx = -1;
        return;
    }
    clearMonitor();
    r.monDirty = true;
    m_recordSeq++;
    recGroupIdx = m_createGroup(QStringLiteral("打磨#%1").arg(m_recordSeq));
    m_recStartTime = ts;
    r.structDirty = true;
    r.seamDirty = true;
    r.started = m_recordSeq;
}

void Recorder::onFallingEdge(TickResult& r) {
    if (recGroupIdx < 0) {
        return;
    }
    DataGroup* g = (*m_groups)[recGroupIdx];
    g->live = false;
    // 收尾未闭合的道次（正常流程下 PassNo 已经在段末被置为非正数，道次已闭合）
    if (g->curPass > 0 && !g->passes.isEmpty() && g->passes.last().no == g->curPass) {
        g->passes.last().t1 = g->times.last();
        g->passes.last().i1 = g->times.size() - 1;
    }
    // 段表收尾：把最后一段（通常是收尾的非打磨段）闭合，保证段表完整
    if (!g->times.isEmpty()) {
        g->seam.finishSegments(g->times.last(), g->times.size() - 1);
    }
    g->curPass = 0;
    g->seam.clearLive();
    r.sealed = true;
    r.sealedGroup = g;
    r.sealedName = g->name;
    r.sealedPasses = g->passes.size();
    recGroupIdx = -1;
    r.structDirty = true;
    r.passDirty = true;
    r.seamDirty = true;
}

void Recorder::append(const Sample& s, TickResult& r) {
    DataGroup* g = (*m_groups)[recGroupIdx];
    const double ts = s.ts;
    const int pos = s.pos;
    const int force = s.force;
    const int pn = s.passNo;
    const int es = s.esPos;
    const double rt = ts - m_recStartTime;
    g->times.append(rt);
    g->pos.append(pos);
    g->force.append(force);
    g->passNo.append(pn);
    g->esPos.append(es);

    // 实时组建组时数组是空的，hasEs 只能在数据流入后判定
    if (es && !g->hasEs) {
        g->hasEs = true;
        r.esEnabled.append(g);
        r.structDirty = true;
    }

    updatePasses(g, rt, pn, r);

    DetectVals dv;
    dv.has = s.hasDetect;
    dv.weldHum = s.weldHum;
    dv.baseH = s.baseH;
    dv.misalignUm = s.misalignUm;
    dv.rounds = s.rounds;
    g->seam.feed(rt, pn, pos, force, es, g->times.size() - 1, dv);

    trimIfNeeded(g, r);

    r.touched.insert(recGroupIdx);
    r.hasLastVal = true;
    r.lastPos = pos;
    r.lastForce = force;
    r.lastEs = es;
    r.lastPass = pn;
    r.lastSeam = g->seam.curSeam;
    r.lastPhase = g->seam.curPhase;
    r.lastCraft = g->seam.curCraft;
}

void Recorder::updatePasses(DataGroup* g, double rt, int pn, TickResult& r) {
    const int cur = g->curPass;
    if (pn != cur) {
        if (cur > 0) {  // 闭合上一道（当前帧 PassNo 已变为非打磨段或下一道）
            const int last = g->times.size() - 2;
            if (!g->passes.isEmpty() && last >= g->passes.last().i0) {
                g->passes.last().t1 = g->times[last];
                g->passes.last().i1 = last;
            }
        }
        if (pn > 0) {  // 仅正数 PassNo 开启新一道；0 与负数都是非打磨段
            Pass p;
            p.no = pn;
            p.seam = g->seam.curSeam;
            p.t0 = rt;
            p.t1 = rt;
            p.i0 = g->times.size() - 1;
            p.i1 = g->times.size() - 1;
            g->passes.append(p);
            g->seam.passOpened(g->passes.last());
            r.seamDirty = true;
        }
        g->curPass = pn > 0 ? pn : 0;
        r.passDirty = true;
    } else if (pn > 0 && !g->passes.isEmpty()) {  // 道次内：持续延长右边界
        g->passes.last().t1 = rt;
        g->passes.last().i1 = g->times.size() - 1;
    }
}

void Recorder::trimIfNeeded(DataGroup* g, TickResult& r) {
    if (g->times.size() <= m_maxPoints + m_trimChunk) {
        return;
    }
    g->times.remove(0, m_trimChunk);
    g->pos.remove(0, m_trimChunk);
    g->force.remove(0, m_trimChunk);
    g->passNo.remove(0, m_trimChunk);
    g->esPos.remove(0, m_trimChunk);
    g->passes = computePasses(g->times, g->passNo);
    const int lastPn = g->passNo.isEmpty() ? 0 : g->passNo.last();
    g->curPass = lastPn > 0 ? lastPn : 0;
    g->decCache.clear();      // 下标全变了，抽稀缓存失效
    rebuildSeamStats(*g);     // 焊缝统计同样要从裁剪后的数据重建
    r.passDirty = true;
    r.seamDirty = true;
}

void Recorder::appendMonitor(double ts, int pos, int force, TickResult& r) {
    if (!monStartValid) {
        monStartTime = ts;
        monStartValid = true;
    }
    monTimes.append(ts - monStartTime);
    monPos.append(pos);
    monForce.append(force);
    if (monTimes.size() > m_maxPoints + m_trimChunk) {
        monTimes.remove(0, m_trimChunk);
        monPos.remove(0, m_trimChunk);
        monForce.remove(0, m_trimChunk);
        monDec.clear();
    }
    r.monDirty = true;
}
