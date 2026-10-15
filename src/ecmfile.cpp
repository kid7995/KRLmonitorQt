#include "ecmfile.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace {

const char kMagic[4] = {'E', 'C', 'M', 'B'};
constexpr quint16 kVersion = 1;
constexpr quint16 kFlagCompress = 0x0001;
constexpr quint16 kFlagObfuscate = 0x0002;
constexpr int kHeaderSize = 32;
constexpr int kSegStride = 24;

// 混淆口令：改了它，旧文件就读不出来了 —— 不要随意改。
const char kPassPhrase[] = "KRLmonitor.ecmb.v1";

quint32 crc32Of(const QByteArray& data) {
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < data.size(); ++i) {
        crc = table[(crc ^ quint8(data[i])) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// 密钥流：SHA-256(口令 || nonce || 块号)，逐块 XOR。可逆，同一个 nonce 即可还原。
QByteArray xorKeystream(const QByteArray& in, quint32 nonce) {
    QByteArray out = in;
    const int n = out.size();
    int off = 0;
    quint32 block = 0;
    while (off < n) {
        QByteArray seed(kPassPhrase);
        QDataStream ds(&seed, QIODevice::Append);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds << nonce << block;
        const QByteArray ks = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
        const int take = qMin(ks.size(), n - off);
        for (int i = 0; i < take; ++i) {
            out[off + i] = char(quint8(out[off + i]) ^ quint8(ks[i]));
        }
        off += take;
        ++block;
    }
    return out;
}

QString craftToStr(Craft c) {
    switch (c) {
    case Craft::Rough: return QStringLiteral("Rough");
    case Craft::Fine:  return QStringLiteral("Fine");
    case Craft::Unknown: break;
    }
    return QStringLiteral("Unknown");
}

Craft craftFromStr(const QString& s) {
    if (s == QLatin1String("Rough")) return Craft::Rough;
    if (s == QLatin1String("Fine"))  return Craft::Fine;
    return Craft::Unknown;
}

QByteArray buildMetaJson(const DataGroup& g, const EcmFileMeta& meta) {
    QJsonObject root;
    root["name"] = g.name.isEmpty() ? meta.groupName : g.name;
    root["partLabel"] = meta.partLabel;
    root["partId"] = meta.partId;
    root["incomplete"] = meta.incomplete || g.incomplete;
    root["hasEs"] = g.hasEs;
    root["points"] = g.times.size();

    QJsonArray seams;
    for (const SeamStat& s : g.seam.seams) {
        QJsonObject o;
        o["seam"] = s.seam;
        o["detected"] = s.detected;
        o["detectEs"] = s.detectEs;
        o["hasValues"] = s.hasValues;
        o["weldHum"] = s.weldHum;
        o["baseH"] = s.baseH;
        o["misalignUm"] = s.misalignUm;
        o["rounds"] = s.rounds;
        o["craft"] = craftToStr(s.craft);
        o["normalN"] = s.normalN;
        o["retryN"] = s.retryN;
        o["curPass"] = s.curPass;
        o["pts"] = s.pts;
        o["t0"] = s.t0;
        o["t1"] = s.t1;
        o["i0"] = s.i0;
        o["i1"] = s.i1;
        seams.append(o);
    }
    root["seams"] = seams;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray buildPayload(const DataGroup& g) {
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::LittleEndian);

    const QVector<Segment>& segs = g.seam.segments;
    for (const Segment& s : segs) {
        ds << quint32(s.i0) << quint32(s.i1);
        ds << quint8(s.kind) << quint8(s.craft) << quint8(quint32(s.seam) & 0xFF) << quint8(0);
        ds << qint16(s.passNo) << quint8(s.retry ? 1 : 0) << quint8(0);
        ds << float(s.t0) << float(s.t1);
    }
    const int n = g.times.size();
    for (int i = 0; i < n; ++i) ds << float(g.times[i]);
    for (int i = 0; i < n; ++i) ds << qint32(g.pos[i]);
    for (int i = 0; i < n; ++i) ds << qint16(g.force[i]);
    for (int i = 0; i < n; ++i) ds << qint16(g.passNo[i]);
    for (int i = 0; i < n; ++i) ds << qint32(g.esPos[i]);
    return out;
}

bool parsePayload(const QByteArray& raw, quint32 segCount, quint32 pointCount,
                  DataGroup& g, QString& err) {
    QDataStream ds(raw);
    ds.setByteOrder(QDataStream::LittleEndian);

    g.seam.segments.clear();
    for (quint32 i = 0; i < segCount; ++i) {
        Segment s;
        quint32 i0 = 0, i1 = 0;
        quint8 kind = 0, craft = 0, seam = 0, pad = 0, retry = 0;
        qint16 passNo = 0;
        float t0 = 0.f, t1 = 0.f;
        ds >> i0 >> i1 >> kind >> craft >> seam >> pad >> passNo >> retry >> pad >> t0 >> t1;
        s.i0 = int(i0);
        s.i1 = int(i1);
        s.kind = SeamPhase(kind);
        s.craft = Craft(craft);
        s.seam = int(seam);
        s.passNo = int(passNo);
        s.retry = (retry != 0);
        s.t0 = double(t0);
        s.t1 = double(t1);
        g.seam.segments.append(s);
    }

    g.times.clear(); g.pos.clear(); g.force.clear(); g.passNo.clear(); g.esPos.clear();
    const int n = int(pointCount);
    g.times.reserve(n); g.pos.reserve(n); g.force.reserve(n);
    g.passNo.reserve(n); g.esPos.reserve(n);
    for (int i = 0; i < n; ++i) { float v = 0.f; ds >> v; g.times.append(double(v)); }
    for (int i = 0; i < n; ++i) { qint32 v = 0; ds >> v; g.pos.append(int(v)); }
    for (int i = 0; i < n; ++i) { qint16 v = 0; ds >> v; g.force.append(int(v)); }
    for (int i = 0; i < n; ++i) { qint16 v = 0; ds >> v; g.passNo.append(int(v)); }
    for (int i = 0; i < n; ++i) { qint32 v = 0; ds >> v; g.esPos.append(int(v)); }

    if (ds.status() != QDataStream::Ok || g.times.size() != n) {
        err = QStringLiteral("数据区损坏或不完整");
        return false;
    }

    // 道次表由段表推导（不单独存盘，避免两份数据不一致）
    g.passes.clear();
    for (const Segment& s : g.seam.segments) {
        if (s.kind != SeamPhase::Grind || s.passNo <= 0) {
            continue;
        }
        Pass p;
        p.no = s.passNo;
        p.seam = s.seam;
        p.phase = SeamPhase::Grind;
        p.retry = s.retry;
        p.t0 = s.t0;
        p.t1 = s.t1;
        p.i0 = s.i0;
        p.i1 = s.i1;
        g.passes.append(p);
    }
    return true;
}

void applyMetaJson(const QByteArray& json, DataGroup& g, EcmFileMeta& meta) {
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    g.name = root.value("name").toString();
    g.hasEs = root.value("hasEs").toBool();
    g.incomplete = root.value("incomplete").toBool();
    meta.groupName = g.name;
    meta.partLabel = root.value("partLabel").toString();
    meta.partId = root.value("partId").toInt();
    meta.incomplete = g.incomplete;
    meta.hasEs = g.hasEs;

    g.seam.seams.clear();
    const QJsonArray seams = root.value("seams").toArray();
    for (const QJsonValue& v : seams) {
        const QJsonObject o = v.toObject();
        SeamStat s;
        s.seam = o.value("seam").toInt();
        s.detected = o.value("detected").toBool();
        s.detectEs = o.value("detectEs").toInt();
        s.hasValues = o.value("hasValues").toBool();
        s.weldHum = o.value("weldHum").toInt();
        s.baseH = o.value("baseH").toInt();
        s.misalignUm = o.value("misalignUm").toInt();
        s.rounds = o.value("rounds").toInt();
        s.craft = craftFromStr(o.value("craft").toString());
        s.normalN = o.value("normalN").toInt();
        s.retryN = o.value("retryN").toInt();
        s.curPass = o.value("curPass").toInt();
        s.pts = o.value("pts").toInt();
        s.t0 = o.value("t0").toDouble();
        s.t1 = o.value("t1").toDouble();
        s.i0 = o.value("i0").toInt();
        s.i1 = o.value("i1").toInt(-1);
        s.live = false;
        s.retryNow = s.retryN > 0;
        g.seam.seams.append(s);
    }
    g.seam.curSeam = 0;
    g.seam.curPhase = SeamPhase::None;
    g.seam.curCraft = Craft::Unknown;
    g.seam.refreshLive();
}

bool readHeader(QFile& f, quint16& version, quint16& flags, quint32& metaLen,
                quint32& segCount, quint32& pointCount, quint32& payloadLen,
                quint32& crc, quint32& nonce, QString& err) {
    const QByteArray head = f.read(kHeaderSize);
    if (head.size() != kHeaderSize || qstrncmp(head.constData(), kMagic, 4) != 0) {
        err = QStringLiteral("不是 KRLmonitor 曲线文件（ECMB）或文件已损坏");
        return false;
    }
    QDataStream ds(head);
    ds.setByteOrder(QDataStream::LittleEndian);
    char magic[4];
    ds.readRawData(magic, 4);
    ds >> version >> flags >> metaLen >> segCount >> pointCount >> payloadLen >> crc >> nonce;
    if (version != kVersion) {
        err = QStringLiteral("不支持的文件版本：%1").arg(version);
        return false;
    }
    return true;
}

}  // namespace

bool saveEcm(const QString& path, const DataGroup& g, const EcmFileMeta& meta,
             const EcmSaveOptions& opt, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        err = QStringLiteral("无法写入文件：%1").arg(f.errorString());
        return false;
    }

    const QByteArray metaJson = buildMetaJson(g, meta);
    QByteArray payload = buildPayload(g);
    if (opt.compress) {
        payload = qCompress(payload, 6);
    }
    quint32 nonce = 0;
    if (opt.obfuscate) {
        nonce = QRandomGenerator::global()->generate();
        if (nonce == 0) {
            nonce = 1;
        }
        payload = xorKeystream(payload, nonce);
    }
    // 压缩与混淆都是"落在盘上的形态"，flags 必须如实记录
    const quint16 flags = quint16((opt.compress ? kFlagCompress : 0)
                                  | (opt.obfuscate ? kFlagObfuscate : 0));

    QByteArray head;
    {
        QDataStream ds(&head, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds.writeRawData(kMagic, 4);
        ds << kVersion << flags
           << quint32(metaJson.size())
           << quint32(g.seam.segments.size())
           << quint32(g.times.size())
           << quint32(payload.size())
           << crc32Of(payload)
           << nonce;
    }

    if (f.write(head) != head.size() || f.write(metaJson) != metaJson.size()
        || f.write(payload) != payload.size()) {
        err = QStringLiteral("写入不完整（磁盘空间？）：%1").arg(f.errorString());
        f.close();
        return false;
    }
    f.close();
    return true;
}

bool readEcmImpl(QFile& f, DataGroup* g, EcmFileMeta* meta, QString& err) {
    quint16 version = 0, flags = 0;
    quint32 metaLen = 0, segCount = 0, pointCount = 0, payloadLen = 0, crc = 0, nonce = 0;
    if (!readHeader(f, version, flags, metaLen, segCount, pointCount, payloadLen, crc, nonce, err)) {
        return false;
    }
    const QByteArray metaJson = f.read(metaLen);
    if (quint32(metaJson.size()) != metaLen) {
        err = QStringLiteral("元数据区不完整");
        return false;
    }
    QByteArray payload = f.read(payloadLen);
    if (quint32(payload.size()) != payloadLen) {
        err = QStringLiteral("数据区不完整（文件被截断？）");
        return false;
    }
    if (crc32Of(payload) != crc) {
        err = QStringLiteral("校验失败（文件被改动或损坏）");
        return false;
    }
    if (!g) {
        // peek：只读头部元数据，不必解压/还原曲线
        EcmFileMeta tmp;
        const QJsonObject root = QJsonDocument::fromJson(metaJson).object();
        tmp.groupName = root.value("name").toString();
        tmp.partLabel = root.value("partLabel").toString();
        tmp.partId = root.value("partId").toInt();
        tmp.incomplete = root.value("incomplete").toBool();
        tmp.hasEs = root.value("hasEs").toBool();
        if (meta) {
            *meta = tmp;
        }
        return true;
    }
    if (flags & kFlagObfuscate) {
        payload = xorKeystream(payload, nonce);
    }
    if (flags & kFlagCompress) {
        payload = qUncompress(payload);
        if (payload.isEmpty()) {
            err = QStringLiteral("解压失败（文件损坏？）");
            return false;
        }
    }

    if (g) {
        g->times.clear(); g->pos.clear(); g->force.clear(); g->passNo.clear(); g->esPos.clear();
        g->passes.clear();
        g->seam.segments.clear();
        g->seam.seams.clear();
        g->decCache.clear();
        g->live = false;
        g->curPass = 0;
        if (!parsePayload(payload, segCount, pointCount, *g, err)) {
            return false;
        }
    }
    if (meta || g) {
        EcmFileMeta tmp;
        if (g) {
            applyMetaJson(metaJson, *g, tmp);
        } else {
            const QJsonObject root = QJsonDocument::fromJson(metaJson).object();
            tmp.groupName = root.value("name").toString();
            tmp.partLabel = root.value("partLabel").toString();
            tmp.partId = root.value("partId").toInt();
            tmp.incomplete = root.value("incomplete").toBool();
            tmp.hasEs = root.value("hasEs").toBool();
        }
        if (meta) {
            *meta = tmp;
        }
    }
    return true;
}

bool loadEcm(const QString& path, DataGroup& g, EcmFileMeta& meta, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return false;
    }
    return readEcmImpl(f, &g, &meta, err);
}

bool peekEcm(const QString& path, EcmFileMeta& meta, QString& err) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        err = QStringLiteral("无法打开文件：%1").arg(f.errorString());
        return false;
    }
    return readEcmImpl(f, nullptr, &meta, err);
}
