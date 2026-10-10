# MinShi261010 —— 全局打磨程序（已接入 PassNo 负数标记）

本目录是 **KUKA 侧全局打磨程序**的落地版本：逻辑改动已完成，可直接回拷 WorkVisual 后编译、下载到 KRC。

- **来源**：WorkVisual 工程库 `8141797(PCRC-FURFL4R2R5)`，`KRC\R1\MinShi261010\`
- **原始上传版**：见 `original/`（未改动的基线，用于对比与回退）
- **改动日期**：2026-10-10
- **编码/行尾**：纯 ASCII + LF，与原文件一致（`krl/**` 在 `.gitattributes` 中已关闭行尾转换）

## 文件

| 文件 | 说明 |
|---|---|
| `CuMoAllPart1_2.src` | 全局粗磨程序，焊缝（工位）1–4 —— **已改** |
| `CuMoAllPart2_2.src` | 全局粗磨程序，焊缝（工位）5–8 —— **已改** |
| `CuMoAllPart1_2.dat` / `CuMoAllPart2_2.dat` | 点位/运动数据 —— 未改动 |

## 改动内容（每个焊缝段落）

| 位置 | 原来 | 改为 | 含义 |
|---|---|---|---|
| 检测段开头 `PassNo = 0` | `0` | `PassNo = -k` | 正在检测焊缝 k |
| 检测段 `ESpos = InitLen<k>` 之后 | （无） | 新增 `MonRef = ESpos` | 把检测结果（基准高度 InitLen）随 `ECM/EsPos` 上传 |
| 打磨段 `;---- Grinding station k ----` 之后 | （无） | 新增 `PassNo = -(10+k)` | 进入焊缝 k 的打磨阶段（含接近/等待） |
| 打磨段内各处 `PassNo = 0` | `0` | `PassNo = -(10+k)` | 道间返回 / 等待，归属焊缝 k |
| `;---- Move to station k ... ----` 之后 | （无） | 新增 `PassNo = -(10+k)` | 工位间转移，归属焊缝 k |
| `PassNo = jN`（第几遍） | 不变 | 不变 | 打磨遍数 |
| `PassNo = TIMES+1+RetryCount+10`（补充打磨） | 不变 | 不变 | 编号跳增即补充打磨 |

统计：Part1 = 33 行新增 / 24 行替换；Part2 相同。

`&ACCESS` / `&PARAM` / `&REL` 等模块头**未改动**。

## PassNo 编码约定（与上位机一致）

```
 PassNo = n > 0          打磨中：第 n 遍（补充打磨为跳增编号）
 PassNo = 0              未标记（兼容旧程序 / 旧 CSV）
 PassNo = -(k)           检测阶段：正在检测焊缝 k        k = 1..8
 PassNo = -(10 + k)      打磨阶段·非打磨段：焊缝 k 的道间返回 / 等待 / 接近
 PassNo = -(20 + k)      工位间转移：正在转移到焊缝 k（预留，当前程序未使用）
 PassNo = -99            程序收尾（预留）
```

焊缝号 **1–8 全局编号**：Part1 = 1–4，Part2 = 5–8。
（若现场希望两个 Part 各自从 1 编号，只需把 Part2 的 `-5..-8` / `-15..-18` 改成 `-1..-4` / `-11..-14`。）

## 回拷到 WorkVisual

把 `CuMoAllPart1_2.src`、`CuMoAllPart2_2.src` 覆盖到：

```
C:\Users\<用户>\Documents\WorkVisual 6.0\Repositories\8141797(PCRC-FURFL4R2R5)\KRC\R1\MinShi261010\
```

然后在 WorkVisual 中编译、下载到 KRC。

## 上线顺序（重要）

1. **先升级上位机（KRLmonitor）**：旧版本收到负数 `PassNo` 会把负值误当成新道次（出现"第 -1 道"的假道次），因此必须先换新版本。
2. 再下载本 KRL 程序到 KRC。
3. 需要回退时：KRL 换回 `original/` 的版本即可，上位机自动退回"无焊缝标记"的旧行为，无需再换 exe。

## 现场验证

1. 单帧自检：把任意一处 `PassNo = -k` 临时改成 `-1`，确认上位机数值栏出现「焊缝 1/8 · 检测」且抓帧里是 `<PassNo>-1</PassNo>`。
2. 完整跑一次 Part1：上位机应依次点亮焊缝 1–4 四张卡片，"正常道次 / 补充次数"与操作员记录一致。