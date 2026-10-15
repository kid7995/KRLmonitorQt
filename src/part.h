#pragma once

#include <QDateTime>
#include <QString>

#include "model.h"   // Craft

// ── "件"（料件）划分 ──────────────────────────────────────────────
// 协议里没有件号、也没有程序号，所以"换料件了"只能靠行为推断。
// 这里把可选判据集中成一台小状态机，规则可切换（设置界面），并且任何规则下
// 都保留"手工切分"与"超时切分"两个兜底手段。
//
//   PerCycle      一次 RecordOn 周期 = 一件。最确定，不依赖任何推断。
//                 当前现场是"一件 = 一次程序运行（4 条焊缝）"，这就是正确语义。
//   CraftRestart  工序由"细磨"回到"粗磨"时视为新件。
//                 适用于"一件 = 粗磨 1-8 + 细磨 1-8"，且全靠上位机判定。
//   SeamWrap      同一工序内焊缝号回绕（8 之后又出现 1）视为新件。
//                 适用于"一个工序内就把一件的所有焊缝做完"，且工序之间会切换。
//
// 注意：后两条都是启发式，现场节奏一变就可能误判，因此界面上必须能手工切分
//       （见 MainWindow 的"新件"按钮）。
enum class PartRule {
    PerCycle,
    CraftRestart,
    SeamWrap
};

struct PartInfo {
    int id = 0;                       // 件号
    QString label;                    // "件0007"
    QDateTime t0;
    QDateTime t1;
    int cycles = 0;                   // 属于本件的 RecordOn 周期数
};

class PartSplitter {
public:
    void configure(PartRule rule, int timeoutMin);
    void reset(const QDateTime& now);
    void setStartSeq(int seq);        // 跨重启续号（从当日存档数量推）

    PartRule rule() const { return m_rule; }
    int timeoutMin() const { return m_timeoutMin; }
    const PartInfo& current() const { return m_cur; }
    bool started() const { return m_started; }

    // RecordOn 上升沿：按规则决定是否开新件
    bool onCycleStart(const QDateTime& now);
    // 周期内见到某焊缝的检测段：CraftRestart / SeamWrap 在此判定
    bool onSeamSeen(int seam, Craft craft, const QDateTime& now);
    // 封组
    void onCycleEnd(const QDateTime& now);
    // 长时间无数据（兜底）
    bool onIdle(const QDateTime& now);
    // 手工切分
    bool startNewPart(const QDateTime& now);

private:
    void beginPart(const QDateTime& now);

    PartRule m_rule = PartRule::PerCycle;
    int m_timeoutMin = 30;
    PartInfo m_cur;
    bool m_started = false;
    int m_seq = 0;
    int m_maxSeam = 0;                // 本件（同工序）内见过的最大焊缝号
    Craft m_craft = Craft::Unknown;
    QDateTime m_lastData;
};

// 按模板生成存档文件名（不含目录与扩展名）。
// 占位符：{time} 到分钟、{date}、{part}、{craft}、{seams}
// 默认模板："{time}_{part}_{craft}_{seams}"
QString buildArchiveName(const QString& tmpl, const QDateTime& t,
                         const PartInfo& part, Craft craft,
                         int firstSeam, int lastSeam);

// 现场常见非法字符清理 + 长度限制
QString sanitizeFileName(const QString& in);
