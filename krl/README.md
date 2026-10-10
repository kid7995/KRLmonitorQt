# KRL 侧程序参考快照

本目录存放 **KUKA（KRL）侧打磨程序**的只读参考快照，用于 KRLmonitor 上位机的对接开发。

- **来源**：WorkVisual 工程库 `8141797(PCRC-FURFL4R2R5)`，`KRC\R1\`
- **快照日期**：2026-10-10
- **说明文档**：见 `../docs/KRL_GLOBAL_PROGRAM.md`（程序结构、时序语义、通讯字段映射、上位机待办）

## 目录内容

| 路径 | 内容 |
|---|---|
| `MinShi261010/` | **全局打磨程序（已接入 PassNo 负数标记，可回拷 WorkVisual）**，见该目录 README |
| `MinShi261010/original/` | 上述程序的原始上传基线（对比 / 回退用） |
| `MinShi260921/CuMoAllPart1.src` `.dat` | 上一版全局粗磨程序，工位 1–4（历史快照） |
| `MinShi260921/CuMoAllPart2.src` `.dat` | 全局粗磨程序，工位 5–8（点号 P5x–P8x） |
| `MinShi260921/CuMo1.src` | 单工位样例（独立版），用于与 All 版对照 |
| `MinShi260921/MinShiLib.src` | `MoInitCu()` / `MoInitXi()` 公共初始化 |
| `System/sps.sub` | **EKI 发送程序（通讯核心）**：通道 `ECMMonitor`，字段 `ECM/PosR|ForceR|RecordOn|PassNo|EsPos` |
| `excerpts/config_dat_ECM_declarations.txt` | `$config.dat` 第 832–1010 行：ECM/MINSHI 声明与 EtherCAT 信号定义 |
| `excerpts/CalAngleFix_ToSigned.txt` | `ToSigned()` / `ToUnSigned()` 定义（`CalAngleFix.src` 第 124–149 行） |

## 约定

1. 本目录**只读**：这些文件是现场程序的快照，不要在这里修改；现场改动请回到 WorkVisual 工程库，再重新同步快照。
2. 文件保持**原始字节**（`.src`/`.dat`/`.sub` 按原编码与行尾保存），`.gitattributes` 中已对本目录设置 `-text`，Git 不做行尾转换。
3. 未收录的完整文件（`$config.dat` 全文、`CuMo1`…`CuMo8`、`XiMo1_1`…`XiMo8_1`、`CalAngleFix.src` 全文等）仍在上述 WorkVisual 工程库中，需要时再按需补充。