#pragma once

// 全局配置常量（对应 Python ecm_monitor/config.py）

// ── 网络 ────────────────────────────────────────────────────────
// 绑定所有网卡（KUKA 侧 ECMMonitor.xml 的 <EXTERNAL><IP> 填本机真实地址）
inline const char* BIND_HOST = "0.0.0.0";
constexpr int LISTEN_PORT = 65002;

// ── 数据容量 ────────────────────────────────────────────────────
// 采样率由 SPS 提交解释器循环周期决定，实测 ~45~101 Hz。
// MAX_POINTS 决定单组最长记录时长，按 100 Hz 保守估算约 33 分钟。
constexpr int MAX_POINTS = 200000;
// 超出上限后一次性裁掉最旧的这么多点
constexpr int TRIM_CHUNK = 20000;
// 最多同时显示的数据组数量
constexpr int MAX_GROUPS = 4;

// ── 绘图抽稀 ────────────────────────────────────────────────────
// 抽稀目标点数（超过时按 min/max 抽稀，保留峰值包络）
constexpr int DISPLAY_MAX_PTS = 4000;
// 两次全量抽稀之间，最多允许多少个新增点走"尾部拼接"的快路径
constexpr int DEC_APPEND_MAX = 600;

// ── 绘图字体自适应 ────────────────────────────────────────────────
// 图表字号随窗口高度放大：FONT_REF_H 为默认窗口（1400×860）下图表的典型
// 高度（此时倍率为 1，保持原设计字号），全屏放大时字号同步增大，
// FONT_SCALE_MAX 为上限倍率。倍率下限为 1：窗口缩小时字号不随之变小。
constexpr double FONT_REF_H = 660.0;
constexpr double FONT_SCALE_MAX = 1.8;

// ── 交互 ────────────────────────────────────────────────────────
// 滚轮每格的缩放倍率
constexpr double ZOOM_FACTOR = 1.25;
// 动画帧间隔（毫秒）
constexpr int FRAME_INTERVAL_MS = 120;
