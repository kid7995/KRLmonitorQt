#include "protocol.h"

#include <QXmlStreamReader>
#include <cstdint>

bool parsePacket(const QString& xml, Sample& out) {
    QXmlStreamReader r(xml);
    bool gotPos = false;
    bool gotForce = false;

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement()) {
            continue;
        }
        QString name = r.name().toString();
        if (name == QLatin1String("PosR")) {
            bool ok = false;
            int v = r.readElementText().trimmed().toInt(&ok);
            if (ok) { out.pos = v; gotPos = true; }
        } else if (name == QLatin1String("ForceR")) {
            bool ok = false;
            int v = r.readElementText().trimmed().toInt(&ok);
            if (ok) {
                // KRL 侧 ToSigned 已转过一次，这里再转一次是幂等保险
                out.force = (int16_t)(uint16_t)(v & 0xFFFF);
                gotForce = true;
            }
        } else if (name == QLatin1String("RecordOn")) {
            QString s = r.readElementText().trimmed().toLower();
            out.recordOn = (s == QLatin1String("true") || s == QLatin1String("1"));
        } else if (name == QLatin1String("PassNo")) {
            bool ok = false;
            double d = r.readElementText().trimmed().toDouble(&ok);
            out.passNo = ok ? (int)d : 0;
        } else if (name == QLatin1String("EsPos")) {
            bool ok = false;
            double d = r.readElementText().trimmed().toDouble(&ok);
            out.esPos = ok ? (int)d : 0;
        }
    }

    if (r.hasError() || !gotPos || !gotForce) {
        return false;
    }
    return true;
}
