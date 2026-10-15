#pragma once

#include <QMainWindow>
#include <QHash>
#include <QVector>
#include <QSet>
#include <QColor>

#include "model.h"
#include "recorder.h"
#include "part.h"
#include "chartview.h"
#include "ecmfile.h"

class QLabel;
class QPushButton;
class QButtonGroup;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QMenu;
class QWidget;
class QFrame;
class QTimer;
class QDockWidget;
class QToolBar;
class QPlainTextEdit;
class QCloseEvent;
class ChartView;
class TcpServer;

// 数据组列表里每一行的控件（组列表是"多组对比"的控制台）
struct GroupRowWidgets {
    QPushButton* vis = nullptr;      // 参与对比显示
    QPushButton* star = nullptr;     // 设为参考组
    QPushButton* posBtn = nullptr;
    QPushButton* forceBtn = nullptr;
    QLabel* ptsLabel = nullptr;
};

// 归档与连续生产设置（持久化到 QSettings）
struct ArchiveSettings {
    QString dir;                                      // 存档根目录
    QString nameTmpl;                                 // 文件名模板
    bool autoSave = AUTO_SAVE_DEFAULT;                // 每个周期封组后自动落盘
    bool compress = ARCHIVE_COMPRESS_DEFAULT;
    bool obfuscate = ARCHIVE_OBFUSCATE_DEFAULT;
    int keepGroups = KEEP_GROUPS_DEFAULT;             // 内存里保留多少组
    PartRule rule = PartRule::PerCycle;               // 分件规则
    int timeoutMin = PART_TIMEOUT_MIN_DEFAULT;        // 超时切分（分钟）
    bool autoArm = true;                              // 打开即开始监听
};

// 主窗口：把各层组装起来并做事件编排（对应 Python ecm_monitor/app.py 的 App）。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    // 退出时保存窗口/面板布局
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onTick();
    void onStart();
    void onPause();
    void onSave();
    void onLoad();
    void onClear();
    void onNewPart();
    void onSettings();
    void onServerDisconnected();
    void togglePhaseView();
    void toggleCraftView();
    void toggleSegments();
    void toggleOverlay();
    void toggleEsPos();
    void toggleAutoPause();
    void onLockChanged(bool locked, const QString& msg);
    void onStatus(const QString& msg);

private:
    // 构建
    void buildMenuBar();
    void buildMainToolBar();
    void buildStatusBar();
    void buildDocks();
    void buildChartArea();                 // 中央区：图表工具条 + 图表
    QWidget* buildZoomControls();          // 缩放/视图控件（塞进主工具条）
    QWidget* buildGroupsPanel();
    QWidget* buildSeamPanel();
    QWidget* buildValuePanel();
    QWidget* buildDetailPanel();
    QWidget* buildLinkPanel();             // 链路状态（端口/本机 IP/连接）
    QWidget* buildLogPanel();
    QDockWidget* makeDock(const QString& title, QWidget* content, const QString& objName);
    void applyDefaultDockLayout();          // 默认列宽/高度分配（首次运行或"恢复默认布局"）
    void logMessage(const QString& msg);   // 状态消息 → 状态栏 + 日志面板
    void saveUiState();
    void restoreUiState();
    void refreshGroupsPanel();
    void refreshSeamPanel();          // 刷新焊缝栏（chip 状态 + 检测结果行）
    QString seamSummary(const DataGroup& g) const;
    int seamPanelGroupIdx() const;    // 当前应显示哪一组的焊缝
    void setSeamSelected(int seam, bool on);   // 焊缝 chip 点击
    void refreshSeamChips();                   // 只刷新 chip 的选中/高亮状态
    void applyViewFilters();                   // 把当前过滤条件推给图表
    void refreshSeamDetail();                  // 刷新"检测结果"那行文字
    void refreshLinkLabel();                   // 刷新"端口/KUKA 连接"指示灯
    void refreshLocalIps();
    void writeStartupLog();                    // 启动日志（监听结果写文件，便于现场排查）

    // ── 件 / 自动存档 ────────────────────────────────────────────
    void loadSettings();
    void saveSettings();
    QString todayArchiveDir() const;
    QString archiveNameFor(const DataGroup& g) const;
    bool autoArchiveGroup(DataGroup* g);        // 封组后自动落盘
    void enforceMemoryPolicy();                 // 内存回收（只回收已落盘的组）
    QString groupDisplayName(const DataGroup& g) const;   // "件0007 · 粗磨 · S1-4"
    bool saveGroupToFile(DataGroup* g, const QString& path, QString& err);
    bool loadGroupFromFile(const QString& path, DataGroup& g, EcmFileMeta& meta, QString& err);
    void seedPartSeqFromArchive();              // 跨重启续件号

    // 组管理
    int addGroup(const QString& name, const QVector<double>& times,
                 const QVector<int>& pos, const QVector<int>& force,
                 const QVector<int>& passNo, const QVector<int>& esPos, bool live);
    void removeGroup(int idx);
    void toggleGroupPos(int idx);
    void toggleGroupForce(int idx);
    void toggleGroupVisible(int idx);
    void setAllGroupsVisible(bool on);
    void invertGroupVisibility();
    void setRefGroup(int idx);
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
    QDockWidget* m_dockLink = nullptr;      // 🔗 链路状态
    QDockWidget* m_dockGroups = nullptr;    // 📊 数据组
    QDockWidget* m_dockSeams = nullptr;     // 🧲 焊缝
    QDockWidget* m_dockValues = nullptr;    // 📈 数值
    QDockWidget* m_dockDetail = nullptr;    // 🔍 详情
    QDockWidget* m_dockLog = nullptr;       // 📝 日志（默认隐藏）
    QToolBar* m_chartBar = nullptr;         // 图表工具条
    QMenu* m_panelMenu = nullptr;           // 工具条上的"面板"下拉（各面板显隐）
    QPlainTextEdit* m_logView = nullptr;
    QLabel* m_linkPanel = nullptr;          // 链路状态面板里的多行详情
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnPause = nullptr;
    QPushButton* m_btnSeg = nullptr;
    QPushButton* m_btnOverlay = nullptr;
    QPushButton* m_btnEsPos = nullptr;
    QPushButton* m_btnAutoPause = nullptr;
    QLabel* m_statusLabel = nullptr;   // 状态栏右侧：最近一条消息
    QLabel* m_linkLabel = nullptr;     // 状态栏左侧常驻：端口监听 / KUKA 连接
    QString m_localIps;
    bool m_listenOk = false;
    int m_tickCount = 0;
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
    QGridLayout* m_seamLayout = nullptr;
    QMenu* m_viewMenu = nullptr;       // 视图菜单（面板显隐在 buildDocks() 里补齐）
    QLabel* m_seamContext = nullptr;   // 数值栏：焊缝 k/8 · 阶段 · 第 n 遍
    QLabel* m_detectLabel = nullptr;   // 焊缝栏右侧：检测结果（高度/基材/错边/轮次）
    QHash<int, QPushButton*> m_seamChips;   // 焊缝号 -> chip
    QSet<int> m_seamSel;               // 要"突出显示"的焊缝（空 = 全部正常显示）
    int m_detailSeam = 0;              // 右侧详情显示哪条焊缝（0 = 跟随当前）
    PhaseView m_phaseView = PhaseView::All;
    CraftView m_craftView = CraftView::All;
    QPushButton* m_btnPhase = nullptr;
    QPushButton* m_btnCraft = nullptr;

    // 件与存档
    PartSplitter m_parts;
    ArchiveSettings m_arch;

    QLabel* m_groupsHint = nullptr;
    QWidget* m_cardsWidget = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;   // 组卡片竖排（放进滚动区）
    QHash<DataGroup*, GroupRowWidgets> m_groupRows;
    int m_refGroup = -1;                    // ★ 参考组（-1 = 自动：正在记录的组或第一组）
};
