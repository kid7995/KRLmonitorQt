#include "chartview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>
#include <limits>

#include "theme.h"
#include "decimate.h"
#include "config.h"

namespace {

QVector<double> toDouble(const QVector<int>& v) {
    QVector<double> r(v.size());
    for (int i = 0; i < v.size(); ++i) {
        r[i] = double(v[i]);
    }
    return r;
}

QColor lerp(const QColor& a, const QColor& b, double t) {
    t = qBound(0.0, t, 1.0);
    return QColor(a.red() + int((b.red() - a.red()) * t),
                  a.green() + int((b.green() - a.green()) * t),
                  a.blue() + int((b.blue() - a.blue()) * t));
}

double niceNum(double x, bool roundUp) {
    if (x <= 0) return 1.0;
    double e = std::floor(std::log10(x));
    double f = x / std::pow(10.0, e);
    double nf;
    if (roundUp) {
        if (f < 1.5)      nf = 1.0;
        else if (f < 3.0) nf = 2.0;
        else if (f < 7.0) nf = 5.0;
        else              nf = 10.0;
    } else {
        if (f <= 1.0)     nf = 1.0;
        else if (f <= 2.0) nf = 2.0;
        else if (f <= 5.0) nf = 5.0;
        else               nf = 10.0;
    }
    return nf * std::pow(10.0, e);
}

QVector<double> niceTicks(double lo, double hi, int maxTicks) {
    QVector<double> out;
    if (!(hi > lo)) {
        if (lo == hi) out.append(lo);
        return out;
    }
    double range = niceNum(hi - lo, false);
    double step = niceNum(range / std::max(1, maxTicks - 1), true);
    double start = std::ceil(lo / step) * step;
    for (double v = start; v <= hi + step * 1e-9; v += step) {
        if (v < lo - step * 1e-9) continue;
        out.append(v);
        if (out.size() > 1000) break;
    }
    return out;
}

QString formatTick(double v) {
    double av = std::fabs(v);
    if (av < 1e-12) return QStringLiteral("0");
    if (av >= 1000.0) return theme::formatThousands((long long)std::llround(v));
    QString s;
    if (av < 10.0) s = QString::number(v, 'f', 3);
    else           s = QString::number(v, 'f', 1);
    while (s.endsWith('0')) s.chop(1);
    if (s.endsWith('.')) s.chop(1);
    return s;
}

// times 单调递增（实时时间戳与 CSV 均按时间顺序写入）：
// 二分查找距 x 最近的下标，等距时取左侧点。替代原全量线性扫描，
// 避免满数据量下每次鼠标移动都做几十万次比较而卡住界面。
int nearestIndex(const QVector<double>& t, double x) {
    int n = t.size();
    auto it = std::lower_bound(t.begin(), t.end(), x);
    int i = int(it - t.begin());
    if (i >= n) {
        return n - 1;
    }
    if (i > 0 && (x - t[i - 1]) <= (t[i] - x)) {
        return i - 1;
    }
    return i;
}

}  // namespace

ChartView::ChartView(QVector<DataGroup*>* groups, Recorder* recorder, QWidget* parent)
    : QWidget(parent), m_groups(groups), m_recorder(recorder) {
    setMouseTracking(true);
    setAutoFillBackground(false);
    setMinimumSize(400, 200);
}

// ── 坐标变换 ────────────────────────────────────────────────────

// 字号随窗口高度自适应：全屏放大时图表变大，固定像素字号会显得过小。
// 基准高度（FONT_REF_H）下倍率为 1，默认窗口显示与原来完全一致；
// 下限 1 保证缩小窗口时字号不会比原来更小。
double ChartView::fontScale() const {
    const double k = height() / FONT_REF_H;
    return k < 1.0 ? 1.0 : (k > FONT_SCALE_MAX ? FONT_SCALE_MAX : k);
}

int ChartView::scaleFontPx(int nominal) const {
    return qRound(nominal * fontScale());
}

QRect ChartView::plotRect() const {
    const double fs = fontScale();
    int l = qRound(width() * 0.085);
    int r = qRound(width() * 0.085);
    int t = qMax(qRound(34 * fs), qRound(height() * 0.06));
    int b = qMax(qRound(44 * fs), qRound(height() * 0.12));
    return QRect(l, t, width() - l - r, height() - t - b);
}

double ChartView::xToPx(double x) const {
    QRect r = plotRect();
    if (m_x1 == m_x0) return r.left() + r.width() / 2.0;
    return r.left() + (x - m_x0) / (m_x1 - m_x0) * r.width();
}

double ChartView::y1ToPx(double y) const {
    QRect r = plotRect();
    if (m_y11 == m_y10) return r.bottom() - r.height() / 2.0;
    return r.bottom() - (y - m_y10) / (m_y11 - m_y10) * r.height();
}

double ChartView::y2ToPx(double y) const {
    QRect r = plotRect();
    if (m_y21 == m_y20) return r.bottom() - r.height() / 2.0;
    return r.bottom() - (y - m_y20) / (m_y21 - m_y20) * r.height();
}

double ChartView::pxToX(double px) const {
    QRect r = plotRect();
    if (m_x1 == m_x0) return m_x0;
    return m_x0 + (px - r.left()) / double(r.width()) * (m_x1 - m_x0);
}

double ChartView::pxToY1(double py) const {
    QRect r = plotRect();
    return m_y10 + (r.bottom() - py) / double(r.height()) * (m_y11 - m_y10);
}

double ChartView::pxToY2(double py) const {
    QRect r = plotRect();
    return m_y20 + (r.bottom() - py) / double(r.height()) * (m_y21 - m_y20);
}

DataGroup* ChartView::refGroup() const {
    if (m_groups->isEmpty()) return nullptr;
    int idx = m_refGroupIdx;
    if (idx < 0 || idx >= m_groups->size()) idx = 0;
    return (*m_groups)[idx];
}

// ── 范围重算 ────────────────────────────────────────────────────

void ChartView::computeBounds(double& x0, double& x1, double& y10, double& y11,
                              double& y20, double& y21,
                              bool& hasX, bool& hasY1, bool& hasY2) {
    const double INF = std::numeric_limits<double>::infinity();
    x0 = y10 = y20 = INF;
    x1 = y11 = y21 = -INF;
    hasX = hasY1 = hasY2 = false;

    auto feed = [&](const XY& xy, bool fx, bool fy1, bool fy2) {
        for (int i = 0; i < xy.t.size(); ++i) {
            double tv = xy.t[i];
            double yv = xy.y[i];
            if (fx)  { if (tv < x0) x0 = tv; if (tv > x1) x1 = tv; hasX = true; }
            if (fy1) { if (yv < y10) y10 = yv; if (yv > y11) y11 = yv; hasY1 = true; }
            if (fy2) { if (yv < y20) y20 = yv; if (yv > y21) y21 = yv; hasY2 = true; }
        }
    };

    if (m_overlayMode) {
        DataGroup* g = refGroup();
        if (g) {
            for (const Pass& p : g->passes) {
                int i1 = p.i1 + 1;
                if (i1 > g->times.size()) i1 = g->times.size();
                QVector<double> tt;
                QVector<int> pp, ff;
                for (int i = p.i0; i < i1; ++i) {
                    tt.append(g->times[i] - p.t0);
                    pp.append(g->pos[i]);
                    ff.append(g->force[i]);
                }
                if (tt.isEmpty()) continue;
                if (g->showPos)   feed(decimate(tt, toDouble(pp)), true, true, false);
                if (g->showForce) feed(decimate(tt, toDouble(ff)), true, false, true);
            }
        }
    } else {
        if (m_showMonitor && m_recorder) {
            feed(cachedXY(m_recorder->monDec, QStringLiteral("pos"),
                          m_recorder->monTimes, m_recorder->monPos), true, true, false);
            feed(cachedXY(m_recorder->monDec, QStringLiteral("force"),
                          m_recorder->monTimes, m_recorder->monForce), true, false, true);
        }
        for (DataGroup* g : *m_groups) {
            if (filterActive()) {
                // 过滤视图：范围只按"看得见的段"算，否则缩放到选中的焊缝就失去意义
                const int n = g->times.size();
                for (const Segment& s : g->seam.segments) {
                    if (!segmentVisible(s)) {
                        continue;
                    }
                    const int a = qMax(0, s.i0);
                    const int b = qMin(n - 1, s.i1);
                    if (b < a) {
                        continue;
                    }
                    for (int i = a; i <= b; ++i) {
                        const double tv = g->times[i];
                        if (tv < x0) x0 = tv;
                        if (tv > x1) x1 = tv;
                        hasX = true;
                        if (g->showPos) {
                            const double yv = double(g->pos[i]);
                            if (yv < y10) y10 = yv;
                            if (yv > y11) y11 = yv;
                            hasY1 = true;
                        }
                        if (m_showEsPos && g->hasEs) {
                            const double yv = double(g->esPos[i]);
                            if (yv < y10) y10 = yv;
                            if (yv > y11) y11 = yv;
                            hasY1 = true;
                        }
                        if (g->showForce) {
                            const double fv = double(g->force[i]);
                            if (fv < y20) y20 = fv;
                            if (fv > y21) y21 = fv;
                            hasY2 = true;
                        }
                    }
                }
                continue;
            }
            if (g->showPos) {
                feed(cachedXY(g->decCache, QStringLiteral("pos"), g->times, g->pos), true, true, false);
            }
            if (g->showForce) {
                feed(cachedXY(g->decCache, QStringLiteral("force"), g->times, g->force), true, false, true);
            }
            if (m_showEsPos && g->hasEs) {
                feed(cachedXY(g->decCache, QStringLiteral("es"), g->times, g->esPos), true, true, false);
            }
        }
    }
}

void ChartView::relimAll() {
    double x0, x1, y10, y11, y20, y21;
    bool hasX, hasY1, hasY2;
    computeBounds(x0, x1, y10, y11, y20, y21, hasX, hasY1, hasY2);

    auto apply = [](bool has, double lo, double hi, double& o0, double& o1) {
        if (has) {
            double pad = (hi - lo) * 0.05;
            if (pad <= 0) pad = std::max(1.0, std::fabs(hi) * 0.05);
            o0 = lo - pad;
            o1 = hi + pad;
        } else {
            o0 = 0.0;
            o1 = 1.0;
        }
    };
    apply(hasX, x0, x1, m_x0, m_x1);
    apply(hasY1, y10, y11, m_y10, m_y11);
    apply(hasY2, y20, y21, m_y20, m_y21);
    update();
}

void ChartView::resetView() {
    m_locked = false;
    emit lockChanged(false, QString());
    relimAll();
}

// ── 绘制 ────────────────────────────────────────────────────────

void ChartView::drawLine(QPainter& p, const XY& xy, const QColor& color,
                         Qt::PenStyle style, double lw, bool y2, double alpha) {
    if (xy.t.isEmpty()) return;
    QColor c = color;
    c.setAlphaF(alpha);
    QPen pen(c);
    pen.setWidthF(lw);
    pen.setCosmetic(true);
    pen.setStyle(style);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPolygonF poly;
    poly.reserve(xy.t.size());
    for (int i = 0; i < xy.t.size(); ++i) {
        poly << QPointF(xToPx(xy.t[i]), y2 ? y2ToPx(xy.y[i]) : y1ToPx(xy.y[i]));
    }
    p.drawPolyline(poly);
}

void ChartView::drawStair(QPainter& p, const XY& xy, const QColor& color,
                          double lw, double alpha) {
    if (xy.t.size() < 1) return;
    QColor c = color;
    c.setAlphaF(alpha);
    QPen pen(c);
    pen.setWidthF(lw);
    pen.setCosmetic(true);
    pen.setStyle(Qt::CustomDashLine);
    pen.setDashPattern(QVector<qreal>{6.0, 3.0});  // (0,(6,3)) steps-post 虚线
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPolygonF poly;
    for (int i = 0; i < xy.t.size(); ++i) {
        double px = xToPx(xy.t[i]);
        double py = y1ToPx(xy.y[i]);
        poly << QPointF(px, py);
        if (i + 1 < xy.t.size()) {
            poly << QPointF(xToPx(xy.t[i + 1]), py);
        }
    }
    p.drawPolyline(poly);
}

// 取可见窗口 [m_x0, m_x1] 内的曲线数据。
// 窗口点数不超过 DISPLAY_MAX_PTS 时直接切片原始数据绘制：全范围 min/max
// 抽稀放大后每个桶只剩两个包络极值点，连出来是乱跳的锯齿；窗口切片
// 还原真实波形。超过阈值（未放大/全览）退回全范围抽稀缓存路径，
// 行为与原来完全一致（times 单调递增，二分定位 O(log n)）。
XY ChartView::visibleXY(DecCache& cache, const QString& key,
                        const QVector<double>& t, const QVector<int>& y) const {
    int i0 = int(std::lower_bound(t.begin(), t.end(), m_x0) - t.begin());
    int i1 = int(std::upper_bound(t.begin(), t.end(), m_x1) - t.begin());
    if (i1 > i0 && i1 - i0 <= DISPLAY_MAX_PTS) {
        XY r;
        r.t.reserve(i1 - i0);
        r.y.reserve(i1 - i0);
        for (int i = i0; i < i1; ++i) {
            r.t.append(t[i]);
            r.y.append(double(y[i]));
        }
        return r;
    }
    return cachedXY(cache, key, t, y);
}

// ── 拆分视图：按焊缝 / 阶段 / 工序过滤 ──────────────────────────

void ChartView::setSeamFocus(const QSet<int>& seams) {
    if (m_seamFocus == seams) {
        return;
    }
    m_seamFocus = seams;
    clearHover();
    // 只改绘制浓淡、不隐藏任何数据：坐标范围不重算，视图不会跳
    update();
}

void ChartView::setPhaseView(PhaseView v) {
    if (m_phaseView == v) {
        return;
    }
    m_phaseView = v;
    clearHover();
    relimAll();
    update();
}

void ChartView::setCraftView(CraftView v) {
    if (m_craftView == v) {
        return;
    }
    m_craftView = v;
    clearHover();
    relimAll();
    update();
}

bool ChartView::filterActive() const {
    return m_phaseView != PhaseView::All || m_craftView != CraftView::All;
}

bool ChartView::focusActive() const {
    return !m_seamFocus.isEmpty();
}

bool ChartView::segmentVisible(const Segment& s) const {
    // 工序：只排除"明确是另一工序"的段，未定工序（旧程序）保留，避免误隐藏
    if (m_craftView == CraftView::RoughOnly && s.craft == Craft::Fine) {
        return false;
    }
    if (m_craftView == CraftView::FineOnly && s.craft == Craft::Rough) {
        return false;
    }
    switch (m_phaseView) {
    case PhaseView::DetectOnly: return s.kind == SeamPhase::Detect;
    case PhaseView::GrindOnly:  return s.kind == SeamPhase::Grind;
    case PhaseView::GapOnly:    return s.kind == SeamPhase::GrindGap;
    case PhaseView::All:        break;
    }
    return true;
}

// 是否属于"被突出显示"的焊缝
bool ChartView::segmentFocused(const Segment& s) const {
    return !m_seamFocus.isEmpty() && s.seam > 0 && m_seamFocus.contains(s.seam);
}

// 该焊缝在当前视图下是否还有可见的段（决定装饰层——底色带/分界线/焊缝标注——要不要画）
bool ChartView::seamVisibleInView(const DataGroup& g, int seam) const {
    if (seam <= 0) {
        return false;
    }
    if (!filterActive()) {
        return true;
    }
    for (const Segment& s : g.seam.segments) {
        if (s.seam == seam && segmentVisible(s)) {
            return true;
        }
    }
    return false;
}

// 该道次在当前视图下是否还看得见（阶段/工序过滤会隐藏一部分段）
bool ChartView::passVisibleInView(const DataGroup& g, const Pass& p) const {
    if (!filterActive()) {
        return true;
    }
    for (const Segment& s : g.seam.segments) {
        if (s.kind == SeamPhase::Grind && s.passNo == p.no && s.seam == p.seam) {
            return segmentVisible(s);
        }
    }
    return true;    // 没有段表（旧 CSV）时不隐藏
}

// 过滤视图：逐段取点、逐段成线。
// 之所以不能把过滤后的点并成一条折线，是因为段与段之间隔着空移，
// 连起来会画出跨越焊缝的斜线；逐段绘制同时也让"段类型"可以直接用明暗表达。
void ChartView::drawSegmented(QPainter& p, DataGroup* g) {
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    const int n = g->times.size();
    if (n == 0) {
        return;
    }

    for (const Segment& s : g->seam.segments) {
        if (!segmentVisible(s)) {
            continue;
        }
        const int i0 = qMax(0, s.i0);
        const int i1 = qMin(n - 1, s.i1);
        if (i1 < i0) {
            continue;
        }

        // 段只画落在当前窗口内的部分
        const double t0 = g->times[i0];
        const double t1 = g->times[i1];
        if (t1 < m_x0 || t0 > m_x1) {
            continue;
        }

        double alpha = 0.9;
        switch (s.kind) {
        case SeamPhase::Detect:   alpha = 1.0;  break;
        case SeamPhase::Grind:    alpha = 0.95; break;
        case SeamPhase::Prepare:  alpha = 0.55; break;   // 到预备位
        case SeamPhase::Lift:     alpha = 0.40; break;   // 打磨后抬起
        case SeamPhase::GrindGap: alpha = 0.30; break;   // 返回/接近段：淡
        default:                  alpha = 0.18; break;   // 转移、未标记：更淡
        }
        // 被突出的段：全亮加粗；其余保持"浅色底色层"的浓淡（不隐藏、也不额外变暗）
        const bool focused = segmentFocused(s);
        if (focusActive()) {
            alpha = focused ? CURVE_FOCUS_ALPHA : CURVE_BASE_ALPHA;
        }
        const double wPos = focused ? 2.6 : 1.6;
        const double wForce = focused ? 2.3 : 1.4;
        const double esAlpha = focused ? 0.9 : CURVE_ES_ALPHA;

        XY xy;
        const int cnt = i1 - i0 + 1;
        if (cnt > DISPLAY_MAX_PTS) {
            QVector<double> tt, yy;
            tt.reserve(cnt);
            yy.reserve(cnt);
            for (int i = i0; i <= i1; ++i) {
                tt.append(g->times[i]);
                yy.append(double(g->pos[i]));
            }
            xy = decimate(tt, yy);
        } else {
            xy.t.reserve(cnt);
            xy.y.reserve(cnt);
            for (int i = i0; i <= i1; ++i) {
                xy.t.append(g->times[i]);
                xy.y.append(double(g->pos[i]));
            }
        }
        if (g->showPos && xy.t.size() >= 1) {
            drawLine(p, xy, pal.pos, Qt::SolidLine, wPos, false, alpha);
        }
        if (g->showForce) {
            XY xf;
            if (cnt > DISPLAY_MAX_PTS) {
                QVector<double> tt, yy;
                tt.reserve(cnt);
                yy.reserve(cnt);
                for (int i = i0; i <= i1; ++i) {
                    tt.append(g->times[i]);
                    yy.append(double(g->force[i]));
                }
                xf = decimate(tt, yy);
            } else {
                xf.t.reserve(cnt);
                xf.y.reserve(cnt);
                for (int i = i0; i <= i1; ++i) {
                    xf.t.append(g->times[i]);
                    xf.y.append(double(g->force[i]));
                }
            }
            drawLine(p, xf, pal.force, Qt::DashLine, wForce, true, alpha);
        }
        if (m_showEsPos && g->hasEs) {
            XY xe;
            xe.t.reserve(cnt);
            xe.y.reserve(cnt);
            for (int i = i0; i <= i1; ++i) {
                xe.t.append(g->times[i]);
                xe.y.append(double(g->esPos[i]));
            }
            drawStair(p, xe, pal.pos, focused ? 1.6 : 1.0, esAlpha);
        }

        // 检测段只有几个采样点，光靠折线看不出内容：额外打一个标记点，
        // 位置 = 该段末尾的 ESpos（就是这一条焊缝的基准高度 InitLen）
        if (s.kind == SeamPhase::Detect) {
            const double x = xToPx(g->times[i1]);
            const double y = y1ToPx(double(g->esPos[i1]));
            p.setPen(QPen(pal.pos, 1.2));
            p.setBrush(pal.pos);
            p.drawEllipse(QPointF(x, y), 3.5, 3.5);
            p.setBrush(Qt::NoBrush);
        }
    }
}

// 正常视图（没有阶段/工序过滤）下的"突出显示"：把被选中焊缝的那几段用更粗更亮的线
// 叠画一遍。其余曲线保持原样不隐藏 —— 用户要的是"突出这一段"，不是"只剩这一段"。
void ChartView::drawFocusSegments(QPainter& p, DataGroup* g) {
    if (!focusActive() || !g) {
        return;
    }
    const int n = g->times.size();
    if (n == 0) {
        return;
    }
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    for (const Segment& s : g->seam.segments) {
        if (!segmentFocused(s)) {
            continue;
        }
        const int i0 = qMax(0, s.i0);
        const int i1 = qMin(n - 1, s.i1);
        if (i1 < i0) {
            continue;
        }
        if (g->times[i1] < m_x0 || g->times[i0] > m_x1) {
            continue;                       // 整段都在窗口外
        }
        QVector<double> tt, yp, yf;
        tt.reserve(i1 - i0 + 1);
        yp.reserve(i1 - i0 + 1);
        yf.reserve(i1 - i0 + 1);
        for (int i = i0; i <= i1; ++i) {
            tt.append(g->times[i]);
            yp.append(double(g->pos[i]));
            yf.append(double(g->force[i]));
        }
        // 段太长才抽稀（短段直接画原始点，放大后是真实波形）
        const bool thick = (i1 - i0 + 1) > DISPLAY_MAX_PTS;
        const XY xp = thick ? decimate(tt, yp) : XY{tt, yp};
        const XY xf = thick ? decimate(tt, yf) : XY{tt, yf};
        if (g->showPos) {
            drawLine(p, xp, pal.pos, Qt::SolidLine, 2.6, false, 1.0);
        }
        if (g->showForce) {
            drawLine(p, xf, pal.force, Qt::DashLine, 2.3, true, 1.0);
        }
        if (m_showEsPos && g->hasEs) {
            QVector<double> ye;
            ye.reserve(i1 - i0 + 1);
            for (int i = i0; i <= i1; ++i) {
                ye.append(double(g->esPos[i]));
            }
            drawStair(p, thick ? decimate(tt, ye) : XY{tt, ye}, pal.pos, 1.6, 0.9);
        }
    }
}

void ChartView::drawGrid(QPainter& p) {
    QRect plot = plotRect();
    QColor c = theme::BORDER;
    c.setAlphaF(0.6);
    QPen pen(c);
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.5);
    pen.setCosmetic(true);
    p.setPen(pen);

    QVector<double> xt = niceTicks(m_x0, m_x1, 8);
    for (double v : xt) {
        double px = xToPx(v);
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
    }
    QVector<double> y1t = niceTicks(m_y10, m_y11, 8);
    for (double v : y1t) {
        double py = y1ToPx(v);
        p.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
    }
}

void ChartView::drawPassDividers(QPainter& p) {
    if (!m_showSegments || m_overlayMode) return;
    QRect plot = plotRect();

    // 焊缝（工位）分界：实线 + 焊缝号，用于区分 1..8 号焊缝
    DataGroup* rg = refGroup();
    if (rg && !rg->seam.seams.isEmpty()) {
        const theme::Palette& rpal = theme::paletteOf(rg->paletteIdx);
        QColor sc = rpal.accent;
        sc.setAlphaF(0.75);
        QPen spen(sc);
        spen.setWidthF(1.4);
        spen.setCosmetic(true);
        p.setPen(spen);
        for (const SeamStat& s : rg->seam.seams) {
            if (!seamVisibleInView(*rg, s.seam)) {
                continue;       // 过滤后看不见的焊缝不画分界线
            }
            const double x = xToPx(s.t0);
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        QFont sf = p.font();
        sf.setPixelSize(scaleFontPx(11));
        p.setFont(sf);
        QFontMetrics sfm(sf);
        QColor tc = rpal.accent;
        tc.setAlphaF(0.95);
        p.setPen(tc);
        for (const SeamStat& s : rg->seam.seams) {
            if (!seamVisibleInView(*rg, s.seam)) {
                continue;
            }
            const double x = xToPx(s.t0);
            p.drawText(QPointF(x + 4, plot.top() + sfm.ascent() + 2),
                       QStringLiteral("焊缝 %1").arg(s.seam));
        }
    }

    for (DataGroup* g : *m_groups) {
        if (g->passes.isEmpty()) continue;
        const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
        QColor c = pal.accent;
        c.setAlphaF(0.40);
        QPen pen(c);
        pen.setWidthF(0.9);
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        for (const Pass& pass : g->passes) {
            if (!passVisibleInView(*g, pass)) {
                continue;
            }
            double x = xToPx(pass.t0);
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            x = xToPx(pass.t1);
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    }
}

void ChartView::drawPassAnnotations(QPainter& p) {
    if (!m_showSegments || m_overlayMode) return;
    DataGroup* g = refGroup();
    if (!g || g->passes.isEmpty()) return;
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    QRect plot = plotRect();
    const double fs = fontScale();

    QFont f = p.font();
    f.setPixelSize(scaleFontPx(10));
    p.setFont(f);
    QFontMetrics fm(f);
    QColor c = pal.accent;
    c.setAlphaF(0.95);
    p.setPen(c);

    int n = g->passes.size();
    int step = (n <= 20) ? 1 : (n + 19) / 20;
    for (int k = 0; k < n; ++k) {
        if (k % step) continue;
        const Pass& pass = g->passes[k];
        if (!passVisibleInView(*g, pass)) {
            continue;       // 过滤后看不见的道次不标注，避免"#n 标在别的段上"
        }
        double x = xToPx((pass.t0 + pass.t1) / 2.0);
        QString txt = QStringLiteral("#%1").arg(pass.no);
        if (pass.retry) {
            txt += QStringLiteral("补");
        }
        int w = fm.horizontalAdvance(txt);
        int h = fm.height();
        p.drawText(QRectF(x - w / 2.0, plot.top() - qRound(4 * fs) - h, w, h),
                   Qt::AlignHCenter | Qt::AlignTop, txt);
    }
}

void ChartView::drawOverlay(QPainter& p) {
    DataGroup* g = refGroup();
    if (!g || g->passes.isEmpty()) return;
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    QColor dark(0x2b, 0x31, 0x38);
    int n = g->passes.size();
    for (int k = 0; k < n; ++k) {
        const Pass& pass = g->passes[k];
        double frac = double(k + 1) / n;
        QColor cpos = lerp(dark, pal.pos, frac);
        QColor cforce = lerp(dark, pal.force, frac);

        int i0 = pass.i0;
        int i1 = pass.i1 + 1;
        if (i1 > g->times.size()) i1 = g->times.size();
        // 只取可见窗口（道次相对时间 [m_x0, m_x1]）内的点：深放大时
        // 画出窗口内真实波形，而不是整道抽稀后的包络锯齿。
        // 窗口小时 decimate 原样直通；窗口大（全览）时行为与原来一致。
        int lo = int(std::lower_bound(g->times.begin() + i0, g->times.begin() + i1,
                                      pass.t0 + m_x0) - g->times.begin());
        int hi = int(std::upper_bound(g->times.begin() + i0, g->times.begin() + i1,
                                      pass.t0 + m_x1) - g->times.begin());
        QVector<double> tt;
        QVector<int> pp, ff;
        for (int i = lo; i < hi; ++i) {
            tt.append(g->times[i] - pass.t0);
            pp.append(g->pos[i]);
            ff.append(g->force[i]);
        }
        if (tt.isEmpty()) continue;
        if (g->showPos)   drawLine(p, decimate(tt, toDouble(pp)), cpos, Qt::SolidLine, 1.4, false, 0.9);
        if (g->showForce) drawLine(p, decimate(tt, toDouble(ff)), cforce, Qt::DashLine, 1.2, true, 0.85);
    }
}

void ChartView::drawAxes(QPainter& p) {
    QRect plot = plotRect();
    const double fs = fontScale();

    p.setPen(QPen(theme::BORDER, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(plot);

    QFont f = p.font();
    f.setPixelSize(scaleFontPx(11));
    p.setFont(f);
    QFontMetrics fm(f);
    p.setPen(theme::C_DIM);

    // X 轴刻度
    QVector<double> xt = niceTicks(m_x0, m_x1, 8);
    for (double v : xt) {
        double px = xToPx(v);
        p.drawLine(QPointF(px, plot.bottom()), QPointF(px, plot.bottom() + 3));
        QString s = formatTick(v);
        int w = fm.horizontalAdvance(s);
        p.drawText(QPointF(px - w / 2.0, plot.bottom() + 3 + fm.ascent()), s);
    }
    // Y1 轴刻度
    QVector<double> y1t = niceTicks(m_y10, m_y11, 8);
    for (double v : y1t) {
        double py = y1ToPx(v);
        p.drawLine(QPointF(plot.left() - 3, py), QPointF(plot.left(), py));
        QString s = formatTick(v);
        int w = fm.horizontalAdvance(s);
        p.drawText(QPointF(plot.left() - 5 - w, py + fm.ascent() / 2 - 1), s);
    }
    // Y2 轴刻度
    QVector<double> y2t = niceTicks(m_y20, m_y21, 8);
    for (double v : y2t) {
        double py = y2ToPx(v);
        p.drawLine(QPointF(plot.right(), py), QPointF(plot.right() + 3, py));
        QString s = formatTick(v);
        p.drawText(QPointF(plot.right() + 5, py + fm.ascent() / 2 - 1), s);
    }

    // X 轴标签
    int xlw = fm.horizontalAdvance(m_xlabel);
    p.drawText(QPointF(plot.center().x() - xlw / 2.0,
                       plot.bottom() + 3 + fm.ascent() + qRound(22 * fs)),
               m_xlabel);

    // Y 轴标签（旋转）
    QFont yf = p.font();
    yf.setPixelSize(scaleFontPx(12));
    p.setFont(yf);
    p.setPen(theme::C_DIM);
    QFontMetrics yfm(yf);
    p.save();
    p.translate(plot.left() - qRound(40 * fs), plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-60, -yfm.height() / 2.0, 120, yfm.height()),
               Qt::AlignCenter, QStringLiteral("PosR"));
    p.restore();
    p.save();
    p.translate(plot.right() + qRound(40 * fs), plot.center().y());
    p.rotate(90);
    p.drawText(QRectF(-60, -yfm.height() / 2.0, 120, yfm.height()),
               Qt::AlignCenter, QStringLiteral("ForceR"));
    p.restore();
}

void ChartView::drawLegend(QPainter& p) {
    struct Entry {
        QColor color;
        Qt::PenStyle style;
        bool customDash;
        double dashOn;
        double dashOff;
        QString text;
    };
    QVector<Entry> entries;
    QString title;

    if (m_overlayMode) {
        DataGroup* g = refGroup();
        if (g && !g->passes.isEmpty()) {
            const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
            QColor dark(0x2b, 0x31, 0x38);
            int n = g->passes.size();
            for (int k = 0; k < n; ++k) {
                double frac = double(k + 1) / n;
                Entry e;
                e.color = lerp(dark, pal.pos, frac);
                e.style = Qt::SolidLine;
                e.customDash = false;
                e.text = QStringLiteral("#%1").arg(g->passes[k].no);
                entries.append(e);
            }
            title = QStringLiteral("道次（实线 PosR / 虚线 ForceR）");
        }
    } else {
        for (DataGroup* g : *m_groups) {
            const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
            if (g->showPos) {
                Entry e; e.color = pal.pos; e.style = Qt::SolidLine; e.customDash = false;
                e.text = QStringLiteral("%1  PosR （左轴）").arg(pal.label);
                entries.append(e);
            }
            if (g->showForce) {
                Entry e; e.color = pal.force; e.style = Qt::DashLine; e.customDash = false;
                e.text = QStringLiteral("%1  ForceR （右轴）").arg(pal.label);
                entries.append(e);
            }
            if (m_showEsPos && g->hasEs) {
                Entry e; e.color = pal.pos; e.style = Qt::CustomDashLine; e.customDash = true;
                e.dashOn = 6; e.dashOff = 3;
                e.text = QStringLiteral("%1  ESpos 设定 （左轴）").arg(pal.label);
                entries.append(e);
            }
        }
    }
    if (entries.isEmpty()) return;

    QFont f = p.font();
    f.setPixelSize(scaleFontPx(11));
    p.setFont(f);
    QFontMetrics fm(f);
    int lineH = fm.height() + 4;
    int padX = 8, padY = 6;
    int sampleW = 22;
    int maxW = 0;
    for (const Entry& e : entries) maxW = qMax(maxW, fm.horizontalAdvance(e.text));
    int titleW = title.isEmpty() ? 0 : fm.horizontalAdvance(title);
    int boxW = padX + sampleW + 6 + maxW + padX;
    if (titleW + 2 * padX > boxW) boxW = titleW + 2 * padX;
    int boxH = padY + (title.isEmpty() ? 0 : fm.height() + 4) + entries.size() * lineH + padY;

    QRect plot = plotRect();
    int bx = plot.left() + 6;
    int by = plot.top() + 6;
    QRect box(bx, by, boxW, boxH);

    QColor panel = theme::PANEL;
    panel.setAlphaF(0.9);
    p.setPen(QPen(theme::BORDER, 1));
    p.setBrush(panel);
    p.drawRoundedRect(box, 4, 4);

    int y = by + padY;
    if (!title.isEmpty()) {
        p.setPen(theme::C_DIM);
        p.drawText(QPointF(bx + padX, y + fm.ascent()), title);
        y += fm.height() + 4;
    }
    for (const Entry& e : entries) {
        QPen pen(e.color);
        pen.setWidthF(1.6);
        if (e.customDash) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern(QVector<qreal>{e.dashOn, e.dashOff});
        } else {
            pen.setStyle(e.style);
        }
        p.setPen(pen);
        int sy = y + fm.ascent() - 2;
        p.drawLine(QPointF(bx + padX, sy), QPointF(bx + padX + sampleW, sy));
        p.setPen(theme::C_TEXT);
        p.drawText(QPointF(bx + padX + sampleW + 6, y + fm.ascent()), e.text);
        y += lineH;
    }
}

void ChartView::drawHover(QPainter& p) {
    QRect plot = plotRect();
    QFont f = p.font();
    f.setPixelSize(scaleFontPx(11));
    p.setFont(f);
    QFontMetrics fm(f);

    for (const HoverMark& m : m_hoverMarks) {
        QColor acc = m.accent;
        acc.setAlphaF(0.6);
        QPen pen(acc);
        pen.setStyle(Qt::DashLine);
        pen.setWidthF(0.8);
        pen.setCosmetic(true);
        p.setPen(pen);
        double px = xToPx(m.tx);
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));

        if (m.showPos) {
            QColor c = m.posColor;
            p.setPen(c);
            p.setBrush(c);
            double py = y1ToPx(m.pos);
            p.drawEllipse(QPointF(px, py), 3.5, 3.5);
        }
        if (m.showForce) {
            QColor c = m.forceColor;
            p.setPen(c);
            p.setBrush(c);
            double py = y2ToPx(m.force);
            p.drawRect(QRectF(px - 3.5, py - 3.5, 7, 7));
        }
    }

    if (m_hoverLines.isEmpty()) return;

    int maxW = 0;
    for (const QString& l : m_hoverLines) maxW = qMax(maxW, fm.horizontalAdvance(l));
    int lineH = fm.height() + 2;
    int padX = 10, padY = 6;
    int boxW = maxW + 2 * padX;
    int boxH = m_hoverLines.size() * lineH + 2 * padY;

    double xfrac = (m_hoverX - m_x0) / qMax(m_x1 - m_x0, 1e-9);
    double anchorX = xToPx(m_hoverX);
    double anchorY = plot.top();
    double boxX = (xfrac < 0.72) ? (anchorX + 10) : (anchorX - 10 - boxW);
    double boxTop = anchorY - 8;
    if (boxTop < 2) boxTop = 2;
    if (boxX < plot.left()) boxX = plot.left();
    if (boxX + boxW > plot.right()) boxX = plot.right() - boxW;

    QRectF box(boxX, boxTop, boxW, boxH);
    QColor panel = theme::PANEL;
    panel.setAlphaF(0.95);
    p.setPen(QPen(theme::BORDER, 1));
    p.setBrush(panel);
    p.drawRoundedRect(box, 5, 5);
    p.setPen(theme::C_TEXT);
    double ty = boxTop + padY + fm.ascent();
    for (const QString& l : m_hoverLines) {
        p.drawText(QPointF(boxX + padX, ty), l);
        ty += lineH;
    }
}

void ChartView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    // 线条抗锯齿：对齐 Python 版 matplotlib（Agg）的渲染观感，
    // 消除放大后斜线段的像素阶梯毛刺
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), theme::BG);
    QRect plot = plotRect();
    // 曲线背后的底色：整块绘图区同一个深度，不按焊缝/道次叠加深浅
    p.fillRect(plot, theme::PLOT_BG);

    // 底色带 / 网格 / 分界线 / 曲线一律裁剪到绘图区内。
    // 否则平移或过滤之后，落在区外的矩形/直线会糊到坐标轴甚至整个窗口上（"画出界"）。
    p.save();
    p.setClipRect(plot);
    drawGrid(p);
    drawPassDividers(p);

    if (m_overlayMode) {
        drawOverlay(p);
    } else if (filterActive()) {
        // 阶段/工序过滤：只画匹配的段（逐段绘制、段间不连线）。
        // 注意这里必须**不再画未过滤的全曲线**，否则过滤就成了"叠加"，隐藏不掉任何东西。
        for (DataGroup* g : *m_groups) {
            drawSegmented(p, g);
        }
    } else {
        // 底色层一律浅色：没选中任何焊缝时整张图就是浅色的；
        // 选中某条焊缝后，它由 drawFocusSegments 叠一遍更粗更亮的线，其余保持浅色不变。
        const double base = CURVE_BASE_ALPHA;
        const double monA = CURVE_BASE_ALPHA;
        if (m_showMonitor && m_recorder) {
            drawLine(p, visibleXY(m_recorder->monDec, QStringLiteral("pos"),
                                  m_recorder->monTimes, m_recorder->monPos),
                     theme::C_MONITOR, Qt::SolidLine, 1.0, false, monA);
            drawLine(p, visibleXY(m_recorder->monDec, QStringLiteral("force"),
                                  m_recorder->monTimes, m_recorder->monForce),
                     theme::C_MONITOR, Qt::DashLine, 1.0, true, monA);
        }
        for (DataGroup* g : *m_groups) {
            const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
            if (g->showPos) {
                drawLine(p, visibleXY(g->decCache, QStringLiteral("pos"), g->times, g->pos),
                         pal.pos, Qt::SolidLine, 1.6, false, base);
            }
            if (g->showForce) {
                drawLine(p, visibleXY(g->decCache, QStringLiteral("force"), g->times, g->force),
                         pal.force, Qt::DashLine, 1.6, true, base);
            }
            if (m_showEsPos && g->hasEs) {
                drawStair(p, visibleXY(g->decCache, QStringLiteral("es"), g->times, g->esPos),
                          pal.pos, 1.0, CURVE_ES_ALPHA);
            }
        }
        for (DataGroup* g : *m_groups) {
            drawFocusSegments(p, g);        // 把被选中的那几条焊缝叠画出来
        }
    }
    p.restore();

    drawAxes(p);

    // 道次标注画在绘图区上方，需要一个"含上方留白"的裁剪区，
    // 否则平移后落在很左边的标注会横穿整个窗口
    p.save();
    p.setClipRect(QRect(plot.left(), 0, plot.width(), plot.bottom() + 1));
    drawPassAnnotations(p);
    p.restore();

    drawLegend(p);
    if (m_hoverValid) drawHover(p);
}

// ── 交互 ────────────────────────────────────────────────────────

void ChartView::startPan(const QPointF& pos) {
    m_panActive = true;
    m_panStart = pos;
    m_px0 = m_x0; m_px1 = m_x1;
    m_py10 = m_y10; m_py11 = m_y11;
    m_py20 = m_y20; m_py21 = m_y21;
    if (!m_locked) {
        m_locked = true;
        emit lockChanged(true, QStringLiteral("🔒 手动平移中"));
    }
    clearHover();
    setCursor(Qt::ClosedHandCursor);
}

void ChartView::doPan(const QPointF& pos) {
    QRect plot = plotRect();
    double dx = pos.x() - m_panStart.x();
    double dy = pos.y() - m_panStart.y();
    double w = plot.width() ? plot.width() : 1.0;
    double h = plot.height() ? plot.height() : 1.0;
    bool doX = (m_zoomMode == QStringLiteral("X轴") || m_zoomMode == QStringLiteral("双轴"));
    bool doY = (m_zoomMode == QStringLiteral("Y轴") || m_zoomMode == QStringLiteral("双轴"));

    if (doX) {
        double xPerPx = (m_px1 - m_px0) / w;
        double sx = -dx * xPerPx;
        m_x0 = m_px0 + sx;
        m_x1 = m_px1 + sx;
    }
    if (doY) {
        double sy1 = -dy * ((m_py11 - m_py10) / h);
        m_y10 = m_py10 + sy1;
        m_y11 = m_py11 + sy1;
        double sy2 = -dy * ((m_py21 - m_py20) / h);
        m_y20 = m_py20 + sy2;
        m_y21 = m_py21 + sy2;
    }
    update();
}

void ChartView::endPan() {
    m_panActive = false;
    unsetCursor();
}

static void zoom1d(double lo, double hi, double cursor, double scale,
                   double& nlo, double& nhi) {
    if (hi == lo) { nlo = lo; nhi = hi; return; }
    double lr = (cursor - lo) / (hi - lo);
    double ns = (hi - lo) * scale;
    nlo = cursor - lr * ns;
    nhi = cursor + (1 - lr) * ns;
}

void ChartView::zoomAt(const QPointF& pos, double scale) {
    QRect plot = plotRect();
    double fx = (pos.x() - plot.left()) / (plot.width() ? plot.width() : 1.0);
    double fy = (plot.bottom() - pos.y()) / (plot.height() ? plot.height() : 1.0);
    bool doX = (m_zoomMode == QStringLiteral("X轴") || m_zoomMode == QStringLiteral("双轴"));
    bool doY = (m_zoomMode == QStringLiteral("Y轴") || m_zoomMode == QStringLiteral("双轴"));

    if (doX) {
        double cx = m_x0 + fx * (m_x1 - m_x0);
        double nlo, nhi;
        zoom1d(m_x0, m_x1, cx, scale, nlo, nhi);
        m_x0 = nlo; m_x1 = nhi;
    }
    if (doY) {
        double cy1 = m_y10 + fy * (m_y11 - m_y10);
        zoom1d(m_y10, m_y11, cy1, scale, m_y10, m_y11);
        double cy2 = m_y20 + fy * (m_y21 - m_y20);
        zoom1d(m_y20, m_y21, cy2, scale, m_y20, m_y21);
    }
    if (!m_locked) {
        m_locked = true;
        emit lockChanged(true, QStringLiteral("🔒 手动缩放中"));
    }
    update();
}

void ChartView::clearHover() {
    if (!m_hoverValid && m_hoverMarks.isEmpty()) return;
    m_hoverValid = false;
    m_hoverMarks.clear();
    m_hoverLines.clear();
    update();
}

void ChartView::computeHover(double xm) {
    m_hoverValid = true;
    m_hoverX = xm;
    m_hoverMarks.clear();
    m_hoverLines.clear();

    // 头部时间与下方读数保持一致：下方读的是"最近采样点"的值，
    // 头部就显示该采样点的时间（取参考组口径，与道次标注一致）。
    // 无参考组数据时退回鼠标原始时间。
    DataGroup* rg = refGroup();
    double dispT = xm;

    for (DataGroup* g : *m_groups) {
        if (g->times.isEmpty()) continue;
        int idx = nearestIndex(g->times, xm);
        double tx = g->times[idx];
        if (g == rg) dispT = tx;
        const theme::Palette& pal = theme::paletteOf(g->paletteIdx);

        HoverMark m;
        m.tx = tx;
        m.accent = pal.accent;
        m.posColor = pal.pos;
        m.forceColor = pal.force;
        m.showPos = g->showPos;
        m.showForce = g->showForce;
        m.pos = g->pos[idx];
        m.force = g->force[idx];
        m_hoverMarks.append(m);

        if (idx < g->passNo.size()) {
            const int pn = g->passNo[idx];
            QString tag;
            if (pn > 0) {
                tag = QStringLiteral("第%1遍").arg(pn);
                for (const Pass& pp : g->passes) {     // 找到该样点所属道次，取焊缝/补充标记
                    if (idx >= pp.i0 && idx <= pp.i1) {
                        if (pp.seam > 0) {
                            tag = QStringLiteral("焊缝%1 第%2遍").arg(pp.seam).arg(pp.no);
                        }
                        if (pp.retry) {
                            tag += QStringLiteral("（补充）");
                        }
                        break;
                    }
                }
            } else {
                const PassMark mk = decodePassNo(pn);   // 负数标记：焊缝 + 阶段
                if (mk.seam > 0) {
                    tag = QStringLiteral("焊缝%1 %2").arg(mk.seam).arg(seamPhaseLabel(mk.phase));
                } else {
                    // 无标记：道次之间是上一道的反向返回段；首道之前称"非打磨段"
                    int prev = -1;
                    for (int k = g->passes.size() - 1; k >= 0; --k) {
                        if (g->passes[k].i1 < idx) { prev = k; break; }
                    }
                    tag = (prev >= 0) ? QStringLiteral("第%1遍 返回段").arg(g->passes[prev].no)
                                      : QStringLiteral("非打磨段");
                }
            }
            m_hoverLines << QStringLiteral(" %1  [%2]").arg(pal.label, tag);
        }
        if (g->showPos) {
            m_hoverLines << QStringLiteral(" %1  PosR   = %2")
                            .arg(pal.label, theme::formatThousands(g->pos[idx]));
        }
        if (g->showForce) {
            m_hoverLines << QStringLiteral(" %1  ForceR = %2")
                            .arg(pal.label, theme::formatThousands(g->force[idx]));
        }
        if (m_showEsPos && g->hasEs && idx < g->esPos.size()) {
            int es = g->esPos[idx];
            long long err = (long long)es - g->pos[idx];
            QString errs = err >= 0 ? QStringLiteral("+") + theme::formatThousands(err)
                                    : theme::formatThousands(err);
            m_hoverLines << QStringLiteral(" %1  ESpos  = %2   (跟随误差 %3)")
                            .arg(pal.label, theme::formatThousands(es), errs);
        }
    }

    // 用 %1 占位符 + 'f' 格式（Qt 的 arg 不识别 printf 风格 %.3f，
    // 原写法导致显示字面量 "%.3f" 并刷屏 "Argument missing" 警告）
    m_hoverLines.prepend(QStringLiteral(" t ≈ %1 s").arg(dispT, 0, 'f', 3));
    update();
}

void ChartView::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (!plotRect().contains(e->pos())) return;
    startPan(e->pos());
}

void ChartView::mouseMoveEvent(QMouseEvent* e) {
    if (m_panActive) {
        doPan(e->pos());
        return;
    }
    if (m_hoverActive && !m_overlayMode) {
        if (!plotRect().contains(e->pos())) {
            clearHover();
            return;
        }
        computeHover(pxToX(e->pos().x()));
    }
}

void ChartView::mouseReleaseEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    if (!m_panActive) return;
    endPan();
}

void ChartView::wheelEvent(QWheelEvent* e) {
    QPointF pos = e->posF();
    if (!plotRect().contains(pos.toPoint())) return;
    double factor = ZOOM_FACTOR;
    double scale = (e->angleDelta().y() > 0) ? (1.0 / factor) : factor;
    zoomAt(pos, scale);
}

void ChartView::mouseDoubleClickEvent(QMouseEvent* e) {
    Q_UNUSED(e);
    resetView();
}

void ChartView::leaveEvent(QEvent* e) {
    Q_UNUSED(e);
    if (!m_panActive) clearHover();
}
