#pragma once

#include <QString>

// 一帧采样（含 PC 接收时刻 ts）
struct Sample {
    double ts = 0.0;
    int pos = 0;
    int force = 0;
    bool recordOn = false;
    int passNo = 0;
    int esPos = 0;

    // ── 检测结果（KRL 侧 2026-10-14 起新增的 ECM/WeldH|BaseH|Misalign|Rounds）──
    // 量纲与 KRL 侧 $config.dat 的声明保持一致：
    //     weldHum    = Measure_Height<k> * 1000  → µm（显示时 /1000 得 mm）
    //     baseH      = Baseheight<k>             → 位置量（与 EsPos 同量纲）
    //     misalignUm = Cuobian<k> * 1000         → µm，可负（显示时 /1000 得 mm）
    //     rounds     = TIMES<k>                  → 计划打磨轮次
    // 这些量只在检测段被 KRL 写入，因此上位机在检测段内持续刷新、段末定格。
    int weldHum = 0;
    int baseH = 0;
    int misalignUm = 0;
    int rounds = 0;
    // 本帧是否带了检测字段：旧程序 / 未升级的 EKI 配置下为 false，
    // 上位机据此退回"只有 EsPos 基准"的旧行为，而不是把 0 当成有效值。
    bool hasDetect = false;
};

// 解析一帧 <ECM>…</ECM>（不含 ts），失败返回 false。
// 字段格式由 KRL 侧 ECMMonitor.xml 的 <SEND> 块定义。
bool parsePacket(const QString& xml, Sample& out);
