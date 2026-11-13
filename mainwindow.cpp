#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qdatetime.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QSettings>
#include <QProcess>
#include <QMenu>
#include <QProgressDialog>
#include <QSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGridLayout>
#include <QScrollBar>
#include <QWheelEvent>
#include <QScreen>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QInputDialog>
#include <QComboBox>
#include <QToolTip>
#include <QMessageBox>

// 自動更新ごとに書き出すCSV
static const char *CSV_FILE = "/home_2023/xfel/xfelopr/users/kenichi/dvlp/corr_data/corr.csv";
#include <cmath>
#include <algorithm>

// ===== テーマ(配色) =====
// メニューバーの Theme メニューで、実行中に切り替えられる(選んだものは設定ディレクトリの theme.txt に覚える)。
// 起動時の既定は、theme.txt、無ければ settings.ini の [Theme] name、それも無ければ dark:
//   dark          : 暗い背景(青緑)           dark-contrast : 真っ黒な背景 + 明るい原色(いちばん見やすい)
//   dark-amber    : 暗い暖色の背景(琥珀色)   dark-navy     : 暗い紺色の背景(青)
//   dark-pink-deepest : 別プロジェクト trend の「Dark Pink」を基に、背景を真っ黒に近づけたもの(ピンクは強調の色と文字)
//   classic       : 以前の、白い背景のグラフ
// さらに [Colors](load_colors)で、強調の色と最新の点の色を個別に上書きできる(classic のときだけ)。
struct Theme{
    bool dark = true;
    // GUI
    QColor window,panel,border,text,muted,accent;
    // グラフ
    QColor plot_bg,grid,axis_line;
    QColor normal_point;    // 通常の(強調されていない)点
    QColor band,label_bg,label_border,curve_line,grad_end;
    int point_size = 5;         // 通常の点のサイズ
    double point_pen = 1.0;     // 点の輪の線の太さ
    int fill_alpha = 70;        // 輪の中を塗る色の不透明度(0〜255)。ダークテーマのみ
};
// メニューに出すテーマ(名前, 表示名)
static const QList<QPair<QString,QString>> THEME_LIST = {
    {"dark","Dark (Teal)"},{"dark-contrast","Dark High Contrast"},{"dark-amber","Dark Amber"},{"dark-navy","Dark Navy"},
    {"dark-pink-deepest","Dark Pink (Deepest)"},
    {"classic","Classic (Light)"}};
static QString g_theme_name = "dark";
static Theme T;

// 強調する点・線・文字に使う色。最新の点の色: LATEST1=最新(サイズ15)、LATEST2=2番目(13)、LATEST3=3番目(11)。
// 4番目以降は通常の点と同じ。いずれも、テーマの既定で初期化し、[Colors] で上書きする
static QColor MAIN_COLOR;
static QColor LATEST1_COLOR,LATEST2_COLOR,LATEST3_COLOR;
// 十字印が「優秀なショットの平均に近い」(閾値以上の)ときの色。settings.ini の cross_near で指定。
// 指定が無ければ、強調の色(MAIN_COLOR)と同じ
static QColor CROSS_NEAR_COLOR;
static QColor cross_near_color(){return CROSS_NEAR_COLOR.isValid() ? CROSS_NEAR_COLOR : MAIN_COLOR;}
// 十字印が「近くない」ときの色。settings.ini の cross で指定
static QColor CROSS_COLOR;

static void set_theme(QString name)
{
    bool known = false;
    for(const auto &t : THEME_LIST)if(t.first==name)known = true;
    if(!known)name = "dark";
    g_theme_name = name;
    T = Theme();
    CROSS_NEAR_COLOR = QColor();
    // 暗いテーマ共通の組み立て: GUI 6色 + グラフ 3色
    auto base = [](const char *window,const char *panel,const char *border,const char *text,const char *muted,const char *accent,
                   const char *grid,const char *axis){
        T.dark = true;
        T.window = QColor(window);  T.panel = QColor(panel);  T.border = QColor(border);
        T.text = QColor(text);  T.muted = QColor(muted);  T.accent = QColor(accent);
        T.plot_bg = T.panel;  T.grid = QColor(grid);  T.axis_line = QColor(axis);
        T.label_bg = T.window;  T.label_bg.setAlpha(200);  T.label_border = T.border;
        T.curve_line = T.muted;
        T.band = QColor(160,170,190,45);
        T.point_size = 7;  T.point_pen = 1.5;       // 暗い背景では、小さく細い点は見えにくい
    };
    if(name=="classic"){
        T.dark = false;
        T.window = QColor(240,240,240);  T.panel = Qt::white;  T.border = QColor(200,200,200);
        T.text = Qt::black;  T.muted = QColor(60,60,60);  T.accent = QColor(0,105,107);
        T.plot_bg = Qt::white;  T.grid = QColor(200,200,200);  T.axis_line = Qt::black;
        T.normal_point = QColor(170,170,170);
        T.band = QColor(190,190,190,90);  T.label_bg = QColor(255,255,255,230);  T.label_border = Qt::white;
        T.curve_line = QColor(90,90,90);  T.grad_end = Qt::blue;
        MAIN_COLOR = QColor(0,105,107);
        LATEST1_COLOR = QColor("#ff0000");  LATEST2_COLOR = QColor("#b3424a");  LATEST3_COLOR = QColor("#8000ff");
        CROSS_COLOR = Qt::black;
    }else if(name=="dark-contrast"){
        // 真っ黒な背景に、彩度の高い明るい色。点は大きく、輪は太く
        base("#000000","#000000","#3A3A3A","#FFFFFF","#C8C8C8","#00E5FF","#262626","#8C8C8C");
        T.normal_point = QColor("#B8C0CC");  T.grad_end = QColor("#FF4DFF");
        T.point_size = 8;  T.point_pen = 2.0;  T.fill_alpha = 90;
        MAIN_COLOR = QColor("#00E5FF");
        LATEST1_COLOR = QColor("#FF3B3B");  LATEST2_COLOR = QColor("#FF9F1C");  LATEST3_COLOR = QColor("#FFE600");
        CROSS_COLOR = QColor("#FFFFFF");
    }else if(name=="dark-amber"){
        base("#14110F","#1D1916","#332D28","#F2E9DF","#A39485","#FFB000","#2B2520","#4A4037");
        T.normal_point = QColor("#A8998A");  T.grad_end = QColor("#5CE1E6");
        T.fill_alpha = 80;
        MAIN_COLOR = QColor("#FFB000");
        LATEST1_COLOR = QColor("#FF4D4D");  LATEST2_COLOR = QColor("#FF7AB6");  LATEST3_COLOR = QColor("#7AD7FF");
        CROSS_COLOR = QColor("#FFFFFF");
    }else if(name=="dark-navy"){
        base("#0B1220","#111B2E","#22314D","#E8F0FF","#9AAECF","#4DA3FF","#1C2A44","#3B4F73");
        T.normal_point = QColor("#93A8CC");  T.grad_end = QColor("#FF7AD9");
        T.fill_alpha = 80;
        MAIN_COLOR = QColor("#4DA3FF");
        LATEST1_COLOR = QColor("#FF5A5F");  LATEST2_COLOR = QColor("#FFA94D");  LATEST3_COLOR = QColor("#C084FC");
        CROSS_COLOR = QColor("#FFFFFF");
    }else if(name=="dark-pink-deepest"){
        // trend の Dark Pink(文字 #f3d3e4、強調 hotpink 系、平均 #ffe0f0)を基に、背景を真っ黒に近づけたもの
        base("#000000","#040002","#240A1A","#f8dcea","#A8789A","#FF1F8E","#1A0A14","#45203A");
        T.normal_point = QColor("#B878A4");  T.grad_end = QColor("#8BE9FD");
        T.fill_alpha = 80;
        MAIN_COLOR = T.accent;
        LATEST1_COLOR = QColor("#FFF176");  LATEST2_COLOR = QColor("#FFB74D");  LATEST3_COLOR = QColor("#7DF9FF");
        CROSS_COLOR = QColor("#ffe0f0");
    }else{      // dark (Teal)
        base("#0F1216","#171B22","#262C36","#E6EAF0","#8A94A6","#2EC4B6","#232A33","#3A4350");
        T.normal_point = QColor("#8190A5");  T.grad_end = QColor("#4D9BFF");
        MAIN_COLOR = QColor("#2EC4B6");
        LATEST1_COLOR = QColor("#FF5A5F");  LATEST2_COLOR = QColor("#E8646E");  LATEST3_COLOR = QColor("#B794FF");
        CROSS_COLOR = QColor("#E6EAF0");
    }
}

// 散布図の点のスタイル(丸)。filled なら塗りつぶし、そうでなければ輪(ダークテーマでは、中を半透明に塗って見やすくする)
static QCPScatterStyle point_style(const QColor &col,bool filled)
{
    QColor fill = filled ? col : (T.dark ? QColor(col.red(),col.green(),col.blue(),T.fill_alpha) : T.plot_bg);
    return QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(col,T.point_pen),QBrush(fill),T.point_size);
}

// 移動平均(直近 w 点。先頭付近は、そこまでの点だけで平均する)
static QVector<double> moving_average(const QVector<double> &y,int w)
{
    QVector<double> out(y.size());
    if(w<1)w = 1;
    double sum = 0;
    for(int i=0;i<y.size();i++){
        sum += y[i];
        if(i>=w)sum -= y[i-w];
        out[i] = sum/qMin(i+1,w);
    }
    return out;
}
static QPen ma_pen(){QColor col = T.text;col.setAlpha(210);return QPen(col,2);}

static QString ini_string(QSettings &st,const QString &key,const QString &def);     // 定義は後ろ(settings.ini の値を文字列で読む)

// Target Signal リストの項目: 信号名と重みを持つ(表示は「信号名   [w=重み]」)
static const int TNAME_ROLE = Qt::UserRole + 2;
static const int TWEIGHT_ROLE = Qt::UserRole + 3;
static QString target_item_name(const QListWidgetItem *it){return it ? it->data(TNAME_ROLE).toString() : QString();}
static double target_item_weight(const QListWidgetItem *it){return it ? it->data(TWEIGHT_ROLE).toDouble() : 1.0;}
static void set_target_item(QListWidgetItem *it,const QString &name,double w)
{
    it->setData(TNAME_ROLE,name);
    it->setData(TWEIGHT_ROLE,w);
    it->setText(QString("%1   [w=%2]").arg(name).arg(w,0,'g',6));
}

// 設定ファイルのディレクトリ。実行時の引数で指定されたときはそれ、なければ ~/.config/Correlation_plot
static QString g_config_dir;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    load_theme();               // 配色(dark / classic)を決める。ウィジェットを作る前に
    apply_theme_widgets();
    ui->setupUi(this);
    cerr << "Config directory : " << QFileInfo(signal_list_file("signals.txt")).absolutePath().toStdString()
         << (g_config_dir.isEmpty() ? "  (default)" : "  (specified by argument)") << endl;
    load_colors();      // plot_init などで色を使う前に、settings.ini の色を読む

    // File メニューの Detrend。corrdata を作る前から状態を参照するので、先に作る(初期値はオフ)
    detrend_act = new QAction(tr("&Detrend"), this);
    detrend_act->setCheckable(true);
    detrend_act->setChecked(false);
    detrend_act->setStatusTip(tr("Remove slow drift (subtract moving average) before computing correlation"));

    stats_act = new QAction(tr("&Stats"), this);
    stats_act->setCheckable(true);
    stats_act->setChecked(false);
    stats_act->setStatusTip(tr("Show Spearman rho, p-value, MIC null95 and the binned mean curve"));

    gradient_act = new QAction(tr("&Gradient"), this);
    gradient_act->setCheckable(true);
    gradient_act->setChecked(false);
    gradient_act->setStatusTip(tr("Color points from teal (oldest) to blue (newest)"));

    line_act = new QAction(tr("&Connect Latest 3 Points"), this);
    line_act->setCheckable(true);
    line_act->setChecked(false);
    line_act->setStatusTip(tr("Draw a line through the latest 3 points of each scatter plot and the trend graph"));

    mean5_act = new QAction(tr("Show &Mean of Latest Shots"), this);
    mean5_act->setCheckable(true);
    mean5_act->setChecked(true);
    mean5_act->setStatusTip(tr("Mark the weighted mean (x and y) of the latest N shots with a cross on each scatter plot (newer shots weigh more; settings.ini LatestMean: shots, weighting, half_life)"));

    // 散布図の並び順。ラジオボタン相当の排他選択(QActionGroup)
    rank_mic_act = new QAction(tr("Rank by &MIC"), this);
    rank_imp_act = new QAction(tr("Rank by &Importance"), this);
    rank_gap_act = new QAction(tr("Rank by &Gap of Means"), this);
    rank_list_act = new QAction(tr("Rank by &List Order"), this);
    rank_mic_act->setStatusTip(tr("Order the scatter plots by MIC"));
    rank_imp_act->setStatusTip(tr("Order the scatter plots by random forest importance"));
    rank_gap_act->setStatusTip(tr("Order the scatter plots by the gap between the two Highlight means (divided by the standard deviation)"));
    rank_group = new QActionGroup(this);
    rank_group->setExclusive(true);
    rank_list_act->setStatusTip(tr("Order the scatter plots in the order of the Plot Signal list (top to bottom, after the first item)"));
    for(QAction *a : {rank_mic_act,rank_imp_act,rank_gap_act,rank_list_act}){
        a->setCheckable(true);
        rank_group->addAction(a);
    }
    rank_mic_act->setChecked(true);

    int status = db_init();
    cerr << "#### DB Init : " << status << endl;
    if(DB_OK != status)
    {
        exit(1);
    }
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    ui->dateTimeEdit->setDisplayFormat(tr("yyyy/MM/dd HH:mm:ss"));

    plot_init(ui->widget_1);
    plot_init(ui->widget_2);
    plot_init(ui->widget_3);
    plot_init(ui->widget_7);
    plot_init(ui->widget_8);
    plot_init(ui->widget_9);
    create_extra_plots();
    plot_init(ui->widget);

    if(!load_signal_list(ui->listWidget,"signals.txt")){
        ui->listWidget->addItem("sr_safety_topup_intlk_beam_sr_dcct1/charge_diff");
        ui->listWidget->addItem("xfel_mon_bpm_xsbt_3_x/position");
        ui->listWidget->addItem("xfel_mon_bpm_xsbt_5_x/position");
        ui->listWidget->addItem("sr_mon_bpm_xsbt_25_x/position");
        ui->listWidget->addItem("sr_mon_bpm_xsbt_25_y/position");
        ui->listWidget->addItem("xfel_mon_msbpm_bc1_x/position");
        ui->listWidget->addItem("xfel_mon_msbpm_bc2_x/position");
        ui->listWidget->addItem("xfel_mon_msbpm_bc3_x/position");
    }
    // 基準信号のリスト(targets.txt: 1行1信号「信号名  重み」)。無ければ、これまでの設定を引き継いで
    // Plot Signal の先頭(これまでの基準信号)を、重み 1 の基準信号にする
    if(!load_targets()){
        if(ui->listWidget->count()>0){
            QListWidgetItem *first = ui->listWidget->takeItem(0);
            add_target_item(first->text(),1.0);
            delete first;
            cerr << "NOTE: targets.txt not found; the first Plot Signal was moved to the Target Signal list (weight 1)." << endl;
        }
        save_targets();
    }
    // 基準信号と同じ名前の信号は、Plot Signal(説明変数)から外す
    for(int i=ui->listWidget->count();i-->0;){
        QString nm = ui->listWidget->item(i)->text();
        for(int k=0;k<ui->listWidget_2->count();k++){
            if(target_item_name(ui->listWidget_2->item(k))==nm){delete ui->listWidget->takeItem(i);break;}
        }
    }
    // listWidget, listWidget_2 が変更されたら信号リストファイルに保存する
    connect_save_signal_list(ui->listWidget,"signals.txt");
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget,&QListWidget::customContextMenuRequested,this,&MainWindow::list_context_menu);
    connect(ui->listWidget_2->model(),&QAbstractItemModel::rowsInserted,this,[this](){save_targets();});
    connect(ui->listWidget_2->model(),&QAbstractItemModel::rowsRemoved,this,[this](){save_targets();});
    ui->listWidget_2->setToolTip(tr("Target signals (the signals you want to maximize). Double-click to change the weight (negative = minimize)."));
    load_target_settings();

    create_tree();
    ui->V_threshold->setText(QString::number(0.05));
    ui->checkBox_add->setChecked(true);     // Add: 自動更新で増えたショットを捨てず、増やし続ける(初期値オン)
    ui->checkBox_th->setChecked(false);
    ui->checkBox->setChecked(true);     // Auto reload
    timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_data()));
    timer->start();
    corrdata = new Corr_data(ui->spinBox->value());
    corrdata->set_stats(stats_act->isChecked());
    corrdata->set_tag_offset(tag_off_sacla,tag_off_sr);
    corrdata->set_detrend(detrend_act->isChecked());
    corrdata->set_rank_mode(rank_mode());
    createActions();
    createStatusBar();
    ui->radioButton->setChecked(true);


    connect(ui->widget,SIGNAL(mousePress(QMouseEvent*)),this,SLOT(mousePress()));
    ui->widget->addGraph();
    ui->widget->graph(3)->setLineStyle(QCPGraph::lsLine);
    ui->widget->graph(3)->setVisible(false);
    ui->widget->addGraph();
    ui->widget->graph(4)->setLineStyle(QCPGraph::lsLine);
    ui->widget->graph(4)->setVisible(false);

    tracer = new QCPItemTracer(ui->widget);
    tracer->setGraph(ui->widget->graph(0));
    tracer->setGraphKey(0.0);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setPen(QPen(MAIN_COLOR));
    tracer->setBrush(MAIN_COLOR);
    tracer->setSize(7);
    connect(ui->widget,SIGNAL(mouseMove(QMouseEvent*)),this,SLOT(trendMouseMoved(QMouseEvent*)));
    ui->widget->installEventFilter(this);
    tracerText = new QCPItemText(ui->widget);
    tracerText->position->setType(QCPItemPosition::ptAxisRectRatio);
    tracerText->setFont(QFont(font().family(),8));
    tracerText->setPositionAlignment(Qt::AlignLeft);
    tracerText->position->setCoords(QPointF(0,0));
    tracerText->setText("(-)");
    tracerText->setColor(T.text);
    tracer->setVisible(false);
    tracerText->setVisible(false);

    QTimer::singleShot(0, this, SLOT(check_config_files()));
}

void MainWindow::set_config_dir(const QString &dir)
{
    g_config_dir = dir;
}

QString MainWindow::signal_list_file(const QString &name)
{
    QString dir = g_config_dir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            : g_config_dir;
    return dir + "/" + name;
}

// 1行1信号のテキストファイルから list を読み込む。
// ファイルが無い/空の場合は false を返す(呼び出し側で既定リストを使う)
// signals.txt の1行は「信号名  フラグ ...」(空白区切り)。フラグはリストの項目に持たせ(FLAGS_ROLE)、
// 保存するときにそのまま書き戻す。フラグ:
//   mean_label : この信号が横軸の散布図に、閾値以上の平均値の数値(左下)を出す。
//                1つも付いていなければ、すべての信号に出す。
static const int FLAGS_ROLE = Qt::UserRole + 1;
static const char *FLAG_MEAN_LABEL = "mean_label";

static bool item_has_flag(const QListWidgetItem *it,const QString &flag)
{
    return it && it->data(FLAGS_ROLE).toStringList().contains(flag);
}

// フラグ付きの項目は、リスト上で青緑の文字にして見分けられるようにする
static void apply_item_style(QListWidgetItem *it)
{
    it->setForeground(item_has_flag(it,FLAG_MEAN_LABEL) ? QBrush(MAIN_COLOR) : QBrush());
}

bool MainWindow::load_signal_list(QListWidget *list, const QString &name)
{
    QFile fp(signal_list_file(name));
    if(!fp.open(QIODevice::ReadOnly | QIODevice::Text)){
        return false;
    }
    QTextStream in(&fp);
    while(!in.atEnd()){
        QString line = in.readLine().trimmed();
        if(line.isEmpty() || line.startsWith('#'))continue;
        QStringList tok = line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
        QListWidgetItem *it = new QListWidgetItem(tok.takeFirst());
        if(!tok.isEmpty()){
            tok.removeDuplicates();
            it->setData(FLAGS_ROLE,tok);
            apply_item_style(it);
        }
        list->addItem(it);
    }
    cerr << "Load signal list : " << signal_list_file(name).toStdString()
         << "  (" << list->count() << " signals)" << endl;
    return list->count() > 0;
}

void MainWindow::save_signal_list(QListWidget *list, const QString &name)
{
    QString fname = signal_list_file(name);
    QDir().mkpath(QFileInfo(fname).absolutePath());
    QFile fp(fname);
    if(!fp.open(QIODevice::WriteOnly | QIODevice::Text)){
        qWarning().noquote() << "Cannot write signal list file  " << fname;
        return;
    }
    QTextStream out(&fp);
    for(int i=0;i<list->count();++i){
        QString line = list->item(i)->text();
        QStringList flags = list->item(i)->data(FLAGS_ROLE).toStringList();
        if(!flags.isEmpty())line += "    " + flags.join(" ");
        out << line << Qt::endl;
    }
}

void MainWindow::connect_save_signal_list(QListWidget *list, const QString &name)
{
    auto save = [this, list, name](){ save_signal_list(list, name); };
    connect(list->model(), &QAbstractItemModel::rowsInserted, this, save);
    connect(list->model(), &QAbstractItemModel::rowsRemoved, this, save);
}

// ===== 基準信号(Target Signal) =====
// targets.txt: 1行1信号「信号名  重み」(重みは省略すると 1。'#' で始まる行は無視)
bool MainWindow::load_targets()
{
    QFile fp(signal_list_file("targets.txt"));
    if(!fp.open(QIODevice::ReadOnly | QIODevice::Text))return false;
    QTextStream in(&fp);
    while(!in.atEnd()){
        QString line = in.readLine().trimmed();
        if(line.isEmpty() || line.startsWith('#'))continue;
        QStringList tok = line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
        double w = 1.0;
        if(tok.size()>1){
            bool ok = false;
            double v = tok[1].toDouble(&ok);
            if(ok)w = v;
            else cerr << "WARNING: targets.txt: weight '" << tok[1].toStdString() << "' of " << tok[0].toStdString() << " is not a number; using 1" << endl;
        }
        add_target_item(tok[0],w);
    }
    cerr << "Load targets : " << signal_list_file("targets.txt").toStdString() << "  (" << ui->listWidget_2->count() << " targets)" << endl;
    return ui->listWidget_2->count()>0;
}

void MainWindow::save_targets()
{
    QString fname = signal_list_file("targets.txt");
    QDir().mkpath(QFileInfo(fname).absolutePath());
    QFile fp(fname);
    if(!fp.open(QIODevice::WriteOnly | QIODevice::Text)){
        qWarning().noquote() << "Cannot write targets file  " << fname;
        return;
    }
    QTextStream out(&fp);
    out << "# Target signals (the signals to maximize):  signal_name  weight   (negative weight = minimize)" << Qt::endl;
    for(int i=0;i<ui->listWidget_2->count();++i){
        QListWidgetItem *it = ui->listWidget_2->item(i);
        out << target_item_name(it) << "    " << QString::number(target_item_weight(it),'g',10) << Qt::endl;
    }
}

void MainWindow::add_target_item(const QString &name,double w)
{
    QListWidgetItem *it = new QListWidgetItem;
    set_target_item(it,name,w);
    ui->listWidget_2->addItem(it);
}

// settings.ini [Target]:  combine=sum|min   normalize=auto|zscore|none
void MainWindow::load_target_settings()
{
    QFileInfo fi(signal_list_file("settings.ini"));
    if(!fi.exists())return;
    QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
    bool okw = false;
    int mw = ini_string(st,"MovingAverage/shots","10").toInt(&okw);
    if(okw && mw>=1)ma_window = mw;
    QString cb = ini_string(st,"Target/combine","sum").toLower();
    QString nm = ini_string(st,"Target/normalize","auto").toLower();
    if(cb=="min")target_combine = Corr_data::COMBINE_MIN;
    else if(cb!="sum")cerr << "WARNING: settings.ini Target/combine must be sum or min; using sum" << endl;
    if(nm=="zscore")target_normalize = Corr_data::NORM_ZSCORE;
    else if(nm=="none")target_normalize = Corr_data::NORM_NONE;
    else if(nm!="auto")cerr << "WARNING: settings.ini Target/normalize must be auto, zscore or none; using auto" << endl;
}

// Target Signal リスト(名前と重み)と合成の方法を、corrdata に渡す。変わったら true
// 移動平均線(メインのトレンド、子パネルの各トレンド)。File メニューでオン/オフ、窓の点数は Moving Average Window...(既定は settings.ini [MovingAverage] shots)
void MainWindow::update_moving_average()
{
    const bool on = ma_act && ma_act->isChecked();
    // メインのトレンド(合成した基準値)
    if(scatter_xy.contains(ui->widget)){
        const ScatterData &s = scatter_xy[ui->widget];
        if(!main_ma){
            main_ma = ui->widget->addGraph();
            main_ma->setLineStyle(QCPGraph::lsLine);
            main_ma->setScatterStyle(QCPScatterStyle::ssNone);
        }
        main_ma->setPen(ma_pen());
        if(on && s.x.size()>1)main_ma->setData(s.x,moving_average(s.y,ma_window));
        main_ma->setVisible(on && s.x.size()>1);
        ui->widget->replot(QCustomPlot::rpQueuedReplot);
    }
    // 子パネル
    for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
        TrendRow &row = it.value();
        if(!row.ma_graph){
            row.ma_graph = row.plot->addGraph();
            row.ma_graph->setLineStyle(QCPGraph::lsLine);
            row.ma_graph->setScatterStyle(QCPScatterStyle::ssNone);
        }
        row.ma_graph->setPen(ma_pen());
        bool ok = on && row.y.size()>1 && row.y.size()==trend_x.size();
        if(ok)row.ma_graph->setData(trend_x,moving_average(row.y,ma_window));
        row.ma_graph->setVisible(ok);
        row.plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

// ===== ヒートマップ(子パネル) =====
// 信号どうしの相関行列を色で表す。信号は、合成した基準値、基準信号、説明変数の順(子パネルのトレンドと同じ)。
//   Spearman : 順位相関(-1〜1。青=負、赤=正)      MIC : 0〜1(非線形な関係も拾う。信号が多いと計算に時間がかかる)
// 全ショットを使う(Threshold は使わない。Detrend はオンなら適用)。自動更新では描き直さず、Update ボタンで更新する
QStringList MainWindow::panel_signal_names()
{
    QStringList names;
    if(!corrdata)return names;
    for(int i=0;i<corrdata->sig_size();i++){
        names << QString::fromStdString(corrdata->sig_name(i));
        if(i==0)for(int k=0;k<corrdata->target_count();k++)names << QString::fromStdString(corrdata->target_name(k));
    }
    return names;
}

void MainWindow::open_heatmap_panel()
{
    if(!heat_win){
        heat_win = new QWidget(this,Qt::Window);
        heat_win->setWindowTitle("Correlation Heatmap");
        QVBoxLayout *top = new QVBoxLayout(heat_win);
        top->setContentsMargins(6,6,6,6);
        QHBoxLayout *bar = new QHBoxLayout;
        bar->addWidget(new QLabel("Metric:",heat_win));
        heat_metric = new QComboBox(heat_win);
        heat_metric->addItem("Spearman (signed)");
        heat_metric->addItem("MIC");
        bar->addWidget(heat_metric);
        QPushButton *upd = new QPushButton("Update",heat_win);
        upd->setToolTip("Recompute with the current data (not updated automatically by Auto reload)");
        bar->addWidget(upd);
        heat_info = new QLabel(heat_win);
        bar->addWidget(heat_info,1);
        top->addLayout(bar);
        heat_plot = new QCustomPlot(heat_win);
        heat_plot->setMouseTracking(true);
        top->addWidget(heat_plot,1);
        connect(heat_metric,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){update_heatmap(false);});
        connect(upd,&QPushButton::clicked,this,[this](){update_heatmap(true);});
        connect(heat_plot,&QCustomPlot::mouseMove,this,[this](QMouseEvent *ev){
            const int N = heat_names.size();
            if(N<2 || heat_vals.size()!=N*N)return;
            int col = qRound(heat_plot->xAxis->pixelToCoord(ev->pos().x()));
            int row = qRound(heat_plot->yAxis->pixelToCoord(ev->pos().y()));
            if(col<0 || col>=N || row<0 || row>=N || !heat_plot->axisRect()->rect().contains(ev->pos())){
                QToolTip::hideText();
                return;
            }
            QToolTip::showText(ev->globalPosition().toPoint(),
                QString("%1\n%2\n%3 = %4").arg(heat_names[row],heat_names[col],heat_is_mic ? "MIC" : "Spearman").arg(heat_vals[row*N+col],0,'f',3),heat_plot);
        });
        heat_win->resize(920,800);
    }
    heat_win->show();
    heat_win->raise();
    heat_win->activateWindow();
    update_heatmap(false);
}

void MainWindow::update_heatmap(bool force)
{
    if(!heat_win || !heat_win->isVisible())return;
    if(!corrdata || corrdata->data_size()<2 || corrdata->sig_size()<2){
        heat_info->setText("No data yet. Press Get first.");
        return;
    }
    const QStringList names = panel_signal_names();
    const int m = heat_metric->currentIndex();
    const QString key = QString::number(corrdata->data_version()) + "|" + names.join(",") + "|" + QString::number(detrend_act->isChecked());
    if(force || heat_key[m]!=key || heat_cache[m].size()!=names.size()*names.size()){
        vector<string> nm;
        for(const QString &s : names)nm.push_back(s.toStdString());
        struct BusyGuard{bool &f;BusyGuard(bool &b):f(b){f = true;}~BusyGuard(){f = false;}} busy_guard(get_busy);   // 計算中は自動更新を止める
        QProgressDialog dlg("Computing the correlation matrix...","Cancel",0,100,heat_win);
        dlg.setWindowTitle("Correlation Heatmap");
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setMinimumDuration(400);
        vector<double> M = corrdata->corr_matrix(nm,m==1,[&dlg](int done,int total){
            dlg.setValue(total>0 ? 100*done/total : 100);
            QApplication::processEvents();
            return !dlg.wasCanceled();
        });
        dlg.setValue(100);
        if(M.empty()){heat_info->setText("Canceled.");return;}
        heat_cache[m] = QVector<double>(M.begin(),M.end());
        heat_key[m] = key;
        heat_cache_time[m] = QDateTime::currentDateTime();
        heat_cache_shots[m] = corrdata->used_shots();
    }
    heat_names.clear();
    for(const QString &s : names)heat_names << (s==QString::fromStdString(Corr_data::target_name_virtual()) ? QString::fromStdString(corrdata->target_label())+" (combined)" : s);
    heat_vals = heat_cache[m];
    heat_is_mic = (m==1);
    heat_info->setText(QString("%1 signals, %2 shots  (computed %3)").arg(names.size()).arg(heat_cache_shots[m]).arg(heat_cache_time[m].toString("HH:mm:ss")));
    draw_heatmap();
}

void MainWindow::draw_heatmap()
{
    if(!heat_plot)return;
    const int N = heat_names.size();
    if(N<2 || heat_vals.size()!=N*N)return;
    style_plot(heat_plot);
    heat_plot->clearPlottables();
    heat_plot->clearItems();
    QCPColorMap *map = new QCPColorMap(heat_plot->xAxis,heat_plot->yAxis);
    map->data()->setSize(N,N);
    map->data()->setRange(QCPRange(0,N-1),QCPRange(0,N-1));
    for(int r=0;r<N;r++)for(int cc=0;cc<N;cc++)map->data()->setCell(cc,r,heat_vals[r*N+cc]);
    map->setInterpolate(false);
    if(!heat_scale){
        heat_scale = new QCPColorScale(heat_plot);
        heat_plot->plotLayout()->addElement(0,1,heat_scale);
        heat_scale->setType(QCPAxis::atRight);
    }
    map->setColorScale(heat_scale);
    QCPColorGradient g;
    g.clearColorStops();
    const QColor mid = T.dark ? T.plot_bg : QColor(Qt::white);
    if(!heat_is_mic){
        map->setDataRange(QCPRange(-1,1));
        g.setColorStopAt(0.0,T.dark ? QColor("#4D9BFF") : QColor("#1f77b4"));
        g.setColorStopAt(0.5,mid);
        g.setColorStopAt(1.0,T.dark ? QColor("#FF5A5F") : QColor("#d62728"));
    }else{
        map->setDataRange(QCPRange(0,1));
        g.setColorStopAt(0.0,mid);
        g.setColorStopAt(1.0,MAIN_COLOR);
    }
    map->setGradient(g);
    heat_scale->axis()->setLabel(heat_is_mic ? "MIC" : "Spearman");
    heat_scale->axis()->setLabelColor(T.text);
    heat_scale->axis()->setTickLabelColor(T.muted);
    heat_scale->axis()->setBasePen(QPen(T.axis_line));
    heat_scale->axis()->setTickPen(QPen(T.axis_line));
    heat_scale->axis()->setSubTickPen(QPen(T.axis_line));

    QSharedPointer<QCPAxisTickerText> tx(new QCPAxisTickerText),ty(new QCPAxisTickerText);
    for(int i=0;i<N;i++){tx->addTick(i,heat_names[i]);ty->addTick(i,heat_names[i]);}
    heat_plot->xAxis->setTicker(tx);
    heat_plot->yAxis->setTicker(ty);
    QFont f = font();
    f.setPointSize(8);
    for(QCPAxis *ax : {heat_plot->xAxis,heat_plot->yAxis}){
        ax->setTickLabelFont(f);
        ax->setSubTicks(false);
        ax->grid()->setVisible(false);
        ax->setRange(-0.5,N-0.5);
    }
    heat_plot->xAxis->setTickLabelRotation(-60);
    heat_plot->yAxis->setRangeReversed(true);           // 1行目を上に
    heat_plot->xAxis->setRangeReversed(false);
    heat_plot->setInteractions(QCP::Interaction(0));
    // 信号が少ないときは、マスの中に値も書く
    if(N<=25){
        QFont tf = font();
        tf.setPointSize(N<=12 ? 9 : 7);
        for(int r=0;r<N;r++)for(int cc=0;cc<N;cc++){
            QCPItemText *t = new QCPItemText(heat_plot);
            t->position->setType(QCPItemPosition::ptPlotCoords);
            t->position->setCoords(cc,r);
            t->setText(QString::number(heat_vals[r*N+cc],'f',2));
            t->setFont(tf);
            t->setColor(T.text);
        }
    }
    heat_plot->replot();
}

bool MainWindow::sync_targets()
{
    vector<Corr_data::Target> v;
    for(int i=0;i<ui->listWidget_2->count();++i){
        QListWidgetItem *it = ui->listWidget_2->item(i);
        v.push_back({target_item_name(it).toStdString(),target_item_weight(it)});
    }
    return corrdata->set_targets(v,target_combine,target_normalize);
}

// 重みや合成の方法が変わったとき、保持しているデータで合成値を計算し直して描き直す(DBは読まない)。
// 基準信号の名前が変わっているときは、新しい信号の値が無いので、次の Get / 自動更新で反映する
void MainWindow::targets_changed()
{
    corrdata->rank_init();
    if(!has_calculated)return;
    bool same = corrdata->target_count()==ui->listWidget_2->count();
    for(int k=0;same && k<ui->listWidget_2->count();k++)
        same = (QString::fromStdString(corrdata->target_name(k))==target_item_name(ui->listWidget_2->item(k)));
    if(!same)return;
    sync_targets();
    refresh_plots();
    plot_trend(ui->widget);
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::trendMouseMoved(QMouseEvent *event){
    QPoint pos = event->pos();
    double key = floor(ui->widget->graph(0)->keyAxis()->pixelToCoord(pos.x()));
    tracer->setGraphKey(key);
    double x,y;
    x = qBound(0.,(tracer->position->key()-ui->widget->graph(0)->keyAxis()->range().lower)/ui->widget->graph(0)->keyAxis()->range().size(),0.94);
    y = qBound(0.,1.- (tracer->position->value()-ui->widget->graph(0)->valueAxis()->range().lower)/ui->widget->graph(0)->valueAxis()->range().size(),0.9);
    tracerText->position->setCoords(x,y);
    tracerText->setText("("+QString::number((tracer->position->value()))+")");
    tracer->setVisible(true);
    tracerText->setVisible(true);
    ui->widget->replot();

    // マウスが指しているデータ点(12ピクセル以内で最も近い点)のショットを、散布図でも囲んで示す
    int hit = -1;
    if(scatter_xy.contains(ui->widget)){
        const ScatterData &s = scatter_xy[ui->widget];
        double best = 12.0*12.0;
        for(int k=0;k<s.x.size();k++){
            double dx = ui->widget->xAxis->coordToPixel(s.x[k]) - pos.x();
            double dy = ui->widget->yAxis->coordToPixel(s.y[k]) - pos.y();
            double d = dx*dx + dy*dy;
            if(d<best){best = d;hit = s.shot[k];}
        }
    }
    highlight_shot(hit);
}

// トレンドグラフで指したショット(-1 なら指していない)を、各散布図の対応する点を輪で囲んで示す。
// 検定や Detrend で表示から外れているショットは、その散布図には出ない。
void MainWindow::highlight_shot(int shot)
{
    if(shot==hover_shot)return;
    hover_shot = shot;
    update_trend_cursor();
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        QCustomPlot *plot = it.key();
        if(plot==ui->widget || !plot->isVisible())continue;
        QCPGraph *&hg = hover_graph[plot];
        const ScatterData &s = it.value();
        int k = -1;
        if(shot>=0){
            auto pos = std::lower_bound(s.shot.begin(),s.shot.end(),shot);
            if(pos!=s.shot.end() && *pos==shot)k = pos - s.shot.begin();
        }
        if(k<0){
            if(hg && hg->visible()){
                hg->setVisible(false);
                plot->replot(QCustomPlot::rpQueuedReplot);
            }
            continue;
        }
        if(!hg){
            hg = plot->addGraph();
            hg->setLineStyle(QCPGraph::lsNone);
            hg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(MAIN_COLOR,2.5),QBrush(Qt::NoBrush),18));
        }
        QVector<double> hx(1,s.x[k]),hy(1,s.y[k]);
        hg->setData(hx,hy);
        hg->setVisible(true);
        plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

// トレンドグラフからマウスが出たら、散布図の輪を消す
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj==ui->widget && event->type()==QEvent::Leave){
        highlight_shot(-1);
    }
    if(trend_win && obj==ui->widget && event->type()==QEvent::Resize){
        fit_trend_panel_width();        // 子パネルのトレンドグラフの幅を、メインのトレンドグラフに合わせる
    }
    if(extra_scroll && obj==extra_scroll->viewport() && event->type()==QEvent::Resize){
        fit_extra_row_height();
    }
    // 追加の散布図の上のホイールは、グラフの拡大縮小ではなく領域のスクロールにする(Ctrl+ホイールで拡大縮小)
    if(event->type()==QEvent::Wheel && extra_scroll){
        QCustomPlot *plot = qobject_cast<QCustomPlot*>(obj);
        if(plot && extra_plots.contains(plot)){
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            if(!(we->modifiers() & Qt::ControlModifier)){
                QScrollBar *bar = extra_scroll->verticalScrollBar();
                bar->setValue(bar->value() - we->angleDelta().y()/2);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj,event);
}
void MainWindow::mousePress()
{
    QVector<double> x(2),y(2);
    x[0] = tracer->position->key();
    x[1] = tracer->position->key();
    y[0] = 1e9;
    y[1] = -1e9;
    if(ui->widget->graph(3)->data()->begin()->key==0){
        ui->widget->graph(3)->setData(x,y);
        ui->widget->graph(3)->setVisible(true);
    }else if(ui->widget->graph(4)->data()->begin()->key==0){
        ui->widget->graph(4)->setData(x,y);
        ui->widget->graph(4)->setVisible(true);
    }else if(abs(ui->widget->graph(3)->data()->begin()->key-x[0])<
             abs(ui->widget->graph(4)->data()->begin()->key-x[0])){
        ui->widget->graph(3)->setData(x,y);
        ui->widget->graph(3)->setVisible(true);
    }else{
        ui->widget->graph(4)->setData(x,y);
        ui->widget->graph(4)->setVisible(true);
    }
    ui->widget->replot();
}

void MainWindow::on_pushButton_clicked()
{
    if(ui->listWidget_2->count()==0){
        QMessageBox::warning(this,"Correlation Plot","基準信号(Target Signal)が1つもありません。\n信号ツリーの上の「Target Signal」を選んでから、信号をダブルクリックして追加してください。");
        return;
    }
    trend_rebase = true;        // 子パネルの初期値(点線)は、この Get で取れたショットの平均値にする
    delete corrdata;
    corrdata = new Corr_data(ui->spinBox->value());
    corrdata->set_stats(stats_act->isChecked());
    corrdata->set_tag_offset(tag_off_sacla,tag_off_sr);
    if(tag_off_sacla!=0 || tag_off_sr!=0){
        cerr << "[tag] NOTE: tag offset is specified (sacla_tag" << showpos << tag_off_sacla
             << ", sr_tag" << tag_off_sr << noshowpos << "). Values are matched with tag+offset." << endl;
    }
    corrdata->set_detrend(detrend_act->isChecked());
    corrdata->set_rank_mode(rank_mode());

    sync_targets();         // 基準信号(重み、合成の方法)を先に渡す。説明変数は、これと同じ名前のものを追加しない
    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
    }

    // 進捗表示: ダイアログ(0〜100%)とステータスバー。段階ごとの割合は、タグ45%・値45%・計算と描画10%
    struct BusyGuard{bool &f;BusyGuard(bool &b):f(b){f = true;}~BusyGuard(){f = false;}} busy_guard(get_busy);
    QProgressDialog prog("Get",QString(),0,100,this);
    prog.setWindowTitle("Get");
    prog.setWindowModality(Qt::WindowModal);        // 取得中は、このウィンドウを操作できない
    prog.setCancelButton(nullptr);
    prog.setMinimumDuration(300);                   // 短い取得では、ダイアログを出さない
    auto step = [this,&prog](const QString &label,int v){
        prog.setLabelText(label);
        prog.setValue(v);
        statusBar()->showMessage(label);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    step("Get: start",0);
    corrdata->set_progress([&step](int stage,int done,int total){
        static const char *names[] = {"ショットを検索してタグを決定中","信号の値を取得中"};
        static const int base[] = {0,45},span[] = {45,45};
        int v = base[stage] + (total>0 ? span[stage]*done/total : span[stage]);
        step(QString("%1  %2 / %3").arg(names[stage]).arg(done).arg(total),v);
    });

    corrdata->get_tag(ui->dateTimeEdit->dateTime().toString("yyyy/MM/dd HH:mm:ss").toStdString());
    int found = corrdata->tag_count();
    corrdata->db_get_sync_data();
    corrdata->set_progress(nullptr);               // prog が無くなる前に外す
    if(corrdata->data_size()<2){                    // タグが決まったショットが足りない(捨てたものを除く)ので、描画しない
        statusBar()->showMessage(QString("Get: 有効なショットが %1 件しかありません").arg(corrdata->data_size()));
        QMessageBox::warning(this,"Correlation Plot",
            QString("タグ(sacla_tag/sr_tag)が決まった有効なショットが %1 件しかなく、グラフを描けません。\n"
                    "日時の指定や、DB の接続を確認してください。詳細はターミナルの [tag] の行にあります。").arg(corrdata->data_size()));
        return;
    }
    step("MIC などを計算中",90);
    for(int i=0;corr_debug() && i<corrdata->data_size();i++){
        DLOG<<"beg = "<<corrdata->begtime(i)<<
              "  end = "<<corrdata->endtime(i)<<
              "  sacla = "<< corrdata->sacla_tag(i)<<
              "  sr = "<<corrdata->sr_tag(i)<<endl;
        for(int j=0;j<corrdata->sig_size();j++){
            DLOG<<i<<" "<<j<<"  "<<
                  corrdata->sig_name(j)<<"  "<<
                  corrdata->value(corrdata->sig_name(j),i)<<endl;
        }

    }
    if(ui->checkBox_th->checkState()==Qt::Checked){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }
    update_rank_scores();
    for(int j=0;corr_debug() && j<corrdata->sig_size();j++){
        DLOG<<" "<<j<<"  "<<
              corrdata->sig_name(j)<<"  "<<
              corrdata->mic(j)<<endl;
    }

    step("グラフを描画中",95);
    plot_data(ui->widget_1,corrdata->get_ranking(1));
    plot_data(ui->widget_2,corrdata->get_ranking(2));
    plot_data(ui->widget_3,corrdata->get_ranking(3));
    plot_data(ui->widget_7,corrdata->get_ranking(4));
    plot_data(ui->widget_8,corrdata->get_ranking(5));
    plot_data(ui->widget_9,corrdata->get_ranking(6));
    plot_extra(true);
    plot_trend(ui->widget);
    mark_calculated();
    init_highlight_threshold();
    corrdata->outfile(CSV_FILE);
    prog.setValue(100);
    statusBar()->showMessage(QString("Get 完了: %1 ショット").arg(corrdata->data_size()));
    if(found==0){
        QMessageBox::warning(this,"Correlation Plot",
            "ショットが 1 つも見つかりませんでした。\n\n"
            "日時の指定や、DB の接続を確認してください。\n"
            "DB の応答(status と count)は、ターミナルの [get] の行に出ています。");
    }
}
void MainWindow::plot_init(QCustomPlot *plot)
{
    style_plot(plot);
    plot->addGraph();
    plot->graph(0)->setPen(QPen(MAIN_COLOR));
    plot->graph(0)->setLineStyle(QCPGraph::lsNone);
    plot->graph(0)->setScatterStyle(point_style(MAIN_COLOR,false));
    plot->addGraph();
    plot->graph(1)->setPen(QPen(LATEST2_COLOR));
    plot->graph(1)->setLineStyle(QCPGraph::lsNone);
    plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST2_COLOR,LATEST2_COLOR,13));
    plot->addGraph();
    plot->graph(2)->setPen(QPen(LATEST1_COLOR));
    plot->graph(2)->setLineStyle(QCPGraph::lsNone);
    plot->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST1_COLOR,LATEST1_COLOR,15));

}
void MainWindow::plot_trend(QCustomPlot *plot)
{
    if(plot==ui->widget)update_trend_panel();       // 子パネルの信号ごとのトレンドも更新
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    QFont legendFont = font();
    legendFont.setPointSize(9);
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("HH:mm:ss");
    plot->xAxis->setTicker(dateTicker);
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  1****"<<endl;
    const string &name_y0 = corrdata->sig_name(0);
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = QDateTime::fromString(QString::fromStdString(corrdata->endtime(i)),"yyyy/MM/dd HH:mm:ss.z").toSecsSinceEpoch();
        y[i] = corrdata->value(name_y0,i);
    }
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;
    QVector<int> shot_all(x.size());
    for(int i=0;i<shot_all.size();i++)shot_all[i] = i;
    set_scatter_data(plot,x,y,y,shot_all);

    set_recent_points(plot,x,y);
    if(plot==ui->widget)update_moving_average();
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;

    if(ui->checkBox_th->checkState()==Qt::Checked){
        double lo,hi;
        corrdata->get_range(0,0.5,0,ui->V_threshold->text().toDouble(),lo,hi);
        plot->yAxis->setRange(lo,hi);
    }else{
        double lo,hi;
        corrdata->get_range(0,0.5,lo,hi);
        plot->yAxis->setRange(lo,hi);

    }
    plot->xAxis->setRange(x[0]-60,x[corrdata->data_size()-1]+60);
    //plot->graph(0)->rescaleAxes(true);
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  3****"<<endl;
    plot->xAxis->setLabel("time");
    plot->yAxis->setLabel(QString::fromStdString(corrdata->target_label()));
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  4****"<<endl;
    QVector<double> x4(2),y4(2);
    x4[0] = x4[1] = y4[0] = y4[1] = 0;
    ui->widget->graph(3)->setData(x4,y4);
    ui->widget->graph(4)->setData(x4,y4);
    ui->widget->graph(3)->setVisible(false);
    ui->widget->graph(4)->setVisible(false);
    tracer->setVisible(false);
    tracerText->setVisible(false);

    plot->replot(QCustomPlot::rpQueuedReplot);
}
// x(信号)を等頻度の区間に分け、各区間の y(基準信号)の平均±標準誤差の折れ線を重ねる。
// 効率が最大になる x の位置を読み取るためのもの。最大の区間を説明する文字列を返す。
QString MainWindow::update_binned_curve(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    BinnedItems &b = binned[plot];
    if(!stats_act->isChecked()){
        if(b.graph)b.graph->setVisible(false);
        if(b.err)b.err->setVisible(false);
        return QString();
    }
    if(!b.graph){
        b.graph = plot->addGraph();
        b.graph->setPen(QPen(T.text,2));
        b.graph->setLineStyle(QCPGraph::lsLine);
        b.graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,T.text,6));
        b.err = new QCPErrorBars(plot->xAxis,plot->yAxis);
        b.err->setDataPlottable(b.graph);
        b.err->setPen(QPen(T.text,1.5));
    }

    // MIC の計算と同じ条件(Threshold オンなら基準信号が閾値を超えたショットだけ)
    bool useTh = ui->checkBox_th->checkState()==Qt::Checked;
    bool detrended = detrend_act->isChecked();   // 表示データは閾値で絞り込み済み
    double th = ui->V_threshold->text().toDouble();
    QVector<QPair<double,double>> pts;
    for(int i=0;i<x.size() && i<y.size();i++){
        if(!std::isfinite(x[i]) || !std::isfinite(y[i]))continue;
        if(useTh && !detrended && !(y[i]>th))continue;
        pts.append(qMakePair(x[i],y[i]));
    }
    std::sort(pts.begin(),pts.end());

    int n = pts.size();
    int nb = qBound(0,n/10,10);     // 1区間あたり10点以上
    b.graph->setVisible(true);
    b.err->setVisible(true);
    if(nb<3){
        b.graph->data()->clear();
        b.err->data().clear();
        return QString();
    }
    QVector<double> kx,ky,ke;
    int best = -1;
    for(int k=0;k<nb;k++){
        int s = k*n/nb, e = (k+1)*n/nb, cnt = e-s;
        double mx=0,my=0;
        for(int i=s;i<e;i++){mx+=pts[i].first;my+=pts[i].second;}
        mx/=cnt;my/=cnt;
        double var=0;
        for(int i=s;i<e;i++)var+=(pts[i].second-my)*(pts[i].second-my);
        double sem = sqrt(var/(cnt-1))/sqrt((double)cnt);
        kx.append(mx);ky.append(my);ke.append(sem);
        if(best<0 || my>ky[best])best = k;
    }
    b.graph->setData(kx,ky);
    b.err->setData(ke);
    return QString("best x : %1  (mean %2)").arg(kx[best],0,'g',5).arg(ky[best],0,'g',5);
}

// グラフ右上の説明文(MIC、無相関の95%点、Spearman、p値、最適位置)
void MainWindow::set_info_text(QCustomPlot *plot,int n,const QString &best,const QFont &font)
{
    BinnedItems &b = binned[plot];
    if(!b.text){
        b.text = new QCPItemText(plot);
        b.text->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
        b.text->position->setType(QCPItemPosition::ptAxisRectRatio);
        b.text->position->setCoords(0.75, 0);
        b.text->setPen(QPen(T.label_border));
        b.text->setBrush(QBrush(T.label_bg));
    }
    double mic = corrdata->mic(n);
    b.text->setVisible(true);
    b.text->setFont(font);
    // 重要度順のときは重要度[%]、平均の差順のときは差(標準偏差単位)を併記する
    QString impLine;
    if(corrdata->rank_importance()){
        impLine = QString("\nimp : %1 %").arg(corrdata->importance(n)*100,0,'f',1);
    }
    if(corrdata->rank_gap()){
        impLine = QString("\ngap : %1 sd").arg(corrdata->gap(n),0,'f',2);
    }
    if(!stats_act->isChecked()){
        b.text->setText(QString("mic : %1").arg(mic,0,'f',3) + impLine);
        b.text->setColor(T.text);
        return;
    }
    double nul = corrdata->mic_null(n);
    QString s = QString("mic : %1  (null95 %2)\nrho : %3  p : %4")
            .arg(mic,0,'f',3).arg(nul,0,'f',3)
            .arg(corrdata->rho(n),0,'f',2).arg(corrdata->pval(n),0,'f',3);
    s += impLine;
    if(!best.isEmpty())s += "\n" + best;
    b.text->setText(s);
    // 偶然の範囲を超えていれば青緑字
    bool significant = mic>nul && corrdata->pval(n)<0.05;
    b.text->setColor(significant ? MAIN_COLOR : T.muted);
}

// 最新の3点を色分けして重ねる(色は settings.ini の [Colors] latest1〜latest3):
// 最新=赤(サイズ15, graph(2))、2番目=えんじ色(13, graph(1))、3番目=紫(11)。
// 4番目以降は通常の点と同じ(青緑の輪、Highlight なら塗りつぶし/灰色の輪、Gradient ならその色)
void MainWindow::set_recent_points(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    RecentGraphs &r = recent[plot];
    if(!r.g3){
        r.g3 = plot->addGraph();
        r.g3->setPen(QPen(LATEST3_COLOR));
        r.g3->setLineStyle(QCPGraph::lsNone);
        r.g3->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST3_COLOR,LATEST3_COLOR,11));
        // 最新3点は、後から作られる青緑の点(Highlight)やグラデーションの点に隠れて白抜きに見えないよう、
        // 常に手前(overlay レイヤー)に描く。後から移したものほど手前なので、最新の点が一番上になる
        r.g3->setLayer("overlay");
        plot->graph(1)->setLayer("overlay");
        plot->graph(2)->setLayer("overlay");
    }
    auto pick = [&x,&y](QCPGraph *g,int back){
        QVector<double> xs,ys;
        if(x.size()>=back && y.size()>=back){
            xs.append(x[x.size()-back]);
            ys.append(y[y.size()-back]);
        }
        g->setData(xs,ys);
        g->setVisible(true);
    };
    pick(plot->graph(2),1);
    pick(plot->graph(1),2);
    pick(r.g3,3);
    r.range_mode = false;
    update_recent_line(plot,x,y);
    update_recent_mean(plot,x,y);
}

// settings.ini の値を文字列で読む。QSettings は行の途中のコメントを扱えず、カンマを含む値は
// リストになるので、コメントを取り除き、リストはカンマでつなぎ直して返す。
static QString ini_string(QSettings &st,const QString &key,const QString &def)
{
    if(!st.contains(key))return def;
    QVariant var = st.value(key);
    QString s = (var.metaType().id()==QMetaType::QStringList) ? var.toStringList().join(",") : var.toString();
    int cut = s.indexOf(QRegularExpression("(;|\\s#)"));    // 「;」、または空白の後の「#」から後ろはコメント(「#8000ff」のような色指定は残す)
    if(cut>=0)s = s.left(cut);
    return s.trimmed();
}

// 起動時に settings.ini の [Colors] から、強調の色と最新の点の色を読む(再起動で切り替わる)。
// 既定の色は、[Theme] name で選んだテーマ(dark / classic)による。ここで書いた色は、テーマの色より優先する。
//   [Colors]
//   highlight=#2EC4B6     ; 閾値以上の点・縦線・数値・十字印などの色(dark の既定。classic は #00696B)
//   latest1=#FF5A5F       ; 最新の点(サイズ15)       (dark の既定。classic は #ff0000)
//   latest2=#E8646E       ; 2番目の点(サイズ13)      (dark の既定。classic は #b3424a)
//   latest3=#B794FF       ; 3番目の点(サイズ11)      (dark の既定。classic は #8000ff)
//   cross_near=#2EC4B6    ; 十字印が優秀なショットの平均に近いとき(閾値以上のとき)の色。無ければ highlight と同じ
//   cross=#E6EAF0         ; 十字印が近くないときの色(dark の既定。classic は黒)
// 色は「#RRGGBB」または色名(teal, red など)。無い/不正なキーは既定の色のまま。
void MainWindow::load_colors()
{
    if(T.dark)return;       // [Colors] の上書きは classic のときだけ(暗いテーマは、見やすさのために色の組み合わせを決めてあるので)
    QFileInfo fi(signal_list_file("settings.ini"));
    if(!fi.exists())return;
    QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
    auto get = [&st](const char *key,QColor &col){
        QString s = ini_string(st,key,"");
        if(s.isEmpty())return;
        QColor c(s);
        if(c.isValid())col = c;
        else cerr << "WARNING: settings.ini " << key << "=" << s.toStdString() << " is not a valid color; using the default" << endl;
    };
    get("Colors/highlight",MAIN_COLOR);
    get("Colors/latest1",LATEST1_COLOR);
    get("Colors/latest2",LATEST2_COLOR);
    get("Colors/latest3",LATEST3_COLOR);
    get("Colors/cross_near",CROSS_NEAR_COLOR);
    get("Colors/cross",CROSS_COLOR);
}

// 起動時に settings.ini の [Theme] name を読んで、配色を決める(既定 dark)。QSettings が読めるように、先に設定ディレクトリを決めておく
void MainWindow::load_theme()
{
    QString name = "dark";
    QFileInfo fi(signal_list_file("settings.ini"));
    if(fi.exists()){
        QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
        name = ini_string(st,"Theme/name","dark").toLower();
        bool ok = false;
        for(const auto &t : THEME_LIST)if(t.first==name)ok = true;
        if(!ok){
            cerr << "WARNING: settings.ini Theme/name must be dark, dark-contrast, dark-amber, dark-navy, dark-pink-deepest or classic; using dark" << endl;
            name = "dark";
        }
    }
    QFile f(signal_list_file("theme.txt"));      // Theme メニューで最後に選んだもの(ini より優先)
    if(f.open(QIODevice::ReadOnly | QIODevice::Text)){
        QString s = QString::fromUtf8(f.readLine()).trimmed();
        for(const auto &t : THEME_LIST)if(t.first==s)name = s;
    }
    set_theme(name);
    cerr << "Theme : " << g_theme_name.toStdString() << endl;
}

// ダークテーマ: Fusion スタイル + 暗いパレット + スタイルシート(ボタン、入力欄、リスト、メニュー、スクロールバー、進捗バーなど)。
// スピンボックスやチェックボックスは、スタイルシートで枠を指定すると矢印や印が消えることがあるので、パレットだけで色を付ける
void MainWindow::apply_theme_widgets()
{
    // 起動直後のスタイルを覚えておき、classic に戻すときに使う
    static QString orig_style;
    if(orig_style.isEmpty())orig_style = QApplication::style()->objectName();
    if(!T.dark){
        QApplication::setStyle(orig_style);
        QApplication::setPalette(QApplication::style()->standardPalette());
        qApp->setStyleSheet(QString());
        return;
    }
    auto mix = [](const QColor &a,const QColor &b,double t){      // a に b を t の割合で混ぜる
        return QColor(qRound(a.red()*(1-t)+b.red()*t),qRound(a.green()*(1-t)+b.green()*t),qRound(a.blue()*(1-t)+b.blue()*t));
    };
    const QColor base = T.window;                       // 入力欄・リストの背景
    const QColor button = T.panel.lighter(125);
    const QColor bborder = T.border.lighter(140);
    const QColor hover = button.lighter(120);
    const QColor pressed = button.darker(125);
    const QColor sel = mix(T.window,T.accent,0.30);     // リストの選択行
    const QColor off = mix(T.muted,T.window,0.55);      // 無効な文字
    const QColor handle = bborder, handle_h = bborder.lighter(130);
    QApplication::setStyle("Fusion");
    QPalette p;
    p.setColor(QPalette::Window,T.window);
    p.setColor(QPalette::WindowText,T.text);
    p.setColor(QPalette::Base,base);
    p.setColor(QPalette::AlternateBase,T.panel);
    p.setColor(QPalette::Text,T.text);
    p.setColor(QPalette::Button,button);
    p.setColor(QPalette::ButtonText,T.text);
    p.setColor(QPalette::ToolTipBase,T.panel);
    p.setColor(QPalette::ToolTipText,T.text);
    p.setColor(QPalette::PlaceholderText,T.muted);
    p.setColor(QPalette::Highlight,T.accent);
    p.setColor(QPalette::HighlightedText,T.window);
    p.setColor(QPalette::Link,T.accent);
    p.setColor(QPalette::Light,button.lighter(130));
    p.setColor(QPalette::Dark,T.window.darker(150));
    p.setColor(QPalette::Mid,pressed);
    p.setColor(QPalette::Disabled,QPalette::WindowText,off);
    p.setColor(QPalette::Disabled,QPalette::Text,off);
    p.setColor(QPalette::Disabled,QPalette::ButtonText,off);
    QApplication::setPalette(p);

    qApp->setStyleSheet(QString(
        "QToolTip{background:%1;color:%2;border:1px solid %3;}"
        "QPushButton{background:%7;border:1px solid %8;border-radius:6px;padding:2px 10px;}"
        "QPushButton:hover{background:%9;border-color:%4;}"
        "QPushButton:pressed{background:%10;}"
        "QPushButton:disabled{color:%11;}"
        "QLineEdit{background:%12;border:1px solid %8;border-radius:4px;padding:1px 4px;"
        "selection-background-color:%4;selection-color:%5;}"
        "QLineEdit:focus{border-color:%4;}"
        "QListWidget,QTreeWidget,QTableWidget{background:%12;border:1px solid %3;border-radius:4px;}"
        "QListWidget::item:selected,QTreeWidget::item:selected,QTableWidget::item:selected{background:%13;color:%2;}"
        "QHeaderView::section{background:%1;color:%6;border:0;border-bottom:1px solid %3;padding:3px 6px;}"
        "QMenuBar{background:%5;}"
        "QMenuBar::item:selected{background:%7;}"
        "QMenu{background:%1;border:1px solid %3;}"
        "QMenu::item{padding:4px 22px;}"
        "QMenu::item:selected{background:%4;color:%5;}"
        "QMenu::separator{height:1px;background:%3;margin:4px 8px;}"
        "QScrollBar:vertical{background:%5;width:10px;margin:0;}"
        "QScrollBar::handle:vertical{background:%14;border-radius:5px;min-height:24px;}"
        "QScrollBar::handle:vertical:hover{background:%15;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{background:%5;height:10px;margin:0;}"
        "QScrollBar::handle:horizontal{background:%14;border-radius:5px;min-width:24px;}"
        "QScrollBar::handle:horizontal:hover{background:%15;}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "QProgressBar{border:1px solid %8;border-radius:4px;text-align:center;background:%12;}"
        "QProgressBar::chunk{background:%4;border-radius:3px;}"
        "QStatusBar{background:%5;color:%6;}"
    ).arg(T.panel.name(),T.text.name(),T.border.name(),T.accent.name(),T.window.name(),T.muted.name())
     .arg(button.name(),bborder.name(),hover.name(),pressed.name(),off.name(),base.name())
     .arg(sel.name(),handle.name(),handle_h.name()));
}

// グラフの背景・軸・格子をテーマにそろえる(ダークテーマのときだけ。classic は QCustomPlot の既定のまま)
void MainWindow::style_plot(QCustomPlot *plot)
{
    plot->setBackground(QBrush(T.plot_bg));
    plot->axisRect()->setBackground(QBrush(T.plot_bg));
    for(QCPAxis *ax : {plot->xAxis,plot->yAxis}){
        double w = T.dark ? 1 : 0;
        ax->setBasePen(QPen(T.axis_line,w));
        ax->setTickPen(QPen(T.axis_line,w));
        ax->setSubTickPen(QPen(T.axis_line,w));
        ax->setTickLabelColor(T.muted);
        ax->setLabelColor(T.text);
        if(T.dark){
            ax->grid()->setPen(QPen(T.grid,1));
            ax->grid()->setZeroLinePen(QPen(T.grid,1));
        }else{      // QCustomPlot の既定
            ax->grid()->setPen(QPen(QColor(200,200,200),0,Qt::DotLine));
            ax->grid()->setZeroLinePen(QPen(QColor(99,99,99),0,Qt::SolidLine));
        }
        ax->grid()->setSubGridVisible(false);
    }
}

// テーマを切り替えたとき、すでに作ってあるグラフの色を新しいテーマの色に変える(データの再計算はしない)
void MainWindow::restyle_plot(QCustomPlot *plot)
{
    style_plot(plot);
    if(plot->graphCount()>=3){
        plot->graph(1)->setPen(QPen(LATEST2_COLOR));
        plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST2_COLOR,LATEST2_COLOR,13));
        plot->graph(2)->setPen(QPen(LATEST1_COLOR));
        plot->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST1_COLOR,LATEST1_COLOR,15));
    }
    if(recent.contains(plot)){
        RecentGraphs &r = recent[plot];
        if(r.g3){
            r.g3->setPen(QPen(LATEST3_COLOR));
            r.g3->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,LATEST3_COLOR,LATEST3_COLOR,11));
        }
        if(r.line)r.line->setPen(QPen(T.curve_line,1.5));
    }
    if(highlight_graph.contains(plot) && highlight_graph[plot]){
        highlight_graph[plot]->setPen(QPen(MAIN_COLOR));
        highlight_graph[plot]->setScatterStyle(point_style(MAIN_COLOR,true));
    }
    if(hover_graph.contains(plot) && hover_graph[plot])
        hover_graph[plot]->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(MAIN_COLOR,2.5),QBrush(Qt::NoBrush),18));
    if(binned.contains(plot)){
        BinnedItems &b = binned[plot];
        if(b.graph){
            b.graph->setPen(QPen(T.text,2));
            b.graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,T.text,6));
        }
        if(b.err)b.err->setPen(QPen(T.text,1.5));
        if(b.text){
            b.text->setPen(QPen(T.label_border));
            b.text->setBrush(QBrush(T.label_bg));
            b.text->setColor(T.muted);
        }
    }
    if(mean_lines.contains(plot)){
        MeanLines &m = mean_lines[plot];
        if(m.hi)m.hi->setPen(QPen(MAIN_COLOR,1.5));
        if(m.band)m.band->setBrush(QBrush(T.band));
        if(m.hi_text){
            m.hi_text->setColor(MAIN_COLOR);
            m.hi_text->setBrush(QBrush(T.label_bg));
        }
    }
}

// Theme メニューで選んだテーマに切り替える(実行中。ウィジェット、グラフ、点の色をすべて変える)
void MainWindow::switch_theme(const QString &name)
{
    set_theme(name);
    load_colors();              // classic のときだけ、settings.ini の [Colors] を反映
    apply_theme_widgets();
    // 設定ディレクトリの theme.txt に覚える(次回の起動で使う。settings.ini は書き換えない)
    QString file = signal_list_file("theme.txt");
    QDir().mkpath(QFileInfo(file).absolutePath());
    QFile f(file);
    if(f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))f.write((g_theme_name+"\n").toUtf8());

    QList<QCustomPlot*> plots = {ui->widget,ui->widget_1,ui->widget_2,ui->widget_3,ui->widget_7,ui->widget_8,ui->widget_9};
    for(QCustomPlot *p : extra_plots)plots.append(p);
    for(QCustomPlot *p : plots)restyle_plot(p);
    // 点の色(通常/強調/グラデーション)と十字印は、保持しているデータからすぐに描き直せる
    for(auto it=scatter_xy.begin();it!=scatter_xy.end();++it){
        apply_display(it.key());
        update_recent_mean(it.key(),it.value().x,it.value().y);
    }
    if(tracer){
        tracer->setPen(QPen(MAIN_COLOR));
        tracer->setBrush(MAIN_COLOR);
        tracerText->setColor(T.text);
    }
    // リストの「mean_label」の文字色
    for(QListWidget *l : {ui->listWidget})
        for(int i=0;i<l->count();i++)apply_item_style(l->item(i));
    style_add_badge();
    update_trend_highlight();
    update_moving_average();
    draw_heatmap();
    for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
        style_plot(it->plot);
        if(it->cursor)it->cursor->setPen(QPen(T.text,1.5,Qt::DashLine));
        if(it->base_line)it->base_line->setPen(QPen(T.muted,1.2,Qt::DashLine));
        it->plot->replot();
    }
    for(QCustomPlot *p : plots)p->replot();
    statusBar()->showMessage(tr("Theme: %1").arg(g_theme_name),3000);
}

// ===== 子パネル(Trend Panel) =====
// Plot Signal の全信号のトレンドグラフ(時刻 vs 値)を縦に並べる。
//  - 各グラフの幅は、メインのトレンドグラフと同じ。横軸(時刻)の範囲も連動する
//  - 信号ごとのチェックボックス(既定は全部オン)で、表示する/しないを選ぶ
//  - 値はそのまま表示し、直近の Get で取れたショットの平均値に点線を引く。データ点には○を付ける
void MainWindow::open_trend_panel()
{
    if(!trend_win){
        trend_win = new QWidget(this,Qt::Window);
        trend_win->setWindowTitle("Trend Panel");
        QVBoxLayout *top = new QVBoxLayout(trend_win);
        top->setContentsMargins(6,6,6,6);
        QHBoxLayout *bar = new QHBoxLayout;
        QPushButton *all_on = new QPushButton("All ON",trend_win);
        QPushButton *all_off = new QPushButton("All OFF",trend_win);
        bar->addWidget(all_on);
        bar->addWidget(all_off);
        bar->addStretch();
        top->addLayout(bar);
        trend_scroll = new QScrollArea(trend_win);
        trend_scroll->setWidgetResizable(true);
        trend_scroll->setFrameShape(QFrame::NoFrame);
        QWidget *inner = new QWidget;
        trend_lay = new QVBoxLayout(inner);
        trend_lay->setContentsMargins(0,0,0,0);
        trend_lay->setSpacing(4);
        trend_lay->addStretch();
        trend_scroll->setWidget(inner);
        top->addWidget(trend_scroll);
        auto set_all = [this](bool on){
            for(auto it=trend_rows.begin();it!=trend_rows.end();++it)it->cb->setChecked(on);
        };
        connect(all_on,&QPushButton::clicked,this,[set_all](){set_all(true);});
        connect(all_off,&QPushButton::clicked,this,[set_all](){set_all(false);});
        // メインのトレンドグラフの横軸が動いたら、子パネルも合わせる(子のグラフ側の操作は update_trend_panel で連動させる)
        connect(ui->widget->xAxis,QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),this,[this](const QCPRange &r){
            if(trend_syncing)return;
            trend_syncing = true;
            for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
                it->plot->xAxis->setRange(r);
                it->plot->replot(QCustomPlot::rpQueuedReplot);
            }
            trend_syncing = false;
        });
    }
    update_trend_panel();
    fit_trend_panel_width();
    int w = ui->widget->width() + 12 + trend_scroll->verticalScrollBar()->sizeHint().width() + 6;
    trend_win->resize(w,std::max(500,height()*3/4));
    trend_win->show();
    trend_win->raise();
    trend_win->activateWindow();
}

// 各トレンドグラフの幅を、メインのトレンドグラフの幅にそろえる
void MainWindow::fit_trend_panel_width()
{
    int w = ui->widget->width();
    for(auto it=trend_rows.begin();it!=trend_rows.end();++it)it->plot->setFixedWidth(w);
}

void MainWindow::update_trend_panel()
{
    if(!corrdata)return;
    const int n = corrdata->data_size(),ns = corrdata->sig_size();
    // 子パネルに出す信号: 合成した基準値、基準信号、説明変数の順
    QStringList names;
    for(int i=0;i<ns;i++){
        names << QString::fromStdString(corrdata->sig_name(i));
        if(i==0)for(int k=0;k<corrdata->target_count();k++)names << QString::fromStdString(corrdata->target_name(k));
    }
    // 初期値は、Get した時点の全ショットの平均値。パネルが閉じていても記録する
    auto mean_of = [this,n](const QString &name){
        double s = 0;
        for(int i=0;i<n;i++)s += corrdata->value(name.toStdString(),i);
        return n>0 ? s/n : 0.0;
    };
    if(trend_rebase && n>0){
        trend_base.clear();
        for(const QString &nm : names)trend_base[nm] = mean_of(nm);
        trend_rebase = false;
    }
    if(!trend_win || !trend_win->isVisible())return;
    if(n<1 || ns<1)return;

    if(names!=trend_order){         // 信号の構成が変わったので、行を作り直す(チェックの状態は信号名で覚えている)
        for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
            trend_lay->removeWidget(it->box);
            it->box->deleteLater();
        }
        trend_rows.clear();
        trend_order = names;
        for(int i=0;i<names.size();i++){
            const QString &name = names[i];
            TrendRow row;
            row.box = new QWidget;
            QVBoxLayout *l = new QVBoxLayout(row.box);
            l->setContentsMargins(0,0,0,0);
            l->setSpacing(0);
            row.cb = new QCheckBox(name==QString::fromStdString(Corr_data::target_name_virtual()) ? QString::fromStdString(corrdata->target_label())+"  (combined target)" : name,row.box);
            row.cb->setChecked(trend_on.value(name,true));
            row.plot = new QCustomPlot(row.box);
            row.plot->setFixedHeight(150);
            row.plot->setFixedWidth(ui->widget->width());
            style_plot(row.plot);
            row.plot->addGraph();
            QColor col = QColor::fromHsv((i*47)%360,170,235);
            if(!T.dark)col = QColor::fromHsv((i*47)%360,220,170);
            row.plot->graph(0)->setPen(QPen(col,1.5));
            row.plot->graph(0)->setLineStyle(QCPGraph::lsNone);      // 実線は引かず、データ点だけ
            row.plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(col,1.2),QBrush(T.plot_bg),6));
            
            QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
            dateTicker->setDateTimeFormat("HH:mm:ss");
            row.plot->xAxis->setTicker(dateTicker);
            row.plot->yAxis->setLabel("value");
            row.plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
            row.plot->axisRect()->setRangeDrag(Qt::Horizontal);
            row.plot->axisRect()->setRangeZoom(Qt::Horizontal);
            row.plot->setVisible(row.cb->isChecked());
            l->addWidget(row.cb);
            l->addWidget(row.plot);
            trend_lay->insertWidget(trend_lay->count()-1,row.box);
            QCustomPlot *pl = row.plot;
            connect(row.cb,&QCheckBox::toggled,this,[this,name,pl](bool on){
                trend_on[name] = on;
                pl->setVisible(on);
            });
            connect(row.plot->xAxis,QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged),this,[this,pl](const QCPRange &r){
                if(trend_syncing)return;        // 子のグラフを動かしたら、メインと他の子も連動
                trend_syncing = true;
                ui->widget->xAxis->setRange(r);
                ui->widget->replot(QCustomPlot::rpQueuedReplot);
                for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
                    if(it->plot==pl)continue;
                    it->plot->xAxis->setRange(r);
                    it->plot->replot(QCustomPlot::rpQueuedReplot);
                }
                trend_syncing = false;
            });
            trend_rows[name] = row;
        }
    }
    QVector<double> x(n);
    for(int i=0;i<n;i++)
        x[i] = QDateTime::fromString(QString::fromStdString(corrdata->endtime(i)),"yyyy/MM/dd HH:mm:ss.z").toSecsSinceEpoch();
    trend_x = x;
    trend_syncing = true;       // 横軸は、メインのトレンドグラフと同じ範囲にする(連動のループを防ぐ)
    for(int k=0;k<names.size();k++){
        const QString &name = names[k];
        TrendRow &row = trend_rows[name];
        if(!trend_base.contains(name))trend_base[name] = mean_of(name);
        const double b = trend_base[name];
        QVector<double> y(n);
        for(int i=0;i<n;i++)y[i] = corrdata->value(name.toStdString(),i);
        row.plot->graph(0)->setData(x,y);
        row.y = y;
        row.plot->graph(0)->rescaleValueAxis();
        // 初期値(Get で取れたショットの平均値)の点線。縦軸の範囲に必ず入れる
        if(!row.base_line){
            row.base_line = new QCPItemStraightLine(row.plot);
            row.base_line->setPen(QPen(T.muted,1.2,Qt::DashLine));
        }
        row.base_line->point1->setCoords(0,b);
        row.base_line->point2->setCoords(1,b);
        QCPRange yr = row.plot->yAxis->range();
        if(b<yr.lower || b>yr.upper){
            yr.expand(QCPRange(b,b));
            row.plot->yAxis->setRange(yr);
        }
        row.plot->xAxis->setRange(ui->widget->xAxis->range());
        row.plot->replot(QCustomPlot::rpQueuedReplot);
    }
    trend_syncing = false;
    update_trend_highlight();
    update_moving_average();
}

// Highlight がオンのとき、メインのトレンドグラフと同じく、優秀なショット(基準信号が閾値以上)の点に
// [Colors] highlight の色(MAIN_COLOR)を付ける。それ以外の点は通常の点の色(灰色系)にする。オフなら信号ごとの色
void MainWindow::update_trend_highlight()
{
    if(!trend_win || trend_rows.isEmpty())return;
    const ScatterData *s = scatter_xy.contains(ui->widget) ? &scatter_xy[ui->widget] : nullptr;
    bool hl = ui->checkBox_highlight->isChecked() && s && s->hi.size()==s->x.size() && s->shot.size()==s->hi.size();
    QVector<char> hi_by_shot;
    if(hl){
        hi_by_shot.fill(0,trend_x.size());
        for(int k=0;k<s->shot.size();k++)
            if(s->shot[k]>=0 && s->shot[k]<hi_by_shot.size())hi_by_shot[s->shot[k]] = s->hi[k];
    }
    for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
        TrendRow &row = it.value();
        QCPGraph *g0 = row.plot->graph(0);
        QColor col = g0->pen().color();
        if(!row.hi_graph){
            row.hi_graph = row.plot->addGraph();
            row.hi_graph->setLineStyle(QCPGraph::lsNone);
            row.hi_graph->setVisible(false);
        }
        if(hl && row.y.size()==hi_by_shot.size()){
            g0->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(T.normal_point,1.2),QBrush(T.plot_bg),6));
            QVector<double> hx,hy;
            for(int i=0;i<hi_by_shot.size();i++)if(hi_by_shot[i]){hx.append(trend_x[i]);hy.append(row.y[i]);}
            row.hi_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(MAIN_COLOR,1.2),QBrush(MAIN_COLOR),7));
            row.hi_graph->setData(hx,hy);
            row.hi_graph->setVisible(true);
        }else{
            g0->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(col,1.2),QBrush(T.plot_bg),6));
            row.hi_graph->setVisible(false);
        }
        row.plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

// メインのトレンドグラフで指しているショットの時刻に、子パネルの各グラフへ縦線を引く(指していなければ消す)
void MainWindow::update_trend_cursor()
{
    if(!trend_win || trend_rows.isEmpty())return;
    bool show = false;
    double t = 0;
    if(hover_shot>=0 && scatter_xy.contains(ui->widget)){
        const ScatterData &s = scatter_xy[ui->widget];
        auto pos = std::lower_bound(s.shot.begin(),s.shot.end(),hover_shot);
        if(pos!=s.shot.end() && *pos==hover_shot){
            t = s.x[pos - s.shot.begin()];
            show = true;
        }
    }
    for(auto it=trend_rows.begin();it!=trend_rows.end();++it){
        TrendRow &row = it.value();
        if(!row.plot->isVisible())continue;
        if(!row.cursor){
            if(!show)continue;
            row.cursor = new QCPItemStraightLine(row.plot);
            row.cursor->setPen(QPen(T.text,1.5,Qt::DashLine));
        }
        if(show){
            row.cursor->point1->setCoords(t,0);
            row.cursor->point2->setCoords(t,1);
        }
        row.cursor->setVisible(show);
        row.plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

// ステータスバーの「ADD」の目印の色(テーマに合わせる)
void MainWindow::style_add_badge()
{
    if(!add_badge)return;
    add_badge->setStyleSheet(QString("background:%1;color:%2;border-radius:4px;font-weight:bold;padding:1px 6px;")
                             .arg(T.accent.name(),T.dark ? T.window.name() : QString("#ffffff")));
}

// Highlight の閾値を、基準信号の値 v から決める。
//   percentile : 値を小さい順に並べた value パーセンタイル(例:75 なら上位25%が閾値以上)。線形補間
//   sigma      : 平均 + value × 標準偏差
//   fixed      : value そのもの
static double compute_highlight_threshold(std::vector<double> v,const QString &method,double value)
{
    int n = (int)v.size();
    if(method=="fixed")return value;
    if(method=="sigma"){
        double sum = 0;
        for(int i=0;i<n;i++)sum += v[i];
        double mean = sum/n,var = 0;
        for(int i=0;i<n;i++)var += (v[i]-mean)*(v[i]-mean);
        return mean + value*sqrt(var/n);
    }
    std::sort(v.begin(),v.end());
    double p = std::min(100.0,std::max(0.0,value));
    double pos = p/100.0*(n-1);
    int lo = (int)floor(pos),hi = (int)ceil(pos);
    return v[lo] + (v[hi]-v[lo])*(pos-lo);
}

// 最新Nショットの平均での、k 番目に新しいショットの重み(k=0 が最新、n はショット数)。
//   equal       : すべて 1
//   linear      : 最新が n、最古が 1 の直線
//   exponential : 0.5^(k/半減期)  ── 半減期 h ショット前のショットの重みが、最新の半分になる
double MainWindow::recent_weight(int k,int n) const
{
    switch(mean_weighting){
    case WEIGHT_LINEAR:      return n - k;
    case WEIGHT_EXPONENTIAL: return std::pow(0.5,k/mean_half_life);
    default:                 return 1.0;
    }
}

// 十字印の平均に使う最新のショット数と、重み付け。
// 設定ファイル ~/.config/Correlation_plot/settings.ini の
//   [LatestMean]
//   shots=5
// から読む(1〜1000。無い/不正なときは5)。更新日時が変わったら読み直す(再起動不要)。
int MainWindow::latest_mean_shots()
{
    QFileInfo fi(signal_list_file("settings.ini"));
    QDateTime mt = fi.exists() ? fi.lastModified() : QDateTime();
    if(!settings_loaded || mt!=settings_mtime){
        settings_loaded = true;
        settings_mtime = mt;
        mean_shots = 5;
        mean_weighting = WEIGHT_EXPONENTIAL;
        mean_half_life = 0;         // 0 のままなら、N の 0.4 倍(下で決める)
        cross_sigma = 1.0;
        cross_require_closer = true;
        if(fi.exists()){
            QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
            bool ok = false;
            int n = ini_string(st,"LatestMean/shots","5").toInt(&ok);
            if(ok && n>=1 && n<=1000)mean_shots = n;
            else DLOG << "settings.ini: invalid LatestMean/shots, using 5" << endl;
            // 平均の重み付け: weighting=equal|linear|exponential(既定 exponential)、half_life=半減期(ショット数)
            QString wt = ini_string(st,"LatestMean/weighting","exponential").toLower();
            if(wt=="equal")mean_weighting = WEIGHT_EQUAL;
            else if(wt=="linear")mean_weighting = WEIGHT_LINEAR;
            else if(wt=="exponential")mean_weighting = WEIGHT_EXPONENTIAL;
            else cerr << "WARNING: settings.ini LatestMean/weighting must be equal, linear or exponential; using exponential" << endl;
            bool okh = false;
            double hl = ini_string(st,"LatestMean/half_life","").toDouble(&okh);
            if(okh && hl>0 && hl<=1000)mean_half_life = hl;
            else if(st.contains("LatestMean/half_life"))cerr << "WARNING: settings.ini LatestMean/half_life is invalid; using 0.4 x shots" << endl;
            // [CrossColor] 十字印を青緑にする基準
            bool ok2 = false;
            double k = ini_string(st,"CrossColor/sigma","1").toDouble(&ok2);
            if(ok2 && k>0 && k<=100)cross_sigma = k;
            else if(st.contains("CrossColor/sigma"))DLOG << "settings.ini: invalid CrossColor/sigma, using 1" << endl;
            QString rc = ini_string(st,"CrossColor/require_closer_than_low","true").toLower();
            if(rc=="false" || rc=="0" || rc=="no" || rc=="off")cross_require_closer = false;
            DLOG << "Load settings : " << fi.absoluteFilePath().toStdString()
                 << "  (LatestMean/shots=" << mean_shots << ")" << endl;
        }
        // 半減期の既定は N の 0.4 倍(1/3〜1/2 の中ほど)
        if(mean_half_life<=0)mean_half_life = 0.4*mean_shots;
        DLOG << "Latest mean weighting : " << (mean_weighting==WEIGHT_EQUAL ? "equal" : mean_weighting==WEIGHT_LINEAR ? "linear" : "exponential")
             << "  half_life=" << mean_half_life << endl;
    }
    return mean_shots;
}

// 閾値以上(または未満)のショットの x の代表値と標準偏差。
//   Gravity オフ: 平均と標準偏差
//   Gravity オン: 縦軸の値(yref。Detrend 中も引く前の元の値)を重みにした重心と、重心まわりの重み付き標準偏差。
//                 重みが正の点だけを使い、重みの合計が 0 以下なら平均に戻す
// n は有限な x の個数。
MainWindow::GroupStat MainWindow::group_stat(const ScatterData &s,bool hi_group,bool gravity)
{
    GroupStat g;
    double sum = 0;
    for(int i=0;i<s.x.size() && i<s.hi.size();i++){
        if(!std::isfinite(s.x[i]) || (s.hi[i]!=0)!=hi_group)continue;
        sum += s.x[i];
        g.n++;
    }
    if(g.n==0)return g;
    g.center = sum/g.n;

    bool weighted = false;
    if(gravity){
        double sw = 0,swx = 0;
        for(int i=0;i<s.x.size() && i<s.hi.size() && i<s.yref.size();i++){
            if(!std::isfinite(s.x[i]) || (s.hi[i]!=0)!=hi_group)continue;
            double w = s.yref[i];
            if(!std::isfinite(w) || w<=0)continue;
            sw += w;
            swx += w*s.x[i];
        }
        if(sw>0){
            g.center = swx/sw;
            weighted = true;
        }
    }

    double var = 0,wsum = 0;
    for(int i=0;i<s.x.size() && i<s.hi.size();i++){
        if(!std::isfinite(s.x[i]) || (s.hi[i]!=0)!=hi_group)continue;
        double w = 1;
        if(weighted){
            w = (i<s.yref.size() && std::isfinite(s.yref[i]) && s.yref[i]>0) ? s.yref[i] : 0;
        }
        var += w*(s.x[i]-g.center)*(s.x[i]-g.center);
        wsum += w;
    }
    g.sd = wsum>0 ? sqrt(var/wsum) : 0;
    return g;
}

// 十字印の x 座標(最新Nショットの平均 xbar)が、閾値以上のショットの x の平均(Gravity オンなら重心)に「近い」かどうか。
//   (A) |xbar − μ_hi| ≤ k × σ_hi        σ_hi: 閾値以上のショットの x の標準偏差
//   (B) |xbar − μ_hi| < |xbar − μ_lo|   閾値未満の中心より閾値以上の中心に近い(require_closer_than_low)
// 閾値以上のショットが5未満、または σ_hi が 0 のときは判定しない(false)。
bool MainWindow::near_good_mean(const ScatterData &s,double xbar)
{
    bool gravity = ui->checkBox_gravity->isChecked() && ui->checkBox_gravity->isEnabled();
    GroupStat hi = group_stat(s,true,gravity);
    GroupStat lo = group_stat(s,false,gravity);
    if(hi.n<5)return false;
    if(hi.sd<=0)return false;
    double d_hi = fabs(xbar-hi.center);
    if(d_hi > cross_sigma*hi.sd)return false;
    if(cross_require_closer && lo.n>0){
        if(!(d_hi < fabs(xbar-lo.center)))return false;
    }
    return true;
}

// 最新Nショット(Nは settings.ini の LatestMean/shots、既定5)の平均((横軸の平均, 縦軸の平均))を十字印(+)で示す。
// 散布図だけが対象(トレンドは対象外)。Nショットに満たないときは出さない。
void MainWindow::update_recent_mean(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    if(plot==ui->widget)return;
    RecentGraphs &r = recent[plot];
    const int N = latest_mean_shots();
    bool on = mean5_act->isChecked() && !r.range_mode && x.size()>=N && y.size()>=N;
    if(!on){
        if(r.mean5)r.mean5->setVisible(false);
        return;
    }
    if(!r.mean5){
        r.mean5 = plot->addGraph();
        r.mean5->setLineStyle(QCPGraph::lsNone);
        r.mean5->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(CROSS_COLOR,2.5),QBrush(Qt::NoBrush),18));
        r.mean5->setLayer("overlay");       // 他の点より手前に描く
    }
    // 新しいショットほど重い重み付き平均(重みは recent_weight)。k=0 が最新
    double sx = 0,sy = 0,sw = 0;
    for(int k=0;k<N;k++){
        double w = recent_weight(k,N);
        sx += w*x[x.size()-1-k];
        sy += w*y[y.size()-1-k];
        sw += w;
    }
    sx /= sw;
    sy /= sw;
    QVector<double> mx(1,sx),my(1,sy);
    r.mean5->setData(mx,my);
    // Highlight がオンで、横軸の平均が「優秀なショットの平均」に近ければ、青緑に太くして目立たせる
    bool near = false;
    if(ui->checkBox_highlight->isChecked() && scatter_xy.contains(plot)){
        const ScatterData &s = scatter_xy[plot];
        near = near_good_mean(s,sx);
    }
    r.mean5->setScatterStyle(near
        ? QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(cross_near_color(),3.5),QBrush(Qt::NoBrush),22)
        : QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(CROSS_COLOR,2.5),QBrush(Qt::NoBrush),18));
    r.mean5->setVisible(true);
}

// 最新3点(古い順に 3番目 → 2番目 → 最新)を線でつなぐ。散布図とトレンドグラフの両方が対象。
// QCPGraph は x の順に並べ替えてしまうので、順序を保つ QCPCurve を使う。
void MainWindow::update_recent_line(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    RecentGraphs &r = recent[plot];
    bool on = line_act->isChecked() && !r.range_mode && x.size()>=3 && y.size()>=3;
    if(!on){
        if(r.line)r.line->setVisible(false);
        return;
    }
    if(!r.line){
        r.line = new QCPCurve(plot->xAxis,plot->yAxis);
        r.line->setPen(QPen(T.curve_line,1.5));
        r.line->setLayer("grid");   // 点(main)の下に描く
    }
    int n = x.size();
    QVector<double> t(3),lx(3),ly(3);
    for(int k=0;k<3;k++){
        t[k] = k;
        lx[k] = x[n-3+k];
        ly[k] = y[y.size()-3+k];
    }
    r.line->setData(t,lx,ly);
    r.line->setVisible(true);
}

void MainWindow::hide_recent_points(QCustomPlot *plot)
{
    if(recent.contains(plot) && recent[plot].g3){
        recent[plot].g3->setVisible(false);
    }
    recent[plot].range_mode = true;
    if(recent[plot].line)recent[plot].line->setVisible(false);
    if(recent[plot].mean5)recent[plot].mean5->setVisible(false);
}

// 追加の散布図を入れるスクロール領域(初期は非表示)を作る。File メニューの More Scatter Plots で開閉する。
// 散布図は、必要な数(7位以降の信号の数)だけ、plot_extra() が増やす。
// 領域の高さは3段分で、それを超える分は縦にスクロールして見る。
// 散布図の高さは、この領域の見える高さの 1/3(fit_extra_row_height)。上段の散布図とそろう。
void MainWindow::create_extra_plots()
{
    extra_scroll = new QScrollArea(ui->centralwidget);
    extra_scroll->setWidgetResizable(true);
    extra_scroll->setFrameShape(QFrame::NoFrame);
    extra_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    extra_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    extra_container = new QWidget;
    extra_grid = new QGridLayout(extra_container);
    extra_grid->setContentsMargins(0,0,0,0);
    extra_scroll->setWidget(extra_container);
    // 最小でも散布図3段分。レイアウトの比率(上段の各段:この領域 = 1:3)で、見える高さは上段3段分と同じになる
    extra_scroll->setMinimumHeight(3*230+2*extra_grid->spacing()+2);
    extra_scroll->viewport()->installEventFilter(this);     // 大きさが変わったら散布図の高さを合わせ直す
    extra_scroll->hide();
    ui->verticalLayout_5->addWidget(extra_scroll,3);   // 既存の2段(各1)に対して3段分
}

// スクロール領域の見える高さの 1/3 を、追加の散布図1段分の高さにする。
// 領域の高さはレイアウトの比率で上段3段分と同じになるので、追加の散布図も上段と同じ高さになる。
void MainWindow::fit_extra_row_height()
{
    if(!extra_scroll)return;
    int vh = extra_scroll->viewport()->height();
    int h = (vh - 2*extra_grid->spacing())/3;
    if(h<100 || h==extra_row_h)return;      // まだ大きさが決まっていない / 変化なし
    extra_row_h = h;
    for(QCustomPlot *p : extra_plots)p->setFixedHeight(extra_row_h);
}

// 追加の散布図を n 個以上にする(足りない分だけ作る。減らしはしない)
void MainWindow::ensure_extra_plots(int n)
{
    while(extra_plots.size()<n){
        int k = extra_plots.size();
        QCustomPlot *p = new QCustomPlot(extra_container);
        p->setMinimumWidth(248);
        p->setFixedHeight(extra_row_h);
        plot_init(p);
        p->installEventFilter(this);        // ホイールでスクロールするため
        extra_grid->addWidget(p,k/3,k%3);
        p->show();
        extra_plots.append(p);
    }
}

// File メニュー: ウィンドウを下に広げて、7位以降の信号の散布図をスクロール領域に出す/元に戻す
void MainWindow::set_extra_plots(bool show)
{
    if(show==extra_visible)return;
    bool resizable = !isMaximized() && !isFullScreen();
    if(show){
        extra_dh = extra_scroll->minimumHeight() + ui->verticalLayout_5->spacing();
        extra_scroll->show();
        extra_visible = true;
        if(resizable){
            int h = height() + extra_dh;
            if(screen())h = qMin(h,screen()->availableGeometry().height());
            resize(width(),h);
        }
        plot_extra(true);       // 今ある計算結果で7位以降を描く
    }else{
        extra_scroll->hide();
        extra_visible = false;
        if(resizable){
            resize(width(),qMax(minimumSizeHint().height(),height()-extra_dh));
        }
    }
}

// 空の信号順位(信号が足りない)のグラフは、基準信号同士の無意味な図にならないよう空にする
void MainWindow::clear_plot(QCustomPlot *plot)
{
    for(int i=0;i<plot->graphCount();i++)plot->graph(i)->data()->clear();
    if(binned.contains(plot)){
        if(binned[plot].text)binned[plot].text->setVisible(false);
        if(binned[plot].err)binned[plot].err->data().clear();
    }
    if(recent.contains(plot) && recent[plot].line)recent[plot].line->setVisible(false);
    if(recent.contains(plot) && recent[plot].mean5)recent[plot].mean5->setVisible(false);
    if(mean_lines.contains(plot)){
        if(mean_lines[plot].hi)mean_lines[plot].hi->setVisible(false);
        if(mean_lines[plot].hi_text)mean_lines[plot].hi_text->setVisible(false);
        if(mean_lines[plot].band)mean_lines[plot].band->setVisible(false);
    }
    scatter_xy.remove(plot);
    plot->xAxis->setLabel(QString());
    plot->yAxis->setLabel(QString());
    plot->replot(QCustomPlot::rpQueuedReplot);
}

// 7位以降の散布図(File メニューの More Scatter Plots で開いているときだけ)。
// 7位以降の信号の数(信号の総数 − 7)だけ散布図を用意し、使わない分は空にして隠す。
void MainWindow::plot_extra(bool b)
{
    if(!extra_visible || corrdata->sig_size()<2 || corrdata->data_size()<2)return;
    int need = qMax(0,corrdata->sig_size()-7);
    ensure_extra_plots(need);
    fit_extra_row_height();
    for(int k=0;k<extra_plots.size();k++){
        if(k<need){
            extra_plots[k]->setVisible(true);
            plot_data(extra_plots[k],corrdata->get_ranking(7+k,b));
        }else{
            clear_plot(extra_plots[k]);
            extra_plots[k]->setVisible(false);
        }
    }
}
void MainWindow::plot_extra_range(int beg,int end)
{
    if(!extra_visible || corrdata->sig_size()<2)return;
    int need = qMax(0,corrdata->sig_size()-7);
    ensure_extra_plots(need);
    fit_extra_row_height();
    for(int k=0;k<extra_plots.size();k++){
        if(k<need){
            extra_plots[k]->setVisible(true);
            plot_data(extra_plots[k],corrdata->get_ranking(7+k),beg,end);
        }else{
            clear_plot(extra_plots[k]);
            extra_plots[k]->setVisible(false);
        }
    }
}

// 散布図のデータを設定する(古い順に並んだ x,y)
void MainWindow::set_scatter_data(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y,const QVector<double> &yref,const QVector<int> &shot)
{
    ScatterData &s = scatter_xy[plot];
    s.x = x;
    s.y = y;
    s.yref = yref;
    s.shot = shot;
    // データが入れ替わると添字の意味が変わるので、指した点の輪は消す
    if(hover_graph.contains(plot) && hover_graph[plot])hover_graph[plot]->setVisible(false);
    hover_shot = -1;
    update_highlight_mask(plot);
    apply_display(plot);
}

// Highlight の閾値以上かどうかを点ごとに求める
void MainWindow::update_highlight_mask(QCustomPlot *plot)
{
    if(!scatter_xy.contains(plot))return;
    ScatterData &s = scatter_xy[plot];
    double thr = ui->V_highlight->text().toDouble();
    s.hi.resize(s.x.size());
    for(int i=0;i<s.x.size();i++){
        s.hi[i] = (i<s.yref.size() && s.yref[i]>=thr) ? 1 : 0;
    }
}

// 平均値の数値(左下)を出す信号かどうか。
// Plot Signal リストの項目に mean_label フラグ(signals.txt、右クリックの Show mean label)が付いた信号だけに出す。
// 基準信号(リストの先頭)は散布図の横軸にならないので数えない。
// フラグの付いた信号が1つもなければ、すべての信号に出す。
bool MainWindow::mean_label_enabled(const QString &signal)
{
    bool any = false,hit = false;
    for(int i=0;i<ui->listWidget->count();i++){
        QListWidgetItem *it = ui->listWidget->item(i);
        if(!item_has_flag(it,FLAG_MEAN_LABEL))continue;
        any = true;
        if(it->text()==signal)hit = true;
    }
    return !any || hit;
}

// 平均値の数値の出し方が変わったとき、全散布図の表示を更新する
void MainWindow::refresh_mean_labels()
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        update_mean_lines(it.key());
        it.key()->replot();
    }
}

// Plot Signal リストの右クリックメニュー: その信号の平均値の数値(mean_label フラグ)の付け外し
void MainWindow::list_context_menu(const QPoint &pos)
{
    QListWidgetItem *it = ui->listWidget->itemAt(pos);
    if(!it)return;
    QMenu menu(this);
    QAction *act = menu.addAction(tr("Show mean label"));
    act->setCheckable(true);
    act->setChecked(item_has_flag(it,FLAG_MEAN_LABEL));
    if(menu.exec(ui->listWidget->viewport()->mapToGlobal(pos))!=act)return;

    QStringList flags = it->data(FLAGS_ROLE).toStringList();
    if(act->isChecked())flags.append(FLAG_MEAN_LABEL);
    else flags.removeAll(FLAG_MEAN_LABEL);
    flags.removeDuplicates();
    it->setData(FLAGS_ROLE,flags);
    apply_item_style(it);
    save_signal_list(ui->listWidget,"signals.txt");
    refresh_mean_labels();
}

// Highlight 中は、散布図に縦線を引く。
//   青緑 : 閾値以上の点の x (横軸の信号) の平均
//   (閾値未満の点の x の平均は、線は引かず、薄い灰色の帯の片端としてだけ使う)
// トレンドグラフは対象外(横軸が時刻のため)。該当する点がなければその線は出さない。
void MainWindow::update_mean_lines(QCustomPlot *plot)
{
    MeanLines &m = mean_lines[plot];
    bool hl = ui->checkBox_highlight->isChecked();
    if(plot==ui->widget || !hl || !scatter_xy.contains(plot)){
        if(m.hi)m.hi->setVisible(false);
        if(m.hi_text)m.hi_text->setVisible(false);
        if(m.band)m.band->setVisible(false);
        return;
    }
    const ScatterData &s = scatter_xy[plot];
    // Gravity がオンなら、平均の代わりに縦軸の値を重みにした重心を使う(Highlight がオフの間は無効)
    bool gravity = ui->checkBox_gravity->isChecked() && ui->checkBox_gravity->isEnabled();
    GroupStat g_hi = group_stat(s,true,gravity);
    GroupStat g_lo = group_stat(s,false,gravity);
    int n_hi = g_hi.n,n_lo = g_lo.n;
    double c_hi = g_hi.center,c_lo = g_lo.center;
    auto place = [plot](QCPItemStraightLine *&line,const QColor &col,bool show,double xv){
        if(!line){
            line = new QCPItemStraightLine(plot);
            line->setPen(QPen(col,1.5));
        }
        line->point1->setCoords(xv,0);
        line->point2->setCoords(xv,1);
        line->setVisible(show);
    };
    place(m.hi,MAIN_COLOR,n_hi>0,c_hi);

    // 青緑の線(閾値以上の平均)と、閾値未満の平均の位置の間を薄い灰色で塗る(両方の平均があるときだけ)。
    // 閾値未満の平均の線は引かない。y は軸の上端〜下端いっぱい
    if(!m.band){
        m.band = new QCPItemRect(plot);
        m.band->setPen(Qt::NoPen);
        m.band->setBrush(QBrush(T.band));
        m.band->setLayer("grid");           // 点(main)の下に描く
        m.band->topLeft->setTypeX(QCPItemPosition::ptPlotCoords);
        m.band->topLeft->setTypeY(QCPItemPosition::ptAxisRectRatio);
        m.band->bottomRight->setTypeX(QCPItemPosition::ptPlotCoords);
        m.band->bottomRight->setTypeY(QCPItemPosition::ptAxisRectRatio);
    }
    bool both = n_hi>0 && n_lo>0;
    if(both){
        double xa = c_hi,xb = c_lo;
        m.band->topLeft->setCoords(std::min(xa,xb),0);
        m.band->bottomRight->setCoords(std::max(xa,xb),1);
    }
    m.band->setVisible(both);

    // 青緑の線(閾値以上の平均値)の数値を左下に表示する
    if(!m.hi_text){
        m.hi_text = new QCPItemText(plot);
        m.hi_text->setPositionAlignment(Qt::AlignBottom|Qt::AlignLeft);
        m.hi_text->position->setType(QCPItemPosition::ptAxisRectRatio);
        m.hi_text->position->setCoords(0.02,0.98);
        m.hi_text->setColor(MAIN_COLOR);
        m.hi_text->setPen(Qt::NoPen);
        m.hi_text->setBrush(QBrush(T.label_bg));
        QFont f = font();
        f.setPointSize(9);
        m.hi_text->setFont(f);
    }
    bool show_label = n_hi>0 && mean_label_enabled(plot_signal.value(plot));
    if(show_label)m.hi_text->setText(QString("%1(>=) : %2").arg(gravity ? "gravity" : "mean").arg(c_hi,0,'g',5));
    m.hi_text->setVisible(show_label);
}

// 点の色を決めて表示する。
//   通常      : 全点を青緑(graph(0))
//   Gradient  : 最古=青緑 → 最新=青 のグラデーション。1点ごとの色指定はできないので、
//               色ごとのグラフ(GRADIENT_BINS個)に点を振り分ける
//   Highlight : 閾値以上の点だけ色を付け(青緑、またはGradient)、それ以外は灰色(graph(0))にする
// 最新3点の色(赤/えんじ色/紫)は別のグラフで重ねているので、どの設定でも変わらない。
void MainWindow::apply_display(QCustomPlot *plot)
{
    apply_display_impl(plot);
    apply_shape(plot);
    if(plot==ui->widget)update_trend_highlight();       // 子パネルのトレンドにも、同じ強調を反映
}

// 点の形(横軸の信号名に「/phase」を含む散布図はひし形、それ以外は丸)を、点を描くすべてのグラフに反映する。
// 色やサイズはそのままで、形だけを変える。
void MainWindow::apply_shape(QCustomPlot *plot)
{
    QCPScatterStyle::ScatterShape shape = plot_shape.value(plot,QCPScatterStyle::ssCircle);
    auto set = [shape](QCPGraph *g){
        if(!g)return;
        QCPScatterStyle st = g->scatterStyle();
        if(st.shape()==shape)return;
        st.setShape(shape);
        g->setScatterStyle(st);
    };
    for(int i=0;i<3 && i<plot->graphCount();i++)set(plot->graph(i));
    if(recent.contains(plot)){
        set(recent[plot].g3);
    }
    if(highlight_graph.contains(plot))set(highlight_graph[plot]);
    if(gradient_first.contains(plot)){
        int first = gradient_first[plot];
        for(int b=0;b<50 && first+b<plot->graphCount();b++)set(plot->graph(first+b));
    }
}

void MainWindow::apply_display_impl(QCustomPlot *plot)
{
    if(!scatter_xy.contains(plot))return;
    const int GRADIENT_BINS = 50;
    ScatterData &s = scatter_xy[plot];
    bool grad = gradient_act->isChecked();
    bool hl = ui->checkBox_highlight->isChecked();

    // graph(0): 全点。Highlight 中は灰色、それ以外は青緑。
    // Gradient だけで非表示になる graph(0) には、データを入れない(setData は x での並べ替えが走る)。
    // トレンド(ui->widget)は tracer が graph(0) のデータを使うので常に入れる。
    QCPGraph *g0 = plot->graph(0);
    if(!grad || hl || plot==ui->widget)g0->setData(s.x,s.y);
    g0->setVisible(!grad || hl);
    QColor c0 = hl ? T.normal_point : MAIN_COLOR;
    g0->setPen(QPen(c0));
    g0->setScatterStyle(point_style(c0,false));

    // 強調する点だけの青緑のグラフ(Highlight かつ Gradient でないとき)
    if(hl && !grad){
        QCPGraph *&hg = highlight_graph[plot];
        if(!hg){
            hg = plot->addGraph();
            hg->setPen(QPen(MAIN_COLOR));
            hg->setLineStyle(QCPGraph::lsNone);
            hg->setScatterStyle(point_style(MAIN_COLOR,true));    // 閾値以上の点は塗りつぶし
        }
        QVector<double> hx,hy;
        for(int i=0;i<s.x.size();i++){
            if(s.hi[i]){hx.append(s.x[i]);hy.append(s.y[i]);}
        }
        hg->setData(hx,hy);
        hg->setVisible(true);
    }else if(highlight_graph.contains(plot) && highlight_graph[plot]){
        highlight_graph[plot]->setVisible(false);
    }
    update_mean_lines(plot);

    // グラデーション用のグラフは、その後ろ(最初に作った時点の末尾)に追加する
    if(grad && !gradient_first.contains(plot)){
        gradient_first[plot] = plot->graphCount();
    }
    if(!gradient_first.contains(plot))return;
    const int FIRST = gradient_first[plot];

    // 位置 b のグラデーションの色: 暗い青緑(古い側)→ 青(新しい側)
    auto bin_color = [GRADIENT_BINS](int b){
        double t = b / (double)(GRADIENT_BINS - 1);
        return QColor(qRound(MAIN_COLOR.red() * (1 - t) + T.grad_end.red() * t),
                      qRound(MAIN_COLOR.green() * (1 - t) + T.grad_end.green() * t),
                      qRound(MAIN_COLOR.blue() * (1 - t) + T.grad_end.blue() * t));
    };
    if(grad){
        while(plot->graphCount() < FIRST + GRADIENT_BINS){
            QColor col = bin_color(plot->graphCount() - FIRST);
            QCPGraph *g = plot->addGraph();
            g->setPen(QPen(col));
            g->setLineStyle(QCPGraph::lsNone);
            g->setScatterStyle(point_style(col,hl));
        }
    }
    for(int b=0;b<GRADIENT_BINS && FIRST + b < plot->graphCount();b++){
        plot->graph(FIRST + b)->setVisible(grad);
    }
    if(grad){
        // Highlight 中は、色が付く(閾値以上の)点だけなので塗りつぶし。そうでなければ輪
        for(int b=0;b<GRADIENT_BINS && FIRST + b < plot->graphCount();b++){
            QColor col = bin_color(b);
            plot->graph(FIRST + b)->setScatterStyle(point_style(col,hl));
        }
    }

    if(grad){
        int n = s.x.size();
        QVector<QVector<double>> bx(GRADIENT_BINS), by(GRADIENT_BINS);
        for(int i=0;i<n;i++){
            if(hl && !s.hi[i])continue;     // 閾値未満は灰色(graph(0))
            double t = (n > 1) ? i / (double)(n - 1) : 1.0;     // 位置は全点の中での古さ
            int b = qRound(t * (GRADIENT_BINS - 1));
            bx[b].append(s.x[i]);
            by[b].append(s.y[i]);
        }
        for(int b=0;b<GRADIENT_BINS;b++){
            plot->graph(FIRST + b)->setData(bx[b], by[b]);
        }
    }
}

// 計算結果に影響する設定を1つの文字列にまとめる(変わったかどうかの比較用)
QString MainWindow::calc_signature()
{
    QStringList parts;
    parts << QString::number(ui->checkBox_th->checkState())
          << ui->V_threshold->text()
          << QString::number(target_combine) << QString::number(target_normalize)
          << QString::number(detrend_act->isChecked())
          << QString::number(stats_act->isChecked())
          << QString::number(rank_mode())
          << (rank_mode()==Corr_data::RANK_GAP ? ui->V_highlight->text() : QString())
          << QString::number(ui->checkBox_fix->checkState())
          << QString::number(ui->checkBox_add->checkState())
          << QString::number(ui->spinBox->value());
    for(int i=0;i<ui->listWidget->count();i++)parts << ui->listWidget->item(i)->text();
    parts << "|";
    for(int i=0;i<ui->listWidget_2->count();i++)parts << ui->listWidget_2->item(i)->text();      // 基準信号(名前と重み)
    return parts.join("\n");
}

// 今の状態で計算と描画を済ませたことを記録する
void MainWindow::mark_calculated()
{
    has_calculated = true;
    last_version = corrdata->data_version();
    last_signature = calc_signature();
}

// 保持しているデータからMIC等を計算し直して6枚の散布図を描き直す(DBは読まない)
void MainWindow::refresh_plots()
{
    if(corrdata->sig_size()<2 || corrdata->data_size()<2)return;
    if(ui->checkBox_th->checkState()==Qt::Checked){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }
    update_rank_scores();
    bool b = ui->checkBox_fix->checkState()!=Qt::Checked;
    plot_data(ui->widget_1,corrdata->get_ranking(1,b));
    plot_data(ui->widget_2,corrdata->get_ranking(2,b));
    plot_data(ui->widget_3,corrdata->get_ranking(3,b));
    plot_data(ui->widget_7,corrdata->get_ranking(4,b));
    plot_data(ui->widget_8,corrdata->get_ranking(5,b));
    plot_data(ui->widget_9,corrdata->get_ranking(6,b));
    plot_extra(b);
    mark_calculated();
}

void MainWindow::stats_toggled(bool checked)
{
    corrdata->set_stats(checked);
    refresh_plots();
}

int MainWindow::rank_mode() const
{
    if(rank_list_act->isChecked())return Corr_data::RANK_LIST;
    if(rank_gap_act->isChecked())return Corr_data::RANK_GAP;
    if(rank_imp_act->isChecked())return Corr_data::RANK_IMPORTANCE;
    return Corr_data::RANK_MIC;
}

// 並び順に使う指標(重要度、または2群の平均の差)を更新する。選ばれている方だけ計算する。
// beg<end なら時間範囲の表示用(Threshold は使わない)。
void MainWindow::update_rank_scores(int beg,int end)
{
    if(rank_mode()==Corr_data::RANK_LIST){
        // Plot Signal リストの上から順
        vector<string> names;
        for(int i=0;i<ui->listWidget->count();i++)names.push_back(ui->listWidget->item(i)->text().toStdString());
        corrdata->update_list_order(names);
    }
    double hl = ui->V_highlight->text().toDouble();
    if(beg<end){
        corrdata->update_importance(false,0,beg,end);
        corrdata->update_gap(false,0,hl,beg,end);
    }else{
        bool useTh = ui->checkBox_th->checkState()==Qt::Checked;
        double th = ui->V_threshold->text().toDouble();
        corrdata->update_importance(useTh,th);
        corrdata->update_gap(useTh,th,hl);
    }
}

void MainWindow::rank_mode_changed()
{
    corrdata->set_rank_mode(rank_mode());
    corrdata->rank_init();      // Fix で保持している順位も作り直す
    refresh_plots();
}

void MainWindow::detrend_toggled(bool checked)
{
    corrdata->set_detrend(checked);
    refresh_plots();
}

// 起動時に ~/.config/Correlation_plot の設定ファイルを確認し、無いものがあれば警告する。
// 無くても既定の設定で動く。ダイアログは操作を止めない(Get などは裏で続く)。
void MainWindow::check_config_files()
{
    struct Item{const char *file;const char *meaning;};
    const Item items[] = {
        {"signals.txt",             "信号リスト(既定の8信号を使用)"},
        {"targets.txt",             "基準信号のリスト(無ければ、Plot Signal の先頭を重み1の基準信号にして作成)"},
        {"tree_groups.txt",         "信号ツリーのグループ名(既定の14グループを使用)"},
        {"settings.ini",            "最新ショットの平均のショット数、Highlight の閾値の決め方(最新5ショット、上位25%を使用)"},
    };
    QStringList missing;
    for(const Item &it : items){
        if(!QFile::exists(signal_list_file(it.file))){
            missing << QString("  %1 : %2").arg(it.file).arg(QString::fromUtf8(it.meaning));
        }
    }
    if(missing.isEmpty())return;

    QString dir = QFileInfo(signal_list_file("signals.txt")).absolutePath();
    QString text = QString("設定ファイルが見つかりません。既定の設定で動作します。\n\n保存先: %1\n\n%2\n\n"
                           "信号リストは、リストを変更すると自動で保存されます。")
            .arg(dir).arg(missing.join("\n"));
    cerr << "WARNING: config file(s) not found in " << dir.toStdString() << endl;
    statusBar()->showMessage(tr("Config file(s) not found: using defaults"));
    QMessageBox *box = new QMessageBox(QMessageBox::Warning,"Correlation Plot",text,QMessageBox::Ok,this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    // メインウィンドウの真ん中に重ねて出す(ウィンドウマネージャ任せだと離れた所に出て気づきにくい)
    auto center = [this,box](){
        QPoint c = mapToGlobal(rect().center());
        box->move(c.x()-box->frameGeometry().width()/2,c.y()-box->frameGeometry().height()/2);
    };
    box->adjustSize();
    center();
    box->open();        // 閉じるまでこのウィンドウは操作できないが、処理(Getなど)は止めない
    box->raise();
    box->activateWindow();
    QTimer::singleShot(0,box,center);       // 表示後に枠を含めた大きさで置き直す
}

// 設定ディレクトリ(~/.config/Correlation_plot)の中のテキストファイル(*.txt, *.ini)を、
// pluma でまとめて開く(pluma が起動済みなら、タブとして追加される)。
void MainWindow::open_config_files()
{
    QString dir = QFileInfo(signal_list_file("signals.txt")).absolutePath();
    QStringList names = QDir(dir).entryList(QStringList() << "*.txt" << "*.ini",QDir::Files,QDir::Name);
    if(names.isEmpty()){
        QMessageBox::information(this,"Correlation Plot",
            QString("開くテキストファイルがありません。\n\n%1").arg(dir));
        return;
    }
    QStringList files;
    for(const QString &n : names)files << QDir(dir).absoluteFilePath(n);
    if(!QProcess::startDetached("pluma",files)){
        QMessageBox::warning(this,"Correlation Plot",
            QString("pluma を起動できませんでした。pluma がインストールされ、PATH にあるか確認してください。\n\n%1")
            .arg(files.join("\n")));
    }
}

// File メニュー: sacla_tag と sr_tag のオフセットを指定する子パネル(既定は両方0)。
// 各信号のデータ点を選ぶとき、タグにオフセットを足したイベント番号と照合する(タグの決定自体は変わらない)。
// Apply で設定して、全ショットのデータを取り直す(Get と同じ)。
void MainWindow::show_tag_offset()
{
    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Tag Offset");
    QFormLayout *form = new QFormLayout;
    QSpinBox *sacla = new QSpinBox(dlg);
    QSpinBox *sr = new QSpinBox(dlg);
    for(QSpinBox *s : {sacla,sr}){
        s->setRange(-1000000,1000000);
        s->setPrefix(" ");
    }
    sacla->setValue((int)tag_off_sacla);
    sr->setValue((int)tag_off_sr);
    form->addRow("sacla_tag offset", sacla);
    form->addRow("sr_tag offset", sr);

    QLabel *note = new QLabel("信号のデータ点を、タグ + オフセット のイベント番号と照合して選びます。\n"
                              "Apply で設定して、全ショットのデータを取り直します(Get と同じ)。\n"
                              "0 のままなら、これまでと同じです。ターミナルのタグの表示に、\n"
                              "オフセットを指定していることが明示されます。",dlg);
    note->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(dlg);
    QPushButton *apply = buttons->addButton("Apply",QDialogButtonBox::ApplyRole);
    QPushButton *reset = buttons->addButton("Reset to 0",QDialogButtonBox::ResetRole);
    buttons->addButton(QDialogButtonBox::Close);

    QVBoxLayout *lay = new QVBoxLayout(dlg);
    lay->addLayout(form);
    lay->addWidget(note);
    lay->addWidget(buttons);

    connect(reset,&QPushButton::clicked,dlg,[sacla,sr](){sacla->setValue(0);sr->setValue(0);});
    connect(buttons,&QDialogButtonBox::rejected,dlg,&QDialog::close);
    connect(apply,&QPushButton::clicked,this,[this,sacla,sr](){
        tag_off_sacla = sacla->value();
        tag_off_sr = sr->value();
        cerr << "[tag] tag offset set: sacla_tag" << showpos << tag_off_sacla << ", sr_tag" << tag_off_sr << noshowpos
             << (tag_off_sacla==0 && tag_off_sr==0 ? "  (no offset)" : "  (OFFSET SPECIFIED)") << endl;
        ui->pushButton->click();        // 新しいオフセットで全ショットのデータを取り直す
    });
    dlg->show();
}

// File メニュー「open baratsuki-hikaku」: 別の GUI を起動する。
// 実行ファイルのフルパスは settings.ini の
//   [Tools]
//   baratsuki_hikaku=/full/path/to/gui
// に書く。メニューを選ぶたびに読むので、書き換えても再起動は要らない。
// 起動したGUIは、このアプリとは独立に動く(このアプリを閉じても残る)。作業ディレクトリは、その実行ファイルのあるディレクトリ。
void MainWindow::open_baratsuki_hikaku()
{
    QString ini = signal_list_file("settings.ini");
    QString path;
    if(QFileInfo::exists(ini)){
        QSettings st(ini,QSettings::IniFormat);
        path = ini_string(st,"Tools/baratsuki_hikaku","");
    }
    if(path.isEmpty()){
        QMessageBox::information(this,"Correlation Plot",
            QString("起動する GUI のパスが、設定されていません。\n\n"
                    "次のファイルに、フルパスで書いてください:\n  %1\n\n"
                    "[Tools]\nbaratsuki_hikaku=/full/path/to/gui").arg(ini));
        return;
    }
    QFileInfo fi(path);
    if(!fi.isAbsolute()){
        QMessageBox::warning(this,"Correlation Plot",
            QString("パスは、フルパス(/ から始まるパス)で書いてください。\n\n  %1\n\n設定ファイル: %2").arg(path).arg(ini));
        return;
    }
    if(!fi.exists() || !fi.isFile()){
        QMessageBox::warning(this,"Correlation Plot",
            QString("ファイルが見つかりません。\n\n  %1\n\n設定ファイル: %2").arg(path).arg(ini));
        return;
    }
    if(!fi.isExecutable()){
        QMessageBox::warning(this,"Correlation Plot",
            QString("実行権限がありません。\n\n  %1").arg(path));
        return;
    }
    cerr << "Open : " << path.toStdString() << endl;
    if(!QProcess::startDetached(path,QStringList(),fi.absolutePath())){
        // スクリプトの1行目に #!(シバン)が無いと、実行できない。実行ファイル(ELF)でもシバンでもなければ、ヒントを出す
        QString hint;
        QFile f(path);
        if(f.open(QIODevice::ReadOnly)){
            QByteArray head = f.read(4);
            if(!head.startsWith("#!") && !head.startsWith("\x7f""ELF")){
                hint = "\n\nスクリプトの場合は、1 行目に #!/bin/bash のような行(シバン)が必要です。";
            }
        }
        QMessageBox::warning(this,"Correlation Plot",QString("起動できませんでした。\n\n  %1%2").arg(path).arg(hint));
    }
}

// 全信号を同時に使ったランダムフォレストの重要度を表にして出す。
// 1信号ずつのMIC/rhoと違い、似た信号の重複や他の信号の影響を踏まえた順位になる。
void MainWindow::show_importance()
{
    if(corrdata->sig_size()<3 || corrdata->data_size()<2){
        QMessageBox::information(this,"Importance","Get/Plot でデータを取得し、信号を3つ以上(基準信号+2つ以上)にしてください。");
        return;
    }
    bool useTh = ui->checkBox_th->checkState()==Qt::Checked;
    corrdata->set_detrend(detrend_act->isChecked());
    // 表のMICは現在の条件で計算し直したものにする
    if(useTh){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }

    vector<Importance> imp;
    double r2 = 0;
    int nrows = 0;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = corrdata->calc_importance(useTh,ui->V_threshold->text().toDouble(),imp,r2,nrows);
    QApplication::restoreOverrideCursor();
    if(!ok){
        QMessageBox::information(this,"Importance",
            QString("計算に使えるショットが足りません(%1件。20件以上必要です)。").arg(nrows));
        return;
    }

    QDialog *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Importance (random forest)");
    dlg->resize(760,420);
    QVBoxLayout *lay = new QVBoxLayout(dlg);

    QString head = QString("Target : %1%2\nShots : %3    OOB R2 : %4")
            .arg(QString::fromStdString(corrdata->target_label()))
            .arg(detrend_act->isChecked() ? " (detrended)" : "")
            .arg(nrows).arg(r2,0,'f',3);
    QLabel *label = new QLabel(head,dlg);
    lay->addWidget(label);
    if(r2<=0.05){
        QLabel *warn = new QLabel("OOB R2 が小さく、全信号を使っても基準信号を予測できていません。重要度は信用できません。",dlg);
        warn->setStyleSheet(T.dark ? "color: #FF6B6B;" : "color: #c00000;");
        warn->setWordWrap(true);
        lay->addWidget(warn);
    }

    QTableWidget *table = new QTableWidget((int)imp.size(),5,dlg);
    table->setHorizontalHeaderLabels(QStringList() << "Rank" << "Signal" << "Importance [%]" << "rho" << "MIC");
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    for(int i=0;i<(int)imp.size();i++){
        auto num = [](double v)->QTableWidgetItem*{
            QTableWidgetItem *it = new QTableWidgetItem;
            it->setData(Qt::DisplayRole,QString::number(v,'f',2).toDouble());
            return it;
        };
        QTableWidgetItem *rank = new QTableWidgetItem;
        rank->setData(Qt::DisplayRole,i+1);
        table->setItem(i,0,rank);
        table->setItem(i,1,new QTableWidgetItem(QString::fromStdString(imp[i].name)));
        table->setItem(i,2,num(imp[i].importance*100));
        table->setItem(i,3,num(imp[i].rho));
        QTableWidgetItem *mic = new QTableWidgetItem;
        mic->setData(Qt::DisplayRole,QString::number(imp[i].mic,'f',3).toDouble());
        table->setItem(i,4,mic);
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSortingEnabled(true);
    // 有効にした直後は既定の列(Rank)の降順になるので、Importance の高い順を明示する
    table->sortByColumn(2,Qt::DescendingOrder);
    lay->addWidget(table);

    QLabel *note = new QLabel("Importance: その信号の値をシャッフルしたときに、OOB予測誤差が基準信号の分散の何%増えるか。"
                              "似た信号があると重要度は分け合うため、1つずつ見た相関より小さく出ることがある。"
                              "rho は向きの目安(+: 増やすと基準信号が増える傾向)。",dlg);
    note->setWordWrap(true);
    lay->addWidget(note);
    dlg->show();
}

// Detrend オンのときの表示用データ。MICの計算と同じ手順(閾値を超えたショットだけを残し、移動平均を引く)にする
void MainWindow::detrend_xy(QVector<double> &x,QVector<double> &y,QVector<double> &yraw,QVector<int> &shot)
{
    bool useTh = ui->checkBox_th->checkState()==Qt::Checked;
    double th = ui->V_threshold->text().toDouble();
    QVector<double> xs,ys,rs;
    QVector<int> ss;
    for(int i=0;i<x.size() && i<y.size();i++){
        if(useTh && !(y[i]>th))continue;
        xs.append(x[i]);
        ys.append(y[i]);
        rs.append(y[i]);
        ss.append(i<shot.size() ? shot[i] : i);
    }
    if(!xs.isEmpty()){
        Corr_data::detrend(xs.data(),xs.size());
        Corr_data::detrend(ys.data(),ys.size());
    }
    x = xs;
    y = ys;
    yraw = rs;
    shot = ss;
}

// 従来の get_min/get_max(0.5) と同じ余白で、x,y の範囲から軸範囲を決める
void MainWindow::set_range_from(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    auto range = [](const QVector<double> &v,double &lo,double &hi)->bool{
        bool ok = false;
        for(int i=0;i<v.size();i++){
            if(!std::isfinite(v[i]))continue;
            if(!ok){lo = hi = v[i];ok = true;}
            else{lo = std::min(lo,v[i]);hi = std::max(hi,v[i]);}
        }
        if(ok){
            double d = (hi-lo)*0.5;
            lo -= d;hi += d;
        }
        return ok;
    };
    double lo,hi;
    if(range(x,lo,hi) && hi>lo)plot->xAxis->setRange(lo,hi);
    if(range(y,lo,hi) && hi>lo)plot->yAxis->setRange(lo,hi);
}

void MainWindow::gradient_toggled(bool)
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        apply_display(it.key());
        it.key()->replot();
    }
}

// 起動後、最初に取得できたデータの平均(トレンドグラフの値の平均)を Highlight の閾値の初期値にする。
// 2回目以降の取得では、ユーザーが変えた値を上書きしない。
void MainWindow::init_highlight_threshold()
{
    if(highlight_initialized)return;
    std::vector<double> vals;
    const string &name0 = corrdata->sig_name(0);
    for(int i=0;i<corrdata->data_size();i++){
        if(corrdata->begtime(i).empty())continue;       // 取得できていない空のショットは除く
        double v = corrdata->value(name0,i);
        if(!std::isfinite(v))continue;
        vals.push_back(v);
    }
    if(vals.empty())return;
    highlight_initialized = true;

    // 決め方は settings.ini の [Highlight] method / value で選ぶ(既定: percentile 75 = 上位25%)
    QString method = "percentile";
    double value = 75;
    QFileInfo fi(signal_list_file("settings.ini"));
    if(fi.exists()){
        QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
        QString m = ini_string(st,"Highlight/method","percentile").toLower();
        bool ok = false;
        double val = ini_string(st,"Highlight/value","").toDouble(&ok);
        if(m=="percentile" || m=="sigma" || m=="fixed"){
            if(ok){
                method = m;
                value = val;
            }else if(m=="sigma"){
                method = m;
                value = 1;      // k の既定
            }else if(st.contains("Highlight/value")){
                cerr << "WARNING: settings.ini Highlight/value is invalid; using percentile 75" << endl;
            }
        }else{
            cerr << "WARNING: settings.ini Highlight/method must be percentile, sigma or fixed; using percentile 75" << endl;
        }
    }
    double th = compute_highlight_threshold(vals,method,value);
    DLOG << "Highlight threshold : method=" << method.toStdString() << " value=" << value
         << " -> " << th << " (" << vals.size() << " shots)" << endl;
    ui->V_highlight->setText(QString::number(th,'g',5));
    on_V_highlight_editingFinished();
}

void MainWindow::mean5_toggled(bool)
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        update_recent_mean(it.key(),it.value().x,it.value().y);
        it.key()->replot();
    }
}

void MainWindow::line_toggled(bool)
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        update_recent_line(it.key(),it.value().x,it.value().y);
        it.key()->replot();
    }
}

// Highlight の切り替え/閾値の変更は、保持しているデータで表示だけ描き直す
void MainWindow::on_checkBox_highlight_toggled(bool checked)
{
    // Gravity は Highlight がオンの間だけ使える
    ui->checkBox_gravity->setEnabled(checked);
    // Highlight をオンにしたら、並び順を「List Order」にする(オフに戻しても並び順はそのまま)
    if(checked && !rank_list_act->isChecked()){
        rank_list_act->setChecked(true);    // 排他グループなので他の選択は外れる
        rank_mode_changed();                // 並び順を変えて描き直す
    }
    on_V_highlight_editingFinished();
}

// Add がオンの間(ショットを捨てずに増やし続けている間)は、ステータスバーに「ADD」の目印を出す。
void MainWindow::on_checkBox_add_toggled(bool checked)
{
    if(add_badge)add_badge->setVisible(checked);
}

void MainWindow::on_checkBox_gravity_toggled(bool)
{
    on_V_highlight_editingFinished();       // 縦線・数値・十字印の判定を描き直す
}

void MainWindow::on_V_highlight_editingFinished()
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        update_highlight_mask(it.key());
        apply_display(it.key());
        update_recent_mean(it.key(),it.value().x,it.value().y);     // 十字印の色(近いか)も変わる
        it.key()->replot();
    }
    // 平均の差順のときは、閾値が変わると順位も変わる
    QString now = ui->V_highlight->text();
    if(rank_mode()==Corr_data::RANK_GAP && now!=last_highlight_text){
        corrdata->rank_init();
        refresh_plots();
    }
    last_highlight_text = now;
}

void MainWindow::plot_data(QCustomPlot *plot,int n)
{
    if(n<0 || n>=corrdata->sig_size() || corrdata->sig_size()<1 || corrdata->data_size()<1)return;
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    QFont legendFont = font();
    legendFont.setPointSize(9);

    const string &name_x = corrdata->sig_name(n);
    const string &name_y = corrdata->sig_name(0);
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(name_x,i);
        y[i] = corrdata->value(name_y,i);
    }
    bool dt = detrend_act->isChecked();
    plot_shape[plot] = (name_x.find("/phase")!=string::npos)
            ? QCPScatterStyle::ssDiamond : QCPScatterStyle::ssCircle;
    plot_signal[plot] = QString::fromStdString(name_x);
    QVector<double> yraw = y;
    QVector<int> shot(x.size());
    for(int i=0;i<shot.size();i++)shot[i] = i;
    if(dt)detrend_xy(x,y,yraw,shot);
    set_scatter_data(plot,x,y,yraw,shot);
    QString best = update_binned_curve(plot,x,y);

    set_recent_points(plot,x,y);
    apply_shape(plot);

    if(dt){
        set_range_from(plot,x,y);
    }else if(ui->checkBox_th->checkState()==Qt::Checked){
        double th = ui->V_threshold->text().toDouble(),lo,hi;
        corrdata->get_range(n,0.5,0,th,lo,hi);
        plot->xAxis->setRange(lo,hi);
        corrdata->get_range(0,0.5,0,th,lo,hi);
        plot->yAxis->setRange(lo,hi);
    }else{
        double lo,hi;
        corrdata->get_range(n,0.5,lo,hi);
        plot->xAxis->setRange(lo,hi);
        corrdata->get_range(0,0.5,lo,hi);
        plot->yAxis->setRange(lo,hi);
    }

    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(QString::fromStdString(corrdata->target_label()));
    if(dt){
        plot->xAxis->setLabel(plot->xAxis->label()+" (detrended)");
        plot->yAxis->setLabel(plot->yAxis->label()+" (detrended)");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);

    set_info_text(plot,n,best,legendFont);
    plot->graph(1)->setVisible(true);
    plot->graph(2)->setVisible(true);

    plot->replot(QCustomPlot::rpQueuedReplot);

}
void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    DLOG<<column<<item->text(column).toStdString()<<endl;
    DLOG<<ui->listWidget->count()<<endl;
    DLOG<<item->childCount()<<endl;
    if(item->childCount()!=0)return;

    if(ui->radioButton_2->isChecked()){
        for(int i= 0;i<ui->listWidget_2->count();++i){
            if(target_item_name(ui->listWidget_2->item(i))==item->text(column))return;
        }
        // 基準信号になった信号は、Plot Signal(説明変数)から外す
        for(int i=ui->listWidget->count();i-->0;){
            if(ui->listWidget->item(i)->text()==item->text(column))delete ui->listWidget->takeItem(i);
        }
        add_target_item(item->text(column),1.0);
    }else{
        for(int i= 0;i<ui->listWidget->count();++i){
            DLOG<<ui->listWidget->item(i)->text().toStdString()<<endl;
            if(ui->listWidget->item(i)->text().toStdString().compare(item->text(column).toStdString())==0)return;
        }
        ui->listWidget->addItem(item->text(column));
    }
    corrdata->rank_init();
}
// 信号ツリーのグループ名。
// 設定ファイル ~/.config/Correlation_plot/tree_groups.txt に、1行1グループ名で書く
// (空行と '#' で始まる行は無視。重複は最初の1つだけ使う)。
// ファイルが無い、または有効な行が1つもないときは、下の既定のグループを使う。起動時に1回だけ読む。
QStringList MainWindow::load_group_list()
{
    QStringList list;
    QFile fp(signal_list_file("tree_groups.txt"));
    if(fp.open(QIODevice::ReadOnly | QIODevice::Text)){
        QTextStream in(&fp);
        while(!in.atEnd()){
            QString line = in.readLine().trimmed();
            if(line.isEmpty() || line.startsWith('#') || list.contains(line))continue;
            list << line;
        }
    }
    if(!list.isEmpty()){
        cerr << "Load tree groups : " << fp.fileName().toStdString()
             << "  (" << list.size() << " groups)" << endl;
        return list;
    }
    return QStringList{"sr_safety_topup","sr_mon_xsbt","sr_mon_img","xfel_mon_bpm","xfel_mon_xsbt","xfel_mon_msbpm","xfel_mon_csr","xfel_mon_blm","xfel_mon_ct","xfel_mon_timing","xfel_llrf_238","xfel_llrf_476","xfel_llrf_lb","xfel_llrf_sb_1"};
}

void MainWindow::create_tree()
{
    ui->treeWidget->setColumnCount(1);
    ui->treeWidget->setHeaderLabel(tr("Signal"));
    QList<QTreeWidgetItem*> items;
    QStringList list = load_group_list();
    Db_sig_inf *get_inf;
    int get_inf_cnt;

    for(int i=0; i<list.size(); ++i)
        items.append(new QTreeWidgetItem(static_cast<QTreeWidget *>(nullptr), QStringList(list.at(i))));

    for(int i=0; i<list.size(); ++i){
        int status = db_get_sig_list_by_sub_grp(const_cast<char*>(list.at(i).toStdString().c_str()),&get_inf,&get_inf_cnt);
        if(status == DB_OK && get_inf_cnt>0){
            for(int j=0;j<get_inf_cnt;++j){
                db_get_data_kind((get_inf+j)->sig_name,&(get_inf+j)->daq_type);
                if((get_inf+j)->daq_type == DAQ_TYPE_SYNC && (get_inf+j)->off_flg == 0){
                    items.append(new QTreeWidgetItem(items[i], QStringList(QString((get_inf+j)->sig_name))));
                }
            }
            free(get_inf);
        }
    }
    ui->treeWidget->insertTopLevelItems(0, items);

}

void MainWindow::on_pushButton_delete_clicked()
{
    if(ui->radioButton_2->isChecked()){
        QListWidgetItem* currentItem = ui->listWidget_2->takeItem( ui->listWidget_2->currentRow());
        if ( NULL != currentItem )
        {
            delete ( currentItem );
        }

    }else{
        QListWidgetItem* currentItem = ui->listWidget->takeItem( ui->listWidget->currentRow());
        if ( NULL != currentItem )
        {
            delete ( currentItem );
        }
    }
    corrdata->rank_init();
}

// Plot Signal リスト: ダブルクリックした信号を、リストの先頭へ移す(「Rank by Plot Signal list」の並びに使う)
void MainWindow::on_listWidget_doubleClicked(const QModelIndex &index)
{
    QListWidgetItem* currentItem = ui->listWidget->takeItem( index.row());
    if ( NULL != currentItem )
    {
        ui->listWidget->insertItem(0,currentItem);
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    ui->dateTimeEdit->setDisplayFormat(tr("yyyy/MM/dd HH:mm:ss"));
}

void MainWindow::update_data()
{
    if(ui->checkBox->checkState()==0)return;
    if(get_busy)return;     // Get の最中は、データを作り直している途中なので、自動更新しない
    // 起動後、最初の Get(または Plot)でデータを取得するまでは、自動更新しない
    if(!has_calculated)return;

    sync_targets();
    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
    }

    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    int cnt = corrdata->add_tag(ui->dateTimeEdit->dateTime().toString("yyyy/MM/dd HH:mm:ss").toStdString());
    corrdata->add_db_get_sync_data();

    if(cnt>0)
        DLOG<<"*Data size = "<<corrdata->data_size()<<endl;

    if(ui->checkBox_add->checkState()!=Qt::Checked
            && corrdata->data_size()>ui->spinBox->value()){
        corrdata->erase(corrdata->data_size()-ui->spinBox->value());
        if(cnt>0)
            DLOG<<"***Data size = "<<corrdata->data_size()<<endl;
    }


    // 新しいショットも値の更新も設定の変更もなければ、計算と再描画は結果が変わらないので省く
    if(cnt<=0 && has_calculated
            && corrdata->data_version()==last_version
            && calc_signature()==last_signature){
        return;
    }

    if(ui->checkBox_th->checkState()==Qt::Checked){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }
    update_rank_scores();
    if(cnt>0 && corr_debug())
        for(int j=0;j<corrdata->sig_size();j++){
            DLOG<<" "<<j<<"  "<<
                  corrdata->sig_name(j)<<"  "<<
                  corrdata->mic(j)<<endl;
        }
    bool b = true;

    if(ui->checkBox_fix->checkState()==Qt::Checked){
        b = false;
    }
    plot_data(ui->widget_1,corrdata->get_ranking(1,b));
    plot_data(ui->widget_2,corrdata->get_ranking(2,b));
    plot_data(ui->widget_3,corrdata->get_ranking(3,b));
    plot_data(ui->widget_7,corrdata->get_ranking(4,b));
    plot_data(ui->widget_8,corrdata->get_ranking(5,b));
    plot_data(ui->widget_9,corrdata->get_ranking(6,b));
    plot_extra(b);
    plot_trend(ui->widget);
    mark_calculated();
    if(cnt>0)
        corrdata->outfile(CSV_FILE);


}

// Target Signal リスト: ダブルクリックで、その基準信号の重みを変える(負の重みは、その信号を小さくしたいという意味)
void MainWindow::on_listWidget_2_doubleClicked(const QModelIndex &index)
{
    QListWidgetItem *it = ui->listWidget_2->item(index.row());
    if(!it)return;
    bool ok = false;
    double w = QInputDialog::getDouble(this,"Target weight",
        QString("%1\n\nWeight (negative = minimize this signal, 0 = ignore):").arg(target_item_name(it)),
        target_item_weight(it),-1000,1000,3,&ok);
    if(!ok)return;
    set_target_item(it,target_item_name(it),w);
    save_targets();
    targets_changed();
}

void MainWindow::createActions()
{

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    //    QToolBar *fileToolBar = addToolBar(tr("File"));

    const QIcon saveIcon = QIcon::fromTheme("document-save");
    QAction *saveAct = new QAction(saveIcon, tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    //    fileToolBar->addAction(saveAct);

    QAction *importanceAct = new QAction(tr("&Importance..."), this);
    importanceAct->setStatusTip(tr("Rank all signals at once by random forest importance (multivariate)"));
    connect(importanceAct, &QAction::triggered, this, &MainWindow::show_importance);
    fileMenu->addAction(importanceAct);

    QAction *tagOffsetAct = new QAction(tr("Tag &Offset..."), this);
    tagOffsetAct->setStatusTip(tr("Specify offsets added to sacla_tag and sr_tag when matching signal data (default 0)"));
    connect(tagOffsetAct, &QAction::triggered, this, &MainWindow::show_tag_offset);
    fileMenu->addAction(tagOffsetAct);

    QAction *heatAct = new QAction(tr("Correlation &Heatmap..."), this);
    heatAct->setStatusTip(tr("Open a child panel with the correlation matrix of all signals (Spearman or MIC) as a heatmap"));
    connect(heatAct, &QAction::triggered, this, &MainWindow::open_heatmap_panel);
    fileMenu->addAction(heatAct);

    QAction *trendPanelAct = new QAction(tr("Trend &Panel..."), this);
    trendPanelAct->setStatusTip(tr("Open a child panel with the trend graph of every signal in Plot Signal (dashed line at the mean value of the last Get)"));
    connect(trendPanelAct, &QAction::triggered, this, &MainWindow::open_trend_panel);
    fileMenu->addAction(trendPanelAct);

    ma_act = new QAction(tr("Show &Moving Average"), this);
    ma_act->setCheckable(true);
    ma_act->setChecked(true);
    ma_act->setStatusTip(tr("Draw a moving average line on the main trend graph and on every trend graph of the child panel"));
    connect(ma_act, &QAction::toggled, this, [this](bool){update_moving_average();});
    fileMenu->addAction(ma_act);
    QAction *maWinAct = new QAction(tr("Moving Average &Window..."), this);
    maWinAct->setStatusTip(tr("Number of shots averaged by the moving average line (default: settings.ini [MovingAverage] shots, or 10)"));
    connect(maWinAct, &QAction::triggered, this, [this](){
        bool ok = false;
        int w = QInputDialog::getInt(this,"Moving Average","Number of shots in the moving average:",ma_window,1,1000,1,&ok);
        if(!ok)return;
        ma_window = w;
        update_moving_average();
    });
    fileMenu->addAction(maWinAct);

    QAction *openCfgAct = new QAction(tr("Open &Config Files (pluma)"), this);
    openCfgAct->setStatusTip(tr("Open all text files in the config directory with pluma"));
    connect(openCfgAct, &QAction::triggered, this, &MainWindow::open_config_files);
    fileMenu->addAction(openCfgAct);

    QAction *baratsukiAct = new QAction(tr("open baratsuki-hikaku"), this);
    baratsukiAct->setStatusTip(tr("Open another GUI (the full path is set in settings.ini: [Tools] baratsuki_hikaku)"));
    connect(baratsukiAct, &QAction::triggered, this, &MainWindow::open_baratsuki_hikaku);
    fileMenu->addAction(baratsukiAct);

    fileMenu->addSeparator();
    fileMenu->addAction(rank_mic_act);
    fileMenu->addAction(rank_imp_act);
    fileMenu->addAction(rank_gap_act);
    fileMenu->addAction(rank_list_act);

    // 基準信号が複数あるときの合成の方法(排他選択)
    fileMenu->addSeparator();
    QActionGroup *combineGroup = new QActionGroup(this);
    QAction *sumAct = new QAction(tr("Target: &Sum of weighted values"), this);
    QAction *minAct = new QAction(tr("Target: Mi&n (every target must be good)"), this);
    sumAct->setStatusTip(tr("Combined target = sum of weight x (z-score) of the targets"));
    minAct->setStatusTip(tr("Combined target = the worst weighted target (weighted Chebyshev): picks shots where all targets are good at once"));
    for(QAction *a : {sumAct,minAct}){a->setCheckable(true);combineGroup->addAction(a);fileMenu->addAction(a);}
    (target_combine==Corr_data::COMBINE_MIN ? minAct : sumAct)->setChecked(true);
    connect(sumAct,&QAction::triggered,this,[this](){target_combine = Corr_data::COMBINE_SUM;targets_changed();});
    connect(minAct,&QAction::triggered,this,[this](){target_combine = Corr_data::COMBINE_MIN;targets_changed();});
    connect(rank_group, &QActionGroup::triggered, this, &MainWindow::rank_mode_changed);
    fileMenu->addSeparator();
    fileMenu->addAction(detrend_act);
    connect(detrend_act, &QAction::toggled, this, &MainWindow::detrend_toggled);
    fileMenu->addAction(stats_act);
    connect(stats_act, &QAction::toggled, this, &MainWindow::stats_toggled);
    fileMenu->addAction(gradient_act);
    connect(gradient_act, &QAction::toggled, this, &MainWindow::gradient_toggled);
    fileMenu->addAction(line_act);
    connect(line_act, &QAction::toggled, this, &MainWindow::line_toggled);
    fileMenu->addAction(mean5_act);
    connect(mean5_act, &QAction::toggled, this, &MainWindow::mean5_toggled);

    QAction *extraAct = new QAction(tr("&More Scatter Plots"), this);
    extraAct->setCheckable(true);
    extraAct->setStatusTip(tr("Show the scatter plots of the 7th and later signals below (scroll to see all)"));
    connect(extraAct, &QAction::toggled, this, &MainWindow::set_extra_plots);
    fileMenu->addAction(extraAct);

    fileMenu->addSeparator();

    const QIcon exitIcon = QIcon::fromTheme("application-exit");
    QAction *exitAct = fileMenu->addAction(exitIcon, tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));

    // Theme メニュー(File の隣)。排他選択。選ぶとすぐに全体の色が変わり、次回起動でも使われる
    QMenu *themeMenu = menuBar()->addMenu(tr("&Theme"));
    QActionGroup *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    for(const auto &t : THEME_LIST){
        QAction *a = themeMenu->addAction(t.second);
        a->setCheckable(true);
        a->setChecked(t.first==g_theme_name);
        a->setStatusTip(tr("Switch the color theme (remembered in theme.txt of the config directory)"));
        themeGroup->addAction(a);
        const QString id = t.first;
        connect(a,&QAction::triggered,this,[this,id](){switch_theme(id);});
    }
}
void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
    add_badge = new QLabel(" ADD ");
    add_badge->setToolTip(tr("Add is ON: shots are accumulated without discarding the old ones"));
    style_add_badge();
    statusBar()->addPermanentWidget(add_badge);
    add_badge->setVisible(ui->checkBox_add->isChecked());
}
bool MainWindow::save()
{
    if (curFile.isEmpty()) {
        return saveAs();
    } else {
        return saveFile(curFile);
    }
}

bool MainWindow::saveAs()
{
    QFileDialog dialog(this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    return saveFile(dialog.selectedFiles().first());
}
bool MainWindow::saveFile(const QString &fileName)
{

    corrdata->outfile(fileName.toStdString());
    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}
void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = fileName;
    setWindowModified(false);

    QString shownName = curFile;
    if (curFile.isEmpty())
        shownName = "untitled.txt";
    setWindowFilePath(shownName);
}

void MainWindow::on_pushButton_3_clicked()
{
    int beg,end;
    if(ui->widget->graph(3)->data()->begin()->key < ui->widget->graph(4)->data()->begin()->key){
        beg = ui->widget->graph(3)->data()->begin()->key;
        end = ui->widget->graph(4)->data()->begin()->key;
    }else{
        beg = ui->widget->graph(3)->data()->begin()->key;
        end = ui->widget->graph(4)->data()->begin()->key;
    }
    DLOG<<beg<<"   "<<end<<endl;
    corrdata->calc_mic(beg,end);
    update_rank_scores(beg,end);
    DLOG<<beg<<"   "<<end<<endl;

    plot_data(ui->widget_1,corrdata->get_ranking(1),beg,end);
    plot_data(ui->widget_2,corrdata->get_ranking(2),beg,end);
    plot_data(ui->widget_3,corrdata->get_ranking(3),beg,end);
    plot_data(ui->widget_7,corrdata->get_ranking(4),beg,end);
    plot_data(ui->widget_8,corrdata->get_ranking(5),beg,end);
    plot_data(ui->widget_9,corrdata->get_ranking(6),beg,end);
    plot_extra_range(beg,end);
    DLOG<<beg<<"   "<<end<<endl;

    ui->widget->xAxis->setRange(beg,end);
    ui->widget->replot();

}


void MainWindow::plot_data(QCustomPlot *plot,int n,int beg,int end)
{
    if(n<0 || n>=corrdata->sig_size() || corrdata->sig_size()<1 || corrdata->data_size()<1)return;
    QFont legendFont = font();
    legendFont.setPointSize(9);
    int m=0;
    struct tm buf;

    for(int i=0;i<corrdata->data_size();i++){
        strptime(corrdata->endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&buf);
        if(beg < mktime(&buf) && mktime(&buf) < end){
            m++;
        }
    }
    DLOG<<"m = "<<m<<endl;
    QVector<double> x(m),y(m);
    QVector<int> shot(m);
    m=0;
    const string &name_x = corrdata->sig_name(n);
    const string &name_y = corrdata->sig_name(0);
    for(int i=0;i<corrdata->data_size();i++){
        strptime(corrdata->endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&buf);
        if(beg < mktime(&buf) && mktime(&buf) < end){
            x[m] = corrdata->value(name_x,i);
            y[m] = corrdata->value(name_y,i);
            shot[m] = i;
            m++;
        }
    }
    bool dt = detrend_act->isChecked();
    plot_shape[plot] = (name_x.find("/phase")!=string::npos)
            ? QCPScatterStyle::ssDiamond : QCPScatterStyle::ssCircle;
    plot_signal[plot] = QString::fromStdString(name_x);
    QVector<double> yraw = y;
    if(dt)detrend_xy(x,y,yraw,shot);
    set_scatter_data(plot,x,y,yraw,shot);
    QString best = update_binned_curve(plot,x,y);

    if(dt){
        set_range_from(plot,x,y);
    }else if(ui->checkBox_th->checkState()==Qt::Checked){
        double th = ui->V_threshold->text().toDouble(),lo,hi;
        corrdata->get_range(n,0.5,0,th,lo,hi);
        plot->xAxis->setRange(lo,hi);
        corrdata->get_range(0,0.5,0,th,lo,hi);
        plot->yAxis->setRange(lo,hi);
    }else{
        double lo,hi;
        corrdata->get_range(n,0.5,lo,hi);
        plot->xAxis->setRange(lo,hi);
        corrdata->get_range(0,0.5,lo,hi);
        plot->yAxis->setRange(lo,hi);
    }

    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(QString::fromStdString(corrdata->target_label()));
    if(dt){
        plot->xAxis->setLabel(plot->xAxis->label()+" (detrended)");
        plot->yAxis->setLabel(plot->yAxis->label()+" (detrended)");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);

    set_info_text(plot,n,best,legendFont);
    plot->graph(1)->setVisible(false);
    plot->graph(2)->setVisible(false);
    hide_recent_points(plot);

    plot->replot(QCustomPlot::rpQueuedReplot);

}
