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
};

// 焊缝跟踪器：实时记录与 CSV 载入共用同一套统计逻辑（唯一实现）
class SeamTracker {
public:
    void reset();

    // 每来一个被记录的采样点调用一次（rt = 组内相对时间，idx = 样点下标）
    void feed(double rt, int passNo, int pos, int force, int es, int idx);

    // 新道次开启（PassNo > 0）时调用：标注所属焊缝、判定补充打磨
    void passOpened(Pass& p);

    SeamStat* find(int seam);
    const SeamStat* find(int seam) const;
    void refreshLive();
    void clearLive();    // 封组/暂停时清掉 live 标记

    QVector<SeamStat> seams;
    int curSeam = 0;                 // 当前焊缝号（来自最近的负标记）
    SeamPhase curPhase = SeamPhase::None;

private:
    SeamStat& ensure(int seam);
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

    SeamTracker seam;        // 焊缝统计（负数 PassNo 编码）

    DecCache decCache;       // 抽稀缓存（数据被裁剪后须 clear）

    int len() const { return times.size(); }
};

// 由整段数据重建焊缝统计（CSV 载入、裁剪之后调用）
void rebuildSeamStats(DataGroup& g);