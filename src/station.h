#pragma once

#include <QString>

// KRL 侧 PassNo 的编码约定（自 MinShi261010 程序起）：
//     PassNo  = n > 0        打磨中：第 n 遍（补充打磨为跳增编号）
//     PassNo  = 0            未标记 / 旧程序 / 未分类段
//     PassNo  = -(k)         检测阶段：正在检测（测高）焊缝 k
//     PassNo  = -(10 + k)    打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
//     PassNo  = -(20 + k)    工位间转移：正在转移到焊缝 k
//     PassNo  = -99          程序收尾
// 焊缝号 k = 1..8（Part1 = 1..4，Part2 = 5..8），就是现场说的"工位/焊缝编号"。
// 细磨（XiMo）复用同一套编码，整体 +100（工序位）：
//     PassNo  = -(100 + k)    细磨 检测（测高）
//     PassNo  = -(110 + k)    细磨 打磨阶段·非打磨段
//     PassNo  = -(120 + k)    细磨 工位间转移
//     PassNo  = -199          细磨 收尾
enum class SeamPhase {
    None,       // 未标记（旧协议或未分类）
    Detect,     // 检测（测高）
    Grind,      // 正在打磨某一道
    GrindGap,   // 打磨阶段中的非打磨段（道间返回、等待）
    Transfer,   // 工位间转移
    Finish      // 收尾
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
