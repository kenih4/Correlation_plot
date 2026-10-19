#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 設定ファイルの保存先(~/.config/Correlation_plot)を実行ファイル名に依存させない
    QCoreApplication::setApplicationName("Correlation_plot");
    MainWindow w;
    w.show();
    return a.exec();
}
