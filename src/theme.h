#pragma once

#include <QColor>
#include <QString>

// 配色（GitHub Dark）。对应 Python ecm_monitor/theme.py。

namespace theme {

// ── 界面基础色 ──────────────────────────────────────────────────
inline const QColor BG(0x0d, 0x11, 0x17);      // 窗口/坐标区底色
// 绘图区底色：曲线背后的统一淡蓝。整块同深度，不随焊缝/道次变化
//（"哪一段属于哪条焊缝"由分界线、焊缝标注和焊缝 chip 表达，不靠底色深浅区分）。
inline const QColor PLOT_BG(0x12, 0x1a, 0x25);
inline const QColor PANEL(0x16, 0x1b, 0x22);   // 工具栏、卡片、图例底色
inline const QColor BORDER(0x30, 0x36, 0x3d);  // 分隔线、边框、网格线
inline const QColor C_TEXT(0xc9, 0xd1, 0xd9);  // 主文字
inline const QColor C_DIM(0x6e, 0x76, 0x81);   // 次要文字、刻度

// ── 语义色 ──────────────────────────────────────────────────────
inline const QColor C_OK(0x23, 0x86, 0x36);
inline const QColor C_OK_HI(0x2e, 0xa0, 0x43);
inline const QColor C_WARN(0x9e, 0x6a, 0x03);
inline const QColor C_WARN_HI(0xd2, 0x99, 0x22);
inline const QColor C_INFO(0x1f, 0x6f, 0xeb);
inline const QColor C_INFO_HI(0x38, 0x8b, 0xfd);
inline const QColor C_PURPLE(0x6e, 0x40, 0xc9);
inline const QColor C_PURPLE_HI(0x89, 0x57, 0xe5);
inline const QColor C_HOVER(0x48, 0x4f, 0x58);
inline const QColor C_REC(0xf9, 0x75, 0x83);   // 记录中指示灯
inline const QColor C_PASS(0x3f, 0xb9, 0x50);  // 道次指示
inline const QColor C_MONITOR(0x8b, 0x94, 0x9e);  // 灰色实时预览曲线

// ── 四组数据配色 ────────────────────────────────────────────────
struct Palette {
    QString label;
    QColor pos;      // 左轴 PosR 曲线
    QColor force;    // 右轴 ForceR 曲线
    QColor accent;   // 组标识 / 分割线
    QColor bgPos;    // PosR 按钮底色
    QColor bgForce;  // ForceR 按钮底色
};

constexpr int PALETTE_COUNT = 4;

inline const Palette GROUP_PALETTES[PALETTE_COUNT] = {
    // 组 1 — 蓝 / 粉红
    {"组 1", QColor(0x58,0xa6,0xff), QColor(0xf9,0x75,0x83), QColor(0x58,0xa6,0xff),
     QColor(0x0d,0x22,0x38), QColor(0x2d,0x0e,0x14)},
    // 组 2 — 橙 / 绿
    {"组 2", QColor(0xff,0x99,0x44), QColor(0x56,0xd3,0x64), QColor(0xff,0x99,0x44),
     QColor(0x2d,0x1a,0x0e), QColor(0x0d,0x22,0x14)},
    // 组 3 — 紫 / 黄
    {"组 3", QColor(0xd2,0xa8,0xff), QColor(0xe3,0xb3,0x41), QColor(0xd2,0xa8,0xff),
     QColor(0x1f,0x12,0x40), QColor(0x2a,0x1f,0x00)},
    // 组 4 — 青 / 珊瑚
    {"组 4", QColor(0x39,0xd0,0xd8), QColor(0xff,0x7b,0x72), QColor(0x39,0xd0,0xd8),
     QColor(0x05,0x2a,0x2c), QColor(0x2d,0x0e,0x08)},
};

// 取配色的唯一入口：下标自动回绕到 [0, PALETTE_COUNT)。
// 必须回绕 —— 连续生产会创建远超 4 个数据组（MAX_GROUPS = 64），
// 直接 GROUP_PALETTES[i] 越界读到的是野 QString，会直接把进程打崩。
inline const Palette& paletteOf(int idx) {
    if (idx < 0) {
        idx = 0;
    }
    return GROUP_PALETTES[idx % PALETTE_COUNT];
}

// 千分位格式化（等价于 Python 的 format(x, ',')）
inline QString formatThousands(long long v) {
    bool neg = v < 0;
    unsigned long long a = neg ? (unsigned long long)(-(v + 1)) + 1ULL : (unsigned long long)v;
    QString s = QString::number(a);
    for (int i = s.size() - 3; i > 0; i -= 3) {
        s.insert(i, ',');
    }
    return neg ? QStringLiteral("-") + s : s;
}

}  // namespace theme
