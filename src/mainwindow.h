#pragma once

#include <QMainWindow>
#include <QHash>
#include <QVector>
#include <QColor>

#include "model.h"
#include "recorder.h"

class QLabel;
class QPushButton;
class QButtonGroup;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class QFrame;
class QTimer;
class ChartView;
class TcpServer;

struct CardWidgets {
    QPushButton* posBtn = nullptr;
    QPushButton* forceBtn = nullptr;
    QLabel* ptsLabel = nullptr;
};

// 单个焊缝（工位）卡片上的控件
struct SeamCardWidgets {
    QFrame* card = nullptr;
    QLabel* title = nullptr;
    QLabel* state = nullptr;
    QLabel* pass = nullptr;
    QLabel* count = nullptr;
    QLabel* values = nullptr;
};

// 主窗口：把各层组装起来并做事件编排（对应 Python ecm_monitor/app.py 的 App）。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onTick();
    void onStart();
    void onPause();
    void onSave();
    void onLoad();
    void onClear();
    void toggleSegments();
    void toggleOverlay();
    void toggleEsPos();
    void toggleAutoPause();
    void onLockChanged(bool locked, const QString& msg);
    void onStatus(const QString& msg);

private:
    // 构建
    void buildToolbar();
    void buildZoomBar();
    void buildGroupsPanel();
    void buildSeamPanel();
    void buildValueBar();
    void buildChart();
    void refreshGroupsPanel();
    void refreshSeamPanel();          // 重建焊缝卡片集合（焊缝号变化时）
    void updateSeamCards();           // 只刷新焊缝卡片文字（每帧）
    QString seamSummary(const DataGroup& g) const;
    int seamPanelGroupIdx() const;    // 当前应显示哪一组的焊缝

    // 组管理
    int addGroup(const QString& name, const QVector<double>& times,
                 const QVector<int>& pos, const QVector<int>& force,
                 const QVector<int>& passNo, const QVector<int>& esPos, bool live);
    void removeGroup(int idx);
    void toggleGroupPos(int idx);
    void toggleGroupForce(int idx);
    int nextPaletteIdx() const;
    int refGroupIdx() const;
    void updateGroupPoints(DataGroup* g, int n);

    // 状态辅助
    void setStatus(const QString& msg);
    void setRecState(const QString& text, const QColor& c);
    void syncToggles();
    void rebuildOverlayStatus();
    void clearHover();
    void updateHoverActive();

    // 状态
    QVector<DataGroup*> m_groups;
    Recorder* m_recorder = nullptr;
    ChartView* m_chart = nullptr;
    TcpServer* m_server = nullptr;
    QTimer* m_tickTimer = nullptr;

    bool m_paused = false;
    QString m_zoomMode = QStringLiteral("X轴");
    bool m_showSegments = true;
    bool m_overlayMode = false;
    bool m_showEsPos = true;
    bool m_autoPause = true;
    bool m_showMonitor = true;

    // UI
    QVBoxLayout* m_rootLayout = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnPause = nullptr;
    QPushButton* m_btnSeg = nullptr;
    QPushButton* m_btnOverlay = nullptr;
    QPushButton* m_btnEsPos = nullptr;
    QPushButton* m_btnAutoPause = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_lockLabel = nullptr;
    QButtonGroup* m_zoomGroup = nullptr;

    QLabel* m_posValue = nullptr;
    QLabel* m_forceValue = nullptr;
    QLabel* m_esValue = nullptr;
    QLabel* m_errValue = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_passLabel = nullptr;
    QLabel* m_recLabel = nullptr;

    QLabel* m_seamHint = nullptr;
    QWidget* m_seamWidget = nullptr;
    QHBoxLayout* m_seamLayout = nullptr;
    QLabel* m_seamContext = nullptr;   // 数值栏：焊缝 k/8 · 阶段 · 第 n 遍
    QHash<int, SeamCardWidgets> m_seamCards;
    int m_seamCardsGroup = -2;         // 卡片当前对应的组（-2 = 未构建）
    QString m_seamCardsSig;            // 焊缝号序列签名

    QLabel* m_groupsHint = nullptr;
    QWidget* m_cardsWidget = nullptr;
    QHBoxLayout* m_cardsLayout = nullptr;
    QHash<DataGroup*, CardWidgets> m_cards;
};
