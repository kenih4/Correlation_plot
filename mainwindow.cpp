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
#include <QGridLayout>
#include <QScrollBar>
#include <QWheelEvent>
#include <QScreen>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>

// 自動更新ごとに書き出すCSV
static const char *CSV_FILE = "/home_2023/xfel/xfelopr/users/kenichi/dvlp/corr_data/corr.csv";
#include <cmath>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    gradient_act->setStatusTip(tr("Color points from red (oldest) to blue (newest)"));

    line_act = new QAction(tr("&Connect Latest 3 Points"), this);
    line_act->setCheckable(true);
    line_act->setChecked(false);
    line_act->setStatusTip(tr("Draw a line through the latest 3 points of each scatter plot and the trend graph"));

    mean5_act = new QAction(tr("Show &Mean of Latest Shots"), this);
    mean5_act->setCheckable(true);
    mean5_act->setChecked(true);
    mean5_act->setStatusTip(tr("Mark the mean (x and y) of the latest N shots with a cross on each scatter plot (N: settings.ini LatestMean/shots, default 5)"));

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
    rank_list_act->setChecked(true);

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
    if(!load_signal_list(ui->listWidget_2,"signals_denominator.txt")){
        ui->listWidget_2->addItem("sr_mon_bpm_xsbt_20_sum/charge");
        ui->listWidget_2->addItem("sr_mon_bpm_xsbt_21_sum/charge");
    }
    // listWidget, listWidget_2 が変更されたら信号リストファイルに保存する
    connect_save_signal_list(ui->listWidget,"signals.txt");
    connect_save_signal_list(ui->listWidget_2,"signals_denominator.txt");
    QFont lFont = font();
    lFont.setBold(true);
    ui->listWidget->item(0)->setFont(lFont);
    ui->listWidget_2->item(0)->setFont(lFont);

    create_tree();
    ui->V_threshold->setText(QString::number(50));
    ui->checkBox_th->setChecked(true);
    ui->checkBox->setChecked(true);     // Auto reload
    timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_data()));
    timer->start();
    corrdata = new Corr_data(ui->spinBox->value(),ui->checkBox_calc->checkState());
    corrdata->set_stats(stats_act->isChecked());
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
    tracer->setPen(QPen(Qt::red));
    tracer->setBrush(Qt::red);
    tracer->setSize(7);
    connect(ui->widget,SIGNAL(mouseMove(QMouseEvent*)),this,SLOT(trendMouseMoved(QMouseEvent*)));
    ui->widget->installEventFilter(this);
    tracerText = new QCPItemText(ui->widget);
    tracerText->position->setType(QCPItemPosition::ptAxisRectRatio);
    tracerText->setFont(QFont(font().family(),8));
    tracerText->setPositionAlignment(Qt::AlignLeft);
    tracerText->position->setCoords(QPointF(0,0));
    tracerText->setText("(-)");
    tracer->setVisible(false);
    tracerText->setVisible(false);

    // 起動時に Get ボタンを押す(ウィンドウ表示後にDB取得を始めるため、イベントループに載せる)
    QTimer::singleShot(0, ui->pushButton, SLOT(click()));
    QTimer::singleShot(0, this, SLOT(check_config_files()));
}

QString MainWindow::signal_list_file(const QString &name)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" + name;
}

// 1行1信号のテキストファイルから list を読み込む。
// ファイルが無い/空の場合は false を返す(呼び出し側で既定リストを使う)
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
        list->addItem(line);
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
        out << list->item(i)->text() << Qt::endl;
    }
}

void MainWindow::connect_save_signal_list(QListWidget *list, const QString &name)
{
    auto save = [this, list, name](){ save_signal_list(list, name); };
    connect(list->model(), &QAbstractItemModel::rowsInserted, this, save);
    connect(list->model(), &QAbstractItemModel::rowsRemoved, this, save);
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
            hg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,QPen(Qt::red,2.5),QBrush(Qt::NoBrush),18));
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
    delete corrdata;
    corrdata = new Corr_data(ui->spinBox->value(),ui->checkBox_calc->checkState());
    corrdata->set_stats(stats_act->isChecked());
    corrdata->set_detrend(detrend_act->isChecked());
    corrdata->set_rank_mode(rank_mode());

    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
    }
    for(int i= 0;i<ui->listWidget_2->count();++i){
        corrdata->add_signal_d(ui->listWidget_2->item(i)->text().toStdString().c_str());

    }

    corrdata->get_tag(ui->dateTimeEdit->dateTime().toString("yyyy/MM/dd HH:mm:ss").toStdString());
    corrdata->db_get_sync_data();
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

}
void MainWindow::plot_init(QCustomPlot *plot)
{
    plot->addGraph();
    plot->graph(0)->setPen(QPen(Qt::red));
    plot->graph(0)->setLineStyle(QCPGraph::lsNone);
    plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));
    plot->addGraph();
    plot->graph(1)->setPen(QPen(QColor("#8000ff")));
    plot->graph(1)->setLineStyle(QCPGraph::lsNone);
    plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,"#8000ff", "#8f8000ff", 8));
    plot->addGraph();
    plot->graph(2)->setPen(QPen(Qt::blue));
    plot->graph(2)->setLineStyle(QCPGraph::lsNone);
    plot->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, "#0000ff", "#0000ff", 10));

}
void MainWindow::plot_trend(QCustomPlot *plot)
{
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
        //        y[i] = corrdata->value(corrdata->sig_name(0),i)/(corrdata->denominator(corrdata->sig_d_name(0),i)*1e-9/(1.60219e-19))*100;
    }
    //    DLOG<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;
    QVector<int> shot_all(x.size());
    for(int i=0;i<shot_all.size();i++)shot_all[i] = i;
    set_scatter_data(plot,x,y,y,shot_all);

    set_recent_points(plot,x,y);
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
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
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
        b.graph->setPen(QPen(Qt::black,2));
        b.graph->setLineStyle(QCPGraph::lsLine);
        b.graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc,Qt::black,6));
        b.err = new QCPErrorBars(plot->xAxis,plot->yAxis);
        b.err->setDataPlottable(b.graph);
        b.err->setPen(QPen(Qt::black,1.5));
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
        b.text->setPen(QPen(Qt::white));
        b.text->setBrush(QBrush(QColor(255,255,255,230)));
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
        b.text->setColor(Qt::black);
        return;
    }
    double nul = corrdata->mic_null(n);
    QString s = QString("mic : %1  (null95 %2)\nrho : %3  p : %4")
            .arg(mic,0,'f',3).arg(nul,0,'f',3)
            .arg(corrdata->rho(n),0,'f',2).arg(corrdata->pval(n),0,'f',3);
    s += impLine;
    if(!best.isEmpty())s += "\n" + best;
    b.text->setText(s);
    // 偶然の範囲を超えていれば赤字
    bool significant = mic>nul && corrdata->pval(n)<0.05;
    b.text->setColor(significant ? QColor(200,0,0) : QColor(60,60,60));
}

// 最新の4点を色分けして重ねる: 最新=青(サイズ10, graph(2))、2番目=紫(8, graph(1))、
// 3番目=マゼンタ(6)、4番目=オレンジ(5)。それより古い点は通常の赤(またはグラデーション)
void MainWindow::set_recent_points(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    RecentGraphs &r = recent[plot];
    if(!r.g3){
        r.g3 = plot->addGraph();
        r.g3->setPen(QPen(QColor("#ff00ff")));
        r.g3->setLineStyle(QCPGraph::lsNone);
        r.g3->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,"#ff00ff","#8fff00ff",6));
        r.g4 = plot->addGraph();
        r.g4->setPen(QPen(QColor("#ff8000")));
        r.g4->setLineStyle(QCPGraph::lsNone);
        r.g4->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,"#ff8000","#8fff8000",5));
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
    pick(r.g4,4);
    r.range_mode = false;
    update_recent_line(plot,x,y);
    update_recent_mean(plot,x,y);
}

// settings.ini の値を文字列で読む。QSettings は行の途中のコメントを扱えず、カンマを含む値は
// リストになるので、「;」「#」以降を取り除き、リストはカンマでつなぎ直して返す。
static QString ini_string(QSettings &st,const QString &key,const QString &def)
{
    if(!st.contains(key))return def;
    QVariant var = st.value(key);
    QString s = (var.metaType().id()==QMetaType::QStringList) ? var.toStringList().join(",") : var.toString();
    int cut = s.indexOf(QRegularExpression("[;#]"));
    if(cut>=0)s = s.left(cut);
    return s.trimmed();
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

// 十字印の平均に使う最新のショット数。
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
        cross_sigma = 1.0;
        cross_require_closer = true;
        if(fi.exists()){
            QSettings st(fi.absoluteFilePath(),QSettings::IniFormat);
            bool ok = false;
            int n = ini_string(st,"LatestMean/shots","5").toInt(&ok);
            if(ok && n>=1 && n<=1000)mean_shots = n;
            else DLOG << "settings.ini: invalid LatestMean/shots, using 5" << endl;
            // [CrossColor] 十字印を赤くする基準
            bool ok2 = false;
            double k = ini_string(st,"CrossColor/sigma","1").toDouble(&ok2);
            if(ok2 && k>0 && k<=100)cross_sigma = k;
            else if(st.contains("CrossColor/sigma"))DLOG << "settings.ini: invalid CrossColor/sigma, using 1" << endl;
            QString rc = ini_string(st,"CrossColor/require_closer_than_low","true").toLower();
            if(rc=="false" || rc=="0" || rc=="no" || rc=="off")cross_require_closer = false;
            DLOG << "Load settings : " << fi.absoluteFilePath().toStdString()
                 << "  (LatestMean/shots=" << mean_shots << ")" << endl;
        }
    }
    return mean_shots;
}

// 十字印の x 座標(最新Nショットの平均 xbar)が、閾値以上のショットの x の平均に「近い」かどうか。
//   (A) |xbar − μ_hi| ≤ k × σ_hi        σ_hi: 閾値以上のショットの x の標準偏差
//   (B) |xbar − μ_hi| < |xbar − μ_lo|   閾値未満の平均より閾値以上の平均に近い(require_closer_than_low)
// 閾値以上のショットが5未満、または σ_hi が 0 のときは判定しない(false)。
bool MainWindow::near_good_mean(const QVector<double> &x,const QVector<char> &hi,double xbar)
{
    double sum_hi = 0,sum_lo = 0;
    int n_hi = 0,n_lo = 0;
    for(int i=0;i<x.size() && i<hi.size();i++){
        if(!std::isfinite(x[i]))continue;
        if(hi[i]){sum_hi += x[i];n_hi++;}
        else{sum_lo += x[i];n_lo++;}
    }
    if(n_hi<5)return false;
    double mu_hi = sum_hi/n_hi;
    double var = 0;
    for(int i=0;i<x.size() && i<hi.size();i++){
        if(hi[i] && std::isfinite(x[i]))var += (x[i]-mu_hi)*(x[i]-mu_hi);
    }
    double sd = sqrt(var/n_hi);
    if(sd<=0)return false;
    double d_hi = fabs(xbar-mu_hi);
    if(d_hi > cross_sigma*sd)return false;
    if(cross_require_closer && n_lo>0){
        double mu_lo = sum_lo/n_lo;
        if(!(d_hi < fabs(xbar-mu_lo)))return false;
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
        r.mean5->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(Qt::black,2.5),QBrush(Qt::NoBrush),18));
        r.mean5->setLayer("overlay");       // 他の点より手前に描く
    }
    double sx = 0,sy = 0;
    for(int k=0;k<N;k++){
        sx += x[x.size()-1-k];
        sy += y[y.size()-1-k];
    }
    QVector<double> mx(1,sx/N),my(1,sy/N);
    r.mean5->setData(mx,my);
    // Highlight がオンで、横軸の平均が「優秀なショットの平均」に近ければ、赤く太くして目立たせる
    bool near = false;
    if(ui->checkBox_highlight->isChecked() && scatter_xy.contains(plot)){
        const ScatterData &s = scatter_xy[plot];
        near = near_good_mean(s.x,s.hi,sx/N);
    }
    r.mean5->setScatterStyle(near
        ? QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(Qt::red,3.5),QBrush(Qt::NoBrush),22)
        : QCPScatterStyle(QCPScatterStyle::ssPlus,QPen(Qt::black,2.5),QBrush(Qt::NoBrush),18));
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
        r.line->setPen(QPen(QColor(90,90,90),1.5));
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
        recent[plot].g4->setVisible(false);
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
        if(mean_lines[plot].lo)mean_lines[plot].lo->setVisible(false);
        if(mean_lines[plot].hi_text)mean_lines[plot].hi_text->setVisible(false);
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
// 設定ファイル ~/.config/Correlation_plot/mean_label_signals.txt に、1行1つ信号名を書く
// ('*' '?' のワイルドカード可。空行と '#' で始まる行は無視)。
// ファイルが無い、または有効な行が1つもないときは、すべての信号で出す。
// 更新日時が変わったら読み直す(アプリを再起動しなくてよい)。
bool MainWindow::mean_label_enabled(const QString &signal)
{
    QFileInfo fi(signal_list_file("mean_label_signals.txt"));
    QDateTime mt = fi.exists() ? fi.lastModified() : QDateTime();
    if(!mean_label_loaded || mt!=mean_label_mtime){
        mean_label_loaded = true;
        mean_label_mtime = mt;
        mean_label_regex.clear();
        QFile fp(fi.absoluteFilePath());
        if(fi.exists() && fp.open(QIODevice::ReadOnly | QIODevice::Text)){
            QTextStream in(&fp);
            while(!in.atEnd()){
                QString line = in.readLine().trimmed();
                if(line.isEmpty() || line.startsWith('#'))continue;
                mean_label_regex.append(QRegularExpression(QRegularExpression::wildcardToRegularExpression(line)));
            }
            DLOG << "Load mean label signals : " << fi.absoluteFilePath().toStdString()
                 << "  (" << mean_label_regex.size() << " patterns)" << endl;
        }
    }
    if(mean_label_regex.isEmpty())return true;
    for(const QRegularExpression &re : mean_label_regex){
        if(re.match(signal).hasMatch())return true;
    }
    return false;
}

// Highlight 中は、散布図に縦線を引く。
//   赤   : 閾値以上の点の x (横軸の信号) の平均
//   灰色 : 閾値未満の点の x の平均
// トレンドグラフは対象外(横軸が時刻のため)。該当する点がなければその線は出さない。
void MainWindow::update_mean_lines(QCustomPlot *plot)
{
    MeanLines &m = mean_lines[plot];
    bool hl = ui->checkBox_highlight->isChecked();
    if(plot==ui->widget || !hl || !scatter_xy.contains(plot)){
        if(m.hi)m.hi->setVisible(false);
        if(m.lo)m.lo->setVisible(false);
        if(m.hi_text)m.hi_text->setVisible(false);
        return;
    }
    const ScatterData &s = scatter_xy[plot];
    double sum_hi = 0,sum_lo = 0;
    int n_hi = 0,n_lo = 0;
    for(int i=0;i<s.x.size();i++){
        if(!std::isfinite(s.x[i]))continue;
        if(s.hi[i]){sum_hi += s.x[i];n_hi++;}
        else{sum_lo += s.x[i];n_lo++;}
    }
    auto place = [plot](QCPItemStraightLine *&line,const QColor &col,bool show,double xv){
        if(!line){
            line = new QCPItemStraightLine(plot);
            line->setPen(QPen(col,1.5));
        }
        line->point1->setCoords(xv,0);
        line->point2->setCoords(xv,1);
        line->setVisible(show);
    };
    place(m.hi,QColor(Qt::red),n_hi>0,n_hi>0 ? sum_hi/n_hi : 0);
    place(m.lo,QColor(150,150,150),n_lo>0,n_lo>0 ? sum_lo/n_lo : 0);

    // 赤い線(閾値以上の平均値)の数値を左下に表示する
    if(!m.hi_text){
        m.hi_text = new QCPItemText(plot);
        m.hi_text->setPositionAlignment(Qt::AlignBottom|Qt::AlignLeft);
        m.hi_text->position->setType(QCPItemPosition::ptAxisRectRatio);
        m.hi_text->position->setCoords(0.02,0.98);
        m.hi_text->setColor(QColor(Qt::red));
        m.hi_text->setPen(Qt::NoPen);
        m.hi_text->setBrush(QBrush(QColor(255,255,255,200)));
        QFont f = font();
        f.setPointSize(9);
        m.hi_text->setFont(f);
    }
    bool show_label = n_hi>0 && mean_label_enabled(plot_signal.value(plot));
    if(show_label)m.hi_text->setText(QString("mean(>=) : %1").arg(sum_hi/n_hi,0,'g',5));
    m.hi_text->setVisible(show_label);
}

// 点の色を決めて表示する。
//   通常      : 全点を赤(graph(0))
//   Gradient  : 最古=赤 → 最新=青 のグラデーション。1点ごとの色指定はできないので、
//               色ごとのグラフ(GRADIENT_BINS個)に点を振り分ける
//   Highlight : 閾値以上の点だけ色を付け(赤、またはGradient)、それ以外は灰色(graph(0))にする
// 最新4点の色(青/紫/マゼンタ/オレンジ)は別のグラフで重ねているので、どの設定でも変わらない。
void MainWindow::apply_display(QCustomPlot *plot)
{
    apply_display_impl(plot);
    apply_shape(plot);
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
        set(recent[plot].g4);
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

    // graph(0): 全点。Highlight 中は灰色、それ以外は赤。
    // Gradient だけで非表示になる graph(0) には、データを入れない(setData は x での並べ替えが走る)。
    // トレンド(ui->widget)は tracer が graph(0) のデータを使うので常に入れる。
    QCPGraph *g0 = plot->graph(0);
    if(!grad || hl || plot==ui->widget)g0->setData(s.x,s.y);
    g0->setVisible(!grad || hl);
    QColor c0 = hl ? QColor(170,170,170) : QColor(Qt::red);
    g0->setPen(QPen(c0));
    g0->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,c0,Qt::white,5));

    // 強調する点だけの赤いグラフ(Highlight かつ Gradient でないとき)
    if(hl && !grad){
        QCPGraph *&hg = highlight_graph[plot];
        if(!hg){
            hg = plot->addGraph();
            hg->setPen(QPen(Qt::red));
            hg->setLineStyle(QCPGraph::lsNone);
            hg->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,Qt::red,Qt::white,5));
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

    if(grad){
        while(plot->graphCount() < FIRST + GRADIENT_BINS){
            double t = (plot->graphCount() - FIRST) / (double)(GRADIENT_BINS - 1);
            QColor col(qRound(255 * (1 - t)), 0, qRound(255 * t));
            QCPGraph *g = plot->addGraph();
            g->setPen(QPen(col));
            g->setLineStyle(QCPGraph::lsNone);
            g->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, col, Qt::white, 5));
        }
    }
    for(int b=0;b<GRADIENT_BINS && FIRST + b < plot->graphCount();b++){
        plot->graph(FIRST + b)->setVisible(grad);
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
          << QString::number(ui->checkBox_calc->checkState())
          << QString::number(detrend_act->isChecked())
          << QString::number(stats_act->isChecked())
          << QString::number(rank_mode())
          << (rank_mode()==Corr_data::RANK_GAP ? ui->V_highlight->text() : QString())
          << QString::number(ui->checkBox_fix->checkState())
          << QString::number(ui->checkBox_add->checkState())
          << QString::number(ui->spinBox->value());
    for(int i=0;i<ui->listWidget->count();i++)parts << ui->listWidget->item(i)->text();
    parts << "|";
    for(int i=0;i<ui->listWidget_2->count();i++)parts << ui->listWidget_2->item(i)->text();
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
        // Plot Signal リストの上から順(最初の項目は基準信号)
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
        {"signals_denominator.txt", "分母の信号リスト(既定の2信号を使用)"},
        {"mean_label_signals.txt",  "平均値の数値を出す信号(すべての信号に出す)"},
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
            .arg(QString::fromStdString(corrdata->sig_name(0)))
            .arg(detrend_act->isChecked() ? " (detrended)" : "")
            .arg(nrows).arg(r2,0,'f',3);
    QLabel *label = new QLabel(head,dlg);
    lay->addWidget(label);
    if(r2<=0.05){
        QLabel *warn = new QLabel("OOB R2 が小さく、全信号を使っても基準信号を予測できていません。重要度は信用できません。",dlg);
        warn->setStyleSheet("color: #c00000;");
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
void MainWindow::on_checkBox_highlight_toggled(bool)
{
    on_V_highlight_editingFinished();
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
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
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
            DLOG<<ui->listWidget_2->item(i)->text().toStdString()<<endl;
            if(ui->listWidget_2->item(i)->text().toStdString().compare(item->text(column).toStdString())==0)return;
        }
        ui->listWidget_2->addItem(item->text(column));
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

void MainWindow::on_listWidget_doubleClicked(const QModelIndex &index)
{
    QListWidgetItem* currentItem = ui->listWidget->takeItem( index.row());
    QFont lFont = font();
    lFont.setBold(false);
    ui->listWidget->item(0)->setFont(lFont);
    if ( NULL != currentItem )
    {
        ui->listWidget->insertItem(0,currentItem);
    }
    lFont.setBold(true);
    ui->listWidget->item(0)->setFont(lFont);

}

void MainWindow::on_pushButton_2_clicked()
{
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    ui->dateTimeEdit->setDisplayFormat(tr("yyyy/MM/dd HH:mm:ss"));
}

void MainWindow::update_data()
{
    if(ui->checkBox->checkState()==0)return;

    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
    }
    for(int i= 0;i<ui->listWidget_2->count();++i){
        corrdata->add_signal_d(ui->listWidget_2->item(i)->text().toStdString().c_str());
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

void MainWindow::on_listWidget_2_doubleClicked(const QModelIndex &index)
{
    QListWidgetItem* currentItem = ui->listWidget_2->takeItem( index.row());
    QFont lFont = font();
    lFont.setBold(false);
    ui->listWidget_2->item(0)->setFont(lFont);

    if ( NULL != currentItem )
    {
        ui->listWidget_2->insertItem(0,currentItem);
    }
    lFont.setBold(true);
    ui->listWidget_2->item(0)->setFont(lFont);

}

void MainWindow::on_checkBox_calc_stateChanged(int arg1)
{
    corrdata->set_flg(arg1);
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

    QAction *openCfgAct = new QAction(tr("Open &Config Files (pluma)"), this);
    openCfgAct->setStatusTip(tr("Open all text files in ~/.config/Correlation_plot with pluma"));
    connect(openCfgAct, &QAction::triggered, this, &MainWindow::open_config_files);
    fileMenu->addAction(openCfgAct);

    fileMenu->addSeparator();
    fileMenu->addAction(rank_mic_act);
    fileMenu->addAction(rank_imp_act);
    fileMenu->addAction(rank_gap_act);
    fileMenu->addAction(rank_list_act);
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

}
void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
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
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
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
