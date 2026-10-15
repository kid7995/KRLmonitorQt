#include "station.h"

#include "config.h"

PassMark decodePassNo(int passNo) {
    if (passNo > 0) {
        // 正数 = 第 n 道；工序与焊缝号都不在编号里，由调用方用"最近的负标记"归属
        return PassMark{SeamPhase::Grind, Craft::Unknown, 0};
    }
    if (passNo == 0) {
        return PassMark{SeamPhase::None, Craft::Unknown, 0};
    }

    const int a = -passNo;
    if (a == SEAM_MARK_FINISH) {
        return PassMark{SeamPhase::Finish, Craft::Rough, 0};
    }
    if (a == SEAM_MARK_FINISH_FINE) {
        return PassMark{SeamPhase::Finish, Craft::Fine, 0};
    }

    int base = 0;
    SeamPhase phase = SeamPhase::None;
    Craft craft = Craft::Rough;
    if (a <= SEAM_MARK_DETECT + SEAM_COUNT_MAX) {          // 1..8
        base = SEAM_MARK_DETECT;
        phase = SeamPhase::Detect;
    } else if (a > SEAM_MARK_GRIND && a <= SEAM_MARK_GRIND + SEAM_COUNT_MAX) {
        base = SEAM_MARK_GRIND;
        phase = SeamPhase::GrindGap;
    } else if (a > SEAM_MARK_MOVE && a <= SEAM_MARK_MOVE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_MOVE;
        phase = SeamPhase::Transfer;
    } else if (a > SEAM_MARK_PREPARE && a <= SEAM_MARK_PREPARE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_PREPARE;                          // 41..48
        phase = SeamPhase::Prepare;
    } else if (a > SEAM_MARK_LIFT && a <= SEAM_MARK_LIFT + SEAM_COUNT_MAX) {
        base = SEAM_MARK_LIFT;                             // 51..58
        phase = SeamPhase::Lift;
    } else if (a > SEAM_MARK_DETECT_FINE && a <= SEAM_MARK_DETECT_FINE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_DETECT_FINE;                      // 101..108
        phase = SeamPhase::Detect;
        craft = Craft::Fine;
    } else if (a > SEAM_MARK_GRIND_FINE && a <= SEAM_MARK_GRIND_FINE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_GRIND_FINE;                       // 111..118
        phase = SeamPhase::GrindGap;
        craft = Craft::Fine;
    } else if (a > SEAM_MARK_MOVE_FINE && a <= SEAM_MARK_MOVE_FINE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_MOVE_FINE;                        // 121..128
        phase = SeamPhase::Transfer;
        craft = Craft::Fine;
    } else if (a > SEAM_MARK_PREPARE_FINE && a <= SEAM_MARK_PREPARE_FINE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_PREPARE_FINE;                      // 141..148
        phase = SeamPhase::Prepare;
        craft = Craft::Fine;
    } else if (a > SEAM_MARK_LIFT_FINE && a <= SEAM_MARK_LIFT_FINE + SEAM_COUNT_MAX) {
        base = SEAM_MARK_LIFT_FINE;                         // 151..158
        phase = SeamPhase::Lift;
        craft = Craft::Fine;
    } else {
        return PassMark{SeamPhase::None, Craft::Unknown, 0};
    }

    const int k = a - base;
    if (k < 1 || k > SEAM_COUNT_MAX) {
        return PassMark{SeamPhase::None, Craft::Unknown, 0};
    }
    return PassMark{phase, craft, k};
}

QString seamPhaseLabel(SeamPhase p) {
    switch (p) {
    case SeamPhase::Detect:   return QStringLiteral("检测");
    case SeamPhase::Grind:    return QStringLiteral("打磨");
    case SeamPhase::GrindGap: return QStringLiteral("打磨间隙");
    case SeamPhase::Transfer: return QStringLiteral("空走");
    case SeamPhase::Prepare:  return QStringLiteral("预备位");
    case SeamPhase::Lift:     return QStringLiteral("抬起");
    case SeamPhase::Finish:   return QStringLiteral("收尾");
    case SeamPhase::None:     break;
    }
    return QStringLiteral("—");
}

QString craftLabel(Craft c) {
    switch (c) {
    case Craft::Rough: return QStringLiteral("粗磨");
    case Craft::Fine:  return QStringLiteral("细磨");
    case Craft::Unknown: break;
    }
    return QStringLiteral("—");
}
