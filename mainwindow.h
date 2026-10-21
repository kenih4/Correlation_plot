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
    void gradient_toggled(bool checked);     // File メニューの Gradient
    void stats_toggled(bool checked);        // File メニューの Stats
    void on_checkBox_highlight_toggled(bool checked);
    void on_V_highlight_editingFinished();
    void detrend_toggled(bool checked);   // File メニューの Detrend
    void show_importance();     // File メニューの Importance
    void rank_importance_toggled(bool checked);   // File メニューの Rank by Importance

private:
    void set_scatter_data(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y,const QVector<double> &yref);
    void apply_display(QCustomPlot *plot);
    void update_highlight_mask(QCustomPlot *plot);
    void refresh_plots();
    void create_extra_plots();
    void set_extra_plots(bool show);
    void clear_plot(QCustomPlot *plot);
    void plot_extra(bool b);
    void plot_extra_range(int beg,int end);
    // File メニューのチェック式の項目(初期値: Detrend=オン、Rank by Importance=オン、Stats=オフ、Gradient=オフ)
    QAction *detrend_act = nullptr;
    QAction *stats_act = nullptr;
    QAction *gradient_act = nullptr;
    QAction *rank_importance_act = nullptr;
    QWidget *extra_container = nullptr;
    QVector<QCustomPlot*> extra_plots;
    bool extra_visible = false;
    int extra_dh = 0;
    QString calc_signature();
    void mark_calculated();
    bool has_calculated = false;
    unsigned last_version = 0;
    QString last_signature;
    void set_recent_points(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    void hide_recent_points(QCustomPlot *plot);
    struct RecentGraphs{
        QCPGraph *g3 = nullptr;
        QCPGraph *g4 = nullptr;
    };
    QMap<QCustomPlot*,RecentGraphs> recent;
    void detrend_xy(QVector<double> &x,QVector<double> &y,QVector<double> &yraw);
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
    struct ScatterData{
        QVector<double> x,y;
        QVector<double> yref;   // 強調の判定に使う値(トレンドグラフの値)。Detrend時も元の値
        QVector<char> hi;       // yref が閾値以上なら1
    };
    QMap<QCustomPlot*,ScatterData> scatter_xy;
    QMap<QCustomPlot*,QCPGraph*> highlight_graph;
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
