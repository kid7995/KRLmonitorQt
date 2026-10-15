#pragma once

#include <QString>

#include "model.h"

// ── KRLmonitor 曲线存档格式（.ecmb = ECM Binary）────────────────────────
//
// 目的：把一组曲线以"比 CSV 小一个数量级"的体积存盘，并且只能由本软件打开。
//
// 文件布局（全部小端）：
//   0   magic  "ECMB"
//   4   u16    version        （当前 1）
//   6   u16    flags          bit0 = zlib 压缩，bit1 = 混淆
//   8   u32    metaLen        （元数据 JSON 的字节数，不压缩）
//   12  u32    segCount
//   16  u32    pointCount
//   20  u32    payloadLen     （落盘 payload 的字节数，压缩/混淆之后）
//   24  u32    crc32          （落盘 payload 的 CRC32，用于完整性校验）
//   28  u32    nonce          （混淆密钥流用；未混淆时为 0）
//   32  meta JSON             （UTF-8，metaLen 字节）
//   …   payload               （段表 + 列式数据，payloadLen 字节）
//
// payload（压缩/混淆之前）：
//   段表   segCount × 24B：i0 u32 | i1 u32 | kind u8 | craft u8 | seam u8 | pad u8
//                          | passNo i16 | retry u8 | pad u8 | t0 f32 | t1 f32
//   数据列 pointCount 逐列连续存放：t(f32) | pos(i32) | force(i16) | passNo(i16) | esPos(i32)
//
// 说明：混淆是"自定义密钥流 XOR"（SHA-256 计数器模式），目的是防止被文本/通用工具直接读取，
// 不是强度保证的加密；需抗破解请换 AES。

struct EcmFileMeta {
    QString groupName;      // 组名（如 "件0007 粗磨 S1-4"）
    QString partLabel;      // 件号标签（可为空）
    int partId = 0;
    bool incomplete = false;   // 该组是"断线/异常"强制封存的，数据可能不完整
    bool hasEs = false;
};

struct EcmSaveOptions {
    bool compress = true;      // zlib 压缩（强烈建议开）
    bool obfuscate = true;     // 混淆（本软件才能读）
};

// 存盘。返回 false 时 err 给出原因。
bool saveEcm(const QString& path, const DataGroup& g, const EcmFileMeta& meta,
             const EcmSaveOptions& opt, QString& err);

// 读取。成功时填充 g / meta。
bool loadEcm(const QString& path, DataGroup& g, EcmFileMeta& meta, QString& err);

// 只读头部元数据（用于列表/预览，不解码曲线）
bool peekEcm(const QString& path, EcmFileMeta& meta, QString& err);
