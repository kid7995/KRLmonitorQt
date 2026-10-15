#include "mainwindow.h"
#include "theme.h"
#include "uistyle.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 界面字体用系统 UI 字体；数值用等宽字体（在相应控件上单独指定）
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    a.setFont(f);

    uistyle::apply(a);      // 现代浅色工业风：调色板 + 全局样式表

    MainWindow w;
    w.show();
    return a.exec();
}
