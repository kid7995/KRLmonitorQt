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

// ── 曲线浓淡（"突出显示某条焊缝"用）─────────────────────────────
// 底色层永远用浅色：所以"一条焊缝都没选中"时整张图就是浅色的；
// 选中某条焊缝后，它会被**叠画**一遍更粗、全不透明的线（ChartView::drawFocusSegments），
// 其余焊缝保持浅色不变（既不隐藏、也不额外变暗）。
constexpr double CURVE_BASE_ALPHA  = 0.45;   // 未突出的 PosR / ForceR
constexpr double CURVE_ES_ALPHA    = 0.35;   // ESpos 阶梯线（次要信息，再浅一点）
constexpr double CURVE_FOCUS_ALPHA = 1.0;    // 被突出的那一段

// ── 焊缝（工位）标记 ─────────────────────────────────────────────
// KRL 侧用 PassNo 的正负与数值区间同时表达"阶段"和"焊缝号"，详见 src/station.h。
//     PassNo  = n > 0        打磨中：第 n 遍
//     PassNo  = 0            未标记（旧程序 / 未分类段）
//     PassNo  = -(k)         检测阶段：正在检测焊缝 k
//     PassNo  = -(10 + k)    打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
//     PassNo  = -(20 + k)    空走：正在转移到焊缝 k 的打磨预备位
//     PassNo  = -(40 + k)    走到焊缝 k 的打磨预备位（KRL 下到 Pk4，Z=+40 抬高位）
//     PassNo  = -(50 + k)    打磨后抬起（从打磨位回到 Pk4）
//     PassNo  = -99          程序收尾
constexpr int SEAM_COUNT_MAX     = 8;    // 焊缝数量上限（Part1 = 1..4，Part2 = 5..8）
constexpr int SEAM_MARK_DETECT   = 0;    // -k
constexpr int SEAM_MARK_GRIND    = 10;   // -(10+k)
constexpr int SEAM_MARK_MOVE     = 20;   // -(20+k) 空走/转移
constexpr int SEAM_MARK_PREPARE  = 40;   // -(40+k) 走到打磨预备位
constexpr int SEAM_MARK_LIFT     = 50;   // -(50+k) 打磨后抬起
constexpr int SEAM_MARK_FINISH   = 99;   // -99

// 细磨（XiMo）用同一套规则，只是整体加 100（工序位）：
//     PassNo  = -(100 + k)    细磨 检测阶段，焊缝 k
//     PassNo  = -(110 + k)    细磨 打磨阶段·非打磨段，焊缝 k
//     PassNo  = -(120 + k)    细磨 空走，焊缝 k
//     PassNo  = -(140 + k)    细磨 走到打磨预备位，焊缝 k
//     PassNo  = -(150 + k)    细磨 打磨后抬起，焊缝 k
//     PassNo  = -199          细磨 收尾
constexpr int SEAM_MARK_DETECT_FINE = 100;  // -(100+k)
constexpr int SEAM_MARK_GRIND_FINE  = 110;  // -(110+k)
constexpr int SEAM_MARK_MOVE_FINE   = 120;  // -(120+k)
constexpr int SEAM_MARK_PREPARE_FINE = 140; // -(140+k)
constexpr int SEAM_MARK_LIFT_FINE    = 150; // -(150+k)
constexpr int SEAM_MARK_FINISH_FINE = 199;  // -199

// ── 检测结果的"未赋值"哨兵值 ────────────────────────────────────
// KRL 侧把 4 个检测结果（ECM_WeldH/BaseH/Misalign/Rounds）初始化为 -1，
// 含义是"本次还没检测过"（$config.dat: DECL INT ECM_WeldH = -1 …）。
// 四个值在同一次检测块里一起写入，所以收到"全 -1"就说明这一帧还没有检测结果，
// 界面必须显示"—"，不能显示成 -0.001 mm 这种假数据。
// 注意：Rounds = 0（计划 0 轮）是**合法值**，只有 -1 才代表未赋值。
constexpr int DETECT_UNSET = -1;
