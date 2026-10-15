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
// 数据组的硬上限（防止内存失控）。连续生产时真正起作用的是"内存保留组数"策略：
// 组一旦落盘就被允许回收，见 KEEP_GROUPS_DEFAULT 与 MainWindow::enforceMemoryPolicy()。
constexpr int MAX_GROUPS = 64;
// 内存里最多保留多少组（更早的已落盘组会被自动回收）
constexpr int KEEP_GROUPS_DEFAULT = 8;

// ── 连续生产 / 自动存档默认值（都可在设置里改）──────────────────
// 文件名模板占位符：{time} 到分钟、{date}、{part}、{craft}、{seams}
inline const char* ARCHIVE_NAME_TMPL = "{time}_{part}_{craft}_{seams}";
constexpr bool AUTO_SAVE_DEFAULT = true;      // 每个周期封组后自动落盘
constexpr bool ARCHIVE_COMPRESS_DEFAULT = true;   // zlib 压缩
constexpr bool ARCHIVE_OBFUSCATE_DEFAULT = true;  // 混淆（只有本软件能读）
constexpr int PART_TIMEOUT_MIN_DEFAULT = 30;      // 超过这么久没有数据 -> 视为新件

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

// ── 焊缝（工位）标记 ─────────────────────────────────────────────
// KRL 侧用 PassNo 的正负与数值区间同时表达"阶段"和"焊缝号"，详见 src/station.h。
//     PassNo  = n > 0        打磨中：第 n 遍
//     PassNo  = 0            未标记（旧程序 / 未分类段）
//     PassNo  = -(k)         检测阶段：正在检测焊缝 k
//     PassNo  = -(10 + k)    打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
//     PassNo  = -(20 + k)    工位间转移：正在转移到焊缝 k
//     PassNo  = -99          程序收尾
constexpr int SEAM_COUNT_MAX  = 8;    // 焊缝数量上限（Part1 = 1..4，Part2 = 5..8）
constexpr int SEAM_MARK_DETECT = 0;   // -k
constexpr int SEAM_MARK_GRIND  = 10;  // -(10+k)
constexpr int SEAM_MARK_MOVE   = 20;  // -(20+k)
constexpr int SEAM_MARK_FINISH = 99;  // -99

// 细磨（XiMo）用同一套规则，只是整体加 100（工序位）：
//     PassNo  = -(100 + k)    细磨 检测阶段，焊缝 k
//     PassNo  = -(110 + k)    细磨 打磨阶段·非打磨段，焊缝 k
//     PassNo  = -(120 + k)    细磨 工位间转移，焊缝 k
//     PassNo  = -199          细磨 收尾
constexpr int SEAM_MARK_DETECT_FINE = 100;  // -(100+k)
constexpr int SEAM_MARK_GRIND_FINE  = 110;  // -(110+k)
constexpr int SEAM_MARK_MOVE_FINE   = 120;  // -(120+k)
constexpr int SEAM_MARK_FINISH_FINE = 199;  // -199
