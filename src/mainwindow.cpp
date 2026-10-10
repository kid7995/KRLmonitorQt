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
#include <algorithm>

#include "theme.h"
#include "config.h"
#include "chartview.h"
#include "netserver.h"
#include "csvio.h"

namespace {

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
    const theme::Palette& pal = theme::GROUP_PALETTES[g->paletteIdx];
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
    const theme::Palette& pal = theme::GROUP_PALETTES[g->paletteIdx];
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
        const theme::Palette& pal = theme::GROUP_PALETTES[g->paletteIdx];
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
    m_server->startListen(LISTEN_PORT);

    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &MainWindow::onTick);
    m_tickTimer->start(FRAME_INTERVAL_MS);

    syncToggles();
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
    auto* v = new QVBoxLayout(outer);
    v->setContentsMargins(10, 5, 10, 7);
    v->setSpacing(4);

    auto* hdr = new QHBoxLayout();
    auto* title = new QLabel(QStringLiteral("🧲  焊缝（工位）实时数据"), outer);
    title->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;")
                         .arg(theme::C_INFO_HI.name()));
    hdr->addWidget(title);
    m_seamHint = new QLabel(QString(), outer);
    m_seamHint->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
    hdr->addWidget(m_seamHint);
    hdr->addStretch();
    auto* legend = new QLabel(QStringLiteral("焊缝号由 KRL 侧 PassNo 负数标记给出（-k 检测 / -(10+k) 打磨间隙）"), outer);
    legend->setStyleSheet(QStringLiteral("color:%1;font-size:8pt;").arg(theme::C_DIM.name()));
    hdr->addWidget(legend);
    v->addLayout(hdr);

    m_seamWidget = new QWidget(outer);
    m_seamLayout = new QHBoxLayout(m_seamWidget);
    m_seamLayout->setContentsMargins(0, 0, 0, 0);
    m_seamLayout->setSpacing(8);
    v->addWidget(m_seamWidget);

    m_rootLayout->addWidget(outer);
    refreshSeamPanel();
}

void MainWindow::refreshSeamPanel() {
    const int gi = seamPanelGroupIdx();
    QVector<int> seamList;
    if (gi >= 0 && gi < m_groups.size()) {
        for (const SeamStat& s : m_groups[gi]->seam.seams) {
            seamList.append(s.seam);
        }
    }
    std::sort(seamList.begin(), seamList.end());

    QString sig;
    for (int seam : seamList) {
        sig += QStringLiteral("%1,").arg(seam);
    }
    if (gi == m_seamCardsGroup && sig == m_seamCardsSig) {
        updateSeamCards();      // 集合没变，只刷新文字
        return;
    }
    m_seamCardsGroup = gi;
    m_seamCardsSig = sig;
    m_seamCards.clear();

    QLayoutItem* item;
    while ((item = m_seamLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (seamList.isEmpty()) {
        const QString msg = (gi >= 0 && gi < m_groups.size())
            ? QStringLiteral("该数据组不含焊缝标记（旧程序 / 旧 CSV：PassNo 全为非负数）")
            : QStringLiteral("开始监控或载入数据后，这里显示各焊缝（工位）的实时数据与打磨次数");
        auto* lbl = new QLabel(msg, m_seamWidget);
        lbl->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_DIM.name()));
        m_seamLayout->addWidget(lbl);
        m_seamLayout->addStretch();
        m_seamHint->setText(QString());
        return;
    }

    m_seamHint->setText(QStringLiteral("  共 %1 个焊缝").arg(seamList.size()));

    for (int seam : seamList) {
        const QColor accent = theme::GROUP_PALETTES[(seam - 1) % 4].accent;
        auto* card = new QFrame(m_seamWidget);
        card->setStyleSheet(QStringLiteral(
            "QFrame{background-color:%1;border:1px solid %2;border-left:3px solid %3;border-radius:6px;}")
            .arg(theme::BG.name(), theme::BORDER.name(), accent.name()));
        auto* cv = new QVBoxLayout(card);
        cv->setContentsMargins(9, 6, 8, 7);
        cv->setSpacing(3);

        SeamCardWidgets w;
        w.card = card;
        w.title = new QLabel(QStringLiteral("焊缝 %1").arg(seam), card);
        w.title->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;font-size:10pt;")
                               .arg(accent.name()));
        cv->addWidget(w.title);
        w.state = new QLabel(QStringLiteral("—"), card);
        cv->addWidget(w.state);
        w.pass = new QLabel(QStringLiteral("—"), card);
        w.pass->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;").arg(theme::C_TEXT.name()));
        cv->addWidget(w.pass);
        w.count = new QLabel(QStringLiteral("正常 0 道 · 补充 0 次"), card);
        cv->addWidget(w.count);
        w.values = new QLabel(QStringLiteral("PosR  —\nForceR  —\nESpos  —"), card);
        w.values->setStyleSheet(QStringLiteral("color:%1;font-size:8pt;").arg(theme::C_DIM.name()));
        cv->addWidget(w.values);

        m_seamLayout->addWidget(card, 1);
        m_seamCards.insert(seam, w);
    }
    m_seamLayout->addStretch();
    updateSeamCards();
}

void MainWindow::updateSeamCards() {
    const int gi = seamPanelGroupIdx();
    if (gi < 0 || gi >= m_groups.size()) {
        return;
    }
    DataGroup* g = m_groups[gi];

    for (auto it = m_seamCards.begin(); it != m_seamCards.end(); ++it) {
        const SeamStat* s = g->seam.find(it.key());
        if (!s) {
            continue;
        }
        SeamCardWidgets& w = it.value();

        QString st = QStringLiteral("○ 已完成");
        QColor stc = theme::C_DIM;
        if (g->live) {
            if (s->live) {
                const bool grinding = (g->seam.curPhase == SeamPhase::Grind
                                       || g->seam.curPhase == SeamPhase::GrindGap
                                       || g->seam.curPhase == SeamPhase::Transfer);
                st = grinding ? QStringLiteral("● 打磨中") : QStringLiteral("● 检测中");
                stc = grinding ? theme::C_REC : theme::C_WARN_HI;
            } else if (s->detected && s->curPass == 0) {
                st = QStringLiteral("○ 待打磨");
            }
        }
        w.state->setText(st);
        w.state->setStyleSheet(QStringLiteral("color:%1;font-size:9pt;font-weight:bold;")
                               .arg(stc.name()));

        QString passTxt = QStringLiteral("—");
        if (s->curPass > 0) {
            passTxt = QStringLiteral("第 %1 遍").arg(s->curPass);
            if (s->retryNow) {
                passTxt += QStringLiteral("（补充打磨）");
            }
        }
        w.pass->setText(passTxt);

        w.count->setText(QStringLiteral("正常 %1 道 · 补充 %2 次").arg(s->normalN).arg(s->retryN));
        w.count->setStyleSheet(QStringLiteral("color:%1;font-size:8pt;")
                               .arg(s->retryN ? theme::C_WARN_HI.name() : theme::C_PASS.name()));

        QString vals = QStringLiteral("PosR  %1\nForceR  %2\nESpos  %3")
                           .arg(theme::formatThousands(s->lastPos),
                                theme::formatThousands(s->lastForce),
                                theme::formatThousands(s->lastEs));
        if (s->detected) {
            vals += QStringLiteral("\n基准  %1（检测）").arg(theme::formatThousands(s->detectEs));
        }
        w.values->setText(vals);
    }
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

    const QColor posC = theme::GROUP_PALETTES[0].pos;
    const QColor forceC = theme::GROUP_PALETTES[0].force;

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
             ? QStringLiteral("  ·  点击\"添加数据组\"导入 CSV 文件")
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
        const theme::Palette& pal = theme::GROUP_PALETTES[g->paletteIdx];
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
        QString badge = pal.label + (g->live ? QStringLiteral("🔴") : QString());
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
    for (DataGroup* g : m_groups) used.insert(g->paletteIdx);
    for (int i = 0; i < MAX_GROUPS; ++i) {
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
    if (m_paused || !m_server->armed()) return;

    TickResult res = m_recorder->process(m_server->queue(), m_autoPause);
    bool anyTouched = !res.touched.isEmpty();

    if (res.limitHit) {
        setStatus(QStringLiteral("⚠ 已达 %1 组上限，本次记录被忽略，请先\"清除全部\"或删除一组")
                  .arg(MAX_GROUPS));
    }
    if (res.started >= 0) {
        setStatus(QStringLiteral("🔴 记录中 — 打磨#%1").arg(res.started));
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
        const QString summary = res.sealedGroup ? seamSummary(*res.sealedGroup)
                                                : QStringLiteral("%1 道").arg(res.sealedPasses);
        if (m_autoPause) {
            m_paused = true;
            m_server->setArmed(false);
            m_btnPause->setText(QStringLiteral("▶  继续"));
            updateHoverActive();
            setStatus(QStringLiteral("⏹ %1 记录完成（%2 道）— 已冻结暂停，移动鼠标查看数值，点\"继续\"等待下次打磨")
                      .arg(res.sealedName).arg(res.sealedPasses));
        } else {
            setStatus(QStringLiteral("⏹ %1 记录完成（%2 道）— 继续监听，等待下次 RecordOn 触发…")
                      .arg(res.sealedName).arg(res.sealedPasses));
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
    base.replace(QStringLiteral(".csv"), QString());
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存波形数据"), base + QStringLiteral("_saved.csv"),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    QString err;
    if (!saveCsv(path, *g, err)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), err);
        return;
    }
    setStatus(QStringLiteral("✓ 已保存 %1 个点 / %2 道 → %3")
              .arg(theme::formatThousands(g->len())).arg(g->passes.size())
              .arg(QFileInfo(path).fileName()));
}

void MainWindow::onLoad() {
    if (m_groups.size() >= MAX_GROUPS) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("已达到最多 %1 组限制\n请先删除一组后再添加").arg(MAX_GROUPS));
        return;
    }
    QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择波形数据文件（可多选）"), QString(),
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (paths.isEmpty()) return;

    int loaded = 0;
    for (const QString& path : paths) {
        if (m_groups.size() >= MAX_GROUPS) {
            QMessageBox::information(this, QStringLiteral("提示"),
                QStringLiteral("已达到最多 %1 组限制，剩余 %2 个文件未载入")
                    .arg(MAX_GROUPS).arg(paths.size() - loaded));
            break;
        }
        CsvData d;
        QString err;
        if (!loadCsv(path, d, err)) {
            QMessageBox::critical(this, QStringLiteral("载入失败"),
                                  QFileInfo(path).fileName() + QStringLiteral("\n") + err);
            continue;
        }
        QString name = QFileInfo(path).fileName();
        int idx = addGroup(name, d.times, d.pos, d.force, d.passNo, d.esPos, false);
        loaded++;
        int npass = m_groups[idx]->passes.size();
        setStatus(QStringLiteral("✓ 已载入 %1 个点%2 ← %3")
                  .arg(theme::formatThousands(d.times.size()))
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

    m_seamCardsGroup = -2;
    m_seamCardsSig.clear();
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
