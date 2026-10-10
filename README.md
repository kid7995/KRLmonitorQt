# KRLmonitor

KRLmonitor 是一个用于接收 KUKA EKI/ECM 数据并实时显示 `PosR`、`ForceR`、`EsPos` 和道次信息的 Qt 桌面程序。程序按 `RecordOn` 边沿自动建组、封组，并支持 CSV 保存、加载和叠道对比。

## 主要功能

- TCP 接收 KUKA 发送的 `<ECM>...</ECM>` XML 数据帧
- `RecordOn` 上升沿开始记录，下降沿结束记录
- 最多同时管理 4 组数据
- PosR / ForceR 双 Y 轴显示
- 道次分割、返回段识别、叠道对比
- ESpos 阶梯线和跟随误差显示
- 滚轮缩放、拖拽平移、双击重置、悬停读值
- CSV 导入和导出，兼容旧版三列格式

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
</ECM>
```

`PosR` 和 `ForceR` 是必需字段。其他字段缺失时分别按 `RecordOn=false`、`PassNo=0`、`EsPos=0` 处理。

CSV 列格式：

```text
time_s,PosR,ForceR,PassNo,EsPos
```

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