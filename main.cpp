#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFont f(QStringLiteral("Consolas"));
    f.setPointSize(9);
    a.setFont(f);

    MainWindow w;
    w.show();
    return a.exec();
}
