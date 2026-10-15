#include "icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <cmath>

#include "theme.h"

namespace {

constexpr int kCanvas = 32;     // 实际画布（2× 便于高 DPI）
constexpr qreal kU = 2.0;       // 1 单位 = 2px
constexpr double kPi = 3.14159265358979323846;

QPointF P(qreal x, qreal y) { return QPointF(x * kU, y * kU); }

void stroke(QPainter& p, const QColor& c, qreal w = 1.6) {
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

QRectF R(qreal x, qreal y, qreal w, qreal h) {
    return QRectF(x * kU, y * kU, w * kU, h * kU);
}

}  // namespace

QIcon icons::get(const QString& name, const QColor& color) {
    const QColor c = color.isValid() ? color : theme::C_TEXT;
    QPixmap pm(kCanvas, kCanvas);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (name == QLatin1String("play")) {
        fill(p, c);
        QPolygonF t;
        t << P(5, 3.5) << P(13, 8) << P(5, 12.5);
        p.drawPolygon(t);
    } else if (name == QLatin1String("pause")) {
        fill(p, c);
        p.drawRoundedRect(R(4.6, 3.6, 2.4, 8.8), 1, 1);
        p.drawRoundedRect(R(9.0, 3.6, 2.4, 8.8), 1, 1);
    } else if (name == QLatin1String("stop")) {
        fill(p, c);
        p.drawRoundedRect(R(4.6, 4.6, 6.8, 6.8), 1.2, 1.2);
    } else if (name == QLatin1String("save")) {
        stroke(p, c);
        p.drawLine(P(8, 3.5), P(8, 10));
        QPolygonF head;
        head << P(5, 7.4) << P(8, 10.4) << P(11, 7.4);
        p.drawPolyline(head);
        QPolygonF tray;
        tray << P(3.5, 11.4) << P(3.5, 12.8) << P(12.5, 12.8) << P(12.5, 11.4);
        p.drawPolyline(tray);
    } else if (name == QLatin1String("open")) {
        stroke(p, c);
        QPainterPath path;
        path.moveTo(P(2.8, 12.4));
        path.lineTo(P(2.8, 5.2));
        path.lineTo(P(6.6, 5.2));
        path.lineTo(P(8.2, 7.0));
        path.lineTo(P(13.2, 7.0));
        path.lineTo(P(13.2, 12.4));
        path.closeSubpath();
        p.drawPath(path);
    } else if (name == QLatin1String("add")) {
        stroke(p, c, 1.8);
        p.drawLine(P(8, 4), P(8, 12));
        p.drawLine(P(4, 8), P(12, 8));
    } else if (name == QLatin1String("trash")) {
        stroke(p, c);
        p.drawLine(P(3, 4.8), P(13, 4.8));
        p.drawLine(P(6.4, 4.8), P(6.4, 3.2));
        p.drawLine(P(9.6, 4.8), P(9.6, 3.2));
        QPainterPath body;
        body.moveTo(P(4.6, 4.8));
        body.lineTo(P(5.4, 12.8));
        body.lineTo(P(10.6, 12.8));
        body.lineTo(P(11.4, 4.8));
        p.drawPath(body);
    } else if (name == QLatin1String("part")) {
        stroke(p, c);
        QPainterPath page;
        page.moveTo(P(3.6, 3.2));
        page.lineTo(P(9.4, 3.2));
        page.lineTo(P(12.4, 6.2));
        page.lineTo(P(12.4, 12.8));
        page.lineTo(P(3.6, 12.8));
        page.closeSubpath();
        p.drawPath(page);
        stroke(p, c, 1.4);
        p.drawLine(P(9.4, 3.2), P(9.4, 6.2));
        p.drawLine(P(9.4, 6.2), P(12.4, 6.2));
    } else if (name == QLatin1String("settings")) {
        stroke(p, c, 1.5);
        p.drawEllipse(P(8, 8), 2.6 * kU, 2.6 * kU);
        p.drawEllipse(P(8, 8), 5.2 * kU, 5.2 * kU);
        for (int i = 0; i < 8; ++i) {
            const double a = i * kPi / 4.0;
            const QPointF a1(8 * kU + std::cos(a) * 5.2 * kU, 8 * kU + std::sin(a) * 5.2 * kU);
            const QPointF a2(8 * kU + std::cos(a) * 6.8 * kU, 8 * kU + std::sin(a) * 6.8 * kU);
            p.drawLine(a1, a2);
        }
    } else if (name == QLatin1String("reset")) {
        stroke(p, c);
        p.drawArc(R(3.2, 3.2, 9.6, 9.6), 40 * 16, 280 * 16);
        fill(p, c);
        QPolygonF head;
        head << P(11.2, 2.6) << P(13.4, 5.0) << P(10.2, 5.4);
        p.drawPolygon(head);
    } else if (name == QLatin1String("layers")) {
        stroke(p, c, 1.5);
        QPolygonF a;
        a << P(8, 2.8) << P(13.6, 6.0) << P(8, 9.2) << P(2.4, 6.0);
        p.drawPolygon(a);
        p.drawPolyline(QPolygonF({P(2.4, 9.2), P(8, 12.4), P(13.6, 9.2)}));
    } else if (name == QLatin1String("grid")) {
        stroke(p, c, 1.4);
        p.drawRoundedRect(R(2.6, 2.6, 10.8, 10.8), 1, 1);
        p.drawLine(P(6.2, 2.6), P(6.2, 13.4));
        p.drawLine(P(9.8, 2.6), P(9.8, 13.4));
        p.drawLine(P(2.6, 6.2), P(13.4, 6.2));
        p.drawLine(P(2.6, 9.8), P(13.4, 9.8));
    } else if (name == QLatin1String("step")) {
        stroke(p, c, 1.7);
        QPolygonF s;
        s << P(2.6, 12.6) << P(6.0, 12.6) << P(6.0, 9.0) << P(9.4, 9.0)
          << P(9.4, 5.4) << P(13.4, 5.4);
        p.drawPolyline(s);
    } else if (name == QLatin1String("panels")) {
        stroke(p, c, 1.5);
        p.drawRoundedRect(R(2.4, 3.2, 11.2, 9.6), 1.2, 1.2);
        p.drawLine(P(6.4, 3.2), P(6.4, 12.8));
        fill(p, c);
        p.drawRect(R(2.6, 3.4, 3.6, 9.2));
    } else if (name == QLatin1String("link")) {
        stroke(p, c, 1.6);
        p.drawLine(P(5.6, 10.4), P(10.4, 5.6));
        p.drawEllipse(P(4.2, 11.8), 2.0 * kU, 2.0 * kU);
        p.drawEllipse(P(11.8, 4.2), 2.0 * kU, 2.0 * kU);
    } else if (name == QLatin1String("values")) {
        stroke(p, c, 1.5);
        QPolygonF ax;
        ax << P(2.8, 2.8) << P(2.8, 13.2) << P(13.2, 13.2);
        p.drawPolyline(ax);
        stroke(p, c, 1.7);
        QPolygonF w;
        w << P(4.6, 10.4) << P(7.2, 7.2) << P(9.2, 9.2) << P(12.4, 5.0);
        p.drawPolyline(w);
    } else if (name == QLatin1String("detail")) {
        stroke(p, c, 1.5);
        p.drawEllipse(P(8, 8), 5.6 * kU, 5.6 * kU);
        stroke(p, c, 1.7);
        p.drawLine(P(8, 7.2), P(8, 11.0));
        fill(p, c);
        p.drawEllipse(P(8, 4.9), 1.0 * kU, 1.0 * kU);
    } else if (name == QLatin1String("log")) {
        stroke(p, c, 1.5);
        p.drawRoundedRect(R(3.2, 3.0, 9.6, 10.0), 1.2, 1.2);
        p.drawLine(P(5.4, 6.0), P(10.6, 6.0));
        p.drawLine(P(5.4, 8.4), P(10.6, 8.4));
        p.drawLine(P(5.4, 10.6), P(9.0, 10.6));
    } else if (name == QLatin1String("more")) {
        fill(p, c);
        p.drawEllipse(P(4.2, 8), 1.3 * kU, 1.3 * kU);
        p.drawEllipse(P(8.0, 8), 1.3 * kU, 1.3 * kU);
        p.drawEllipse(P(11.8, 8), 1.3 * kU, 1.3 * kU);
    } else if (name == QLatin1String("zoom")) {
        stroke(p, c, 1.6);
        p.drawEllipse(P(7.0, 7.0), 4.0 * kU, 4.0 * kU);
        p.drawLine(P(10.0, 10.0), P(13.4, 13.4));
    } else if (name == QLatin1String("dot")) {
        fill(p, c);
        p.drawEllipse(P(8, 8), 3.2 * kU, 3.2 * kU);
    }

    p.end();
    return QIcon(pm);
}
