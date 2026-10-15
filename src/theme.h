#pragma once

#include <QColor>
#include <QString>

// ── 现代浅色工业风配色 ───────────────────────────────────────────
// 参考 SolidWorks / EPLAN / 现代工业上位机的观感：白面板 + 浅灰标题栏 +
// 1px 细边框 + 少量强调色（强调色只用于"选中 / 当前 / 连接 / 告警"，不做装饰）。

namespace theme {

// ── 界面基础色 ──────────────────────────────────────────────────
inline const QColor BG(0xf3, 0xf5, 0xf7);        // 窗口背景（浅灰）
inline const QColor PLOT_BG(0xff, 0xff, 0xff);   // 绘图区底：纯白，曲线用深色画
inline const QColor PANEL(0xff, 0xff, 0xff);     // 面板 / 卡片 / 工具条
inline const QColor SURFACE(0xfa, 0xfb, 0xfc);   // 次级面（列表底、输入框底）
inline const QColor HEADER(0xe9, 0xee, 0xf3);    // 面板标题栏
inline const QColor BORDER(0xd6, 0xdc, 0xe3);    // 分隔线、边框
inline const QColor GRID(0xe6, 0xea, 0xf0);      // 图表网格线
inline const QColor C_TEXT(0x26, 0x31, 0x3d);    // 主文字
inline const QColor C_DIM(0x7a, 0x86, 0x94);     // 次要文字、刻度

// ── 语义色 ──────────────────────────────────────────────────────
inline const QColor C_OK(0x1e, 0x8e, 0x3e);      // 绿：运行/已连接
inline const QColor C_OK_HI(0x26, 0xa3, 0x4a);
inline const QColor C_WARN(0xc7, 0x77, 0x00);    // 橙：等待/暂停
inline const QColor C_WARN_HI(0xe0, 0x8a, 0x00);
inline const QColor C_INFO(0x0b, 0x72, 0xd0);    // 蓝：强调 / 选中
inline const QColor C_INFO_HI(0x0e, 0x86, 0xe8);
inline const QColor C_PURPLE(0x6a, 0x4f, 0xbf);
inline const QColor C_PURPLE_HI(0x7c, 0x5c, 0xd6);
inline const QColor C_HOVER(0xe7, 0xec, 0xf2);   // 悬停底
inline const QColor C_REC(0xd9, 0x30, 0x25);     // 红：记录中 / 错误
inline const QColor C_PASS(0x1e, 0x8e, 0x3e);    // 道次指示
inline const QColor C_MONITOR(0x98, 0xa2, 0xae); // 灰色实时预览曲线

// ── 数据组配色（6 组；多组对比是常态，取白底上都能看清的中饱和色）──────
struct Palette {
    QString label;
    QColor pos;      // 左轴 PosR 曲线
    QColor force;    // 右轴 ForceR 曲线
    QColor accent;   // 组标识 / 分割线
    QColor bgPos;    // PosR 按钮底色
    QColor bgForce;  // ForceR 按钮底色
};

constexpr int PALETTE_COUNT = 6;

inline const Palette GROUP_PALETTES[PALETTE_COUNT] = {
    // 组 1 — 蓝 / 红
    {"组 1", QColor(0x0b,0x72,0xd0), QColor(0xd9,0x30,0x25), QColor(0x0b,0x72,0xd0),
     QColor(0xe3,0xf0,0xfb), QColor(0xfb,0xe4,0xe2)},
    // 组 2 — 橙 / 绿
    {"组 2", QColor(0xe0,0x7b,0x00), QColor(0x1e,0x8e,0x3e), QColor(0xe0,0x7b,0x00),
     QColor(0xfd,0xf0,0xdd), QColor(0xe4,0xf3,0xe8)},
    // 组 3 — 紫 / 黄
    {"组 3", QColor(0x6a,0x4f,0xbf), QColor(0xb8,0x86,0x0b), QColor(0x6a,0x4f,0xbf),
     QColor(0xec,0xe8,0xf8), QColor(0xf7,0xf0,0xdc)},
    // 组 4 — 玫红 / 靛蓝
    {"组 4", QColor(0xc2,0x18,0x5b), QColor(0x30,0x3f,0x9f), QColor(0xc2,0x18,0x5b),
     QColor(0xfa,0xe3,0xec), QColor(0xe3,0xe6,0xf6)},
    // 组 5 — 青 / 橄榄
    {"组 5", QColor(0x0e,0x8c,0x8c), QColor(0x55,0x8b,0x2f), QColor(0x0e,0x8c,0x8c),
     QColor(0xe0,0xf2,0xf2), QColor(0xea,0xf3,0xe0)},
    // 组 6 — 棕 / 深品红
    {"组 6", QColor(0x8d,0x6e,0x63), QColor(0xad,0x14,0x57), QColor(0x8d,0x6e,0x63),
     QColor(0xf2,0xea,0xe7), QColor(0xf9,0xe3,0xec)},
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
