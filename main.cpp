#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFont f(QStringLiteral("Consolas"));
    f.setPointSize(9);
    a.setFont(f);

    // 深色主题：统一设置调色板（否则停靠面板/菜单栏/滚动条会回落到系统浅色）
    QPalette pal;
    pal.setColor(QPalette::Window, theme::BG);
    pal.setColor(QPalette::WindowText, theme::C_TEXT);
    pal.setColor(QPalette::Base, theme::PANEL);
    pal.setColor(QPalette::AlternateBase, theme::BG);
    pal.setColor(QPalette::ToolTipBase, theme::PANEL);
    pal.setColor(QPalette::ToolTipText, theme::C_TEXT);
    pal.setColor(QPalette::Text, theme::C_TEXT);
    pal.setColor(QPalette::Button, theme::PANEL);
    pal.setColor(QPalette::ButtonText, theme::C_TEXT);
    pal.setColor(QPalette::BrightText, theme::C_REC);
    pal.setColor(QPalette::Highlight, theme::C_INFO);
    pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
    pal.setColor(QPalette::Disabled, QPalette::WindowText, theme::C_DIM);
    pal.setColor(QPalette::Disabled, QPalette::Text, theme::C_DIM);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, theme::C_DIM);
    a.setPalette(pal);

    a.setStyleSheet(QStringLiteral(
        "QMainWindow::separator{background-color:%1;width:4px;height:4px;}"
        "QMenuBar{background-color:%2;color:%3;}"
        "QMenuBar::item{background:transparent;padding:4px 10px;}"
        "QMenuBar::item:selected{background-color:%1;}"
        "QMenu{background-color:%2;color:%3;border:1px solid %1;}"
        "QMenu::item{padding:5px 22px;} QMenu::item:selected{background-color:%1;}"
        "QToolTip{background-color:%2;color:%3;border:1px solid %1;padding:3px;}")
        .arg(theme::BORDER.name(), theme::PANEL.name(), theme::C_TEXT.name()));

    MainWindow w;
    w.show();
    return a.exec();
}
