#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include "corr_data.h"
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void create_tree();
private slots:
    void on_pushButton_clicked();
    void on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_pushButton_delete_clicked();
    void on_listWidget_doubleClicked(const QModelIndex &index);
    void on_pushButton_2_clicked();
    void update_data();
    void plot_data(QCustomPlot *plot,int n);
    void plot_init(QCustomPlot *plot);

private:
    Ui::MainWindow *ui;
    Corr_data *corrdata;
    QTimer *timer;

};
#endif // MAINWINDOW_H
