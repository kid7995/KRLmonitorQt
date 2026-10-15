# MinShi 打磨系统 · 全链路分析（上位机 ↔ 现场 KRL 工程库）

- **生成日期**：2026-10-14
- **上位机工程**：`D:\Saiwider\Code\Qt\KRLmonitor`（分支 `codex/global-polish-program`）
- **现场 KRL 工程库**：`C:\Users\SWR-QY\Documents\WorkVisual 6.0\Repositories\8141797(PCRC-FURFL4R2R5)`
  （WorkVisual 工程库只是**离线编辑副本**，真正跑在控制器上的文件在 `C:\KRC\...`，改动需在 WorkVisual 里编译下载）
- **用途**：把"哪一份文件负责什么、数据从哪儿来到哪儿去、想改 X 该动哪个文件的哪一行"一次性说清，作为后续调整的唯一索引。

> 阅读顺序建议：第 1 章看上位机 → 第 4 章看通讯 → 第 5 章看数据读取与算法 → 第 7 章对照"已落地/未落地" → 第 8 章按目标找改动点。

---

## 0. 一句话概览

```
机器人 EHA/力控模块 ──EtherCAT──▶ $IN/$OUT（$config.dat 信号表）
                                      │
      LVDT 测头 ──EtherCAT──▶ LVDT1/2/3 ──PN_AES.sub──▶ LV1/LV2/LV3 ──▶ 焊缝高度/错边/基准
                                      │
      打磨程序（CuMoAllPart* / XiMoAllPart*）写 RecordOn / PassNo / MonRef / ESpos
                                      │
                              System\sps.sub（Submit 循环）
                                      │ EKI 通道 "ECMMonitor"
                                      ▼
                        PC:65002 ◀── KRLmonitor（Qt）建组、画波形、按焊缝统计
```

---

## 1. 上位机工程 KRLmonitor

### 1.1 目录与职责

| 路径 | 职责 |
|---|---|
| `main.cpp` | 入口（保留在根目录） |
| `src/config.h` | 全部常量：端口 65002、`MAX_POINTS`、组数上限、抽稀目标、**PassNo 编码常量** |
| `src/netserver.*` | QTcpServer，收 KUKA 主动连入的流，切帧后入队（GUI 线程，无锁） |
| `src/protocol.*` | 解析一帧 `<ECM>…</ECM>` → `Sample` |
| `src/station.*` | **`decodePassNo()`：PassNo → 阶段 + 焊缝号，规则唯一来源** |
| `src/model.*` | `Pass` 道次区间、`SeamStat`/`SeamTracker` 焊缝统计、`DataGroup` 数据容器、`rebuildSeamStats()` |
| `src/recorder.*` | `RecordOn` 边沿状态机、建组/封组、逐点写入、裁剪、灰色预览缓冲 |
| `src/chartview.*` | 波形绘制、道次分割线、焊缝底色带、悬停读值、叠道对比 |
| `src/mainwindow.*` | 主界面：数值栏、焊缝（工位）面板、组管理、CSV 导入导出 |
| `src/decimate.*` | min/max 抽稀 + 缓存（`DEC_APPEND_MAX` 尾部拼接快路径） |
| `src/csvio.*` | CSV 读写（列：`time_s,PosR,ForceR,PassNo,EsPos,Seam`） |
| `ECMMonitor.xml` | **控制器侧 EKI 通道配置的本地副本**（真正生效的在控制器 `C:\KRC\ROBOTER\...\EthernetKRL\`） |
| `$config.dat`、`sps.sub` | KRL 侧随包文件（**待回拷现场**，见第 7 章） |
| `krl/` | 现场程序**只读快照**（不要在这里改；改完现场后再同步快照） |
| `krl/KRL_update_20261014/` | 10-14 的机器人侧改动包（含 `MANIFEST.md`） |
| `docs/DEPLOYMENT.md` | Windows 打包部署说明 |
| `tests/` | Qt Test：protocol / model / decimate / csvio / recorder / station |

### 1.2 运行时数据流

```
TcpServer::onReadyRead → 拆帧 → parsePacket() → Sample{ts,pos,force,recordOn,passNo,esPos}
   → Recorder::process(queue, autoPause)      ← 每帧推进一次
       ├─ 未记录：写灰色预览缓冲 monTimes/monPos/monForce，更新 monSeam/monPhase
       └─ 记录中：append() 写 DataGroup，updatePasses() 维护道次区间
                 SeamTracker::feed()/passOpened() 维护焊缝统计
   → TickResult（touched/structDirty/passDirty/seamDirty/sealed…）
   → MainWindow 刷新数值栏 / 焊缝面板；ChartView 重画
```

关键约定：

- **一组 = 一次 `RecordOn` 上升沿到下降沿**。灰底预览段（RecordOn=FALSE）不进 CSV、不建组。
- `arm()`：进入监听时**不造边沿**（首帧只采纳当前值）；暂停恢复用 `markPrevUnknown()`。
- 组数上限 `MAX_GROUPS=4`；单组超过 `MAX_POINTS=200000` 一次裁掉最旧 `TRIM_CHUNK=20000` 点，**裁剪后必须 `decCache.clear()` + `rebuildSeamStats()`**。
- 只有 `PassNo > 0` 才建道次区间（负数段落不建区间，只做焊缝/阶段归属）。

### 1.3 上位机的两个缺口（**2026-10-14 已修复**，此处保留作背景）

> 两者均已按 [MINSHI_V2_DESIGN.md](MINSHI_V2_DESIGN.md) 第 11 节的实施状态落地：
> 4 个检测字段已解析并进 `Sample`，细磨 `-(100+k)`/`-(110+k)` 已能解码（`Craft::Fine`）。
> 下面保留原文，便于回溯当初为什么必须同时改 KRL 与上位机。

| 缺口 | 证据 | 影响 |
|---|---|---|
| **未解析新增 4 个 EKI 字段** | `Sample`（`src/protocol.h`）只有 `ts/pos/force/recordOn/passNo/esPos`；全仓 grep `WeldH\|BaseH\|Misalign\|Rounds` 无命中 | 现场若采纳 10-14 包，`ECM/WeldH` 等 4 个节点会被**静默忽略**；界面拿不到检测结果（焊缝高度/基材高度/错边/计划轮次），只能靠 `EsPos` 推断基准 |
| **未解码细磨 100 段** | `src/config.h` 只定义 `SEAM_MARK_DETECT=0 / GRIND=10 / MOVE=20 / FINISH=99`；`station.cpp` 对 `-(100+k)`/`-(110+k)` 落进 `else → None` | 细磨程序若打 `-101…-108`、`-111…-118` 标记，上位机会显示"未标记"，焊缝卡片不点亮 |

---

## 2. 现场工程库顶层结构

```
8141797(PCRC-FURFL4R2R5)\
├─ Connections.xml / Modifications.xml / actions.log   WorkVisual 元数据
└─ KRC\
   ├─ R1\                      机器人 1 的程序树（= 控制器 C:\KRC\R1\）
   │  ├─ System\               ★ 全局数据与提交程序
   │  ├─ Common\               ★ 公共函数 / 公共 Submit
   │  ├─ EHA\                  ★ EHA(EtherCAT 力控) 状态机 Submit
   │  ├─ MinShi260901\         单工位独立版（CuMo1_1..8_1、XiMo1..8）
   │  ├─ MinShi260921\         全局版 + 公共初始化（CuMoAllPart1/2、XiMoAllPart1/2、MinShiLib）
   │  ├─ MinShi261010\         当前全局粗磨版（CuMoAllPart1_2 / Part2_2）
   │  ├─ JIUMIN\               历史版本集合（gangzhi/shazhi/HanFeng/CePianYi/Xigangzhi）
   │  ├─ henyibiaoding\        焊枪标定相关（BINKKS/PathNum/EHACalib/qy0507）
   │  ├─ Program\              ★ 旧的独立打磨/标定程序（mocahan 系列、biaoding0512、zhu0123、QY…）
   │  ├─ TP\                   EthernetKRL 库源码、p00（程序号机制）、BrakeTest
   │  ├─ Mada\                 $machine.dat / $robcor.dat（机器数据）
   │  └─ cell.src              ★ CELL 模板（**当前无有效调用点**）
   └─ STEU\                    外围设备（$custom.dat / $machine.dat / $option.dat）
```

★ = 与本系统（打磨 + 通讯）直接相关。

### 2.1 三代打磨程序的关系

| 世代 | 目录 | 形态 | 特点 |
|---|---|---|---|
| 单工位（老） | `MinShi260901` | `CuMo1_1`…`CuMo8_1`、`XiMo1`…`XiMo8` | 一个程序一个工位，各自带 `RecordOn` 起止；**无负 PassNo** |
| 全局（上一代） | `MinShi260921` | `CuMoAllPart1/2`（工位 1–4 / 5–8）、`XiMoAllPart1/2`、`MinShiLib`（`MoInitCu()`/`MoInitXi()`） | 一次调用连跑 4 个工位；检测段与打磨段在同一 `RecordOn` 周期内；`PassNo` 仍为 `0`/正数 |
| 全局（当前） | `MinShi261010` | `CuMoAllPart1_2` / `Part2_2`（+ `.dat`） | 在全局版基础上加 **负 PassNo 标记 + 检测段 `MonRef=ESpos`** |

> `JIUMIN\*`（`minShi260414/260424/260427/260618`、`MinShiXiGangZhi260424`）是更早的按"焊条/砂纸/焊缝"分类的实验版本（`gangzhi`=钢制、`shazhi`=砂纸/砂轮、`HanFeng`=焊缝），**仅作历史参考**：它们用 `GLOBAL_Measure_Height`/`Weld_misalignment` 等老式全局量，**不参与当前上位机对接**。

---

## 3. `System\` —— 全局数据与提交程序（改动最常落到这里）

### 3.1 `$config.dat`（1064 行；GBK 编码、LF 行尾）

这是**所有全局变量与 EtherCAT 信号的总表**。行号以现场库当前版本为准：

| 行 | 内容 |
|---|---|
| 1 | `&ACCESS RV$4`（WorkVisual 访问级别版本号） |
| 836 | `BOOL ECM_Enable=FALSE` |
| 855–871 | **AES（Modbus 力控）**：`STATUS_R $IN[3001-3016]`、`Rotation_R $IN[3017-3032]`、`Force_R $IN[3033-3048]`、`Pos_R $IN[3049-3064]`、`Mode_R $IN[3065-3080]`、`Err_R $IN[3081-3096]`、`Tempture_R $IN[3097-3112]`；输出 `Controlword $OUT[3001-3016]`、`Speed_S`、`Force_S`、`Pos_S`、`Mode_S`、`TouchForce_S`、`RampTime_S`、`LoadWeight_S` |
| 877–886 | **EHA（EtherCAT）**：输出 `CONTROLWORD_ETH $OUT[2600-2615]`、`MODE_ETH $OUT[2616-2623]`、`POSITION_S_ETH $OUT[2624-2655]`、`FORCE_S_ETH $OUT[2656-2671]`、`WEIGHT_S_ETH $OUT[2672-2687]`；输入 `ERRCODE_ETH $IN[2600-2615]`、`POSITION_R_ETH $IN[2616-2647]`、`STATUS_R_ETH $IN[2648-2663]`、`FORCE_R_ETH $IN[2664-2679]` |
| 888–893 | `ERforce/ERpos/ERerrcode/ERstatus`；力控设定 `ESfunc=0`、`ESforce=100`、`ESpos=1029`、`ESmode=2`、`ESload=177` |
| 896–903 | **MINSHI 通讯量**：`MonRef=1029`、`PassNo=-14`、`RecordOn=FALSE`、`nPassNo=-14`、`nEsPos=1029`、`bRecordOn=FALSE` |
| 906–917 | **公共初始化基准**：`Ofs_set=30`、`Ofs=30.0`、`Sspeed_set=0`、`ESmode_set=2`、`ESload_set=177`、`Cu_ESforce_set=100`、`Cu_ESpos_set=200`、`Xi_ESforce_set=25`、`Xi_ESpos_set=0` |
| 919–930 | `Laser_GI1/2`（$IN[3500-3531]，**当前未使用**）、`status/Rrotation/...`、`Sfunc=1`、`Sspeed=0`、`Sforce=-100`、`Spos=950`、`Smode=2`、`STouchF=0`、`Sramp=0`、`Sload=170` |
| 932–946 | **LVDT**：`LVDT1 $IN[3129-3144]`、`LVDT2 $IN[3161-3176]`、`LVDT3 $IN[3193-3208]`；`GLOBAL_Measure_Height`、`GLOBAL_LOOPCOUNT`、`Weld_misalignment`、`GLOBAL_RES_Z`、`GLOBAL_TilitC`、`GLOBAL_Poslimit`、`GLOBAL_RES` |
| 948–973 | **粗磨每焊缝量**：`Baseheight1-8`、`Cuobian1-8`、`Measure_Height1-8`（REAL） |
| 975–1000 | **细磨每焊缝量**：`Baseheight1_2-8_2`、`Cuobian1_2-8_2`、`Measure_Height1_2-8_2` |
| 1002–1015 | `LVDT_R1/R2/R3`；测头基准 `L1BaseF=1.29000`、`L2BaseF=3.59000`（大盘；注释留有小盘 `3.49000`）、`L3BaseF=1.01500`；`AVG1-8` 补充基准 |
| 1016–1021 | `FinalPos=1000`、`FinalPos1-4=1000`（EHA 基准） |
| 1022–1036 | `PosShaZhi=1000`、`PosShaZhi1-4=1000`、`AVGShaZhi1-8`（细磨基准） |
| 1038–1046 | `GZoffset1-8`（钢制补偿；1–4 = 0，5/6 = −45，7 = −35，8 = −40） |
| 1048–1056 | `Posoffset1-8`（砂纸补偿；**当前程序未引用**，历史遗留） |
| 1058–1061 | `LV1,LV2,LV3`、`HF1Offset`、`CyTimes` |

> ⚠️ 当前 `PassNo=-14` / `nPassNo=-14`（上电初值）会解码成"打磨阶段·非打磨段·焊缝 4"，上位机在程序启动前可能显示"焊缝 4 · 打磨间隙"。正常流程里 `MoInitCu()/MoInitXi()` 会把它置 0，所以只是**上电瞬间的显示抖动**；若要干净，建议改回 `0`。

### 3.2 `System\sps.sub`（Submit 解释器循环，GBK、LF）

职责：**唯一的 EKI 发送点**。流程见第 4.2 节。关键点：

- `bEKI_Init` 只在启动后初始化一次 → **改 EKI 配置后必须冷启动控制器**（或 `EKI_Close`→`EKI_Init`→`EKI_Open`）。
- `$FLAG[10]` 是 EKI 通道的 **ALIVE 存活标志**（由 `ECMMonitor.xml` 的 `<ALIVE Set_Flag="10"/>` 自动置位/清零，不是 HMI 手动给的）→ 断线自动停发。
- 帧内 5 个字段（`PosR/ForceR/RecordOn/PassNo/EsPos`）在同一时刻取样，保证同源。

### 3.3 其它

| 文件 | 说明 |
|---|---|
| `bas.src` | BASISTECH 运动/碰撞监控库（2070 行，不用改） |
| `MsgLib.src/.dat`、`collmonlib.*`、`ir_stopm.src` | 消息、碰撞监控、中断停机 |
| `masref_main.*`、`Program\masref_user.src` | 参考点移动（空实现） |
| `MqttConfig.dat` | KukaConnect（默认 `KUKACONNECT_ENABLED=FALSE`，未启用） |
| `Global_Points.dat` | 空（无全局点） |

---

## 4. 通讯链路（端到端）

### 4.1 通路

```
EHA/力控模块 ─EtherCAT→ $IN[2600-2679]
   FORCE_R_ETH(16bit) POSITION_R_ETH(32bit) ERRCODE_ETH STATUS_R_ETH
        │
Common\PN_AES.sub（每 SPS 循环采样，见 4.3）
        │  ERforce = ToSigned(FORCE_R_ETH) ; ERpos = ToSigned(POSITION_R_ETH)
        ▼
 全局量  RecordOn / PassNo / MonRef(=上传用的 ESpos)
        │
System\sps.sub ──EKI 通道 "ECMMonitor"── EKI_Send("ECMMonitor","ECM")，KUKA 主动连 PC:65002
        ▼
KRLmonitor：netserver → protocol → recorder → model(chartview/mainwindow/csvio)
```

### 4.2 `System\sps.sub` 发送段（USER PLC 内）

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

> **注意**：`ECM/EsPos` 上传的是 `MonRef`，**不是** `ESpos`。所以打磨程序里只写 `ESpos` 的地方，上位机看不到——这正是 261010 版在每个检测段补写 `MonRef = ESpos` 的原因。

### 4.3 信号采样/写设定：`Common\PN_AES.sub`

| 方向 | 内容 |
|---|---|
| 读（Modbus 侧） | `status/Rrotation/Rforce/Rposition/Rmode/RERR/Rtemp = Swap_ReadBytes(...)` |
| 读（EtherCAT 侧） | `ERstatus/ERforce/ERpos/ERerrcode = ToSigned(...)` |
| 读（LVDT） | `LVDT_R1/2/3 = Swap_ReadBytes(LVDT1/2/3) * 0.001`；`LV1/2/3 = LVDT_R1/2/3 − L1BaseF/L2BaseF/L3BaseF` |
| 写（Modbus） | `Controlword/Speed_S/Force_S/Pos_S/Mode_S/TouchForce_S/RampTime_S/LoadWeight_S = Swap_SetBytes(...)` |
| 写（EtherCAT） | `FORCE_S_ETH=ToUnSigned(ESforce)`、`POSITION_S_ETH=ToUnSigned(ESpos)`、`MODE_ETH=ToUnSigned(ESmode)`、`WEIGHT_S_ETH=ToUnSigned(ESload)` |

（`Laser = GI_TO_REAL(Laser_GI1,Laser_GI2)` 被注释，激光测距当前**不参与**。）

### 4.4 EHA 使能状态机：`EHA\ECMSps.sub`

`ECM_Enable` 由打磨程序置 TRUE（`MoInitCu()`/`MoInitXi()`/老程序）→ 本 Submit 走一遍 CiA402 类时序：

```
ECMSTEP=10 → CONTROLWORD_ETH=128 (0x80 故障复位) → 20
         20 → CONTROLWORD_ETH=6   (0x06 Shutdown)   → 30
         30 → CONTROLWORD_ETH=7   (0x07 Switch on)  → 40
         40 → CONTROLWORD_ETH=15  (0x0F Enable op)  → 0（每步 WAIT 0.5s）
```

**想改力控"上电/使能"行为就改这张表。**

### 4.5 EKI 通道配置（`ECMMonitor`）

- **真正生效的文件不在工程库里**：控制器上位于
  `C:\KRC\ROBOTER\Config\User\Common\EthernetKRL\ECMMonitor\config.xml`
  （或 `...\EthernetKRL\ECMMonitor.xml`，按现场布局）。
- 工程库/上位机侧的副本：`ECMMonitor.xml`（含 9 个 `<ELEMENT>`）、`krl\EKI\ECMMonitor.xml`、`krl\EKI\ECMMonitor.original.xml`（5 个，改动前基线）。
- 配置要点：`<EXTERNAL><IP>192.168.1.100</IP><PORT>65002</PORT>`（**PC 的 IP 与端口，换 PC 就要同步改这里**）、`<INTERNAL><ENVIRONMENT>Submit</ENVIRONMENT>`、`<ALIVE Set_Flag="10"/>`。
- 改字段顺序/名称 → 必须同步 `src/protocol.cpp`；改完 → **冷启动控制器**。

### 4.6 帧字段映射（含 10-14 新增的 4 个）

| EKI 节点 | KRL 来源 | 类型 | 上位机字段 | 语义 |
|---|---|---|---|---|
| `ECM/PosR` | `ToSigned(POSITION_R_ETH)` | INT | `Sample.pos` | 力控实际位置反馈 |
| `ECM/ForceR` | `ToSigned(FORCE_R_ETH)` | INT | `Sample.force` | 实际力反馈（可负） |
| `ECM/RecordOn` | `RecordOn` | BOOL | `Sample.recordOn` | 记录开关（一组一次上升沿+下降沿） |
| `ECM/PassNo` | `PassNo` | INT | `Sample.passNo` | 道次 + 阶段/焊缝编码（见第 6 章） |
| `ECM/EsPos` | `MonRef` | INT | `Sample.esPos` | 位置设定/检测基准（检测段 = `InitLen`/`Poslimit`） |
| `ECM/WeldH` | `Measure_Height<k>` | REAL | **未解析** | 焊缝高度 mm |
| `ECM/BaseH` | `Baseheight<k>` | INT | **未解析** | 基材高度（位置量） |
| `ECM/Misalign` | `Cuobian<k>` | REAL | **未解析** | 错边量 mm（可负） |
| `ECM/Rounds` | `TIMES<k>` | INT | **未解析** | 计划打磨轮次 |

### 4.7 节拍与时序

- 采样率 = SPS 循环率（实测 45–101 Hz，上位机按 100 Hz 保守估算容量）。
- **一次 `RecordOn=TRUE→FALSE` 在上位机里是 1 个数据组**。全局程序一次跑 4 个工位 = **1 组**，含 4 段检测 + 4 段打磨 + 工位间空移。
- 工位开头的 `RecordOn = TRUE` 是重复赋值，**不构成边沿**；旧版的 `PassNo = 0` 同理反复出现。
- 时间轴被 100% 快速 PTP 空移主导，工位内打磨 0.02–0.06 m/s，尺度差异大 → 图上"空白段"就是转移段。

---

## 5. 数据读取与工艺算法

### 5.1 测高链（LVDT）

```
LVDT1/2/3 ($IN)  ── Swap_ReadBytes ──▶ LVDT_R1/2/3 (×0.001)
                                          │  减测头基准
                                          ▼
                          LV1 = LVDT_R1 − L1BaseF (1.29000)
                          LV2 = LVDT_R2 − L2BaseF (3.59000, 大盘)
                          LV3 = LVDT_R3 − L3BaseF (1.01500)
                                          │
                     GetHFZ() = LV2 − (LV1+LV3)/2      ← 焊缝高度（各程序内的局部函数）
                     错边 Cuobian = LV1 − LV3
```

**改测头基准 → 改 `$config.dat:1003-1006`（`L1BaseF/L2BaseF/L3BaseF`）**；换盘子（大/小盘）改 `L2BaseF`。

### 5.2 粗磨（CuMo）基准与道次

每焊缝 k（1–4 在 Part1，5–8 在 Part2）：

```
HF1Offsetk = GetHFZ()
LENGTHk    = HF1Offsetk * 100
TIMESk     = MOD(LENGTHk / Ofs)            ; MOD() = 向下取整（CalAngleFix.src）
IF LENGTHk > 0 THEN RemainLenk = LENGTHk − TIMESk*Ofs
                ELSE  RemainLenk=0 ; LENGTHk=0 ; TIMESk=0

InitLenk   = FinalPos − ((LV1+LV3)/2 − AVGk)*100 − GZoffsetk − LENGTHk
ESpos      = InitLenk            ; 力控起点
MonRef     = ESpos               ; ★ 上传（261010 版新增）
Baseheightk= InitLenk + LENGTHk
Cuobiank   = LV1 − LV3
Measure_Heightk = HF1Offsetk
GLOBAL_*   = 同值（老上位机/示教器用）
```

打磨循环：

```
FOR j = 1 TO TIMES+1
    ESpos = InitLen + (j−1)*Ofs      ; 每道进给 Ofs(=30)
    MonRef = ESpos
    PassNo = j                        ; 正数 = 第 j 道
    走 Pk5 → Pk6（往复一次）
    PassNo = −(10+k)                  ; 道间返回
ENDFOR
; ★补充打磨：最多 10 次，编号 = TIMES+1+Retry+10（≥20 → 上位机判为"补"）
FOR RetryCount = 1 TO 10
    ESpos = InitLen + TIMES*Ofs + RemainLen
    MonRef = ESpos
    PassNo = TIMES+1+RetryCount+10
    走 Pk5 → Pk6
    IF ((InitLen+LENGTH−DIS1)<5) AND ((InitLen+LENGTH−DIS2)<5) THEN EXIT
ENDFOR
```

**调参入口**：`Ofs_set`（单道进给量）、`Cu_ESforce_set`/`Cu_ESpos_set`/`ESmode_set`/`ESload_set`（力控配方）、`FinalPos`（起始基准）、`AVGk`（测高补偿）、`GZoffsetk`（钢制补偿）——**全在 `$config.dat`**。

### 5.3 细磨（XiMo）基准与道次（策略不同）

```
Poslimitk = PosShaZhi − ((LV1+LV3)/2 − AVGShaZhi<k>)*100 − Posoffset<k>
MonRef    = Poslimitk            ; 检测段就上传
Baseheight<k>_2 = Poslimitk

打磨：ESpos = Xi_ESpos_set（=0，固定）；ESmode = 4；Sspeed = 6000
FOR j = 1 TO 30                  ; 固定上限 30 道
    PassNo = j
    走 Pk5 → Pk6
    PassNo = 0                   ; 当前 WV 版仍为 0，10-14 包改成 −(110+k)
    IF DIS >= Poslimit THEN EXIT  ; 位置到位即停，不是"计划道数"
ENDFOR
```

> 细磨与粗磨**量纲/逻辑都不同**：粗磨按 `TIMES+1` 计划道数 + 补充打磨；细磨按"位置到达 `Poslimit` 就退出"，最多 30 道。
> 细磨用 `PosShaZhi / AVGShaZhi<k> / Posoffset<k>`，**不用** `FinalPos / AVGk / GZoffsetk`。

### 5.4 力控/电机设定（写侧）

| 变量 | 写到 | 含义 |
|---|---|---|
| `ESpos` | `POSITION_S_ETH` | 位置设定（道次进给） |
| `ESforce` | `FORCE_S_ETH` | 力设定（粗磨 100 / 细磨 25） |
| `ESmode` | `MODE_ETH` | 控制模式（默认 2；细磨运行时置 4） |
| `ESload` | `WEIGHT_S_ETH` | 负载（177） |
| `Sspeed` | `Speed_S`（Modbus AES） | 主轴转速设定（"总启动"段用 8000） |

---

## 6. PassNo 编码总表

粗磨（0 百位）与细磨（1 百位）共用一套规则：

```
 PassNo = n > 0           打磨中：第 n 道（补充打磨 = 编号跳增，≥20）
 PassNo = 0               未标记（旧程序 / 未分类段 / 旧 CSV）
 PassNo = -(k)            粗磨 检测阶段，焊缝 k         k = 1..8
 PassNo = -(10+k)         粗磨 打磨阶段·非打磨段，焊缝 k（道间返回/等待/接近）
 PassNo = -(100+k)        细磨 检测阶段，焊缝 k
 PassNo = -(110+k)        细磨 打磨阶段·非打磨段，焊缝 k
 PassNo = -(20+k)         工位间转移（预留，当前程序未使用）
 PassNo = -(120+k)        细磨 工位间转移（预留）
 PassNo = -99 / -199      收尾（预留）
```

焊缝号全局 1–8：`Part1 = 1–4`，`Part2 = 5–8`（粗磨 = 0 百位，细磨 = 1 百位）。
未定义区间（如 `-9`、`-10`、`-19`、`-20`、`-29`、`-100`…）一律解码为"未标记"，避免误判。

上位机实现位置：

| 内容 | 文件 |
|---|---|
| 区间常量 | `src/config.h`（`SEAM_COUNT_MAX/SEAM_MARK_DETECT/GRIND/MOVE/FINISH`） |
| 解码规则 | `src/station.cpp`（**唯一来源**；支持细磨 100 段只需扩这里的区间判断） |
| 阶段中文名 | `src/station.cpp:seamPhaseLabel()` |
| 统计与面板 | `src/model.cpp:SeamTracker/rebuildSeamStats()`、`src/mainwindow.cpp` 焊缝面板、`src/chartview.cpp` 底色带/标注 |
| CSV 导出列 | `src/csvio.cpp`（`Seam` 列，导出时按编码反推；载入不依赖该列） |
| 单测 | `tests/tst_station.cpp` |

---

## 7. 现状对照：已落地 / 未落地（**改动前必读**）

对照基准：现场库（2026-10-14 16:47 状态）、`krl/KRL_update_20261014/`（10-14 包）、上位机源码。

### 7.1 KRL 侧

| 项 | 现场库（WorkVisual 副本） | 10-14 包 | 说明 |
|---|---|---|---|
| 粗磨负 PassNo 标记（`-(k)` / `-(10+k)`）+ 检测段 `MonRef=ESpos` | ✅ 已在 `MinShi261010\CuMoAllPart1_2.src`、`Part2_2.src` | ✅ | 一致 |
| 粗磨检测结果上报（`ECM_Seam/WeldH/BaseH/Misalign/Rounds` 6 行块 × 4 段） | ❌ 未落地 | ✅ 每文件 4 段 × 6 行 | 现场库 `CuMoAllPart1_2.src` 比包内小 944 B |
| `$config.dat` 新增 5 个 `ECM_*` 全局量 | ❌ 未落地 | ✅ | 现场库 `$config.dat` 在 `ENDDAT` 前直接结束 |
| `sps.sub` 新增 4 行 `EKI_SetInt/SetReal`（发送 4 个新字段） | ❌ 未落地（现场库为 10/9 版，2961 B） | ✅（3317 B） | 仓库根 `sps.sub` = 包内版本 |
| 细磨负 PassNo 标记（`-(100+k)` / `-(110+k)`） | ❌ `XiMoAllPart1/2.src` 仍是 `PassNo = 0` | ✅（41 增 / 12 删） | 检测段 `MonRef` 两版都有 |
| 细磨检测结果上报 | ❌ | ✅（每段 6 行） | |
| 控制器侧 `EthernetKRL\ECMMonitor` 配置新增 4 个 `<ELEMENT>` | ❌ 文件不在工程库内 | ✅ `EKI/ECMMonitor.xml` | 需手工放到控制器并冷启动 |

### 7.2 上位机侧

| 项 | 状态 |
|---|---|
| 负数 PassNo 解码（粗磨）、焊缝面板、底色带、统计、CSV `Seam` 列 | ✅ 已实现（提交 `0d49513`） |
| 负数 PassNo 解码（**细磨 100 段**） | ❌ `config.h`/`station.cpp` 未覆盖 |
| 解析/显示 `WeldH/BaseH/Misalign/Rounds` | ❌ `Sample` 无字段、`protocol.cpp` 无分支 |
| `ECMMonitor.xml` 含 9 个 `<ELEMENT>` | ✅（根目录副本；`krl/EKI/ECMMonitor.xml` 是 `WeldH/Misalign` 记为 INT/µm 的旧注释版） |

### 7.3 现场"版本杂音"（建议确认后再改）

1. **`MinShi260921\CuMoAllPart1.src`(10/12) 与 `MinShi261010\CuMoAllPart1_2.src`(10/14) 并存**：两者是同一套逻辑的两代；`261010` 多了负 PassNo + `MonRef`。请确认现场实际调用的是哪一个。
2. **`Sspeed` "总启动"段的注释状态不一致**：
   - `MinShi260921\CuMoAllPart1.src` L350：`Sspeed = 8000` + `WAIT FOR Rotation_R>Sspeed-500` **生效**
   - `MinShi261010\CuMoAllPart1_2.src` L356：同上**被注释**
   - `MinShi260921\CuMoAllPart2.src` L349：**生效**；而仓库 `krl/MinShi260921/CuMoAllPart2.src` 快照里是注释
   → 若现场用 261010 版，主轴"总启动"可能不生效（需外部/HMI 给转速），请确认。
3. **`&ACCESS` 级别**：`261010\*_2.src` 是 `RVP`，`260921\*` 是 `RVO`；`$config.dat` 是 `RV$4`。改文件时保持原样，避免 WorkVisual 报访问级别不一致。
4. **`MinShiLib.src` 现场版比仓库快照多了 `PassNo = 0`**（`MoInitCu()`/`MoInitXi()` 各一行）——现场版更完整，快照待同步。
5. `Posoffset1-8`、`XP17/XP27/…`、`LCPDAT*6`、`Laser_GI*` 等**当前程序未引用**（历史遗留），改动时不用动。

---

## 8. 改动指引（按目标找位置）

### 8.1 只调工艺参数（不动逻辑）

| 想改什么 | 改哪里 |
|---|---|
| 单道进给量 | `$config.dat:907` `Ofs=30.0000`（及 `Ofs_set`） |
| 粗磨力/模式/负载 | `$config.dat:913-917` `Cu_ESforce_set / Cu_ESpos_set / ESmode_set / ESload_set` |
| 细磨力/起点 | 同上 `Xi_ESforce_set / Xi_ESpos_set` |
| 起始基准（EHA） | `$config.dat:1017` `FinalPos` |
| 测高补偿 | `$config.dat:1008-1015` `AVG1-8` |
| 钢制补偿 | `$config.dat:1039-1046` `GZoffset1-8` |
| 细磨位置限制基准 | `$config.dat:1023` `PosShaZhi` + `1029-1036` `AVGShaZhi1-8` |
| 测头基准（换测头/换盘） | `$config.dat:1003-1006` `L1BaseF/L2BaseF/L3BaseF` |
| 焊缝（工位）数量上限 | 上位机 `src/config.h` `SEAM_COUNT_MAX` + `station.cpp` 区间判断 |

### 8.2 新增/修改通讯字段（**必须三处齐改**）

1. `$config.dat`：声明全局量（放在 `USER GLOBALS` 折叠区，`ENDDAT` 之前）。
2. `sps.sub`：在 `EKI_Send` **之前**加 `EKI_SetInt/SetReal/SetBool("ECMMonitor","ECM/<Tag>", ...)`。
3. 控制器 `EthernetKRL\ECMMonitor\config.xml` 的 `<SEND><XML>` 加同名 `<ELEMENT Tag="ECM/<Tag>"/>`（**改完冷启动控制器**）。
4. 上位机：`src/protocol.h` 的 `Sample` 加字段 → `src/protocol.cpp` 加解析分支 → 需要显示再动 `mainwindow/chartview`。

> 只加 EKI 字段、不改上位机是**安全**的（`QXmlStreamReader` 会忽略未知标签）。

### 8.3 改焊缝（工位）标记语义

- 想加/改阶段号段（例如启用细磨 `-(100+k)`）：
  1. `src/config.h` 加常量；
  2. `src/station.cpp:decodePassNo()` 加区间判断与 `SeamPhase`；
  3. `tests/tst_station.cpp` 补用例；
  4. 相应 KRL 程序里把 `PassNo = 0` 换成新编码。
- 想让 Part2 也从 1 开始编号：把 `Part2_2.src` 的 `-5..-8` / `-15..-18` 改成 `-1..-4` / `-11..-14`。

### 8.4 编码与行尾（踩坑点）

| 文件 | 编码 | 行尾 |
|---|---|---|
| `$config.dat`、`*.src`、`*.dat`、`ECMMonitor.xml` | 纯 ASCII / UTF-8 无 BOM | **LF** |
| `sps.sub` | **GBK(ANSI)** | LF |

用 WorkVisual 打开编辑最稳；用外部编辑器时不要让工具加 BOM 或转成 CRLF。`krl/**` 在 `.gitattributes` 里已设 `-text`（关闭行尾转换）。

### 8.5 上线顺序（顺序错了会出假数据）

1. **先升级上位机 exe**（识别负数 PassNo 的版本）——旧版会把负数误当新道次，出现"第 −1 道"。
2. 控制器 EKI 配置 + **冷启动**。
3. 下载 KRL 程序（`$config.dat`、`sps.sub`、粗磨 `Part1_2/Part2_2`、细磨 `XiMoAllPart1/2`）。
4. 现场验证：
   - 抓帧应出现 `<WeldH>…</WeldH><BaseH>…</BaseH><Misalign>…</Misalign><Rounds>…</Rounds>`；
   - 粗磨组显示「粗磨 · 焊缝 k/8」，细磨组显示「细磨 · 焊缝 k/8」（**需先补上位机 100 段解码**）；
   - 4 段检测值与示教器 `Measure_Height*` / `Baseheight*` / `Cuobian*` / `TIMES*` 一致；
   - 一通完整流程跑完，"正常道次 / 补充次数"与操作员记录一致。

---

## 9. 其它文件速查（跟 MinShi 有关但不在主线上的）

| 文件 | 作用 | 与本系统的关系 |
|---|---|---|
| `Common\CalAngleFix.src` | 公共函数库：`ToSigned/ToUnSigned/Swap_ReadBytes/Swap_SetBytes/GI_TO_REAL/MOD/AngleFromTP/PlanZ/PlanZ2/ECMZ` | **强相关**：`ToSigned`（sps.sub、PN_AES）、`MOD()`（道次数取下整） |
| `Common\PN_AES.sub` | AES/EtherCAT 信号采样与写设定 | **强相关**（第 4.3 节） |
| `EHA\ECMSps.sub` | EHA 使能状态机 | **强相关**（第 4.4 节） |
| `henyibiaoding\BINKKS.sub` | 另一条 EKI 通道 `BINKKS`：12 ms 上报 `$POS_ACT_MES`/`$AXIS_ACT_MEAS`/`$PRO_MOVE`/`$ACT_BASE`… | 独立通道，**与 ECM 互不影响**（如有上位机连它，注意 PC/端口资源） |
| `henyibiaoding\PathNum1-6.*`、`EHACalib.*`、`qy0507.*` | 焊枪路径编号 / EHA 标定 / 取样程序 | 标定用；`Program\biaoding0512.src` 会置 `ECM_Enable=TRUE` 做测头标定 |
| `Program\biaoding0512.src`、`Program\gunbianbiaoding260701.src`、`masref_user.src` | 标定类 | 会动 `ECM_Enable`，排查"EHA 被谁使能"时要看 |
| `Program\mocahan\*`（`damohanfeng*`、`xixiao1215`、`jiguanggaodu0224`…） | 旧的手工打磨/测高程序 | **不参与**当前对接：全库 grep 显示 `RecordOn`/`PassNo`/`ESpos` 只出现在 `MinShi260901/260921/261010` 家族 + `System` 里 |
| `Program\zhu0123\*`、`Program\QY\*`、`Program\JiaoduPianyi\*` | 激光测高/取样/角度偏移实验 | 同上，仅历史参考 |
| `TP\EthernetKRL\EthernetKRL.src` | EKI 功能库源码（19 KB） | 底层库；`ECMMonitor` 通道配置**不在这里** |
| `TP\p00.src` | 程序号选择 `P00(#EXT_PGNO,…)` | `cell.src` 用它选程序号 |
| `cell.src` | CELL 模板（`SWITCH PGNO` 三个空 CASE） | **当前无有效调用点** → 谁调用 `CuMoAllPart*` 需现场确认 |

---

## 10. 待确认问题（建议逐条落定后再改）

1. 现场实际跑的是 `MinShi261010\*_2`（新）还是 `MinShi260921\*`（旧）？要不要把 `260921` 下的 All 版清掉以免误下载？
2. 谁按什么顺序调用 `Part1` → `Part2`（CELL 程序号 / 上层 / 人工）？`cell.src` 目前是空模板。
3. `261010\CuMoAllPart1_2.src` 里 `Sspeed = 8000` / `WAIT FOR Rotation_R>…` 被注释是**有意**还是**调试残留**？
4. 是否采纳 10-14 的 `ECM_*` 上报包？采纳则上位机必须同步加解析（第 8.2 节），否则只是白占带宽。
5. 上位机是否需要区分**粗磨/细磨**（细磨 100 段解码）并分别显示？
6. `$config.dat` 里 `PassNo=-14` 的上电初值是否改回 `0`？
7. 是否要按**焊缝**而不是"一次 RecordOn = 一组"来切分显示/导出？（当前是一组含 4 焊缝）
8. 单组数据量：4 工位 × 多道次 + 空移，`MAX_POINTS=200000` 裁剪会砍掉组头，是否要加大或按焊缝分包？
