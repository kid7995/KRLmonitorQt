#pragma once

#include <QString>

// KRL 侧 PassNo 的编码约定（自 MinShi261010 程序起）：
//     PassNo  = n > 0        打磨中：第 n 遍（补充打磨为跳增编号）
//     PassNo  = 0            未标记 / 旧程序 / 未分类段
//     PassNo  = -(k)         检测阶段：正在检测（测高）焊缝 k
//     PassNo  = -(10 + k)    打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
//     PassNo  = -(20 + k)    空走：正在转移到焊缝 k 的打磨预备位
//     PassNo  = -(40 + k)    走到焊缝 k 的打磨预备位（抬高位 Pk4）
//     PassNo  = -(50 + k)    打磨后抬起（从打磨位回到 Pk4）
//     PassNo  = -99          程序收尾
// 焊缝号 k = 1..8（Part1 = 1..4，Part2 = 5..8），就是现场说的"工位/焊缝编号"。
// 细磨（XiMo）复用同一套编码，整体 +100（工序位）：
//     PassNo  = -(100 + k)    细磨 检测（测高）
//     PassNo  = -(110 + k)    细磨 打磨阶段·非打磨段
//     PassNo  = -(120 + k)    细磨 空走
//     PassNo  = -(140 + k)    细磨 走到打磨预备位
//     PassNo  = -(150 + k)    细磨 打磨后抬起
//     PassNo  = -199          细磨 收尾
//
// ⚠ 顺序即落盘格式：这几个值会以 quint8 写进 .ecmb 段表（src/ecmfile.cpp），
//    所以只能在末尾追加，不能插队或重排，否则旧存档的段类型会串位。
enum class SeamPhase {
    None,       // 0 未标记（旧协议或未分类）
    Detect,     // 1 检测（测高）
    Grind,      // 2 正在打磨某一道
    GrindGap,   // 3 打磨阶段中的非打磨段（道间返回、等待）
    Transfer,   // 4 空走：转移到下一个焊缝的打磨预备位
    Finish,     // 5 收尾
    Prepare,    // 6 走到打磨预备位（新增，2026-10-15）
    Lift        // 7 打磨后抬起（新增，2026-10-15）
};

// 工序：粗磨（CuMo，PassNo 百位 = 0） / 细磨（XiMo，PassNo 百位 = 1）
enum class Craft {
    Unknown,    // 无法判定（旧程序 / 旧 CSV / 正数道次之前没见过工序标记）
    Rough,      // 粗磨
    Fine        // 细磨
};

struct PassMark {
    SeamPhase phase = SeamPhase::None;
    Craft craft = Craft::Unknown;
    int seam = 0;   // 焊缝号 1..8；0 = 未给出
};

// PassNo -> 阶段/工序/焊缝（规则唯一来源）
PassMark decodePassNo(int passNo);

// 阶段中文名（界面显示用）
QString seamPhaseLabel(SeamPhase p);

// 工序中文名（界面显示用）
QString craftLabel(Craft c);
