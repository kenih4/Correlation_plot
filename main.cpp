#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // 設定ファイルの保存先(既定は ~/.config/Correlation_plot)を実行ファイル名に依存させない
    QCoreApplication::setApplicationName("Correlation_plot");

    // 設定ファイルのディレクトリは、引数で変えられる:
    //   Correlation_plot /path/to/dir
    //   Correlation_plot -c /path/to/dir      (--config-dir /path/to/dir, --config-dir=/path/to/dir)
    QCommandLineParser parser;
    parser.setApplicationDescription("Correlation plot of signals");
    parser.addHelpOption();
    QCommandLineOption cfg(QStringList() << "c" << "config-dir",
                           "Directory of the config files (default: ~/.config/Correlation_plot).","dir");
    parser.addOption(cfg);
    parser.addPositionalArgument("dir","Config directory (same as --config-dir).","[dir]");
    parser.process(a);

    QString dir = parser.value(cfg);
    const QStringList pos = parser.positionalArguments();
    if(dir.isEmpty() && !pos.isEmpty())dir = pos.first();
    if(pos.size()>1 || (!parser.value(cfg).isEmpty() && !pos.isEmpty())){
        fprintf(stderr,"Error: specify the config directory only once (either --config-dir or a positional argument).\n");
        return 1;
    }
    if(!dir.isEmpty())MainWindow::set_config_dir(QDir(dir).absolutePath());

    MainWindow w;
    w.show();
    return a.exec();
}
