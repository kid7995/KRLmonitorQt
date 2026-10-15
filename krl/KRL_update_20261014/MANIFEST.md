# 机器人侧改动包 — 2026-10-14

配套上位机分支：`codex/global-polish-program`　生成日期：2026-10-14

## 一、文件清单与目标位置

| 本包内文件 | 目标位置 | 改动 |
|---|---|---|
| `EKI/ECMMonitor.xml` | **控制器** `C:\KRC\ROBOTER\Config\User\Common\EthernetKRL\…`（原文件名覆盖，见第四节） | `<SEND>` 新增 4 个节点 `WeldH / BaseH / Misalign / Rounds` |
| `R1/System/$config.dat` | 工程库 `KRC\R1\System\$config.dat` | 新增 5 个全局：`ECM_Seam / ECM_WeldH / ECM_BaseH / ECM_Misalign / ECM_Rounds` |
| `R1/System/sps.sub` | 工程库 `KRC\R1\System\sps.sub` | `EKI_Send` 前新增 4 行 `EKI_SetInt` 发送检测结果 |
| `R1/MinShi261010/CuMoAllPart1_2.src` | 工程库 `KRC\R1\MinShi261010\` | 4 个检测段各新增 6 行（检测结果上报）；负 PassNo 标记为本包之前已改 |
| `R1/MinShi261010/CuMoAllPart2_2.src` | 同上 | 同上 |
| `R1/MinShi260921/XiMoAllPart1.src` | 工程库 `KRC\R1\MinShi260921\` | **细磨负 PassNo 标记（+100 段）** + 4 个检测段各新增 6 行 |
| `R1/MinShi260921/XiMoAllPart2.src` | 同上 | 同上 |

## 二、编码与行尾（务必保持）

| 文件 | 编码 | 行尾 |
|---|---|---|
| `$config.dat`、`sps.sub` | `sps.sub` 为 **GBK(ANSI)**，其余 **纯 ASCII** | 全部 **LF**（无 CRLF） |
| `*.src` | 纯 ASCII | LF |
| `ECMMonitor.xml` | **UTF-8 无 BOM** | LF |

用 U 盘/网络拷贝即可，**不要**用会改编码或加 BOM 的工具另存。

## 三、PassNo 编码约定（最终）

```
 PassNo = n > 0         打磨中：第 n 遍（补充打磨为跳增编号）
 PassNo = 0             未标记（兼容旧程序/旧 CSV）
 PassNo = -(k)          粗磨 检测阶段，焊缝 k        k = 1..8
 PassNo = -(10+k)       粗磨 打磨阶段·非打磨段，焊缝 k
 PassNo = -(100+k)      细磨 检测阶段，焊缝 k
 PassNo = -(110+k)      细磨 打磨阶段·非打磨段，焊缝 k
 PassNo = -(20+k) / -(120+k)  工位间转移（预留）
 PassNo = -99 / -199    收尾（预留）
```
焊缝号全局 1–8：Part1 = 1–4，Part2 = 5–8；粗磨 = 0 百位，细磨 = 1 百位。

## 四、EKI 配置说明

- 控制器路径（按你实际确认的布局覆盖同名文件）：
  `C:\KRC\ROBOTER\Config\User\Common\EthernetKRL\ECMMonitor\config.xml`
  或 `C:\KRC\ROBOTER\Config\User\Common\EthernetKRL\ECMMonitor.xml`
- **改完必须重新初始化 EKI**：冷启动控制器（或 `EKI_Close` → `EKI_Init` → `EKI_Open`），因为 `sps.sub` 的 `bEKI_Init` 只在启动后初始化一次。
- `<INTERNAL>` 无需改动（报文约 215 B，远小于默认缓冲）；若现场发现报文被截断，再加
  `<BUFFERING MODE="MESSAGE" SIZE="4096"/>`。

## 五、上线顺序

1. **先升级上位机 exe**（识别负数 PassNo 的版本，提交 `0d49513` 及以后）；
2. 控制器的 EKI 配置 + 冷启动；
3. 下载 KRL 程序（`$config.dat`、`sps.sub`、粗磨 Part1/2、细磨 Part1/2）；
4. 现场验证（见下）。

> 只改 EKI 配置、不改上位机是安全的：上位机会忽略未知标签，功能不退化。

## 六、验证清单

1. 抓帧应出现：`<WeldH>…</WeldH><BaseH>…</BaseH><Misalign>…</Misalign><Rounds>…</Rounds>`。
2. 粗磨组：界面显示「粗磨 · 焊缝 k/8」；细磨组：「细磨 · 焊缝 k/8」。
3. 4 段检测值与示教器上 `Measure_Height*` / `Baseheight*` / `Cuobian*` / `TIMES*` 一致。
4. 一通完整流程跑完，"正常道次 / 补充次数"与操作员记录一致。