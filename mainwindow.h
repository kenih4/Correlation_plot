#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QRegularExpression>
#include <QDateTime>
#include <QActionGroup>
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
    bool eventFilter(QObject *obj, QEvent *event) override;
    void create_tree();
    QStringList load_group_list();      // 信号ツリーのグループ名(tree_groups.txt、無ければ既定)
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
    void line_toggled(bool checked);         // File メニューの Connect Latest 3 Points
    void mean5_toggled(bool checked);        // File メニューの Mean of Latest 5 Shots
    void stats_toggled(bool checked);        // File メニューの Stats
    void on_checkBox_highlight_toggled(bool checked);
    void on_V_highlight_editingFinished();
    void detrend_toggled(bool checked);   // File メニューの Detrend
    void show_importance();     // File メニューの Importance
    void check_config_files();  // 起動時に設定ファイルの有無を確認して警告する
    void open_config_files();   // File メニュー: 設定ディレクトリのテキストファイルを pluma で全部開く
    void rank_mode_changed();       // File メニューの Rank by ... (MIC / Importance / Gap of Means)

private:
    void set_scatter_data(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y,const QVector<double> &yref,const QVector<int> &shot);
    void highlight_shot(int shot);
    void apply_display(QCustomPlot *plot);
    void apply_display_impl(QCustomPlot *plot);
    void apply_shape(QCustomPlot *plot);
    QMap<QCustomPlot*,QCPScatterStyle::ScatterShape> plot_shape;   // 散布図ごとの点の形
    void update_highlight_mask(QCustomPlot *plot);
    void init_highlight_threshold();
    bool highlight_initialized = false;
    void update_mean_lines(QCustomPlot *plot);
    bool mean_label_enabled(const QString &signal);
    int latest_mean_shots();                    // 十字印の平均に使う最新のショット数(settings.ini)
    bool settings_loaded = false;
    QDateTime settings_mtime;
    int mean_shots = 5;
    double cross_sigma = 1.0;                   // 十字印を赤にする距離(σ_hi の何倍以内か)。settings.ini [CrossColor] sigma
    bool cross_require_closer = true;           // かつ、閾値未満の平均より閾値以上の平均に近いこと。require_closer_than_low
    bool near_good_mean(const QVector<double> &x,const QVector<char> &hi,double xbar);
    bool mean_label_loaded = false;
    QDateTime mean_label_mtime;
    QVector<QRegularExpression> mean_label_regex;   // mean_label_signals.txt の各行(ワイルドカード)
    QMap<QCustomPlot*,QString> plot_signal;         // 散布図ごとの横軸の信号名
    struct MeanLines{
        QCPItemStraightLine *hi = nullptr;  // 閾値以上の点の x の平均(赤)
        QCPItemStraightLine *lo = nullptr;  // 閾値未満の点の x の平均(灰色)
        QCPItemText *hi_text = nullptr;     // 赤い線の位置(閾値以上の平均値)の数値。左下に表示
    };
    QMap<QCustomPlot*,MeanLines> mean_lines;
    void refresh_plots();
    void create_extra_plots();
    void set_extra_plots(bool show);
    void clear_plot(QCustomPlot *plot);
    void plot_extra(bool b);
    void plot_extra_range(int beg,int end);
    // File メニューのチェック式の項目(初期値: Detrend=オフ、Stats=オフ、Gradient=オフ)
    QAction *detrend_act = nullptr;
    QAction *stats_act = nullptr;
    QAction *gradient_act = nullptr;
    // 散布図の並び順(どれか1つ。排他選択): MIC / Importance / 2群の平均の差 / Plot Signal リストの順(初期値)
    QAction *rank_mic_act = nullptr;
    QAction *rank_imp_act = nullptr;
    QAction *rank_gap_act = nullptr;
    QAction *rank_list_act = nullptr;
    QActionGroup *rank_group = nullptr;
    int rank_mode() const;
    void update_rank_scores(int beg=-1,int end=-1);
    QString last_highlight_text;
    QAction *line_act = nullptr;        // 最新3点を線でつなぐ(初期値オフ)
    QAction *mean5_act = nullptr;       // 最新Nショットの平均を十字印で示す(初期値オン)
    QScrollArea *extra_scroll = nullptr;   // 追加の散布図を入れるスクロール領域
    QWidget *extra_container = nullptr;    // その中身(散布図を3列で並べる)
    QGridLayout *extra_grid = nullptr;
    int extra_row_h = 230;                 // 追加の散布図の高さ(広げた時点の上段の散布図の高さに合わせる)
    void ensure_extra_plots(int n);        // 追加の散布図を n 個以上に増やす
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
        QCPCurve *line = nullptr;   // 最新3点をつなぐ線(点の順序を保つため QCPCurve)
        QCPGraph *mean5 = nullptr;  // 最新5ショットの(横軸の平均, 縦軸の平均)の十字印
        bool range_mode = false;    // 時間範囲指定の表示中は「最新」がないので線も出さない
    };
    void update_recent_line(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    void update_recent_mean(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y);
    QMap<QCustomPlot*,RecentGraphs> recent;
    void detrend_xy(QVector<double> &x,QVector<double> &y,QVector<double> &yraw,QVector<int> &shot);
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
        QVector<int> shot;      // 各点がもとのどのショット(データの添字)か。昇順
    };
    QMap<QCustomPlot*,ScatterData> scatter_xy;
    QMap<QCustomPlot*,QCPGraph*> highlight_graph;
    QMap<QCustomPlot*,QCPGraph*> hover_graph;   // トレンドグラフで指した点に対応する散布図の点を囲む輪(赤)
    int hover_shot = -1;
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
