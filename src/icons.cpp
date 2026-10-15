#include "icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>

#include "theme.h"

// ── 图标设计规范（全局统一）──────────────────────────────────────
//   坐标网格 24×24，主体留 2 单位内边距；描边 2 单位、圆头圆角；
//   只在"播放 / 停止 / 状态点"这类需要强识别的地方用填充，其余一律线描。
//   画布 48×48（1 单位 = 2px），由 Qt 缩放到实际显示尺寸。
namespace {

constexpr int kCanvas = 48;
constexpr qreal kU = 2.0;                       // 1 单位 = 2px（24 网格）

QPointF P(qreal x, qreal y) { return QPointF(x * kU, y * kU); }
QRectF R(qreal x, qreal y, qreal w, qreal h) { return QRectF(x * kU, y * kU, w * kU, h * kU); }

void stroke(QPainter& p, const QColor& c, qreal w = 2.0) {
    QPen pen(c);
    pen.setWidthF(w * kU);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
}

void fill(QPainter& p, const QColor& c) {
    p.setPen(Qt::NoPen);
    p.setBrush(c);
}

void poly(QPainter& p, std::initializer_list<QPointF> pts) {
    QPolygonF g;
    for (const QPointF& q : pts) {
        g << q;
    }
    p.drawPolyline(g);
}

}  // namespace

QIcon icons::get(const QString& name, const QColor& color) {
    const QColor c = color.isValid() ? color : theme::C_TEXT;
    QPixmap pm(kCanvas, kCanvas);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (name == QLatin1String("play")) {                  // 实心三角：启动类动作需要强识别
        fill(p, c);
        QPolygonF t;
        t << P(7, 4.5) << P(19.5, 12) << P(7, 19.5);
        p.drawPolygon(t);
    } else if (name == QLatin1String("pause")) {
        fill(p, c);
        p.drawRoundedRect(R(7.5, 5, 3, 14), 1.5, 1.5);
        p.drawRoundedRect(R(13.5, 5, 3, 14), 1.5, 1.5);
    } else if (name == QLatin1String("stop")) {
        fill(p, c);
        p.drawRoundedRect(R(7.5, 7.5, 9, 9), 2, 2);
    } else if (name == QLatin1String("save")) {           // 软盘轮廓（通用语义）
        stroke(p, c);
        p.drawRoundedRect(R(4, 4, 16, 16), 2.5, 2.5);
        stroke(p, c, 1.8);
        p.drawRoundedRect(R(8, 4, 8, 5), 1, 1);
        p.drawRoundedRect(R(8, 14, 8, 6), 1, 1);
    } else if (name == QLatin1String("open")) {           // 文件夹
        stroke(p, c);
        QPainterPath path;
        path.moveTo(P(3.5, 19));
        path.lineTo(P(3.5, 6.5));
        path.lineTo(P(9.5, 6.5));
        path.lineTo(P(12, 9.5));
        path.lineTo(P(20.5, 9.5));
        path.lineTo(P(20.5, 19));
        path.closeSubpath();
        p.drawPath(path);
    } else if (name == QLatin1String("add")) {
        stroke(p, c, 2.2);
        p.drawLine(P(12, 5), P(12, 19));
        p.drawLine(P(5, 12), P(17, 12));
    } else if (name == QLatin1String("trash")) {
        stroke(p, c);
        p.drawLine(P(4, 7.5), P(20, 7.5));
        p.drawLine(P(9.5, 7.5), P(9.5, 5));
        p.drawLine(P(14.5, 7.5), P(14.5, 5));
        QPainterPath body;
        body.moveTo(P(6.5, 7.5));
        body.lineTo(P(7.5, 20));
        body.lineTo(P(16.5, 20));
        body.lineTo(P(17.5, 7.5));
        p.drawPath(body);
    } else if (name == QLatin1String("part")) {           // 新建文档（折角）
        stroke(p, c);
        QPainterPath page;
        page.moveTo(P(5, 3.5));
        page.lineTo(P(14, 3.5));
        page.lineTo(P(19, 8.5));
        page.lineTo(P(19, 20));
        page.lineTo(P(5, 20));
        page.closeSubpath();
        p.drawPath(page);
        stroke(p, c, 1.8);
        p.drawLine(P(14, 3.5), P(14, 8.5));
        p.drawLine(P(14, 8.5), P(19, 8.5));
    } else if (name == QLatin1String("settings")) {       // 三滑杆：小尺寸下比齿轮清楚
        stroke(p, c, 1.9);
        p.drawLine(P(4, 7), P(20, 7));
        p.drawLine(P(4, 12), P(20, 12));
        p.drawLine(P(4, 17), P(20, 17));
        fill(p, c);
        p.drawEllipse(P(9, 7), 2.1 * kU, 2.1 * kU);
        p.drawEllipse(P(15, 12), 2.1 * kU, 2.1 * kU);
        p.drawEllipse(P(8.5, 17), 2.1 * kU, 2.1 * kU);
    } else if (name == QLatin1String("reset")) {
        stroke(p, c);
        p.drawArc(R(4.5, 4.5, 15, 15), 55 * 16, 270 * 16);
        fill(p, c);
        QPolygonF head;
        head << P(15.5, 3.6) << P(19.2, 7.2) << P(14.3, 7.9);
        p.drawPolygon(head);
    } else if (name == QLatin1String("layers")) {
        stroke(p, c, 1.9);
        p.drawPolygon(QPolygonF({P(12, 3.5), P(20.5, 8.5), P(12, 13.5), P(3.5, 8.5)}));
        poly(p, {P(3.5, 13.5), P(12, 18.5), P(20.5, 13.5)});
        poly(p, {P(3.5, 17.5), P(12, 22), P(20.5, 17.5)});
    } else if (name == QLatin1String("grid")) {           // 四宫格（道次分割）
        stroke(p, c, 1.9);
        p.drawRoundedRect(R(4, 4, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(R(13, 4, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(R(4, 13, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(R(13, 13, 7, 7), 1.8, 1.8);
    } else if (name == QLatin1String("step")) {           // 阶梯线（ESpos）
        stroke(p, c, 2.0);
        poly(p, {P(3.5, 19), P(9, 19), P(9, 13.5), P(14.5, 13.5), P(14.5, 8), P(20.5, 8)});
    } else if (name == QLatin1String("panels")) {
        stroke(p, c, 1.9);
        p.drawRoundedRect(R(3.5, 4.5, 17, 15), 2.5, 2.5);
        p.drawLine(P(9.5, 4.5), P(9.5, 19.5));
        fill(p, c);
        p.drawRoundedRect(R(3.7, 4.7, 5.6, 14.6), 2, 2);
    } else if (name == QLatin1String("link")) {
        stroke(p, c, 1.9);
        p.drawLine(P(8.2, 15.8), P(15.8, 8.2));
        p.drawEllipse(P(6.2, 17.8), 2.8 * kU, 2.8 * kU);
        p.drawEllipse(P(17.8, 6.2), 2.8 * kU, 2.8 * kU);
    } else if (name == QLatin1String("values")) {         // 实时曲线
        stroke(p, c, 1.9);
        p.drawPolyline(QPolygonF({P(3.5, 17), P(7.5, 9.5), P(11, 14), P(14.5, 6.5), P(20.5, 12)}));
    } else if (name == QLatin1String("detail")) {         // ⓘ
        stroke(p, c, 1.9);
        p.drawEllipse(P(12, 12), 8.3 * kU, 8.3 * kU);
        p.drawLine(P(12, 11), P(12, 16.5));
        fill(p, c);
        p.drawEllipse(P(12, 7.8), 1.2 * kU, 1.2 * kU);
    } else if (name == QLatin1String("log")) {            // 列表
        stroke(p, c, 1.9);
        p.drawLine(P(9, 7), P(20, 7));
        p.drawLine(P(9, 12), P(20, 12));
        p.drawLine(P(9, 17), P(20, 17));
        fill(p, c);
        p.drawEllipse(P(5, 7), 1.3 * kU, 1.3 * kU);
        p.drawEllipse(P(5, 12), 1.3 * kU, 1.3 * kU);
        p.drawEllipse(P(5, 17), 1.3 * kU, 1.3 * kU);
    } else if (name == QLatin1String("eye")) {            // 可见
        stroke(p, c, 1.9);
        QPainterPath eye;
        eye.moveTo(P(3, 12));
        eye.quadTo(P(12, 4.5), P(21, 12));
        eye.quadTo(P(12, 19.5), P(3, 12));
        p.drawPath(eye);
        fill(p, c);
        p.drawEllipse(P(12, 12), 2.4 * kU, 2.4 * kU);
    } else if (name == QLatin1String("eye-off")) {        // 不可见
        stroke(p, c, 1.9);
        p.drawLine(P(4.5, 4.5), P(19.5, 19.5));
        QPainterPath up;
        up.moveTo(P(3, 12));
        up.quadTo(P(12, 4.5), P(21, 12));
        p.drawPath(up);
    } else if (name == QLatin1String("more")) {           // ⋯
        fill(p, c);
        p.drawEllipse(P(5.5, 12), 1.7 * kU, 1.7 * kU);
        p.drawEllipse(P(12, 12), 1.7 * kU, 1.7 * kU);
        p.drawEllipse(P(18.5, 12), 1.7 * kU, 1.7 * kU);
    } else if (name == QLatin1String("zoom")) {
        stroke(p, c, 1.9);
        p.drawEllipse(P(10.5, 10.5), 6.2 * kU, 6.2 * kU);
        p.drawLine(P(15.2, 15.2), P(20.5, 20.5));
    } else if (name == QLatin1String("dot")) {
        fill(p, c);
        p.drawEllipse(P(12, 12), 5 * kU, 5 * kU);
    }

    p.end();
    return QIcon(pm);
}
