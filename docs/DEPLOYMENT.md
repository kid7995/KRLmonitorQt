# Windows 部署说明

本文档说明如何将 KRLmonitor 部署到未安装 Qt SDK 的 Windows 工控机。推荐使用 Qt 自带的 `windeployqt` 收集运行依赖，不要只复制 `KRLmonitor.exe`。

## 1. 构建 Release 版本

在源码根目录打开 PowerShell：

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/5.15.2/msvc2019_64"

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

确认测试通过，且以下文件存在：

```text
build/Release/KRLmonitor.exe
```

如果使用 Qt 6，请将 `CMAKE_PREFIX_PATH` 和后续 `windeployqt` 路径替换为对应 Qt 6 目录。

## 2. 一键生成部署目录

工程提供脚本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/deploy-qt.ps1 `
  -QtBin "C:\Qt\5.15.2\msvc2019_64\bin" `
  -Configuration Release `
  -Zip
```

默认输出：

```text
dist/KRLmonitor-Release/
dist/KRLmonitor-Release.zip
```

脚本会执行以下操作：

1. 检查 Release 可执行文件和 `windeployqt.exe`
2. 复制 `KRLmonitor.exe`
3. 自动定位 Visual Studio，并运行 `windeployqt --release --compiler-runtime --no-translations`
4. 尽可能携带 `vc_redist.x64.exe`，并写入 `运行说明.txt`
5. 检查 `platforms/qwindows.dll` 是否存在
6. 可选生成 ZIP 压缩包

如果部署目录已经存在，脚本会拒绝覆盖。确认旧目录可以删除后，可增加 `-Force`。

## 3. 手工部署

不使用脚本时，可执行：

```powershell
New-Item -ItemType Directory -Force -Path dist\KRLmonitor-Release

Copy-Item build\Release\KRLmonitor.exe `
  dist\KRLmonitor-Release\KRLmonitor.exe

C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe `
  --release `
  --compiler-runtime `
  --no-translations `
  dist\KRLmonitor-Release\KRLmonitor.exe
```

部署目录应至少包含：

```text
KRLmonitor-Release/
├─ KRLmonitor.exe
├─ Qt5Core.dll
├─ Qt5Gui.dll
├─ Qt5Widgets.dll
├─ Qt5Network.dll
├─ platforms/
│  └─ qwindows.dll
└─ styles/
   └─ qwindowsvistastyle.dll
```

Qt 版本或构建配置不同，实际 DLL 名称可能不同；以 `windeployqt` 生成结果为准。

## 4. 安装到目标机

建议将完整目录复制到固定路径，例如：

```text
D:\KRLmonitor\
```

不要只复制 EXE。目标机不需要安装 Qt SDK，但必须保持 DLL、`platforms` 和 `styles` 目录的相对结构不变。

> 注意：`windeployqt --compiler-runtime` 需要能从当前终端定位 Visual Studio。
> 如果输出 `Cannot find Visual Studio installation directory, VCINSTALLDIR is not set`，
> 它仍会完成 Qt 文件部署，但不会复制 `vcruntime140.dll`、`msvcp140.dll`。

如果目标机缺少 MSVC 运行库，可任选一种方式：

- 使用脚本或 `windeployqt --compiler-runtime` 自动复制运行库
- 在目标机安装对应的 Microsoft Visual C++ Redistributable

## 5. 网络和 KUKA 配置

KRLmonitor 默认监听：

```text
协议：TCP
端口：65002
地址：0.0.0.0
```

部署后需要：

1. 确认目标 PC 使用固定 IP。
2. 在 Windows 防火墙中允许 `KRLmonitor.exe` 或开放入站 TCP 65002。
3. 将 KUKA 侧 `ECMMonitor.xml` 的 `<EXTERNAL><IP>` 配置为该 PC 的实际 IP。
4. 确认 KUKA 与 PC 网络互通，且没有其他程序占用 65002。

可在 PowerShell 中检查端口：

```powershell
Get-NetTCPConnection -LocalPort 65002 -State Listen
```

检查目标 PC 到机器人控制器的连通性：

```powershell
Test-NetConnection <KUKA_IP> -Port <KUKA_EKI_PORT>
```

`<KUKA_IP>` 和 `<KUKA_EKI_PORT>` 应按现场配置替换。

## 6. 部署验证

在目标机按以下顺序检查：

1. 双击 `KRLmonitor.exe`，窗口正常出现且状态栏显示端口监听。
2. KUKA 建立连接后，状态栏显示已连接。
3. 未触发记录时能看到灰色实时预览。
4. 触发 `RecordOn` 后出现新的“打磨#N”数据组。
5. 记录结束后可保存 CSV，并能重新载入。
6. 缩放、平移、道次分割和叠道对比正常。

## 7. 常见问题

### 提示缺少 Qt5Core.dll 或 Qt5Widgets.dll

没有运行 `windeployqt`，或只复制了 EXE。重新执行部署脚本。

### 提示无法加载平台插件 windows

缺少 `platforms/qwindows.dll`，或该目录未被一起复制。确认部署目录结构完整。

### 程序启动后立即退出

可在 PowerShell 中运行 EXE 查看错误信息：

```powershell
.\KRLmonitor.exe
```

同时检查是否缺少 MSVC 运行库。

### 状态栏一直显示监听失败

检查 65002 是否被占用：

```powershell
Get-NetTCPConnection -LocalPort 65002
```

如果被其他进程占用，需要关闭该进程或修改 `config.h` 中的 `LISTEN_PORT` 后重新构建。

### KUKA 无法连接

检查以下项目：

- KUKA 侧 IP 是否填写正确
- Windows 防火墙是否允许入站 TCP 65002
- PC 和控制器是否在同一可达网络
- 现场交换机或 VLAN 是否阻止该端口
- KRL 程序是否已启动 EKI 数据发送

## 8. 发布检查清单

- [ ] Release 构建成功
- [ ] CTest 全部通过
- [ ] `windeployqt` 成功执行
- [ ] `platforms/qwindows.dll` 存在
- [ ] 在无 Qt SDK 的干净机器上启动成功
- [ ] TCP 65002 监听正常
- [ ] KUKA 能连接并发送数据
- [ ] 记录、保存和重新加载流程验证通过