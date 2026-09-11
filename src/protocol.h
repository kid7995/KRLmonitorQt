#pragma once

#include <QString>

// 一帧采样（含 PC 接收时刻 ts）
struct Sample {
    double ts = 0.0;
    int pos = 0;
    int force = 0;
    bool recordOn = false;
    int passNo = 0;
    int esPos = 0;
};

// 解析一帧 <ECM>…</ECM>（不含 ts），失败返回 false。
// 字段格式由 KRL 侧 ECMMonitor.xml 的 <SEND> 块定义。
bool parsePacket(const QString& xml, Sample& out);
