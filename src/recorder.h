#pragma once

#include <QVector>
#include <QSet>
#include <QQueue>
#include <QString>
#include <optional>
#include <functional>

#include "model.h"
#include "protocol.h"
#include "config.h"

// 一帧处理的结果，供调用方决定刷新哪些界面元素
struct TickResult {
    QSet<int> touched;           // 本帧被写入数据的组索引
    bool hasLastVal = false;
    int lastPos = 0;
    int lastForce = 0;
    int lastEs = 0;
    int lastPass = 0;
    int lastSeam = 0;            // 当前焊缝号（0 = 未知 / 旧协议）
    SeamPhase lastPhase = SeamPhase::None;
    bool structDirty = false;    // 建组/封存，需刷新面板
    bool monDirty = false;       // 灰色预览有新数据
    bool passDirty = false;      // 道次区间有变化，需重画分割线
    bool seamDirty = false;      // 焊缝归属/次数有变化，需刷新工位面板
    bool sealed = false;         // 本帧封存了一组
    DataGroup* sealedGroup = nullptr;
    QString sealedName;
    int sealedPasses = 0;
    int started = -1;            // 本帧新建记录的序号（-1 = 无）
    bool limitHit = false;       // 触发时已达组数上限
    QVector<DataGroup*> esEnabled;  // 本帧首次收到非零 ESpos 的组
};

// RecordOn 记录状态机（纯数据操作，不碰任何 GUI）。
class Recorder {
public:
    Recorder(QVector<DataGroup*>* groups, std::function<int(const QString&)> createGroup,
             int maxPoints = MAX_POINTS, int trimChunk = TRIM_CHUNK,
             int maxGroups = MAX_GROUPS);

    // 排空队列并推进状态机，返回 TickResult
    TickResult process(QQueue<Sample>& queue, bool autoPause);

    void clearMonitor();
    void arm();          // 进入监听：不产生假上升沿
    void reset();
    void markPrevUnknown();  // 仅把 prevRecordOn 置未知（暂停后恢复用）
    bool noteGroupRemoved(int idx);  // 组被删除后修正记录索引

    // 当前正在写入的组索引（-1 = 无）
    int recGroupIdx = -1;

    // 灰色预览（未记录）时段的当前阶段 / 焊缝号，供状态栏显示
    int monSeam = 0;
    SeamPhase monPhase = SeamPhase::None;

    // 常驻灰色预览缓冲（ChartView 直接读取）
    QVector<double> monTimes;
    QVector<int> monPos;
    QVector<int> monForce;
    DecCache monDec;
    bool monStartValid = false;
    double monStartTime = 0.0;

private:
    void onRisingEdge(double ts, TickResult& r);
    void onFallingEdge(TickResult& r);
    void append(double ts, int pos, int force, int pn, int es, TickResult& r);
    void updatePasses(DataGroup* g, double rt, int pn, TickResult& r);
    void trimIfNeeded(DataGroup* g, TickResult& r);
    void appendMonitor(double ts, int pos, int force, TickResult& r);

    QVector<DataGroup*>* m_groups;
    std::function<int(const QString&)> m_createGroup;
    int m_maxPoints;
    int m_trimChunk;
    int m_maxGroups;
    double m_recStartTime = 0.0;
    std::optional<bool> m_prevRecordOn;  // 无值 = 未知，不产生边沿
    int m_recordSeq = 0;
};