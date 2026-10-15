#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QDialog>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QListWidget>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <algorithm>

#include "theme.h"
#include "config.h"
#include "chartview.h"
#include "netserver.h"
#include "csvio.h"
#include "part.h"
#include "ecmfile.h"

namespace {

// 一组的"焊缝范围 + 工序"：用于组名与存档文件名
void seamRangeOf(const DataGroup& g, int* first, int* last, Craft* craft) {
    int f = 0;
    int l = 0;
    Craft c = Craft::Unknown;
    for (const Segment& s : g.seam.segments) {
        if (s.seam <= 0) {
            continue;
        }
        if (f == 0 || s.seam < f) {
            f = s.seam;
        }
        if (s.seam > l) {
            l = s.seam;
        }
        if (c == Craft::Unknown && s.craft != Craft::Unknown) {
            c = s.craft;
        }
    }
    if (first) *first = f;
    if (last) *last = l;
    if (craft) *craft = c;
}

QString phaseViewLabel(PhaseView v) {
    switch (v) {
    case PhaseView::DetectOnly: return QStringLiteral("阶段：仅检测");
    case PhaseView::GrindOnly:  return QStringLiteral("阶段：仅打磨");
    case PhaseView::GapOnly:    return QStringLiteral("阶段：仅间隙");
    case PhaseView::All:        break;
    }
    return QStringLiteral("阶段：全部");
}

QString craftViewLabel(CraftView v) {
    switch (v) {
    case CraftView::RoughOnly: return QStringLiteral("工序：仅粗磨");
    case CraftView::FineOnly:  return QStringLiteral("工序：仅细磨");
    case CraftView::All:       break;
    }
    return QStringLiteral("工序：全部");
}

QPushButton* makeBtn(const QString& text, const QColor& bg, const QColor& bgHi,
                     const QColor& fg = QColor(Qt::white), const QColor& fgHi = QColor(Qt::white)) {
    auto* b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QString(
        "QPushButton{background-color:%1;color:%2;border:none;padding:7px 14px;font-weight:bold;font-size:11pt;}"
        "QPushButton:hover{background-color:%3;color:%4;}"
        "QPushButton:disabled{background-color:%5;color:%6;}")
        .arg(bg.name(), fg.name(), bgHi.name(), fgHi.name(),
             theme::BORDER.name(), theme::C_DIM.name()));
    return b;
}

QWidget* vSeparator() {
    auto* w = new QWidget();
    w->setFixedWidth(1);
    w->setStyleSheet(QStringLiteral("background-color:%1;").arg(theme::BORDER.name()));
    w->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return w;
}

void styleToggle(QPushButton* btn, bool on, const QString& label) {
    if (on) {
        btn->setText(label + QStringLiteral(" ✓"));
        btn->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:white;border:none;padding:6px 10px;font-weight:bold;font-size:10pt;}"
            "QPushButton:hover{background-color:%2;}")
            .arg(theme::C_INFO.name(), theme::C_INFO_HI.name()));
    } else {
        btn->setText(label + QStringLiteral(" ✗"));
        btn->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;padding:6px 10px;font-weight:bold;font-size:10pt;}"
            "QPushButton:hover{background-color:%3;color:%4;}")
            .arg(theme::BORDER.name(), theme::C_DIM.name(),
                 theme::C_HOVER.name(), theme::C_TEXT.name()));
    }
}

void stylePosButton(QPushButton* b, DataGroup* g) {
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    if (g->showPos) {
        b->setText(QStringLiteral("● PosR ✓"));
        b->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;font-weight:bold;font-size:9pt;padding:3px 8px;}")
            .arg(pal.bgPos.name(), pal.pos.name()));
    } else {
        b->setText(QStringLiteral("○ PosR ✗"));
        b->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;font-weight:bold;font-size:9pt;padding:3px 8px;}")
            .arg(theme::BORDER.name(), theme::C_DIM.name()));
    }
}

void styleForceButton(QPushButton* b, DataGroup* g) {
    const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
    if (g->showForce) {
        b->setText(QStringLiteral("● ForceR ✓"));
        b->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;font-weight:bold;font-size:9pt;padding:3px 8px;}")
            .arg(pal.bgForce.name(), pal.force.name()));
    } else {
        b->setText(QStringLiteral("○ ForceR ✗"));
        b->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;font-weight:bold;font-size:9pt;padding:3px 8px;}")
            .arg(theme::BORDER.name(), theme::C_DIM.name()));
    }
}

int pickGroupDialog(QWidget* parent, const QVector<DataGroup*>& groups, const QString& title) {
    QDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setStyleSheet(QStringLiteral("background-color:%1;").arg(theme::PANEL.name()));
    auto* v = new QVBoxLayout(&dlg);

    auto* lbl = new QLabel(title, &dlg);
    lbl->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:11pt;")
                       .arg(theme::C_TEXT.name()));
    v->addWidget(lbl);

    int result = -1;
    for (int i = 0; i < groups.size(); ++i) {
        DataGroup* g = groups[i];
        const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
        auto* btn = new QPushButton(
            QStringLiteral("%1  %2  (%3 pts)")
                .arg(pal.label, g->name, theme::formatThousands(g->len())), &dlg);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:none;padding:8px 16px;font-size:10pt;text-align:left;}"
            "QPushButton:hover{background-color:%3;color:%4;}")
            .arg(theme::BG.name(), pal.accent.name(),
                 theme::BORDER.name(), theme::C_TEXT.name()));
        QObject::connect(btn, &QPushButton::clicked, &dlg, [&dlg, &result, i]() {
            result = i;
            dlg.accept();
        });
        v->addWidget(btn);
    }

    auto* cancel = new QPushButton(QStringLiteral("取消"), &dlg);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QString(
        "QPushButton{background-color:%1;color:%2;border:none;padding:8px 16px;font-size:10pt;}")
        .arg(theme::BORDER.name(), theme::C_DIM.name()));
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    v->addWidget(cancel);

    dlg.exec();
    return result;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("ECM Monitor — 实时波形（多组数据）"));
    resize(1400, 860);
    setMinimumSize(1000, 620);

    m_recorder = new Recorder(&m_groups, [this](const QString& name) {
        return addGroup(name, QVector<double>(), QVector<int>(), QVector<int>(),
                        QVector<int>(), QVector<int>(), true);
    });

    auto* central = new QWidget(this);
    auto* v = new QVBoxLayout(central);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    setCentralWidget(central);
    m_rootLayout = v;

    buildToolbar();
    buildZoomBar();
    buildGroupsPanel();
    buildSeamPanel();
    buildValueBar();
    buildChart();

    m_server = new TcpServer(this);
    connect(m_server, &TcpServer::statusChanged, this, &MainWindow::onStatus);
    connect(m_server, &TcpServer::disconnected, this, &MainWindow::onServerDisconnected);
    connect(m_server, &TcpServer::clientChanged, this, [this](bool) { refreshLinkLabel(); });
    m_listenOk = m_server->startListen(LISTEN_PORT);
    refreshLocalIps();
    refreshLinkLabel();
    if (!m_listenOk) {
        // 不用模态对话框：监控程序要能无人值守启动。失败信息常驻在指示灯与状态栏，
        // 同时也写进启动日志（见 writeStartupLog），避免"看起来在跑其实没监听"。
        setStatus(QStringLiteral("✗ 端口 %1 监听失败：%2 —— KUKA 无法连入，请检查端口占用")
                  .arg(LISTEN_PORT).arg(m_server->lastError()));
    }

    loadSettings();
    m_parts.configure(m_arch.rule, m_arch.timeoutMin);
    m_parts.reset(QDateTime::currentDateTime());
    seedPartSeqFromArchive();

    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &MainWindow::onTick);
    m_tickTimer->start(FRAME_INTERVAL_MS);

    syncToggles();
    applyViewFilters();
    writeStartupLog();      // 放在设置载入之后，日志里才能带上真实的配置

    // "打开即通讯、打开即等待数据录入"：默认自动进入监听
    // --arm 命令行参数可强制进入监听（不依赖设置；现场做开机自启/快捷方式时很方便）
    const bool forceArm = QCoreApplication::arguments().contains(QStringLiteral("--arm"));
    if (m_arch.autoArm || forceArm) {
        QTimer::singleShot(0, this, &MainWindow::onStart);
    }
}

MainWindow::~MainWindow() {
    for (DataGroup* g : m_groups) {
        delete g;
    }
}

// ── UI 构建 ─────────────────────────────────────────────────────

void MainWindow::buildToolbar() {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(QStringLiteral("background-color:%1;border-bottom:1px solid %2;")
                       .arg(theme::PANEL.name(), theme::BORDER.name()));
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(8, 6, 8, 6);
    h->setSpacing(2);

    m_btnStart = makeBtn(QStringLiteral("▶ 开始监听"), theme::C_OK, theme::C_OK_HI);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStart);
    h->addWidget(m_btnStart);

    m_btnPause = makeBtn(QStringLiteral("⏸ 暂停"), theme::C_WARN, theme::C_WARN_HI);
    m_btnPause->setEnabled(false);
    connect(m_btnPause, &QPushButton::clicked, this, &MainWindow::onPause);
    h->addWidget(m_btnPause);

    h->addSpacing(8);
    h->addWidget(vSeparator());

    auto* btnSave = makeBtn(QStringLiteral("💾 保存"), theme::C_INFO, theme::C_INFO_HI);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::onSave);
    h->addWidget(btnSave);

    auto* btnLoad = makeBtn(QStringLiteral("➕ 添加数据组"), theme::C_PURPLE, theme::C_PURPLE_HI);
    connect(btnLoad, &QPushButton::clicked, this, &MainWindow::onLoad);
    h->addWidget(btnLoad);

    auto* btnClear = makeBtn(QStringLiteral("✕ 清除全部"), theme::BORDER, theme::C_HOVER,
                             theme::C_DIM, theme::C_TEXT);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::onClear);
    h->addWidget(btnClear);

    auto* btnNewPart = makeBtn(QStringLiteral("✂ 新件"), theme::C_WARN, theme::C_WARN_HI);
    btnNewPart->setToolTip(QStringLiteral("手工切分：下一组数据算作新的一件（分件规则失效时的兜底）"));
    connect(btnNewPart, &QPushButton::clicked, this, &MainWindow::onNewPart);
    h->addWidget(btnNewPart);

    auto* btnSettings = makeBtn(QStringLiteral("⛭ 设置"), theme::BORDER, theme::C_HOVER,
                                theme::C_DIM, theme::C_TEXT);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
    h->addWidget(btnSettings);

    h->addSpacing(8);
    h->addWidget(vSeparator());

    m_btnSeg = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnSeg, &QPushButton::clicked, this, &MainWindow::toggleSegments);
    h->addWidget(m_btnSeg);

    m_btnOverlay = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnOverlay, &QPushButton::clicked, this, &MainWindow::toggleOverlay);
    h->addWidget(m_btnOverlay);

    m_btnEsPos = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnEsPos, &QPushButton::clicked, this, &MainWindow::toggleEsPos);
    h->addWidget(m_btnEsPos);

    m_btnAutoPause = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnAutoPause, &QPushButton::clicked, this, &MainWindow::toggleAutoPause);
    h->addWidget(m_btnAutoPause);

    h->addStretch();
    // 常驻连接指示灯：优先于会不断被刷新的状态文字，保证"看着是否通"这件事永远可见
    m_linkLabel = new QLabel(QStringLiteral("🔌 端口 %1 ○ 等待 KUKA 连接").arg(LISTEN_PORT), bar);
    m_linkLabel->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;")
                               .arg(theme::C_WARN_HI.name()));
    h->addWidget(m_linkLabel);
    h->addSpacing(14);

    m_statusLabel = new QLabel(QStringLiteral("◎ 正在启动…"), bar);
    m_statusLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;").arg(theme::C_DIM.name()));
    h->addWidget(m_statusLabel);

    m_rootLayout->addWidget(bar);
}

void MainWindow::buildZoomBar() {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(QStringLiteral("background-color:%1;border-bottom:1px solid %2;")
                       .arg(theme::PANEL.name(), theme::BORDER.name()));
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(12, 4, 12, 4);
    h->setSpacing(6);

    auto* lbl = new QLabel(QStringLiteral("🔍 滚轮缩放 / 拖拽平移："), bar);
    lbl->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;").arg(theme::C_DIM.name()));
    h->addWidget(lbl);

    m_zoomGroup = new QButtonGroup(bar);
    const QStringList modes{QStringLiteral("X轴"), QStringLiteral("Y轴"), QStringLiteral("双轴")};
    for (const QString& mode : modes) {
        auto* rb = new QRadioButton(mode, bar);
        rb->setCursor(Qt::PointingHandCursor);
        rb->setStyleSheet(QString(
            "QRadioButton{color:%1;font-size:10pt;}"
            "QRadioButton::indicator{width:14px;height:14px;}")
            .arg(theme::C_TEXT.name()));
        if (mode == QStringLiteral("X轴")) rb->setChecked(true);
        m_zoomGroup->addButton(rb);
        h->addWidget(rb);
        connect(rb, &QRadioButton::toggled, this, [this, mode](bool on) {
            if (on) {
                m_zoomMode = mode;
                if (m_chart) m_chart->setZoomMode(mode);
            }
        });
    }

    h->addSpacing(8);
    h->addWidget(vSeparator());

    // 拆分视图：阶段 / 工序 过滤（需求 2、4）
    m_btnPhase = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnPhase, &QPushButton::clicked, this, &MainWindow::togglePhaseView);
    h->addWidget(m_btnPhase);

    m_btnCraft = makeBtn(QString(), theme::BORDER, theme::C_HOVER);
    connect(m_btnCraft, &QPushButton::clicked, this, &MainWindow::toggleCraftView);
    h->addWidget(m_btnCraft);

    auto* btnReset = new QPushButton(QStringLiteral("↺ 重置视图"), bar);
    btnReset->setCursor(Qt::PointingHandCursor);
    btnReset->setStyleSheet(QString(
        "QPushButton{background-color:%1;color:%2;border:none;padding:4px 12px;font-size:10pt;}"
        "QPushButton:hover{background-color:%3;color:%4;}")
        .arg(theme::BORDER.name(), theme::C_DIM.name(),
             theme::C_HOVER.name(), theme::C_TEXT.name()));
    connect(btnReset, &QPushButton::clicked, this, [this]() {
        if (m_chart) m_chart->resetView();
    });
    h->addWidget(btnReset);

    auto* hint = new QLabel(QStringLiteral("（按住拖动可平移  ·  双击图表可重置）"), bar);
    hint->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
    h->addWidget(hint);

    h->addStretch();
    m_lockLabel = new QLabel(QString(), bar);
    m_lockLabel->setStyleSheet(QStringLiteral("color:#f0a500;font-weight:bold;font-size:9pt;"));
    h->addWidget(m_lockLabel);

    m_rootLayout->addWidget(bar);
}

void MainWindow::buildGroupsPanel() {
    auto* outer = new QWidget(this);
    outer->setStyleSheet(QStringLiteral("background-color:%1;border-bottom:1px solid %2;")
                         .arg(theme::PANEL.name(), theme::BORDER.name()));
    auto* v = new QVBoxLayout(outer);
    v->setContentsMargins(10, 5, 10, 7);
    v->setSpacing(4);

    auto* hdr = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("📊  数据组管理"), outer);
    title->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;")
                         .arg(theme::C_PASS.name()));
    hdr->addWidget(title);
    m_groupsHint = new QLabel(QString(), outer);
    m_groupsHint->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
    hdr->addWidget(m_groupsHint);
    hdr->addStretch();
    auto* legend = new QLabel(QStringLiteral("PosR = 实线  |  ForceR = 虚线"), outer);
    legend->setStyleSheet(QStringLiteral("color:%1;font-size:8pt;").arg(theme::C_DIM.name()));
    hdr->addWidget(legend);
    v->addLayout(hdr);

    m_cardsWidget = new QWidget(outer);
    m_cardsLayout = new QHBoxLayout(m_cardsWidget);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(8);
    v->addWidget(m_cardsWidget);

    m_rootLayout->addWidget(outer);
    refreshGroupsPanel();
}

// ── 焊缝（工位）面板 ────────────────────────────────────────────
//
// KRL 侧用 PassNo 的负数标记标出"焊缝号 + 阶段"（见 src/station.h）：
// 记录中依次点亮各焊缝卡片，卡上给出该焊缝的实时数据与打磨次数。

int MainWindow::seamPanelGroupIdx() const {
    if (m_recorder->recGroupIdx >= 0 && m_recorder->recGroupIdx < m_groups.size()) {
        return m_recorder->recGroupIdx;
    }
    return refGroupIdx();
}

void MainWindow::buildSeamPanel() {
    auto* outer = new QWidget(this);
    outer->setStyleSheet(QStringLiteral(
        "background-color:%1;border-top:1px solid %2;border-bottom:1px solid %2;")
        .arg(theme::PANEL.name(), theme::BORDER.name()));
    auto* v = new QHBoxLayout(outer);
    v->setContentsMargins(10, 4, 10, 5);
    v->setSpacing(6);

    auto* title = new QLabel(QStringLiteral("🧲  焊缝（工位）实时数据"), outer);
    title->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;")
                         .arg(theme::C_INFO_HI.name()));
    v->addWidget(title);

    m_seamWidget = new QWidget(outer);
    m_seamLayout = new QHBoxLayout(m_seamWidget);
    m_seamLayout->setContentsMargins(0, 0, 0, 0);
    m_seamLayout->setSpacing(3);
    v->addWidget(m_seamWidget);

    // 8 个 chip 常驻：显示"几号焊缝"，同时兼任"突出显示哪条焊缝"的开关。
    // 默认一个都不选 = 全部正常显示；点某号 → 这一段画粗画亮、其余淡化（仍然可见）。
    for (int k = 1; k <= SEAM_COUNT_MAX; ++k) {
        auto* b = new QPushButton(QString::number(k), m_seamWidget);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(32, 24);
        b->setToolTip(QStringLiteral("焊缝 %1：点击把它的曲线突出显示（再点取消，可多选叠加）").arg(k));
        connect(b, &QPushButton::clicked, this, [this, k]() {
            setSeamSelected(k, !m_seamSel.contains(k));
        });
        m_seamLayout->addWidget(b);
        m_seamChips.insert(k, b);
    }
    auto* btnAll = new QPushButton(QStringLiteral("全选"), m_seamWidget);
    btnAll->setToolTip(QStringLiteral("突出全部焊缝（视觉上等同正常显示）"));
    btnAll->setCursor(Qt::PointingHandCursor);
    btnAll->setFixedHeight(24);
    btnAll->setStyleSheet(QString(
        "QPushButton{background-color:%1;color:%2;border:none;padding:0 8px;font-size:9pt;}"
        "QPushButton:hover{background-color:%3;color:%4;}")
        .arg(theme::BORDER.name(), theme::C_DIM.name(),
             theme::C_HOVER.name(), theme::C_TEXT.name()));
    connect(btnAll, &QPushButton::clicked, this, [this]() {
        for (int k = 1; k <= SEAM_COUNT_MAX; ++k) m_seamSel.insert(k);
        refreshSeamChips();
        applyViewFilters();
    });
    m_seamLayout->addWidget(btnAll);

    auto* btnNone = new QPushButton(QStringLiteral("取消突出"), m_seamWidget);
    btnNone->setToolTip(QStringLiteral("清除突出显示，恢复全部正常显示"));
    btnNone->setCursor(Qt::PointingHandCursor);
    btnNone->setFixedHeight(24);
    btnNone->setStyleSheet(btnAll->styleSheet());
    connect(btnNone, &QPushButton::clicked, this, [this]() {
        m_seamSel.clear();
        refreshSeamChips();
        applyViewFilters();
    });
    m_seamLayout->addWidget(btnNone);

    v->addSpacing(10);

    // 检测结果（需求 1）：该焊缝的焊缝高度 / 基材高度 / 错边量 / 计划轮次
    m_detectLabel = new QLabel(QString(), outer);
    m_detectLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                 .arg(theme::C_PASS.name()));
    m_detectLabel->setMinimumWidth(460);
    v->addWidget(m_detectLabel);

    v->addStretch();
    m_seamHint = new QLabel(QString(), outer);
    m_seamHint->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
    v->addWidget(m_seamHint);

    m_rootLayout->addWidget(outer);

    m_seamSel.clear();            // 默认不突出任何焊缝 = 全部正常显示
    refreshSeamPanel();
}

void MainWindow::refreshSeamPanel() {
    refreshSeamChips();
    refreshSeamDetail();
}

void MainWindow::setSeamSelected(int seam, bool on) {
    if (seam < 1 || seam > SEAM_COUNT_MAX) {
        return;
    }
    if (on) {
        m_seamSel.insert(seam);
        m_detailSeam = seam;        // 顺手把详情切到这条焊缝
    } else {
        m_seamSel.remove(seam);
    }
    refreshSeamChips();
    refreshSeamDetail();
    applyViewFilters();
}

void MainWindow::applyViewFilters() {
    if (!m_chart) {
        return;
    }
    // 焊缝 chip = "突出显示"：选中的焊缝画得更粗更亮，其余淡化但**仍然显示**。
    // 不隐藏任何曲线 → 坐标范围不变，不会出现"图是空的 / 坐标轴乱跳"。
    // 8 条全选（或一个都不选）都表示"没有特别要突出的" → 全部正常显示。
    QSet<int> focus = m_seamSel;
    if (focus.size() >= SEAM_COUNT_MAX) {
        focus.clear();
    }
    m_chart->setSeamFocus(focus);
    m_chart->setPhaseView(m_phaseView);
    m_chart->setCraftView(m_craftView);

    if (focus.isEmpty()) {
        return;
    }
    QStringList names;
    for (int k = 1; k <= SEAM_COUNT_MAX; ++k) {
        if (focus.contains(k)) {
            names << QString::number(k);
        }
    }
    setStatus(QStringLiteral("已突出显示焊缝 %1 — 其余焊缝淡化显示（仍然可见），点\"取消突出\"恢复")
              .arg(names.join(QStringLiteral("、"))));
}

void MainWindow::refreshSeamChips() {
    const int gi = seamPanelGroupIdx();
    const DataGroup* g = (gi >= 0 && gi < m_groups.size()) ? m_groups[gi] : nullptr;

    for (int k = 1; k <= SEAM_COUNT_MAX; ++k) {
        QPushButton* b = m_seamChips.value(k);
        if (!b) {
            continue;
        }
        const SeamStat* s = g ? g->seam.find(k) : nullptr;
        const bool has = (s != nullptr);
        const bool sel = m_seamSel.contains(k);
        const bool liveNow = has && g->live && s->live;
        const QColor accent = theme::paletteOf(k - 1).accent;

        QString txt = QString::number(k);
        if (has && (s->normalN + s->retryN) > 0) {
            txt += QStringLiteral("│%1").arg(s->normalN + s->retryN);
        }
        b->setText(txt);
        b->setChecked(sel);

        QString bg = theme::BORDER.name();
        QString fg = theme::C_DIM.name();
        if (has) {
            bg = sel ? accent.name() : theme::BG.name();
            fg = sel ? QStringLiteral("#ffffff") : accent.name();
        }
        const QString border = liveNow ? theme::C_REC.name() : theme::BORDER.name();
        b->setStyleSheet(QString(
            "QPushButton{background-color:%1;color:%2;border:2px solid %3;"
            "border-radius:4px;font-weight:bold;font-size:9pt;padding:0;}")
            .arg(bg, fg, border));
    }
}

void MainWindow::refreshSeamDetail() {
    if (!m_detectLabel) {
        return;
    }
    const int gi = seamPanelGroupIdx();
    const DataGroup* g = (gi >= 0 && gi < m_groups.size()) ? m_groups[gi] : nullptr;
    if (!g) {
        m_detectLabel->setText(QString());
        return;
    }
    int seam = m_detailSeam;
    if (seam <= 0) {
        seam = g->seam.curSeam;
    }
    if (seam <= 0 && !g->seam.seams.isEmpty()) {
        seam = g->seam.seams.first().seam;
    }
    const SeamStat* s = (seam > 0) ? g->seam.find(seam) : nullptr;
    if (!s) {
        m_detectLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                     .arg(theme::C_DIM.name()));
        m_detectLabel->setText(QStringLiteral("焊缝 —   ·   未检测"));
        return;
    }

    auto mm = [](int um) {
        const double v = um / 1000.0;
        return QStringLiteral("%1%2").arg(v < 0 ? QStringLiteral("-") : QStringLiteral("+"))
                                     .arg(qAbs(v), 0, 'f', 3);
    };
    // KRL 侧用 -1 表示"还没检测/未赋值"（$config.dat 的初值），必须显示"—"，
    // 不能算成 -0.001 mm / -1 轮这种假数据。
    auto mmOrDash = [&mm](int um) {
        return um == DETECT_UNSET ? QStringLiteral("—") : mm(um) + QStringLiteral(" mm");
    };
    auto intOrDash = [](int v) {
        return v == DETECT_UNSET ? QStringLiteral("—") : theme::formatThousands(v);
    };

    QStringList parts;
    parts << QStringLiteral("焊缝 %1").arg(seam);
    if (s->craft != Craft::Unknown) {
        parts << craftLabel(s->craft);
    }
    if (s->hasValues) {
        parts << QStringLiteral("焊缝高度 %1").arg(mmOrDash(s->weldHum));
        parts << QStringLiteral("基材高度 %1").arg(intOrDash(s->baseH));
        parts << QStringLiteral("错边量 %1").arg(mmOrDash(s->misalignUm));
        parts << QStringLiteral("计划 %1 轮").arg(intOrDash(s->rounds));
    } else if (s->detected) {
        parts << QStringLiteral("基准 %1（本程序未上报检测值）").arg(theme::formatThousands(s->detectEs));
    } else {
        parts << QStringLiteral("未检测");
    }
    parts << QStringLiteral("正常 %1 道 / 补 %2 次").arg(s->normalN).arg(s->retryN);

    m_detectLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                 .arg(s->hasValues ? theme::C_PASS.name() : theme::C_DIM.name()));
    m_detectLabel->setText(parts.join(QStringLiteral("   ·   ")));
}

QString MainWindow::seamSummary(const DataGroup& g) const {
    QStringList parts;
    for (const SeamStat& s : g.seam.seams) {
        QString t = QStringLiteral("焊缝%1 %2道").arg(s.seam).arg(s.normalN);
        if (s.retryN > 0) {
            t += QStringLiteral("+补%1").arg(s.retryN);
        }
        parts << t;
    }
    if (parts.isEmpty()) {
        return QStringLiteral("%1 道").arg(g.passes.size());
    }
    return parts.join(QStringLiteral("，"));
}

void MainWindow::buildValueBar() {
    auto* bar = new QWidget(this);
    bar->setStyleSheet(QStringLiteral("background-color:%1;").arg(theme::BG.name()));
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(12, 4, 12, 4);
    h->setSpacing(4);

    const QColor posC = theme::paletteOf(0).pos;
    const QColor forceC = theme::paletteOf(0).force;

    auto addVal = [&](const QString& title, const QColor& color, QLabel*& outVal, int w) {
        auto* t = new QLabel(title, bar);
        t->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;").arg(color.name()));
        h->addWidget(t);
        outVal = new QLabel(QStringLiteral("—"), bar);
        outVal->setStyleSheet(QStringLiteral("color:%1;font-size:15pt;font-weight:bold;").arg(color.name()));
        outVal->setMinimumWidth(w);
        outVal->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        h->addWidget(outVal);
        h->addSpacing(24);
    };

    addVal(QStringLiteral("实时  PosR"), posC, m_posValue, 120);
    addVal(QStringLiteral("ForceR"), forceC, m_forceValue, 90);
    addVal(QStringLiteral("ESpos"), theme::C_MONITOR, m_esValue, 90);

    auto* te = new QLabel(QStringLiteral("跟随误差"), bar);
    te->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;").arg(theme::C_DIM.name()));
    h->addWidget(te);
    m_errValue = new QLabel(QStringLiteral("—"), bar);
    m_errValue->setStyleSheet(QStringLiteral("color:%1;font-size:15pt;font-weight:bold;")
                              .arg(theme::C_WARN_HI.name()));
    m_errValue->setMinimumWidth(90);
    h->addWidget(m_errValue);
    h->addSpacing(24);

    h->addStretch();

    m_countLabel = new QLabel(QString(), bar);
    m_countLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;").arg(theme::C_DIM.name()));
    h->addWidget(m_countLabel);

    m_passLabel = new QLabel(QString(), bar);
    m_passLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                               .arg(theme::C_PASS.name()));
    h->addWidget(m_passLabel);
    h->addSpacing(20);

    m_seamContext = new QLabel(QString(), bar);
    m_seamContext->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                 .arg(theme::C_INFO_HI.name()));
    h->addWidget(m_seamContext);
    h->addSpacing(16);

    m_recLabel = new QLabel(QStringLiteral("○ 未监听"), bar);
    setRecState(QStringLiteral("○ 未监听"), theme::C_DIM);
    h->addWidget(m_recLabel);
    h->addSpacing(20);

    m_rootLayout->addWidget(bar);
}

void MainWindow::buildChart() {
    m_chart = new ChartView(&m_groups, m_recorder, this);
    m_chart->setMinimumHeight(220);
    connect(m_chart, &ChartView::lockChanged, this, &MainWindow::onLockChanged);
    m_rootLayout->addWidget(m_chart, 1);
}

// ── 组管理面板 ──────────────────────────────────────────────────

void MainWindow::refreshGroupsPanel() {
    QLayoutItem* item;
    while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_cards.clear();

    int n = m_groups.size();
    m_groupsHint->setText(QStringLiteral("  %1 / %2 组已载入%3")
        .arg(n).arg(MAX_GROUPS)
        .arg(n < MAX_GROUPS
             ? QStringLiteral("  ·  自动存档目录：%1").arg(todayArchiveDir())
             : QStringLiteral("  ·  已达上限，请先删除一组再添加")));

    if (m_groups.isEmpty()) {
        auto* lbl = new QLabel(
            QStringLiteral("暂无数据组  —  点击上方\"添加数据组\"载入 CSV 文件，或按\"启动\"开始实时采集"),
            m_cardsWidget);
        lbl->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
        m_cardsLayout->addWidget(lbl);
        m_cardsLayout->addStretch();
        return;
    }

    for (int i = 0; i < m_groups.size(); ++i) {
        DataGroup* g = m_groups[i];
        const theme::Palette& pal = theme::paletteOf(g->paletteIdx);
        QString accent = pal.accent.name();

        auto* card = new QFrame(m_cardsWidget);
        card->setStyleSheet(QStringLiteral(
            "QFrame{background-color:%1;border:1px solid %2;border-left:3px solid %3;border-radius:6px;}")
            .arg(theme::PANEL.name(), theme::BORDER.name(), accent));
        auto* cv = new QVBoxLayout(card);
        cv->setContentsMargins(9, 6, 8, 7);
        cv->setSpacing(4);

        // 标题行：徽章 + 文件名 + 删除
        auto* titleRow = new QHBoxLayout();
        // 徽章：组色 + 状态（🔴 记录中 / ✓ 已存档 / ⚠ 断线强制封存，数据可能不完整）
        QString badge = pal.label + (g->live ? QStringLiteral("🔴") : QString());
        if (g->incomplete) {
            badge += QStringLiteral("⚠");
        } else if (g->saved) {
            badge += QStringLiteral("✓");
        }
        if (!g->savedPath.isEmpty()) {
            card->setToolTip(QStringLiteral("已存档：%1").arg(g->savedPath));
        } else if (!g->partLabel.isEmpty()) {
            card->setToolTip(g->partLabel);
        }
        auto* badgeLbl = new QLabel(badge, card);
        badgeLbl->setStyleSheet(QStringLiteral("background-color:%1;color:%2;padding:1px 5px;font-weight:bold;font-size:8pt;")
                                .arg(accent, theme::BG.name()));
        titleRow->addWidget(badgeLbl);
        QString shortName = g->name;
        if (shortName.size() > 20) shortName = shortName.left(18) + QStringLiteral("…");
        auto* nameLbl = new QLabel(shortName, card);
        nameLbl->setStyleSheet(QStringLiteral("color:%1;font-size:8pt;").arg(theme::C_TEXT.name()));
        titleRow->addWidget(nameLbl, 1);
        auto* delBtn = new QPushButton(QStringLiteral("×"), card);
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setFixedSize(20, 20);
        delBtn->setStyleSheet(QString(
            "QPushButton{background-color:transparent;color:%1;border:none;font-weight:bold;font-size:11pt;}"
            "QPushButton:hover{background-color:%2;color:#ff7b72;}")
            .arg(theme::C_DIM.name(), theme::C_HOVER.name()));
        connect(delBtn, &QPushButton::clicked, this, [this, i]() { removeGroup(i); });
        titleRow->addWidget(delBtn, 0, Qt::AlignTop);
        cv->addLayout(titleRow);

        // 色块行
        auto* swatch = new QHBoxLayout();
        auto* posBar = new QLabel(QString(), card);
        posBar->setFixedSize(30, 3);
        posBar->setStyleSheet(QStringLiteral("background-color:%1;").arg(pal.pos.name()));
        swatch->addWidget(posBar);
        auto* posLbl = new QLabel(QStringLiteral("PosR"), card);
        posLbl->setStyleSheet(QStringLiteral("color:%1;font-size:7pt;").arg(pal.pos.name()));
        swatch->addWidget(posLbl);
        swatch->addSpacing(8);
        auto* forceBar = new QLabel(QString(), card);
        forceBar->setFixedSize(30, 3);
        forceBar->setStyleSheet(QStringLiteral("background-color:%1;").arg(pal.force.name()));
        swatch->addWidget(forceBar);
        auto* forceLbl = new QLabel(QStringLiteral("ForceR"), card);
        forceLbl->setStyleSheet(QStringLiteral("color:%1;font-size:7pt;").arg(pal.force.name()));
        swatch->addWidget(forceLbl);
        swatch->addStretch();
        auto* ptsLbl = new QLabel(g->times.isEmpty()
                                  ? QStringLiteral("采集中…")
                                  : QStringLiteral("%1 pts").arg(theme::formatThousands(g->len())), card);
        ptsLbl->setStyleSheet(QStringLiteral("color:%1;font-size:7pt;").arg(theme::C_DIM.name()));
        swatch->addWidget(ptsLbl);
        cv->addLayout(swatch);

        // 按钮行
        auto* btnRow = new QHBoxLayout();
        auto* btnPos = new QPushButton(QString(), card);
        btnPos->setCursor(Qt::PointingHandCursor);
        connect(btnPos, &QPushButton::clicked, this, [this, i]() { toggleGroupPos(i); });
        btnRow->addWidget(btnPos);
        auto* btnForce = new QPushButton(QString(), card);
        btnForce->setCursor(Qt::PointingHandCursor);
        connect(btnForce, &QPushButton::clicked, this, [this, i]() { toggleGroupForce(i); });
        btnRow->addWidget(btnForce);
        cv->addLayout(btnRow);

        stylePosButton(btnPos, g);
        styleForceButton(btnForce, g);

        m_cardsLayout->addWidget(card, 1);
        m_cards.insert(g, CardWidgets{btnPos, btnForce, ptsLbl});
    }
    m_cardsLayout->addStretch();
}

void MainWindow::updateGroupPoints(DataGroup* g, int n) {
    auto it = m_cards.find(g);
    if (it != m_cards.end()) {
        it->ptsLabel->setText(QStringLiteral("%1 pts").arg(theme::formatThousands(n)));
    }
}

// ── 组管理 ──────────────────────────────────────────────────────

int MainWindow::nextPaletteIdx() const {
    QSet<int> used;
    for (DataGroup* g : m_groups) used.insert(g->paletteIdx % theme::PALETTE_COUNT);
    // 只在调色板范围内挑空位：连续生产的组数远超调色板数量，颜色复用是预期行为，
    // 但下标绝不能超过数组长度（paletteOf() 也会再兜一层回绕）。
    for (int i = 0; i < theme::PALETTE_COUNT; ++i) {
        if (!used.contains(i)) return i;
    }
    return 0;
}

int MainWindow::refGroupIdx() const {
    int ref = m_recorder->recGroupIdx;
    if (ref < 0 || ref >= m_groups.size()) {
        ref = m_groups.isEmpty() ? -1 : 0;
    }
    return ref;
}

int MainWindow::addGroup(const QString& name, const QVector<double>& times,
                         const QVector<int>& pos, const QVector<int>& force,
                         const QVector<int>& passNo, const QVector<int>& esPos, bool live) {
    DataGroup* g = new DataGroup();
    g->name = name;
    g->times = times;
    g->pos = pos;
    g->force = force;
    g->passNo = passNo;
    g->esPos = esPos;
    g->passes = computePasses(g->times, g->passNo);
    rebuildSeamStats(*g);
    g->hasEs = !g->esPos.isEmpty() &&
               std::any_of(g->esPos.begin(), g->esPos.end(), [](int v) { return v != 0; });
    g->paletteIdx = nextPaletteIdx();
    g->live = live;

    m_groups.append(g);
    int idx = m_groups.size() - 1;

    refreshGroupsPanel();
    refreshSeamPanel();
    m_chart->setRefGroupIdx(refGroupIdx());
    if (!m_chart->locked() && g->len()) {
        m_chart->relimAll();
    }
    m_chart->update();
    return idx;
}

void MainWindow::removeGroup(int idx) {
    if (idx >= m_groups.size()) return;
    DataGroup* g = m_groups[idx];

    if (m_recorder->noteGroupRemoved(idx)) {
        m_posValue->setText(QStringLiteral("—"));
        m_forceValue->setText(QStringLiteral("—"));
        m_esValue->setText(QStringLiteral("—"));
        m_errValue->setText(QStringLiteral("—"));
    }
    delete g;
    m_groups.removeAt(idx);

    refreshGroupsPanel();
    refreshSeamPanel();
    m_chart->setRefGroupIdx(refGroupIdx());
    if (m_overlayMode) rebuildOverlayStatus();
    if (!m_chart->locked()) m_chart->relimAll();
    m_chart->update();
}

void MainWindow::toggleGroupPos(int idx) {
    if (idx >= m_groups.size()) return;
    DataGroup* g = m_groups[idx];
    g->showPos = !g->showPos;
    refreshGroupsPanel();
    if (m_overlayMode) rebuildOverlayStatus();
    if (!m_chart->locked()) m_chart->relimAll();
    m_chart->update();
}

void MainWindow::toggleGroupForce(int idx) {
    if (idx >= m_groups.size()) return;
    DataGroup* g = m_groups[idx];
    g->showForce = !g->showForce;
    refreshGroupsPanel();
    if (m_overlayMode) rebuildOverlayStatus();
    if (!m_chart->locked()) m_chart->relimAll();
    m_chart->update();
}

// ── 状态辅助 ────────────────────────────────────────────────────

void MainWindow::setStatus(const QString& msg) {
    m_statusLabel->setText(msg);
}

void MainWindow::setRecState(const QString& text, const QColor& c) {
    m_recLabel->setText(text);
    m_recLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;").arg(c.name()));
}

void MainWindow::syncToggles() {
    styleToggle(m_btnSeg, m_showSegments, QStringLiteral("┊ 道次分割"));
    styleToggle(m_btnOverlay, m_overlayMode, QStringLiteral("≡ 叠道对比"));
    styleToggle(m_btnEsPos, m_showEsPos, QStringLiteral("⌐ ESpos"));
    styleToggle(m_btnAutoPause, m_autoPause, QStringLiteral("⏹ 完成后暂停"));
    if (m_btnPhase) {
        styleToggle(m_btnPhase, m_phaseView != PhaseView::All, phaseViewLabel(m_phaseView));
    }
    if (m_btnCraft) {
        styleToggle(m_btnCraft, m_craftView != CraftView::All, craftViewLabel(m_craftView));
    }
}

void MainWindow::rebuildOverlayStatus() {
    if (!m_overlayMode) return;
    int ref = refGroupIdx();
    if (ref < 0) {
        setStatus(QStringLiteral("⚠ 叠道对比：暂无数据组"));
        return;
    }
    DataGroup* g = m_groups[ref];
    int n = g->passes.size();
    if (!n) {
        setStatus(QStringLiteral("⚠ 叠道对比：该组没有道次信息（PassNo 全为 0）— "
                                 "确认 KRL 侧已写入 PassNo，或该 CSV 为旧格式"));
        return;
    }
    setStatus(QStringLiteral("≡ 叠道对比 — %1  共 %2 道（颜色由暗到亮 = 第1道→第%2道）")
              .arg(g->name).arg(n));
}

void MainWindow::clearHover() {
    if (m_chart) m_chart->clearHover();
}

void MainWindow::updateHoverActive() {
    if (m_chart) m_chart->setHoverActive(m_paused);
}

// ── 帧循环 ──────────────────────────────────────────────────────

void MainWindow::onTick() {
    if (++m_tickCount % 50 == 0) {
        // 每 ~6 秒刷一次：DHCP 换址、网线插拔、对端接入/断开都能及时反映出来
        refreshLocalIps();
        refreshLinkLabel();
    }
    if (m_paused || !m_server->armed()) return;

    TickResult res = m_recorder->process(m_server->queue(), m_autoPause);
    bool anyTouched = !res.touched.isEmpty();

    if (res.limitHit) {
        setStatus(QStringLiteral("⚠ 已达 %1 组上限，本次记录被忽略，请先\"清除全部\"或删除一组")
                  .arg(MAX_GROUPS));
    }
    if (res.started >= 0) {
        // 新周期开始：交给分件器决定是"同一件的新周期"还是"新的一件"
        m_parts.onCycleStart(QDateTime::currentDateTime());
        setStatus(QStringLiteral("🔴 记录中 — %1（第 %2 组）")
                  .arg(m_parts.current().label).arg(res.started));
    }

    if ((anyTouched || res.monDirty) && !m_chart->locked()) {
        m_chart->relimAll();
    }

    // 实时数值栏
    if (res.hasLastVal) {
        m_posValue->setText(theme::formatThousands(res.lastPos));
        m_forceValue->setText(theme::formatThousands(res.lastForce));
        m_esValue->setText(theme::formatThousands(res.lastEs));
        long long err = (long long)res.lastEs - res.lastPos;
        QString errs = err >= 0 ? QStringLiteral("+") + theme::formatThousands(err)
                                : theme::formatThousands(err);
        m_errValue->setText(errs);

        if (res.lastPass > 0) {
            m_passLabel->setText(QStringLiteral("⟩ 第 %1 道").arg(res.lastPass));
            m_passLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                       .arg(theme::C_PASS.name()));
        } else {
            // PassNo==0：道次之间是上一道的反向返回段；首道之前不显示
            int prevNo = 0;
            if (m_recorder->recGroupIdx >= 0 && m_recorder->recGroupIdx < m_groups.size()) {
                const DataGroup* g = m_groups[m_recorder->recGroupIdx];
                if (!g->passes.isEmpty()) {
                    prevNo = g->passes.last().no;
                }
            }
            m_passLabel->setText(prevNo ? QStringLiteral("⟩ 第 %1 道 返回段").arg(prevNo)
                                        : QStringLiteral("⟩ 非打磨段"));
            m_passLabel->setStyleSheet(QStringLiteral("color:%1;font-size:10pt;font-weight:bold;")
                                       .arg(theme::C_DIM.name()));
        }

        if (m_recorder->recGroupIdx >= 0 && m_recorder->recGroupIdx < m_groups.size()) {
            DataGroup* g = m_groups[m_recorder->recGroupIdx];
            int n = g->len();
            m_countLabel->setText(QStringLiteral("实时点数  %1   ·   已完成 %2 道")
                                  .arg(theme::formatThousands(n)).arg(g->passes.size()));
            updateGroupPoints(g, n);
        } else {
            m_countLabel->setText(QStringLiteral("预览点数  %1")
                                  .arg(theme::formatThousands(m_recorder->monTimes.size())));
        }
    }

    // 焊缝（工位）上下文与卡片
    if (res.lastSeam > 0) {
        QString t = QStringLiteral("焊缝 %1/8 · %2").arg(res.lastSeam).arg(seamPhaseLabel(res.lastPhase));
        if (res.lastPass > 0) {
            t += QStringLiteral("  第 %1 遍").arg(res.lastPass);
        }
        m_seamContext->setText(t);
    } else if (res.hasLastVal) {
        m_seamContext->setText(QStringLiteral("焊缝 —/8 · 无标记"));
    }

    // 件（料件）识别：把本帧的焊缝号/工序喂给分件器（CraftRestart / SeamWrap 规则在此判定）
    if (res.hasLastVal && res.lastSeam > 0) {
        if (m_parts.onSeamSeen(res.lastSeam, res.lastCraft, QDateTime::currentDateTime())) {
            setStatus(QStringLiteral("✂ 已按焊缝序列切分新件：%1").arg(m_parts.current().label));
        }
        m_detailSeam = 0;      // 详情跟随当前焊缝
    }

    // 记录状态指示灯
    if (m_recorder->recGroupIdx >= 0) {
        setRecState(QStringLiteral("🔴 记录中"), theme::C_REC);
    } else if (res.sealed) {
        setRecState(QStringLiteral("⏹ 已完成"), theme::C_WARN_HI);
    } else {
        setRecState(QStringLiteral("○ 等待触发"), theme::C_DIM);
    }

    if (res.structDirty) {
        refreshGroupsPanel();
    }

    if (anyTouched || res.structDirty || res.seamDirty || res.passDirty) {
        refreshSeamPanel();
    }

    if (anyTouched || res.structDirty || res.monDirty || res.passDirty) {
        m_chart->update();
    }

    // 下降沿：按"完成后暂停"开关决定冻结还是继续监听
    if (res.sealed) {
        m_parts.onCycleEnd(QDateTime::currentDateTime());
        QString label = res.sealedName;
        QString summary = QStringLiteral("%1 道").arg(res.sealedPasses);
        QString tail;
        if (res.sealedGroup) {
            DataGroup* g = res.sealedGroup;
            g->partLabel = m_parts.current().label;
            const QString display = groupDisplayName(*g);
            if (!display.isEmpty()) {
                g->name = display;
            }
            label = g->name;
            summary = seamSummary(*g);
            // 封组就自动落盘（需求 5）：失败不丢内存数据，只在状态栏报错
            autoArchiveGroup(g);
            if (g->saved) {
                tail = QStringLiteral("　已存档：%1").arg(QFileInfo(g->savedPath).fileName());
            } else if (g->incomplete) {
                tail = QStringLiteral("　⚠ 数据可能不完整");
            }
        }
        // 注意：内存回收可能把刚封的那一组也回收掉，此后不得再解引用 res.sealedGroup
        enforceMemoryPolicy();
        refreshGroupsPanel();
        if (m_autoPause) {
            m_paused = true;
            m_server->setArmed(false);
            m_btnPause->setText(QStringLiteral("▶  继续"));
            updateHoverActive();
            setStatus(QStringLiteral("⏹ %1 记录完成（%2）%3 — 已冻结暂停，点\"继续\"等待下次打磨")
                      .arg(label).arg(summary).arg(tail));
        } else {
            setStatus(QStringLiteral("⏹ %1 记录完成（%2）%3 — 继续监听，等待下次 RecordOn 触发…")
                      .arg(label).arg(summary).arg(tail));
        }
    }
}

// ── 工具栏回调 ──────────────────────────────────────────────────

void MainWindow::onStart() {
    m_server->setArmed(true);
    m_paused = false;
    m_recorder->arm();
    m_server->drain();
    m_recorder->clearMonitor();

    m_btnStart->setEnabled(false);
    m_btnPause->setEnabled(true);
    m_btnPause->setText(QStringLiteral("⏸ 暂停"));
    setRecState(QStringLiteral("○ 等待触发"), theme::C_DIM);
    setStatus(QStringLiteral("● 监听中 — 实时预览（灰）中，等待 RecordOn 触发记录…"));
    updateHoverActive();
}

void MainWindow::onPause() {
    if (!m_paused) {
        m_paused = true;
        m_server->setArmed(false);
        m_btnPause->setText(QStringLiteral("▶  继续"));
        setStatus(QStringLiteral("⏸ 已暂停 — 移动鼠标查看数值"));
        updateHoverActive();
    } else {
        m_paused = false;
        m_server->setArmed(true);
        clearHover();
        m_server->drain();
        m_recorder->clearMonitor();
        m_recorder->markPrevUnknown();
        m_btnPause->setText(QStringLiteral("⏸ 暂停"));
        setStatus(QStringLiteral("● 监听中 — 实时预览（灰）中，等待 RecordOn 触发记录…"));
        updateHoverActive();
    }
}

void MainWindow::onSave() {
    if (m_groups.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("暂无数据组可保存"));
        return;
    }
    DataGroup* g;
    if (m_groups.size() == 1) {
        g = m_groups[0];
    } else {
        int pick = pickGroupDialog(this, m_groups, QStringLiteral("选择要保存的数据组"));
        if (pick < 0) return;
        g = m_groups[pick];
    }
    if (!g || g->times.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("所选数据组暂无数据"));
        return;
    }

    QString base = g->name;
    base = sanitizeFileName(base);
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存波形数据"),
        QDir(todayArchiveDir()).filePath(archiveNameFor(*g) + QStringLiteral(".ecmb")),
        QStringLiteral("KRLmonitor 曲线 (*.ecmb);;CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    QString err;
    if (!saveGroupToFile(g, path, err)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), err);
        return;
    }
    if (!path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        g->saved = true;
        g->savedPath = path;
    }
    setStatus(QStringLiteral("✓ 已保存 %1 个点 / %2 道 → %3（%4 KB）")
              .arg(theme::formatThousands(g->len())).arg(g->passes.size())
              .arg(QFileInfo(path).fileName())
              .arg(QFileInfo(path).size() / 1024));
}

void MainWindow::onLoad() {
    if (m_groups.size() >= MAX_GROUPS) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已达到最多 %1 组限制\n请先删除一组后再添加").arg(MAX_GROUPS));
        return;
    }
    QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择波形数据文件（可多选）"), todayArchiveDir(),
        QStringLiteral("KRLmonitor 曲线 (*.ecmb);;CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (paths.isEmpty()) return;

    int loaded = 0;
    for (const QString& path : paths) {
        if (m_groups.size() >= MAX_GROUPS) {
            QMessageBox::information(this, QStringLiteral("提示"),
                QStringLiteral("已达到最多 %1 组限制，剩余 %2 个文件未载入")
                    .arg(MAX_GROUPS).arg(paths.size() - loaded));
            break;
        }
        DataGroup tmp;
        EcmFileMeta meta;
        QString err;
        if (!loadGroupFromFile(path, tmp, meta, err)) {
            QMessageBox::critical(this, QStringLiteral("载入失败"),
                                  QFileInfo(path).fileName() + QStringLiteral("\n") + err);
            continue;
        }
        QString name = meta.groupName.isEmpty() ? QFileInfo(path).completeBaseName() : meta.groupName;
        int idx = addGroup(name, tmp.times, tmp.pos, tmp.force, tmp.passNo, tmp.esPos, false);
        m_groups[idx]->hasEs = tmp.hasEs;
        m_groups[idx]->incomplete = tmp.incomplete;
        m_groups[idx]->seam.segments = tmp.seam.segments;
        m_groups[idx]->seam.seams = tmp.seam.seams;
        m_groups[idx]->seam.curSeam = 0;
        m_groups[idx]->seam.curPhase = SeamPhase::None;
        m_groups[idx]->seam.curCraft = Craft::Unknown;
        m_groups[idx]->seam.refreshLive();
        m_groups[idx]->partLabel = meta.partLabel;
        m_groups[idx]->saved = true;
        m_groups[idx]->savedPath = path;
        loaded++;
        int npass = m_groups[idx]->passes.size();
        setStatus(QStringLiteral("✓ 已载入 %1 个点%2 ← %3")
                  .arg(theme::formatThousands(tmp.times.size()))
                  .arg(npass ? QStringLiteral(" / %1 道").arg(npass)
                             : QStringLiteral("（无道次信息，旧格式 CSV）"))
                  .arg(name));
    }

    if (loaded > 0) {
        m_paused = true;
        m_btnPause->setEnabled(true);
        m_btnPause->setText(QStringLiteral("▶  继续"));
        setStatus(QStringLiteral("✓ 共载入 %1 组数据  —  移动鼠标查看数值").arg(loaded));
        updateHoverActive();
    }
}

void MainWindow::onClear() {
    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("清除所有数据组？")) != QMessageBox::Yes) {
        return;
    }
    m_server->setArmed(false);
    m_paused = false;
    m_chart->unlock();
    m_lockLabel->setText(QString());
    m_recorder->reset();
    clearHover();
    m_server->drain();

    for (DataGroup* g : m_groups) delete g;
    m_groups.clear();
    m_chart->setRefGroupIdx(-1);

    refreshSeamPanel();
    if (m_seamContext) m_seamContext->setText(QString());

    refreshGroupsPanel();
    m_chart->relimAll();

    m_btnStart->setEnabled(true);
    m_btnPause->setEnabled(false);
    m_btnPause->setText(QStringLiteral("⏸ 暂停"));
    m_posValue->setText(QStringLiteral("—"));
    m_forceValue->setText(QStringLiteral("—"));
    m_esValue->setText(QStringLiteral("—"));
    m_errValue->setText(QStringLiteral("—"));
    m_countLabel->setText(QString());
    setRecState(QStringLiteral("○ 未监听"), theme::C_DIM);
    setStatus(QStringLiteral("已清除全部数据组"));
    updateHoverActive();
}

void MainWindow::toggleAutoPause() {
    m_autoPause = !m_autoPause;
    syncToggles();
    setStatus(m_autoPause
              ? QStringLiteral("⏹ 打磨完成（RecordOn 下降沿）后将冻结暂停")
              : QStringLiteral("▶ 打磨完成后继续监听，连续多次打磨自动记成多组"));
}

void MainWindow::toggleSegments() {
    m_showSegments = !m_showSegments;
    syncToggles();
    m_chart->setShowSegments(m_showSegments);
}

void MainWindow::toggleEsPos() {
    m_showEsPos = !m_showEsPos;
    m_chart->setShowEsPos(m_showEsPos);
    syncToggles();
    if (!m_chart->locked()) m_chart->relimAll();
}

void MainWindow::toggleOverlay() {
    m_overlayMode = !m_overlayMode;
    syncToggles();
    m_chart->setOverlayMode(m_overlayMode);
    m_chart->setRefGroupIdx(refGroupIdx());
    m_chart->setXLabel(m_overlayMode ? QStringLiteral("道次内相对时间  (s)")
                                     : QStringLiteral("时间  (s)"));
    m_chart->unlock();
    m_lockLabel->setText(QString());
    m_chart->relimAll();
    rebuildOverlayStatus();
    if (!m_overlayMode) {
        setStatus(QStringLiteral("● 已退出叠道对比"));
    }
}

// ── 信号槽 ──────────────────────────────────────────────────────

void MainWindow::onLockChanged(bool locked, const QString& msg) {
    Q_UNUSED(locked);
    m_lockLabel->setText(msg);
}

void MainWindow::onStatus(const QString& msg) {
    m_statusLabel->setText(msg);
}

// ── 拆分视图：阶段 / 工序 过滤 ──────────────────────────────────

void MainWindow::togglePhaseView() {
    switch (m_phaseView) {
    case PhaseView::All:        m_phaseView = PhaseView::DetectOnly; break;
    case PhaseView::DetectOnly: m_phaseView = PhaseView::GrindOnly;  break;
    case PhaseView::GrindOnly:  m_phaseView = PhaseView::GapOnly;    break;
    case PhaseView::GapOnly:    m_phaseView = PhaseView::All;        break;
    }
    syncToggles();
    applyViewFilters();
    setStatus(m_phaseView == PhaseView::All
                  ? QStringLiteral("阶段过滤：全部（检测 / 打磨 / 间隙）")
                  : QStringLiteral("阶段过滤：%1").arg(phaseViewLabel(m_phaseView).mid(3)));
}

void MainWindow::toggleCraftView() {
    switch (m_craftView) {
    case CraftView::All:       m_craftView = CraftView::RoughOnly; break;
    case CraftView::RoughOnly: m_craftView = CraftView::FineOnly;  break;
    case CraftView::FineOnly:  m_craftView = CraftView::All;       break;
    }
    syncToggles();
    applyViewFilters();
    setStatus(m_craftView == CraftView::All
                  ? QStringLiteral("工序过滤：全部")
                  : QStringLiteral("工序过滤：%1").arg(craftViewLabel(m_craftView).mid(3)));
}

// ── 件 / 存档 ───────────────────────────────────────────────────

QString MainWindow::todayArchiveDir() const {
    const QString root = m_arch.dir.isEmpty()
                             ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                   + QStringLiteral("/KRLmonitor")
                             : m_arch.dir;
    return root + QStringLiteral("/") + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

QString MainWindow::groupDisplayName(const DataGroup& g) const {
    int first = 0;
    int last = 0;
    Craft craft = Craft::Unknown;
    seamRangeOf(g, &first, &last, &craft);
    QStringList parts;
    if (!g.partLabel.isEmpty()) {
        parts << g.partLabel;
    }
    if (craft != Craft::Unknown) {
        parts << craftLabel(craft);
    }
    if (first > 0) {
        parts << (first == last ? QStringLiteral("S%1").arg(first)
                                : QStringLiteral("S%1-%2").arg(first).arg(last));
    }
    if (parts.isEmpty()) {
        return g.name;
    }
    return parts.join(QStringLiteral(" · "));
}

QString MainWindow::archiveNameFor(const DataGroup& g) const {
    int first = 0;
    int last = 0;
    Craft craft = Craft::Unknown;
    seamRangeOf(g, &first, &last, &craft);
    PartInfo p;
    p.label = g.partLabel;
    return buildArchiveName(m_arch.nameTmpl, QDateTime::currentDateTime(), p, craft, first, last);
}

bool MainWindow::saveGroupToFile(DataGroup* g, const QString& path, QString& err) {
    if (!g) {
        err = QStringLiteral("数据组为空");
        return false;
    }
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        return saveCsv(path, *g, err);        // 兼容旧格式导出（给 Excel / 老工具用）
    }
    EcmFileMeta meta;
    meta.groupName = g->name;
    meta.partLabel = g->partLabel;
    meta.partId = m_parts.current().id;
    meta.incomplete = g->incomplete;
    EcmSaveOptions opt;
    opt.compress = m_arch.compress;
    opt.obfuscate = m_arch.obfuscate;
    return saveEcm(path, *g, meta, opt, err);
}

bool MainWindow::loadGroupFromFile(const QString& path, DataGroup& g, EcmFileMeta& meta, QString& err) {
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        CsvData d;
        if (!loadCsv(path, d, err)) {
            return false;
        }
        g.times = d.times;
        g.pos = d.pos;
        g.force = d.force;
        g.passNo = d.passNo;
        g.esPos = d.esPos;
        g.passes = computePasses(g.times, g.passNo);
        rebuildSeamStats(g);
        for (int v : g.esPos) {
            if (v != 0) {
                g.hasEs = true;
                break;
            }
        }
        meta = EcmFileMeta();
        meta.groupName = QFileInfo(path).completeBaseName();
        return true;
    }
    return loadEcm(path, g, meta, err);
}

bool MainWindow::autoArchiveGroup(DataGroup* g) {
    if (!g || !m_arch.autoSave || g->times.isEmpty()) {
        return false;
    }
    const QString dir = todayArchiveDir();
    QDir d(dir);
    if (!d.exists() && !d.mkpath(QStringLiteral("."))) {
        setStatus(QStringLiteral("⚠ 自动存档失败：无法创建目录 %1").arg(dir));
        return false;
    }
    QString base = archiveNameFor(*g);
    if (base.isEmpty()) {
        base = QStringLiteral("未命名");
    }
    QString path = dir + QStringLiteral("/") + base + QStringLiteral(".ecmb");
    int n = 2;
    while (QFile::exists(path)) {
        path = dir + QStringLiteral("/") + base + QStringLiteral("-%1.ecmb").arg(n++);
    }
    QString err;
    if (!saveGroupToFile(g, path, err)) {
        // 存档失败绝不丢数据：只在状态栏明确报错
        setStatus(QStringLiteral("⚠ 自动存档失败：%1（数据仍在内存中，可点\"保存\"手工导出）").arg(err));
        return false;
    }
    g->saved = true;
    g->savedPath = path;
    return true;
}

void MainWindow::enforceMemoryPolicy() {
    const int keep = qMax(1, m_arch.keepGroups);
    while (m_groups.size() > keep) {
        int victim = -1;
        for (int i = 0; i < m_groups.size(); ++i) {
            const DataGroup* g = m_groups[i];
            if (g->saved && !g->live) {     // 只回收"已经落盘"的组
                victim = i;
                break;
            }
        }
        if (victim < 0) {
            break;      // 没有可回收的：宁可多占内存，也不丢没落盘的数据
        }
        const QString name = m_groups[victim]->name;
        removeGroup(victim);
        setStatus(QStringLiteral("♻ 已落盘并回收内存：%1（可在存档目录重新载入）").arg(name));
    }
}

void MainWindow::onNewPart() {
    m_parts.startNewPart(QDateTime::currentDateTime());
    setStatus(QStringLiteral("✂ 已手工切分：下一组数据记为 %1").arg(m_parts.current().label));
}

void MainWindow::onServerDisconnected() {
    // 断线必须封住当前周期：否则重连后 RecordOn 已经是 TRUE，
    // 下一件的数据会被并进上一件那一组（时间轴错乱、焊缝统计串台）。
    TickResult res;
    if (m_recorder->forceSeal(res)) {
        if (res.sealedGroup) {
            res.sealedGroup->partLabel = m_parts.current().label;
            const QString display = groupDisplayName(*res.sealedGroup);
            if (!display.isEmpty()) {
                res.sealedGroup->name = display;
            }
            autoArchiveGroup(res.sealedGroup);
        }
        m_parts.onCycleEnd(QDateTime::currentDateTime());
        enforceMemoryPolicy();
        refreshGroupsPanel();
        refreshSeamPanel();
        m_chart->update();
        setStatus(QStringLiteral("⚠ KUKA 断线：已强制封存当前周期（标记为可能不完整）"));
    }
}

void MainWindow::seedPartSeqFromArchive() {
    const QDir d(todayArchiveDir());
    if (!d.exists()) {
        return;
    }
    // 只用于"续号"：取当日已归档的组数作为起始序号（宁可偏大，重名由 -2/-3 兜底）
    const int n = d.entryList(QStringList{QStringLiteral("*.ecmb")}, QDir::Files).size();
    if (n > 0) {
        m_parts.setStartSeq(n);
    }
}

// ── 设置 ────────────────────────────────────────────────────────

void MainWindow::loadSettings() {
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("KRLmonitor"), QStringLiteral("settings"));
    m_arch.dir = s.value(QStringLiteral("archive/dir")).toString();
    m_arch.nameTmpl = s.value(QStringLiteral("archive/nameTmpl"),
                              QString::fromLatin1(ARCHIVE_NAME_TMPL)).toString();
    m_arch.autoSave = s.value(QStringLiteral("archive/autoSave"), AUTO_SAVE_DEFAULT).toBool();
    m_arch.compress = s.value(QStringLiteral("archive/compress"), ARCHIVE_COMPRESS_DEFAULT).toBool();
    m_arch.obfuscate = s.value(QStringLiteral("archive/obfuscate"), ARCHIVE_OBFUSCATE_DEFAULT).toBool();
    m_arch.keepGroups = s.value(QStringLiteral("view/keepGroups"), KEEP_GROUPS_DEFAULT).toInt();
    m_arch.timeoutMin = s.value(QStringLiteral("part/timeoutMin"), PART_TIMEOUT_MIN_DEFAULT).toInt();
    m_arch.autoArm = s.value(QStringLiteral("run/autoArm"), true).toBool();
    m_arch.rule = PartRule(s.value(QStringLiteral("part/rule"), int(PartRule::PerCycle)).toInt());
}

void MainWindow::saveSettings() {
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QStringLiteral("KRLmonitor"), QStringLiteral("settings"));
    s.setValue(QStringLiteral("archive/dir"), m_arch.dir);
    s.setValue(QStringLiteral("archive/nameTmpl"), m_arch.nameTmpl);
    s.setValue(QStringLiteral("archive/autoSave"), m_arch.autoSave);
    s.setValue(QStringLiteral("archive/compress"), m_arch.compress);
    s.setValue(QStringLiteral("archive/obfuscate"), m_arch.obfuscate);
    s.setValue(QStringLiteral("view/keepGroups"), m_arch.keepGroups);
    s.setValue(QStringLiteral("part/timeoutMin"), m_arch.timeoutMin);
    s.setValue(QStringLiteral("part/rule"), int(m_arch.rule));
    s.setValue(QStringLiteral("run/autoArm"), m_arch.autoArm);
    s.sync();
}

// 连接指示灯：把"是否在监听 / KUKA 是否连上 / 本机 IP 是多少"三件事常驻在工具栏右侧。
// 这三者正好是 ECMMonitor 通讯排查的第一步（XML 里的 <IP> 必须等于这里的本机地址）。
void MainWindow::refreshLocalIps() {
    QStringList ips;
    const QList<QHostAddress> addrs = QNetworkInterface::allAddresses();
    for (const QHostAddress& a : addrs) {
        if (a.protocol() != QAbstractSocket::IPv4Protocol) {
            continue;
        }
        const QString s = a.toString();
        if (s.startsWith(QLatin1String("127.")) || s.startsWith(QLatin1String("169.254."))) {
            continue;   // 回环 / 自动私有地址不算
        }
        ips << s;
    }
    m_localIps = ips.isEmpty() ? QStringLiteral("(未取到本机 IP)") : ips.join(QStringLiteral(" / "));
}

// 启动日志：把"监听结果 / 本机 IP / 关键配置"落到文件里。
// 现场的典型问题是"程序看着在跑，其实没在监听"或"监听了但 IP 和 XML 不一致"，
// 这两种都能从这一行日志直接判断出来。
void MainWindow::writeStartupLog() {
    const QString line =
        QStringLiteral("[%1] exe=%2 listen=%3 err=%4 ips=%5 keepGroups=%6 rule=%7 autoArm=%8 autoSave=%9\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 QCoreApplication::applicationFilePath(),
                 m_listenOk ? QStringLiteral("OK") : QStringLiteral("FAIL"),
                 m_server ? (m_server->lastError().isEmpty() ? QStringLiteral("-") : m_server->lastError())
                          : QStringLiteral("-"),
                 m_localIps)
            .arg(m_arch.keepGroups)
            .arg(int(m_arch.rule))
            .arg(m_arch.autoArm ? 1 : 0)
            .arg(m_arch.autoSave ? 1 : 0);

    // 优先写在 exe 旁边（现场最容易找到）；不行再退回存档目录
    const QStringList dirs{QCoreApplication::applicationDirPath(), todayArchiveDir()};
    for (const QString& d : dirs) {
        QDir dir(d);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            continue;
        }
        QFile f(dir.filePath(QStringLiteral("KRLmonitor.log")));
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            f.write(line.toUtf8());
            f.close();
            break;
        }
    }
}

void MainWindow::refreshLinkLabel() {
    if (!m_linkLabel) {
        return;
    }
    QString txt;
    QColor c = theme::C_DIM;
    if (!m_listenOk) {
        txt = QStringLiteral("🔌 端口 %1 ✗ 监听失败").arg(LISTEN_PORT);
        c = theme::C_REC;
    } else if (m_server && m_server->hasClient()) {
        txt = QStringLiteral("🔌 端口 %1 ● KUKA 已连接").arg(LISTEN_PORT);
        c = theme::C_OK;
    } else {
        txt = QStringLiteral("🔌 端口 %1 ○ 等待 KUKA 连接　本机 %2").arg(LISTEN_PORT).arg(m_localIps);
        c = theme::C_WARN_HI;
    }
    m_linkLabel->setText(txt);
    m_linkLabel->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;").arg(c.name()));
    m_linkLabel->setToolTip(QStringLiteral(
        "KUKA 控制器上 EthernetKRL\\ECMMonitor\\config.xml 里的 <IP> 必须等于这里显示的本机地址，"
        "<PORT> 必须是 %1。\n若此处显示「监听失败」，说明端口被占用或权限被拦。").arg(LISTEN_PORT));
}

void MainWindow::onSettings() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("设置"));
    dlg.setStyleSheet(QStringLiteral("background-color:%1;color:%2;")
                      .arg(theme::PANEL.name(), theme::C_TEXT.name()));
    auto* form = new QFormLayout(&dlg);

    auto* dir = new QLineEdit(m_arch.dir.isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                        + QStringLiteral("/KRLmonitor")
                                  : m_arch.dir,
                              &dlg);
    form->addRow(QStringLiteral("存档目录"), dir);
    auto* tmpl = new QLineEdit(m_arch.nameTmpl, &dlg);
    tmpl->setToolTip(QStringLiteral("占位符：{time} 到分钟、{date}、{part}、{craft}、{seams}"));
    form->addRow(QStringLiteral("文件名模板"), tmpl);
    auto* keep = new QSpinBox(&dlg);
    keep->setRange(1, MAX_GROUPS);
    keep->setValue(m_arch.keepGroups);
    keep->setToolTip(QStringLiteral("内存里保留多少组；更早的组落盘后自动回收"));
    form->addRow(QStringLiteral("内存保留组数"), keep);

    auto* rule = new QComboBox(&dlg);
    rule->addItem(QStringLiteral("一次程序运行 = 一件（推荐）"), int(PartRule::PerCycle));
    rule->addItem(QStringLiteral("细磨回到粗磨 = 新件"), int(PartRule::CraftRestart));
    rule->addItem(QStringLiteral("焊缝号回绕 = 新件"), int(PartRule::SeamWrap));
    rule->setCurrentIndex(qMax(0, rule->findData(int(m_arch.rule))));
    form->addRow(QStringLiteral("分件规则"), rule);
    auto* timeout = new QSpinBox(&dlg);
    timeout->setRange(0, 600);
    timeout->setValue(m_arch.timeoutMin);
    timeout->setToolTip(QStringLiteral("超过这么久没有数据就视为新的一件（0 = 关闭）"));
    form->addRow(QStringLiteral("超时切分（分钟）"), timeout);

    auto* autoSave = new QCheckBox(QStringLiteral("每个周期封组后自动存档"), &dlg);
    autoSave->setChecked(m_arch.autoSave);
    form->addRow(QString(), autoSave);
    auto* compress = new QCheckBox(QStringLiteral("压缩（体积约为 CSV 的 1/10）"), &dlg);
    compress->setChecked(m_arch.compress);
    form->addRow(QString(), compress);
    auto* obf = new QCheckBox(QStringLiteral("混淆（只有本软件能读）"), &dlg);
    obf->setChecked(m_arch.obfuscate);
    form->addRow(QString(), obf);
    auto* autoArm = new QCheckBox(QStringLiteral("打开程序即开始监听"), &dlg);
    autoArm->setChecked(m_arch.autoArm);
    form->addRow(QString(), autoArm);

    auto* ok = new QPushButton(QStringLiteral("确定"), &dlg);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    form->addRow(QString(), ok);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_arch.dir = dir->text().trimmed();
    m_arch.nameTmpl = tmpl->text().trimmed();
    m_arch.keepGroups = keep->value();
    m_arch.rule = PartRule(rule->currentData().toInt());
    m_arch.timeoutMin = timeout->value();
    m_arch.autoSave = autoSave->isChecked();
    m_arch.compress = compress->isChecked();
    m_arch.obfuscate = obf->isChecked();
    m_arch.autoArm = autoArm->isChecked();
    m_parts.configure(m_arch.rule, m_arch.timeoutMin);
    saveSettings();
    enforceMemoryPolicy();
    setStatus(QStringLiteral("✓ 设置已保存（存档目录：%1）").arg(todayArchiveDir()));
}
