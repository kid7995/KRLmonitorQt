# KRLmonitor

KRLmonitor 是一个用于接收 KUKA EKI/ECM 数据并实时显示 `PosR`、`ForceR`、`EsPos` 和道次信息的 Qt 桌面程序。程序按 `RecordOn` 边沿自动建组、封组，并支持 CSV 保存、加载和叠道对比。

## 主要功能

- TCP 接收 KUKA 发送的 `<ECM>...</ECM>` XML 数据帧
- `RecordOn` 上升沿开始记录，下降沿结束记录
- **检测段结果**：焊缝高度 / 基材高度 / 错边量 / 计划轮次（KRL 侧上报，见下）
- **焊缝（工位）chip 栏**：只显示"几号焊缝"，默认全选，点击取消 → 曲线只留选中焊缝
- **拆分视图**：按焊缝 / 按阶段（检测 / 打磨 / 间隙）/ 按工序（粗磨 / 细磨）过滤
- **自动分件**：按规则把连续生产切分成"件"，可手工切分与按件续号
- **自动存档**：每个周期封组后按时间（到分钟）自动存为 `.ecmb` 二进制（压缩 + 混淆）
- 数据组上限 64 组；内存按"保留最近 N 组"策略回收已落盘的组
- PosR / ForceR 双 Y 轴显示
- 道次分割、返回段识别、叠道对比
- ESpos 阶梯线和跟随误差显示
- 滚轮缩放、拖拽平移、双击重置、悬停读值
- CSV 导入和导出，兼容旧版三列格式；`.ecmb` 二进制读写

## 目录结构

```text
main.cpp                 程序入口，保留在根目录
src/                     项目源码（.h / .cpp）
resources/app.rc         Windows EXE 图标资源
fig/logo.ico             EXE 图标源文件
tests/                   Qt Test / CTest 单元测试
docs/DEPLOYMENT.md       Windows 部署说明
scripts/deploy-qt.ps1    windeployqt 一键打包脚本
```

## 构建环境

- Windows 10/11
- CMake 3.16 或更高版本
- Visual Studio 2019/2022，安装“使用 C++ 的桌面开发”
- Qt 5.15 或 Qt 6，包含 `Widgets`、`Network`、`Test` 组件

当前工程已验证组合：Qt 5.15.2 + MSVC 2019 64 bit + Visual Studio 16 2019。

## 配置和构建

在 PowerShell 中执行：

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64"

cmake --build build --config Release --parallel
```

如果使用 Qt 6，将 `CMAKE_PREFIX_PATH` 指向对应 Qt 6 安装目录。

Windows 构建时会自动将 `fig/logo.ico` 嵌入 `KRLmonitor.exe` 作为文件图标。

生成的可执行文件位于：

```text
build/Release/KRLmonitor.exe
```

## 运行测试

测试默认随 CMake 构建启用。执行：

```powershell
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

当前测试覆盖：

- `tst_protocol`：XML 字段解析、有符号力值、必需字段校验
- `tst_model`：道次切分和零值返回段
- `tst_decimate`：小数据直通、min/max 保留、缓存增量与失效
- `tst_csvio`：CSV 往返、旧版三列格式、错误表头
- `tst_recorder`：RecordOn 建组/封组、道次收尾、完成后暂停、组数上限

只构建程序、不构建测试时，可使用：

```powershell
cmake -S . -B build -DBUILD_TESTING=OFF
```

## 数据协议

KUKA 侧连接本机 TCP `65002` 端口，并发送以下形式的帧：

```xml
<ECM>
  <PosR>12345</PosR>
  <ForceR>-12</ForceR>
  <RecordOn>1</RecordOn>
  <PassNo>3</PassNo>
  <EsPos>12400</EsPos>
  <WeldH>1852</WeldH>     <!-- 检测段：焊缝高度，µm（=Measure_Height×1000） -->
  <BaseH>1214</BaseH>     <!-- 检测段：基材高度（=Baseheight，与 EsPos 同量纲） -->
  <Misalign>-120</Misalign><!-- 检测段：错边量，µm 可负（=Cuobian×1000） -->
  <Rounds>12</Rounds>     <!-- 检测段：计划打磨轮次（=TIMES） -->
</ECM>
```

`PosR` 和 `ForceR` 是必需字段。其他字段缺失时分别按 `RecordOn=false`、`PassNo=0`、`EsPos=0` 处理。
后 4 个是检测结果，**缺失时界面显示"—"**（旧程序 / 未升级的 EKI 配置不会误判成 0）。

CSV 列格式：

```text
time_s,PosR,ForceR,PassNo,EsPos,Seam
```

### 曲线存档格式 `.ecmb`

自定义二进制格式：`magic + 版本 + 元数据(JSON) + 段表 + 列式数据 + CRC32`，
默认 zlib 压缩 + 密钥流混淆（只能由本软件打开，可用设置关闭）。
体积约为同内容 CSV 的 1/10。文件名默认 `{time}_{part}_{craft}_{seams}`，例如：

```text
2026-10-14_16-52_件0007_粗磨_S1-4.ecmb
```

### 连续生产

按 `RecordOn` 周期封组并自动落盘；"件"的划分规则（设置里可切换，默认最确定的"一次程序运行 = 一件"）：

| 规则 | 含义 |
|---|---|
| 一次程序运行 = 一件（默认） | 不依赖任何推断，每个 `RecordOn` 周期一件 |
| 细磨回到粗磨 = 新件 | 适用于"一件 = 粗磨 1-8 + 细磨 1-8" |
| 焊缝号回绕 = 新件 | 适用于"一个工序内做完一件的所有焊缝" |

任何规则下都保留两个兜底：**"✂ 新件"按钮**手工切分、**超时切分**（默认 30 分钟无数据算新件）。
断线时会强制封存当前周期并标记"数据可能不完整"，重连后一定新建组，不会把两件并成一组。

## 通讯故障排查（现场最常遇到的三件事）

工具栏右侧有一个**常驻连接指示灯**，三种状态直接对应三种问题：

| 指示灯 | 含义 | 处理 |
|---|---|---|
| `🔌 端口 65002 ✗ 监听失败` | 端口被占用或权限被拦（例如还开着旧版监控程序 / Python 脚本） | 关掉占用端口的程序后重启本软件 |
| `🔌 端口 65002 ○ 等待 KUKA 连接　本机 192.168.1.100` | 本机在监听，KUKA 没连过来 | ① 控制器 `EthernetKRL\ECMMonitor\config.xml` 的 `<IP>` 必须**等于这里显示的本机地址**；② 检查机器人网口与 PC 是否同网段；③ 检查控制器的 EKI 是否初始化失败（见下） |
| `🔌 端口 65002 ● KUKA 已连接` | 链路正常 | 数据在跑 |

每次启动会在 exe 同目录写一行 `KRLmonitor.log`（监听是否成功、错误原因、本机 IP、关键配置），
现场排查时先看这一行，比猜快得多。

**命令行参数**：`KRLmonitor.exe --arm` = 忽略设置强制进入监听（做开机自启/快捷方式时方便）。

**控制器侧两个易踩的点**：

1. `config.xml` 必须放在 `C:\KRC\ROBOTER\Config\User\Common\EthernetKRL\ECMMonitor\config.xml`（通道名目录 + `config.xml` 文件名），放到别处 `EKI_Init` 会失败。
2. `sps.sub` 里 `bEKI_Init` 只初始化一次；若开机那一刻初始化失败，**整场会话都不会再重试**，必须冷启动控制器。若现场偶发连不上，建议把它改成"未连上（`$FLAG[10]` 为 FALSE）时限次重试初始化"。

### PassNo 负数编码（焊缝 / 工位）

KRL 侧全局打磨程序（`MinShi261010` 起）用 `PassNo` 的符号与区间表达"阶段 + 焊缝号"：

```text
 PassNo = n > 0        打磨中：第 n 遍（补充打磨 = 编号跳增）
 PassNo = 0            未标记（旧程序 / 旧 CSV）
 PassNo = -(k)         检测阶段：正在检测焊缝 k        k = 1..8
 PassNo = -(10 + k)    打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
 PassNo = -(20 + k)    工位间转移（预留）
 PassNo = -99          收尾（预留）
```

上位机据此在"焊缝（工位）面板"中按焊缝显示实时数据、当前遍数与正常/补充打磨次数；
旧程序（`PassNo` 全为非负数）自动退回无焊缝标记的显示方式。
详见 [docs/KRL_GLOBAL_PROGRAM.md](docs/KRL_GLOBAL_PROGRAM.md) 与 [krl/MinShi261010/README.md](krl/MinShi261010/README.md)。

## KRL 侧打磨程序

KRLmonitor 对接的机器人侧打磨程序（含全局打磨程序 `CuMoAllPart1/Part2`）快照见
[`krl/`](krl/README.md)，其程序结构、时序语义、`ECM` 通讯字段映射与上位机待办见
[docs/KRL_GLOBAL_PROGRAM.md](docs/KRL_GLOBAL_PROGRAM.md)。

## 部署

程序构建完成后不能只复制 `KRLmonitor.exe`，还需要 Qt 运行库、平台插件和 MSVC 运行库。完整步骤见：

- [Windows 部署说明](docs/DEPLOYMENT.md)

快速打包：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/deploy-qt.ps1 `
  -QtBin "C:\Qt\5.15.2\msvc2019_64\bin" -Zip
```

输出目录默认为 `dist/KRLmonitor-Release`，加 `-Zip` 后同时生成 ZIP。
