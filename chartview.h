#pragma once

#include <QWidget>
#include <QVector>
#include <QStringList>

#include "model.h"
#include "recorder.h"

// 图表视图：双 Y 轴 + 网格 + 道次分割 + 叠道对比 + 灰色预览 + 图例 +
// 悬停读值 + 滚轮缩放 / 拖拽平移 / 双击重置。用 QPainter 自绘，
// 等价于 Python 侧的 matplotlib 行为（DataGroup 与屏幕之间唯一的桥）。
class ChartView : public QWidget {
    Q_OBJECT
public:
    ChartView(QVector<DataGroup*>* groups, Recorder* recorder, QWidget* parent = nullptr);

    void setShowMonitor(bool v) { m_showMonitor = v; update(); }
    void setShowEsPos(bool v) { m_showEsPos = v; update(); }
    void setOverlayMode(bool v) { m_overlayMode = v; update(); }
    void setShowSegments(bool v) { m_showSegments = v; update(); }
    void setRefGroupIdx(int idx) { m_refGroupIdx = idx; update(); }
    void setXLabel(const QString& s) { m_xlabel = s; update(); }
    void setZoomMode(const QString& mode) { m_zoomMode = mode; }

    void setHoverActive(bool v) { m_hoverActive = v; if (!v) clearHover(); }
    bool hoverActive() const { return m_hoverActive; }

    bool locked() const { return m_locked; }
    void unlock() { m_locked = false; emit lockChanged(false, QString()); }
    void resetView();   // 解锁 + 重算范围
    void relimAll();    // 从可见曲线重算范围
    void clearHover();  // 清除悬停标注

signals:
    void lockChanged(bool locked, const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    // 布局与坐标变换
    QRect plotRect() const;
    double xToPx(double x) const;
    double y1ToPx(double y) const;
    double y2ToPx(double y) const;
    double pxToX(double px) const;
    double pxToY1(double py) const;
    double pxToY2(double py) const;
    DataGroup* refGroup() const;

    // 范围
    void computeBounds(double& x0, double& x1, double& y10, double& y11,
                       double& y20, double& y21,
                       bool& hasX, bool& hasY1, bool& hasY2);

    // 绘制
    void drawGrid(QPainter& p);
    void drawPassBands(QPainter& p);
    void drawPassDividers(QPainter& p);
    void drawPassAnnotations(QPainter& p);
    void drawOverlay(QPainter& p);
    void drawAxes(QPainter& p);
    void drawLegend(QPainter& p);
    void drawHover(QPainter& p);
    void drawLine(QPainter& p, const XY& xy, const QColor& color,
                  Qt::PenStyle style, double lw, bool y2, double alpha);
    void drawStair(QPainter& p, const XY& xy, const QColor& color,
                   double lw, double alpha);

    // 交互
    void startPan(const QPointF& pos);
    void doPan(const QPointF& pos);
    void endPan();
    void zoomAt(const QPointF& pos, double scale);
    void computeHover(double xm);

    // 数据
    QVector<DataGroup*>* m_groups;
    Recorder* m_recorder;

    bool m_showMonitor = true;
    bool m_showEsPos = true;
    bool m_overlayMode = false;
    bool m_showSegments = true;
    int m_refGroupIdx = -1;
    QString m_xlabel = QStringLiteral("时间  (s)");
    QString m_zoomMode = QStringLiteral("X轴");
    bool m_hoverActive = false;
    bool m_locked = false;

    double m_x0 = 0.0, m_x1 = 1.0;
    double m_y10 = 0.0, m_y11 = 1.0;
    double m_y20 = 0.0, m_y21 = 1.0;

    // 平移状态
    bool m_panActive = false;
    QPointF m_panStart;
    double m_px0, m_px1, m_py10, m_py11, m_py20, m_py21;

    // 悬停状态
    struct HoverMark {
        double tx = 0.0;
        QColor accent;
        QColor posColor;
        QColor forceColor;
        bool showPos = false;
        bool showForce = false;
        double pos = 0.0;
        double force = 0.0;
    };
    QVector<HoverMark> m_hoverMarks;
    QStringList m_hoverLines;
    double m_hoverX = 0.0;
    bool m_hoverValid = false;
};
