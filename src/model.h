#pragma once

#include <QString>
#include <QVector>

#include "decimate.h"
#include "station.h"

// 道次区间
struct Pass {
    int no = 0;
    int seam = 0;                    // 所属焊缝（1..8；0 = 未知/旧协议）
    SeamPhase phase = SeamPhase::None;
    bool retry = false;              // 补充打磨道次
    double t0 = 0.0;
    double t1 = 0.0;
    int i0 = 0;
    int i1 = 0;
};

// 一帧携带的检测结果（KRL 侧 ECM/WeldH|BaseH|Misalign|Rounds）。
// 这些值只在检测段被写入，上位机在检测段内持续刷新、段末自然定格。
struct DetectVals {
    bool has = false;        // 本帧是否带检测字段（旧程序 / 旧 EKI 配置为 false）
    int weldHum = 0;         // 焊缝高度，µm（Measure_Height × 1000）
    int baseH = 0;           // 基材高度（浮动模块到位值 = Baseheight）
    int misalignUm = 0;      // 错边量，µm，可负（Cuobian × 1000）
    int rounds = 0;          // 计划打磨轮次（TIMES）
};

// 段：同一 PassNo 编码连续出现的一段。比 Pass 多了"工序"与"段类型"，
// 供"按焊缝 / 按阶段 / 按工序"拆分显示使用（需求 2、4）。
// 读法：
//   kind == Grind                        → 打磨道次（passNo > 0；retry 表示补充打磨）
//   kind == Detect/GrindGap/Transfer/Finish → KRL 打的负标记段（带工序 craft 与焊缝号 seam）
//   kind == None                         → 未标记段（等待 / 返回 / 空移；旧协议也全落这里）
// 段与段覆盖全部样点且首尾相接，因此可以按"选中哪些段"重建要绘制的点集。
struct Segment {
    SeamPhase kind = SeamPhase::None;   // Detect / Grind(道次) / GrindGap / Transfer / Finish
    Craft craft = Craft::Unknown;       // 粗磨 / 细磨
    int seam = 0;                       // 1..8；0 = 未归属
    int passNo = 0;                     // kind == Grind 时为道次号，其余为 0
    bool retry = false;                 // 补充打磨道次
    double t0 = 0.0;
    double t1 = 0.0;
    int i0 = 0;
    int i1 = -1;
};

// 从逐点 PassNo 数组切出道次区间列表（PassNo <= 0 的段落不建区间，跳过）
QVector<Pass> computePasses(const QVector<double>& times, const QVector<int>& passNo);

// 单个焊缝（旧称"工位"）统计
struct SeamStat {
    int seam = 0;
    bool detected = false;      // 是否收到过检测（测高）段
    int detectEs = 0;           // 检测段最后收到的 EsPos（= 该焊缝基准高度）
    double t0 = 0.0;            // 该焊缝首次出现（组内相对时间，秒）
    double t1 = 0.0;            // 最近一次出现
    int i0 = 0;
    int i1 = -1;
    int pts = 0;                // 样点数
    int normalN = 0;            // 正常道次数
    int retryN = 0;             // 补充打磨次数
    int curPass = 0;            // 当前/最后一个道次编号（0 = 未开始）
    bool retryNow = false;      // 最后一个道次是否补充打磨
    bool live = false;          // 是否当前正在进行的焊缝
    int lastPos = 0;
    int lastForce = 0;
    int lastEs = 0;

    // ── 检测结果快照（ECM/WeldH|BaseH|Misalign|Rounds）──
    // hasValues = false 表示程序/EKI 未上报（旧协议），界面显示"—"。
    bool hasValues = false;
    int weldHum = 0;
    int baseH = 0;
    int misalignUm = 0;
    int rounds = 0;
    Craft craft = Craft::Unknown;   // 该焊缝最后出现的工序
};

// 焊缝跟踪器：实时记录与 CSV 载入共用同一套统计逻辑（唯一实现）
class SeamTracker {
public:
    void reset();

    // 每来一个被记录的采样点调用一次（rt = 组内相对时间，idx = 样点下标）
    void feed(double rt, int passNo, int pos, int force, int es, int idx,
              const DetectVals& dv = DetectVals());

    // 新道次开启（PassNo > 0）时调用：标注所属焊缝、判定补充打磨
    void passOpened(Pass& p);

    // 收尾：整组数据结束时调用，把最后一段闭合（离线重建 / 封组）
    void finishSegments(double rt, int iEnd);

    SeamStat* find(int seam);
    const SeamStat* find(int seam) const;
    void refreshLive();
    void clearLive();    // 封组/暂停时清掉 live 标记

    QVector<SeamStat> seams;
    QVector<Segment> segments;       // 段表（唯一来源：feed() 增量维护）
    int curSeam = 0;                 // 当前焊缝号（来自最近的负标记）
    SeamPhase curPhase = SeamPhase::None;
    Craft curCraft = Craft::Unknown; // 当前工序（来自最近的负标记）

private:
    SeamStat& ensure(int seam);
    // 段边界：编码（kind/craft/seam/passNo）变化时收上一段、开新一段
    void closeSegment(double rt, int iEnd);
    void feedSegment(const PassMark& m, int passNo, bool retry, double rt, int idx);

    PassMark m_segKey;               // 当前段的编码
    bool m_segOpen = false;
    double m_segT0 = 0.0;
    int m_segI0 = -1;
    int m_segPassNo = 0;
    bool m_segRetry = false;
    // passOpened() 刚开的那一道（feed() 用它给 Segment 打"补充打磨"标记）
    int m_lastPassNo = 0;
    bool m_lastPassRetry = false;
};

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

    bool hasEs = false;      // 是否含非零 ESpos（旧格式 CSV 补 0 -> false）
    bool showPos = true;
    bool showForce = true;
    int paletteIdx = 0;
    bool live = false;
    int curPass = 0;         // 记录中：当前未闭合道次编号（0 = 无）

    // ── 存档与连续生产（需求 5 / 整体愿景）──
    bool incomplete = false; // 断线或异常时强制封存：数据可能不完整，界面要标出来
    bool saved = false;      // 是否已经落盘（内存回收只回收已落盘的组）
    QString savedPath;       // 落盘路径（供"打开所在目录"用）
    QString partLabel;       // 所属件号标签（如 "件0007"）

    SeamTracker seam;        // 焊缝统计（负数 PassNo 编码）

    DecCache decCache;       // 抽稀缓存（数据被裁剪后须 clear）

    int len() const { return times.size(); }
};

// 由整段数据重建焊缝统计（CSV 载入、裁剪之后调用）
void rebuildSeamStats(DataGroup& g);
