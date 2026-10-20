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
    void plot_data(QCustomPlot *plot,int n,int,int);
    void plot_init(QCustomPlot *plot);
    void plot_trend(QCustomPlot *plot);
    void on_listWidget_2_doubleClicked(const QModelIndex &index);

    void on_checkBox_calc_stateChanged(int arg1);
    bool save();
    bool saveAs();

    void mousePress();
    void trendMouseMoved(QMouseEvent *event);

    void on_pushButton_3_clicked();
    void on_checkBox_gradient_toggled(bool checked);
    void on_checkBox_stats_toggled(bool checked);
    void on_checkBox_detrend_toggled(bool checked);
    void on_pushButton_importance_clicked();
    void on_checkBox_importance_toggled(bool checked);

private:
    void set_scatter_data(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    void apply_gradient(QCustomPlot *plot);
    void refresh_plots();
    void set_recent_points(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    void hide_recent_points(QCustomPlot *plot);
    struct RecentGraphs{
        QCPGraph *g3 = nullptr;
        QCPGraph *g4 = nullptr;
    };
    QMap<QCustomPlot*,RecentGraphs> recent;
    void detrend_xy(QVector<double> &x,QVector<double> &y);
    void set_range_from(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    QString update_binned_curve(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    void set_info_text(QCustomPlot *plot,int n,const QString &best,const QFont &font);
    struct BinnedItems{
        QCPGraph *graph = nullptr;
        QCPErrorBars *err = nullptr;
        QCPItemText *text = nullptr;
    };
    QMap<QCustomPlot*,BinnedItems> binned;
    QMap<QCustomPlot*,int> gradient_first;
    QMap<QCustomPlot*,QPair<QVector<double>,QVector<double>>> scatter_xy;
    QString signal_list_file(const QString &name);
    bool load_signal_list(QListWidget *list, const QString &name);
    void save_signal_list(QListWidget *list, const QString &name);
    void connect_save_signal_list(QListWidget *list, const QString &name);
    Ui::MainWindow *ui;
    Corr_data *corrdata;
    QTimer *timer;
    void createActions();
    bool saveFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    void createStatusBar();
    QString curFile;
    QCPItemTracer *tracer;
    QCPItemText *tracerText;

};
#endif // MAINWINDOW_H
