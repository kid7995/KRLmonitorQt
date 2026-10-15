#include "uistyle.h"

#include <QApplication>
#include <QPalette>

#include "theme.h"

namespace {

// 用命名占位符拼样式表（比 %1..%N 好读，也便于统一改色）
QString templ(const QString& src) {
    QString s = src;
    const struct { const char* key; QColor col; } map[] = {
        {"@BG@",      theme::BG},
        {"@PANEL@",   theme::PANEL},
        {"@SURFACE@", theme::SURFACE},
        {"@HEADER@",  theme::HEADER},
        {"@BORDER@",  theme::BORDER},
        {"@GRID@",    theme::GRID},
        {"@TEXT@",    theme::C_TEXT},
        {"@DIM@",     theme::C_DIM},
        {"@ACCENT@",  theme::C_INFO},
        {"@ACCENTHI@",theme::C_INFO_HI},
        {"@HOVER@",   theme::C_HOVER},
        {"@OK@",      theme::C_OK},
        {"@WARN@",    theme::C_WARN},
        {"@REC@",     theme::C_REC},
    };
    for (const auto& m : map) {
        s.replace(QLatin1String(m.key), m.col.name());
    }
    return s;
}

}  // namespace

QString uistyle::styleSheet() {
    return templ(QStringLiteral(R"QSS(
/* ── 基础 ─────────────────────────────────────────────── */
QWidget            { color:@TEXT@; font-size:9pt; }
QMainWindow, QDialog { background:@BG@; }
QToolTip           { background:#26313D; color:#FFFFFF; border:none; padding:4px 7px; border-radius:4px; }

/* ── 菜单 ─────────────────────────────────────────────── */
QMenuBar           { background:@PANEL@; border-bottom:1px solid @BORDER@; padding:2px 6px; }
QMenuBar::item     { background:transparent; padding:5px 12px; border-radius:4px; }
QMenuBar::item:selected { background:@HOVER@; }
QMenu              { background:@PANEL@; border:1px solid @BORDER@; padding:5px; }
QMenu::item        { padding:6px 28px 6px 24px; border-radius:4px; }
QMenu::item:selected { background:@HOVER@; color:@ACCENT@; }
QMenu::separator   { height:1px; background:@GRID@; margin:5px 10px; }
QMenu::indicator   { width:14px; height:14px; }

/* ── 工具条 ───────────────────────────────────────────── */
QToolBar           { background:@PANEL@; border-bottom:1px solid @BORDER@; spacing:3px; padding:4px 8px; }
QToolBar::separator{ background:@GRID@; width:1px; margin:5px 8px; }
QToolButton        { background:transparent; border:1px solid transparent; border-radius:5px;
                     padding:5px 10px; color:@TEXT@; }
QToolButton:hover  { background:@HOVER@; border-color:#CDE0F2; }
QToolButton:pressed{ background:#DCE8F5; }
QToolButton:checked{ background:@HOVER@; border-color:#9CC6EA; color:@ACCENT@; }
QToolButton::menu-indicator { image:none; width:0; }

/* ── 按钮 ─────────────────────────────────────────────── */
QPushButton        { background:@PANEL@; border:1px solid #C9D2DC; border-radius:5px;
                     padding:5px 12px; color:@TEXT@; }
QPushButton:hover  { background:#F4F8FC; border-color:#9CC6EA; }
QPushButton:pressed{ background:@HOVER@; }
QPushButton:checked{ background:@HOVER@; border-color:#7FB2E0; color:@ACCENT@; }
QPushButton:disabled { color:#A9B3BE; background:#F7F8FA; border-color:@GRID@; }
/* btn=primary：主操作（开始/继续） */
QPushButton[btn="primary"]       { background:@ACCENT@; border:1px solid @ACCENT@; color:#FFFFFF; font-weight:600; }
QPushButton[btn="primary"]:hover { background:@ACCENTHI@; border-color:@ACCENTHI@; }
QPushButton[btn="primary"]:disabled { background:#C9D2DC; border-color:#C9D2DC; color:#FFFFFF; }
/* btn=warn：次要强调（暂停 / 新件） */
QPushButton[btn="warn"]          { background:@PANEL@; border:1px solid #E4C79A; color:#A66000; font-weight:600; }
QPushButton[btn="warn"]:hover    { background:#FDF5E8; border-color:@WARN@; }
/* btn=danger：破坏性操作（清除） */
QPushButton[btn="danger"]        { background:@PANEL@; border:1px solid #E9B7B3; color:@REC@; }
QPushButton[btn="danger"]:hover  { background:#FDF0EF; border-color:@REC@; }
/* btn=ghost：低权重（视图开关等） */
QPushButton[btn="ghost"]         { background:transparent; border:1px solid transparent; color:@TEXT@; padding:5px 9px; }
QPushButton[btn="ghost"]:hover   { background:@HOVER@; }
QPushButton[btn="ghost"]:checked { background:@HOVER@; border-color:#9CC6EA; color:@ACCENT@; }

/* ── 停靠面板 ─────────────────────────────────────────── */
QDockWidget        { titlebar-close-icon:none; titlebar-normal-icon:none; }
QDockWidget::title { background:@HEADER@; padding:6px 10px; border-bottom:1px solid @BORDER@;
                     font-weight:600; color:#44515E; }
QDockWidget::close-button, QDockWidget::float-button {
                     background:transparent; border:none; padding:2px; }
QDockWidget::close-button:hover, QDockWidget::float-button:hover { background:#D8E0E8; border-radius:4px; }
QDockWidget > QWidget { background:@PANEL@; }
QMainWindow::separator { background:@GRID@; width:4px; height:4px; }

/* ── 状态栏 ───────────────────────────────────────────── */
QStatusBar         { background:@PANEL@; border-top:1px solid @BORDER@; }
QStatusBar::item   { border:none; }

/* ── 列表 / 树 / 输入 ─────────────────────────────────── */
QListWidget, QTreeWidget, QTableWidget {
                     background:@PANEL@; border:1px solid @BORDER@; border-radius:5px; outline:none; }
QListWidget::item, QTreeWidget::item { padding:4px 6px; border-radius:4px; }
QListWidget::item:selected, QTreeWidget::item:selected { background:@HOVER@; color:@ACCENT@; }
QLineEdit, QSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
                     background:@PANEL@; border:1px solid #C9D2DC; border-radius:5px; padding:4px 7px;
                     selection-background-color:@ACCENT@; selection-color:#FFFFFF; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus { border-color:@ACCENT@; }
QComboBox::drop-down { border:none; width:18px; }
QCheckBox, QRadioButton { spacing:6px; }
QGroupBox          { border:1px solid @BORDER@; border-radius:6px; margin-top:10px; padding-top:6px; }
QGroupBox::title   { subcontrol-origin:margin; left:10px; padding:0 4px; color:#44515E; font-weight:600; }

/* ── 滚动条（细、无箭头）──────────────────────────────── */
QScrollBar:vertical   { background:transparent; width:10px; margin:2px 2px 2px 0; }
QScrollBar:horizontal { background:transparent; height:10px; margin:0 2px 2px 2px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background:#C9D2DC; border-radius:5px; }
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background:#A9B3BE; }
QScrollBar::handle:vertical { min-height:28px; }
QScrollBar::handle:horizontal { min-width:28px; }
QScrollBar::add-line, QScrollBar::sub-line { width:0; height:0; background:transparent; }
QScrollBar::add-page, QScrollBar::sub-page { background:transparent; }

/* ── 分隔条 ───────────────────────────────────────────── */
QSplitter::handle { background:@GRID@; }
)QSS"));
}

void uistyle::apply(QApplication& app) {
    QPalette pal;
    pal.setColor(QPalette::Window, theme::BG);
    pal.setColor(QPalette::WindowText, theme::C_TEXT);
    pal.setColor(QPalette::Base, theme::PANEL);
    pal.setColor(QPalette::AlternateBase, theme::SURFACE);
    pal.setColor(QPalette::ToolTipBase, theme::PANEL);
    pal.setColor(QPalette::ToolTipText, theme::C_TEXT);
    pal.setColor(QPalette::Text, theme::C_TEXT);
    pal.setColor(QPalette::Button, theme::PANEL);
    pal.setColor(QPalette::ButtonText, theme::C_TEXT);
    pal.setColor(QPalette::BrightText, theme::C_REC);
    pal.setColor(QPalette::Highlight, theme::C_INFO);
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0xA9, 0xB3, 0xBE));
    pal.setColor(QPalette::Disabled, QPalette::Text, QColor(0xA9, 0xB3, 0xBE));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0xA9, 0xB3, 0xBE));
    app.setPalette(pal);
    app.setStyleSheet(styleSheet());
}
