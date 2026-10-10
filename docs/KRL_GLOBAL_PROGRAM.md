# 全局打磨程序（KRL 侧）说明与对接分析

- **分支**：`codex/global-polish-program`
- **快照日期**：2026-10-10
- **数据来源**：WorkVisual 工程库 `8141797(PCRC-FURFL4R2R5)`，`KRC\R1\`

## 1. 背景

KRL 侧原有打磨程序是"一个程序 = 一个工位"：`CuMo1`…`CuMo8`、`XiMo1_1`…`XiMo8_1`，共 8 个工位，各自的 `.src` 里自带 `RecordOn` 的起止。

现在的**全局打磨程序**把这些独立程序汇总：一次调用连续跑多个工位。

- `CuMoAllPart1`：工位 1–4（点号 P1x–P4x）
- `CuMoAllPart2`：工位 5–8（点号 P5x–P8x）
- `XiMoAllPart1` / `XiMoAllPart2`：细磨侧的同构汇总

本分支用于承载"上位机（KRLmonitor）适配全局打磨程序"的开发工作，本文是 KRL 侧行为与通讯约定的权威记录。

## 2. 文件清单

| 仓库路径 | 内容 | 原始路径（`KRC\R1\` 下） |
|---|---|---|
| `krl/MinShi260921/CuMoAllPart1.src` | 全局粗磨程序，工位 1–4 | `MinShi260921\CuMoAllPart1.src` |
| `krl/MinShi260921/CuMoAllPart1.dat` | 上述程序的点位/运动数据 | `MinShi260921\CuMoAllPart1.dat` |
| `krl/MinShi260921/CuMoAllPart2.src` | 全局粗磨程序，工位 5–8 | `MinShi260921\CuMoAllPart2.src` |
| `krl/MinShi260921/CuMoAllPart2.dat` | 上述程序的点位/运动数据 | `MinShi260921\CuMoAllPart2.dat` |
| `krl/MinShi260921/CuMo1.src` | **单工位样例**（独立版，便于与 All 版对照） | `MinShi260921\CuMo1.src` |
| `krl/MinShi260921/MinShiLib.src` | `MoInitCu()` / `MoInitXi()` 公共初始化 | `MinShi260921\MinShiLib.src` |
| `krl/System/sps.sub` | **EKI 发送程序（通讯核心）** | `System\sps.sub` |
| `krl/excerpts/config_dat_ECM_declarations.txt` | `$config.dat` 中 ECM/MINSHI 相关声明与信号定义 | `System\$config.dat` 第 832–1010 行 |
| `krl/excerpts/CalAngleFix_ToSigned.txt` | `ToSigned()` / `ToUnSigned()` 定义 | `Common\CalAngleFix.src` 第 124–149 行 |

> 仅为**只读参考快照**，不要在此处修改；现场改动请回到 WorkVisual 工程库。

## 3. 程序层级与执行顺序

```
MoInitCu()                        MinShiLib.src
   ECM_Enable = TRUE; Ofs = Ofs_set; Sspeed = Sspeed_set
   ESforce/ESload/ESmode = *_set;   ESpos = Cu_ESpos_set;   RecordOn = FALSE
   │
CuMoAllPart1   (工位 1..4)
   ① 检测段  station1 -> station2 -> station3 -> station4   （测高/测偏，计算各工位 LENGTH/TIMES/InitLen）
   ② 空移    P11（回到工位 1）
   ③ 打磨段  grinding station1 -> station2 -> station3 -> station4
   ④ RecordOn = FALSE;  Sspeed = 0;  空移回 P11
CuMoAllPart2   (工位 5..8)   —— 同构，点号 P5x..P8x
```

单工位版 `CuMo1` 的结构是"检测 → 打磨"，与 All 版中单个工位段落等价，可直接对照。

**注意**：`CuMoAllPart1` 与 `CuMoAllPart2` 是两个独立 `DEF`，本工程库内**找不到调用点**（`cell.src` 是未填写的模板）。上层如何按顺序调用 Part1/Part2 需现场确认（见第 7 节）。

## 4. 通讯链路

### 4.1 通路

```
KUKA EHA / 力控模块（EtherCAT 输入）
   $IN[2616..2647] POSITION_R_ETH (32bit)    $IN[2664..2679] FORCE_R_ETH (16bit)
        │
KRL 全局变量 RecordOn / PassNo / MonRef      （$config.dat 第 898–900 行）
        │  每个 SPS(submit) 循环采样一次
        ▼
System\sps.sub ── EKI 通道 "ECMMonitor" ── EKI_Send("ECMMonitor","ECM")
        │  TCP 主动连接 PC:65002
        ▼
KRLmonitor：netserver.cpp → protocol.cpp → recorder.cpp → chartview.cpp
```

### 4.2 发送程序（`krl/System/sps.sub`，第 63–88 行）

```krl
IF NOT bEKI_Init THEN
  RET_EKI = EKI_Init("ECMMonitor")
  RET_EKI = EKI_Open("ECMMonitor")
  bEKI_Init = TRUE
ENDIF

IF $FLAG[10] THEN
    nPosR     = ToSigned(POSITION_R_ETH)
    nForceR   = ToSigned(FORCE_R_ETH)
    bRecordOn = RecordOn
    nPassNo   = PassNo
    nEsPos    = MonRef
    RET_EKI = EKI_SetInt ("ECMMonitor", "ECM/PosR",     nPosR)
    RET_EKI = EKI_SetInt ("ECMMonitor", "ECM/ForceR",   nForceR)
    RET_EKI = EKI_SetBool("ECMMonitor", "ECM/RecordOn", bRecordOn)
    RET_EKI = EKI_SetInt ("ECMMonitor", "ECM/PassNo",   nPassNo)
    RET_EKI = EKI_SetInt ("ECMMonitor", "ECM/EsPos",    nEsPos)
    RET_EKI = EKI_Send("ECMMonitor", "ECM")
ELSE
    RET_EKI = EKI_Open("ECMMonitor")
ENDIF
```

### 4.3 字段映射

| EKI 节点 | KRL 侧来源 | 类型/位宽 | KRLmonitor 字段 | 语义 |
|---|---|---|---|---|
| `ECM/PosR` | `ToSigned(POSITION_R_ETH)` | INT，来自 `$IN[2616..2647]`（32bit） | `Sample.pos` | 实际位置反馈 |
| `ECM/ForceR` | `ToSigned(FORCE_R_ETH)` | INT，来自 `$IN[2664..2679]`（16bit） | `Sample.force` | 实际力反馈（可为负） |
| `ECM/RecordOn` | `RecordOn` | BOOL | `Sample.recordOn` | 记录开关，全程序一次上升沿 + 一次下降沿 |
| `ECM/PassNo` | `PassNo` | INT | `Sample.passNo` | 道次号；`0` = 非打磨段（道次间/工位间返回段） |
| `ECM/EsPos` | `MonRef` | INT | `Sample.esPos` | 设定位置（打磨时程序内 `MonRef = ESpos`） |
| — | `$FLAG[10]` | BOOL | （不发送） | 发送总开关：FALSE 时只维持连接不发送数据 |

### 4.4 有符号转换

```krl
GLOBAL DEFFCT INT ToSigned(Signals_in : IN)
   DECL INT Signals_in,Res
   Res = Signals_in
   IF Res > 32767 THEN
     Res = Res - 65536
   ELSE
     Res = Res
   ENDIF
   RETURN Res
ENDFCT
```

`ForceR` 已在 KRL 侧转成有符号数；`src/protocol.cpp:26` 的 `(int16_t)(uint16_t)(v & 0xFFFF)` 是幂等保险，两端口径一致。

### 4.5 发送节奏与协议中缺失的信息

- **节奏**：每个 SPS 循环发一帧 ⇒ 采样率等于 SPS 循环率（`src/config.h` 实测 45–101 Hz）。
- **帧外壳** `<ECM>…</ECM>` 由 EKI 通道配置 XML 定义，该文件位于 `KRC\ROBOTER\Config\User\Common\...\EthernetKRL\config.xml`，**不在本工程库内**。若改动字段顺序/名称，必须同步核对上位机 `protocol.cpp`。
- **协议里没有**：工位号、时间戳、帧序号。PC 侧只能用接收时刻 + `PassNo` 变化来推断结构。

## 5. 全局程序的时序语义（以 CuMoAllPart1 为例）

| 行号 | 事件 | 对 KRLmonitor 的影响 |
|---|---|---|
| 25 | `MoInitCu()`：`RecordOn = FALSE` | 状态机回到"无上升沿"起点 |
| 35–36 | 检测段 1：`RecordOn = TRUE`，`PassNo = 0` | **唯一的记录上升沿** → 新建数据组 |
| 102–111 | 检测段 2：`RecordOn = TRUE`（重复赋值，非边沿） | 不产生新组 |
| 177–186 / 252–261 | 检测段 3 / 4：同上 | 不产生新组 |
| 327–334 | 空移回 P11 | 长距离 PTP 空移（100%），时间轴上的空白 |
| 336–369 | 打磨工位 1：`ESpos = InitLen1+(j1-1)*Ofs`，`PassNo = j1` | 道次 1..TIMES1+1 逐道记录 |
| 384 | `PassNo = 0` | 道次间返回段 |
| 392–394 | 补充打磨：`PassNo = TIMES1+1+RetryCount1+10` | **重试道次数值 ≥ 20**，用 +10 偏移与正常道次区分 |
| 448–550 | 打磨工位 2 | `PassNo` **从 1 重新计数** |
| 559–661 / 670–771 | 打磨工位 3 / 4 | 同上 |
| 772 | `RecordOn = FALSE` | **唯一下降沿** → 封组 |
| 773–780 | 空移回 P11 | 已封组，不再记录 |

### 关键结论

1. **整个 All 程序 = KRLmonitor 里的 1 个数据组**（只有一次真实上升沿、一次下降沿），包含 4 个工位的检测段与打磨段。
2. 每个工位开头的 `RecordOn = TRUE` 只是重复赋值，**不构成边沿**；`PassNo = 0` 同理反复出现。
3. **`PassNo` 在每个工位从 1 重新开始** ⇒ 同一组内会出现重号（如 4 组 `#1…`）。当前 `computePasses()` 是按 PassNo 变化追加区间的，区间列表本身正确，但 `Pass.no` 会重复 ⇒ 图例、道次标注、叠道对比都会出现重号。
4. **补充打磨（重试）道次**编号 = `TIMES+1+RetryCount+10`（≥20），是可靠可识别的语义标记。
5. **`ESpos`（= `MonRef`）每工位基准不同**（`InitLen1..4`）⇒ ESpos 阶梯线在工位边界会出现跳变，跟随误差也会跳。
6. **时间轴被空移主导**：工位间是 100% 快速 PTP 定位，工位内打磨是 0.02–0.06 m/s，两者时间尺度差异大。
7. `$config.dat:900` 中 `DECL BOOL RecordOn=TRUE` 即**上电初值为 TRUE**。但 `MoInitCu()` 会先置 FALSE，所以正常流程仍会产生干净的一次上升沿；上位机 `arm()` 后首帧只采纳当前值、不造边沿，这一点两者一致、无需改造。

## 6. 对 KRLmonitor 的影响（工作项，落地状态见第 8 节）

| 优先级 | 事项 | 相关位置 |
|---|---|---|
| 高 | **工位识别**：协议无工位号，需从 `PassNo` 回退（重号）/ `0` 段 / 时间间隔推断工位边界，并在图上标出工位分割 | `src/model.cpp`、`src/chartview.cpp` |
| 高 | **道次重号**：图例/标注/叠道对比在重号时的可读性（例如显示 `工位k-道次n`） | `chartview.cpp:drawPassAnnotations`、`legend`、`drawOverlay` |
| 中 | **补充打磨道次**（编号 ≥20 或 `no>TIMES+1+10` 的语义）的单独标识/统计 | `src/model.cpp` |
| 中 | **ESpos 每工位基准跳变**的显示与跟随误差计算口径 | `chartview.cpp`、`mainwindow.cpp`（实时数值栏） |
| 中 | **单组数据量**：4 工位 × 多道次 + 空移，一组远超原来的单工位规模；`MAX_POINTS(200000)` 裁剪会砍掉组头，需评估是否加大或按工位分包 | `src/config.h`、`recorder.cpp:trimIfNeeded` |
| 低 | 叠道对比是否需要按"工位+道次"重新编号（当前按 pass 出现顺序叠加） | `chartview.cpp:drawOverlay` |

## 7. 待确认问题

1. 现场是只跑 `CuMoAllPart1/Part2`（全局），还是仍与单工位程序混用？
2. 谁按什么顺序调用 `Part1` → `Part2`（CELL 程序号？上层程序？人工选择）？
3. 上位机是否需要按**工位**切分显示/导出，还是仍按"一次 RecordOn = 一组"？
4. `$FLAG[10]` 由谁置位/复位（HMI？PLC？），与上位机的"开始监听"是否需要对时？
5. 细磨（`XiMoAll*`）是否也要在同一分支内支持？其 `MonRef = Poslimit` 等语义需要单独确认。

## 8. PassNo 负数编码（已实施，2026-10-10）

原方案的"推断工位"存在一个硬伤：**现有协议里"检测段"和"打磨段道间返回"都是 `PassNo = 0`，信息上完全等价**，无法区分阶段。因此改为让 KRL 侧用 `PassNo` 的**符号与数值区间**同时表达"阶段 + 焊缝号"。

### 8.1 编码约定

```
 PassNo = n > 0          打磨中：第 n 遍
 PassNo = 0              未标记（兼容旧程序 / 旧 CSV）
 PassNo = -(k)           检测阶段：正在检测焊缝 k        k = 1..8
 PassNo = -(10 + k)      打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
 PassNo = -(20 + k)      工位间转移：正在转移到焊缝 k（预留）
 PassNo = -99            程序收尾（预留）
```

- 焊缝号 **1–8 全局编号**（Part1 = 1–4，Part2 = 5–8）。
- 补充打磨仍用正数的跳增编号（`TIMES+1+Retry+10`），上位机按"编号不连续"识别。
- 未定义区间（如 `-9`、`-10`、`-19`、`-20`、`-29`）一律解码为"未标记"，避免误判。

### 8.2 KRL 侧改动（已完成）

见 `krl/MinShi261010/README.md`。每个焊缝段落 6 处左右：

1. 检测段 `PassNo = 0` → `PassNo = -k`
2. 检测段 `ESpos = InitLen<k>` 之后新增 `MonRef = ESpos`
   —— 因为 `sps.sub` 上传的是 `MonRef`，而原程序检测段只写 `ESpos`，**检测结果（基准高度）此前根本传不出来**；补这一行后，检测段的 `ECM/EsPos` 即为该焊缝的 `InitLen`。
3. 打磨段 `;---- Grinding station k ----` 之后新增 `PassNo = -(10+k)`
4. 打磨段内各处 `PassNo = 0` → `PassNo = -(10+k)`
5. `;---- Move to station k ... ----` 之后新增 `PassNo = -(10+k)`
6. `PassNo = jN`（遍数）与补充打磨编号保持不变

**不需要**改 `EthernetKRL` 的 `config.xml`（复用 `ECM/PassNo` 字段，不新增节点），也**不需要**改 `$config.dat`（`PassNo` 为 32 位有符号 INT，只写不读，改动不影响机器人逻辑）。

### 8.3 上位机侧改动（已完成）

| 位置 | 内容 |
|---|---|
| `src/station.h` / `station.cpp`（新增） | `decodePassNo()`：`PassNo` → 阶段 + 焊缝号，规则唯一来源 |
| `src/config.h` | `SEAM_COUNT_MAX` / `SEAM_MARK_DETECT` / `SEAM_MARK_GRIND` / `SEAM_MARK_MOVE` / `SEAM_MARK_FINISH` |
| `src/model.h` / `model.cpp` | `Pass::seam/retry`、`SeamStat`、`SeamTracker`（实时与离线共用）、`rebuildSeamStats()` |
| `src/recorder.cpp` | 记录中逐点跟踪焊缝/阶段；新道次标注焊缝与补充打磨；裁剪后重建统计 |
| `src/mainwindow.cpp` | **焊缝（工位）面板**（每焊缝卡片：阶段、当前第几遍、正常/补充次数、实时 PosR/ForceR/ESpos、检测基准）；数值栏 `焊缝 k/8 · 阶段 · 第 n 遍`；封组状态栏汇总 |
| `src/chartview.cpp` | 焊缝底色带 + 焊缝分界实线 + `焊缝 k` 标注；道次标注 `#n`（补充打磨加"补"）；悬停读值显示焊缝/遍数/阶段 |
| `src/csvio.cpp` | 导出 CSV 追加 `Seam` 列（由 `PassNo` 推导；载入不依赖该列） |
| `tests/tst_station.cpp`（新增） | 编码区间、负数不建道次、四阶段全流程统计、补充打磨连续编号、旧协议兼容 |

### 8.4 关键修正（必须先于 KRL 上线）

原代码对 `PassNo` 的真假判断不统一（实时路径 `if (pn)`、离线路径 `if (cur > 0)`），**负数会被实时路径误当成新道次**，`onFallingEdge` 还可能对空道次表取 `.last()`。已统一为 `> 0`：

- `recorder.cpp`：`if (cur > 0)` / `if (pn > 0)` / `if (g->curPass > 0)` / 裁剪后 `curPass` 只保留正数
- `mainwindow.cpp`：`if (res.lastPass > 0)`
- `chartview.cpp`：`if (pn > 0)`

### 8.5 兼容与部署

- **旧程序 / 旧 CSV**（`PassNo` 全为非负数）→ `SeamTracker` 不产生任何焊缝统计，界面退回原行为（焊缝面板给出提示），道次功能不受影响。
- **部署顺序**：先升级上位机 exe → 再下载 KRL 程序。旧版上位机遇到负数会产生假道次，禁止反过来。
- **回退**：KRL 换回 `krl/MinShi261010/original/` 即可，无需再换 exe。

### 8.6 现场验证

1. 临时把一处 `PassNo = 0` 改成 `-1`，确认上位机出现「焊缝 1/8 · 检测」且帧内为 `<PassNo>-1</PassNo>`。
2. 完整跑一次 Part1：四张焊缝卡片的"正常道次 / 补充次数"与操作员记录一致。