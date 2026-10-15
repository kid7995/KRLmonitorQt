#include "part.h"

#include <QRegularExpression>

namespace {

constexpr int kArchiveNameMax = 120;

QString craftToken(Craft c) {
    switch (c) {
    case Craft::Rough: return QStringLiteral("粗磨");
    case Craft::Fine:  return QStringLiteral("细磨");
    case Craft::Unknown: break;
    }
    return QStringLiteral("工序未定");
}

}  // namespace

void PartSplitter::configure(PartRule rule, int timeoutMin) {
    m_rule = rule;
    m_timeoutMin = qMax(0, timeoutMin);
}

void PartSplitter::reset(const QDateTime& now) {
    m_cur = PartInfo();
    m_started = false;
    m_maxSeam = 0;
    m_craft = Craft::Unknown;
    m_lastData = now;
}

void PartSplitter::setStartSeq(int seq) {
    m_seq = qMax(0, seq);
}

void PartSplitter::beginPart(const QDateTime& now) {
    ++m_seq;
    m_cur = PartInfo();
    m_cur.id = m_seq;
    m_cur.label = QStringLiteral("件%1").arg(m_seq, 4, 10, QLatin1Char('0'));
    m_cur.t0 = now;
    m_cur.t1 = now;
    m_cur.cycles = 0;
    m_started = true;
    m_maxSeam = 0;
    m_craft = Craft::Unknown;
    m_lastData = now;
}

bool PartSplitter::onCycleStart(const QDateTime& now) {
    m_lastData = now;
    if (!m_started) {
        beginPart(now);
        ++m_cur.cycles;
        return true;
    }
    if (m_rule == PartRule::PerCycle) {
        beginPart(now);
        ++m_cur.cycles;
        return true;
    }
    ++m_cur.cycles;
    m_cur.t1 = now;
    return false;
}

bool PartSplitter::onSeamSeen(int seam, Craft craft, const QDateTime& now) {
    m_lastData = now;
    m_cur.t1 = now;
    if (!m_started || seam <= 0) {
        return false;
    }

    bool newPart = false;
    if (m_rule == PartRule::CraftRestart) {
        // 同一件内先粗磨后细磨；一旦又回到粗磨，说明换件了
        if (m_craft == Craft::Fine && craft == Craft::Rough) {
            newPart = true;
        }
    } else if (m_rule == PartRule::SeamWrap) {
        // 同一工序内焊缝号回绕 -> 新件；工序切换不算（一件可含粗磨+细磨）
        if (craft != Craft::Unknown && m_craft == craft && m_maxSeam > 0 && seam < m_maxSeam) {
            newPart = true;
        }
    }

    if (newPart) {
        beginPart(now);
        ++m_cur.cycles;
    }
    if (craft != Craft::Unknown && craft != m_craft) {
        m_maxSeam = 0;              // 工序切换：回绕计数重新开始
    }
    if (craft != Craft::Unknown) {
        m_craft = craft;
    }
    m_maxSeam = qMax(m_maxSeam, seam);
    return newPart;
}

void PartSplitter::onCycleEnd(const QDateTime& now) {
    m_lastData = now;
    if (m_started) {
        m_cur.t1 = now;
    }
}

bool PartSplitter::onIdle(const QDateTime& now) {
    if (!m_started || m_timeoutMin <= 0) {
        return false;
    }
    if (m_lastData.isValid() && m_lastData.secsTo(now) < m_timeoutMin * 60) {
        return false;
    }
    // 只是"下一件从这个时间点开始"：真正建件推迟到下一次周期开始
    m_started = false;
    m_maxSeam = 0;
    m_craft = Craft::Unknown;
    m_lastData = now;
    return true;
}

bool PartSplitter::startNewPart(const QDateTime& now) {
    beginPart(now);
    return true;
}

QString buildArchiveName(const QString& tmpl, const QDateTime& t,
                         const PartInfo& part, Craft craft,
                         int firstSeam, int lastSeam) {
    QString s = tmpl.isEmpty() ? QStringLiteral("{time}_{part}_{craft}_{seams}") : tmpl;
    const QString timeStr = t.toString(QStringLiteral("yyyy-MM-dd_HH-mm"));
    const QString dateStr = t.toString(QStringLiteral("yyyy-MM-dd"));
    const QString partStr = part.label.isEmpty() ? QStringLiteral("未分件") : part.label;
    QString seamsStr;
    if (firstSeam > 0 && lastSeam >= firstSeam) {
        seamsStr = (firstSeam == lastSeam)
                       ? QStringLiteral("S%1").arg(firstSeam)
                       : QStringLiteral("S%1-%2").arg(firstSeam).arg(lastSeam);
    } else {
        seamsStr = QStringLiteral("S-");
    }
    s.replace(QStringLiteral("{time}"), timeStr);
    s.replace(QStringLiteral("{date}"), dateStr);
    s.replace(QStringLiteral("{part}"), partStr);
    s.replace(QStringLiteral("{craft}"), craftToken(craft));
    s.replace(QStringLiteral("{seams}"), seamsStr);
    // 容错：模板里没识别的占位符整段清掉，避免出现裸花括号
    static const QRegularExpression leftover(QStringLiteral("\\{[^{}]*\\}"));
    s.remove(leftover);
    return sanitizeFileName(s);
}

QString sanitizeFileName(const QString& in) {
    QString s = in;
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|\\r\\n\\t]"));
    s.replace(bad, QStringLiteral("_"));
    s = s.trimmed();
    while (s.endsWith(QLatin1Char('.'))) {
        s.chop(1);
    }
    if (s.size() > kArchiveNameMax) {
        s.truncate(kArchiveNameMax);
    }
    return s;
}
