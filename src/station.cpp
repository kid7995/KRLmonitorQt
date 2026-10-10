#include "station.h"

#include "config.h"

PassMark decodePassNo(int passNo) {
    if (passNo > 0) {
        return PassMark{SeamPhase::Grind, 0};
    }
    if (passNo == 0) {
        return PassMark{SeamPhase::None, 0};
    }

    const int a = -passNo;
    if (a == SEAM_MARK_FINISH) {
        return PassMark{SeamPhase::Finish, 0};
    }

    int base = 0;
    SeamPhase phase = SeamPhase::None;
    if (a <= SEAM_MARK_DETECT + SEAM_COUNT_MAX) {          // 1..8
        base = SEAM_MARK_DETECT;
        phase = SeamPhase::Detect;
    } else if (a > SEAM_MARK_GRIND && a <= SEAM_MARK_GRIND + SEAM_COUNT_MAX) {
        base = SEAM_MARK_GRIND;
        phase = SeamPhase::GrindGap;
    } else if (a > SEAM_MARK_MOVE && a <= SEAM_MARK_MOVE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_MOVE;
        phase = SeamPhase::Transfer;
    } else {
        return PassMark{SeamPhase::None, 0};
    }

    const int k = a - base;
    if (k < 1 || k > SEAM_COUNT_MAX) {
        return PassMark{SeamPhase::None, 0};
    }
    return PassMark{phase, k};
}

QString seamPhaseLabel(SeamPhase p) {
    switch (p) {
    case SeamPhase::Detect:   return QStringLiteral("检测");
    case SeamPhase::Grind:    return QStringLiteral("打磨");
    case SeamPhase::GrindGap: return QStringLiteral("打磨间隙");
    case SeamPhase::Transfer: return QStringLiteral("转移");
    case SeamPhase::Finish:   return QStringLiteral("收尾");
    case SeamPhase::None:     break;
    }
    return QStringLiteral("—");
}