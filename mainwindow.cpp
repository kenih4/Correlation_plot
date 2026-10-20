#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qdatetime.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
    ui->V_threshold->setText(QString::number(0));
    timer = new QTimer(this);
    timer->setInterval(10000);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_data()));
    timer->start();
    corrdata = new Corr_data(ui->spinBox->value(),ui->checkBox_calc->checkState());
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
    tracerText = new QCPItemText(ui->widget);
    tracerText->position->setType(QCPItemPosition::ptAxisRectRatio);
    tracerText->setFont(QFont(font().family(),8));
    tracerText->setPositionAlignment(Qt::AlignLeft);
    tracerText->position->setCoords(QPointF(0,0));
    tracerText->setText("(-)");
    tracer->setVisible(false);
    tracerText->setVisible(false);


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

    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
    }
    for(int i= 0;i<ui->listWidget_2->count();++i){
        corrdata->add_signal_d(ui->listWidget_2->item(i)->text().toStdString().c_str());

    }

    corrdata->get_tag(ui->dateTimeEdit->dateTime().toString("yyyy/MM/dd HH:mm:ss").toStdString());
    corrdata->db_get_sync_data();
    for(int i=0;i<corrdata->data_size();i++){
        cerr<<"beg = "<<corrdata->begtime(i)<<
              "  end = "<<corrdata->endtime(i)<<
              "  sacla = "<< corrdata->sacla_tag(i)<<
              "  sr = "<<corrdata->sr_tag(i)<<endl;
        for(int j=0;j<corrdata->sig_size();j++){
            cerr<<i<<" "<<j<<"  "<<
                  corrdata->sig_name(j)<<"  "<<
                  corrdata->value(corrdata->sig_name(j),i)<<endl;
        }

    }
    if(ui->checkBox_th->checkState()==Qt::Checked){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }
    for(int j=0;j<corrdata->sig_size();j++){
        cerr<<" "<<j<<"  "<<
              corrdata->sig_name(j)<<"  "<<
              corrdata->mic(j)<<endl;
    }

    plot_data(ui->widget_1,corrdata->get_ranking(1));
    plot_data(ui->widget_2,corrdata->get_ranking(2));
    plot_data(ui->widget_3,corrdata->get_ranking(3));
    plot_data(ui->widget_7,corrdata->get_ranking(4));
    plot_data(ui->widget_8,corrdata->get_ranking(5));
    plot_data(ui->widget_9,corrdata->get_ranking(6));
    plot_trend(ui->widget);
    corrdata->outfile("/home/xfel/xfelopr/users/ytajiri/data/corr.csv");

}
void MainWindow::plot_init(QCustomPlot *plot)
{
    plot->addGraph();
    plot->graph(0)->setPen(QPen(Qt::red));
    plot->graph(0)->setLineStyle(QCPGraph::lsNone);
    plot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));
    plot->addGraph();
    plot->graph(1)->setPen(QPen(Qt::blue));
    plot->graph(1)->setLineStyle(QCPGraph::lsNone);
    plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle,"#0000ff", "#8f0000ff", 5));
    plot->addGraph();
    plot->graph(2)->setPen(QPen(Qt::blue));
    plot->graph(2)->setLineStyle(QCPGraph::lsNone);
    plot->graph(2)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, "#0000ff", "#0000ff", 7));

}
void MainWindow::plot_trend(QCustomPlot *plot)
{
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    QFont legendFont = font();
    legendFont.setPointSize(9);
    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("HH:mm:ss");
    plot->xAxis->setTicker(dateTicker);
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  1****"<<endl;
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = QDateTime::fromString(QString::fromStdString(corrdata->endtime(i)),"yyyy/MM/dd HH:mm:ss.z").toSecsSinceEpoch();
        y[i] = corrdata->value(corrdata->sig_name(0),i);
        //        y[i] = corrdata->value(corrdata->sig_name(0),i)/(corrdata->denominator(corrdata->sig_d_name(0),i)*1e-9/(1.60219e-19))*100;
    }
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;
    set_scatter_data(plot,x,y);

    QVector<double> x2(1),y2(1);
    x2[0] = x[corrdata->data_size()-2];
    y2[0] = y[corrdata->data_size()-2];
    plot->graph(1)->setData(x2,y2);

    QVector<double> x3(1),y3(1);
    x3[0] = x[corrdata->data_size()-1];
    y3[0] = y[corrdata->data_size()-1];
    plot->graph(2)->setData(x3,y3);
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;

    if(ui->checkBox_th->checkState()==Qt::Checked){
        plot->yAxis->setRange(corrdata->get_min(0,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(0,0.5,0,ui->V_threshold->text().toDouble()));
    }else{
        plot->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));

    }
    plot->xAxis->setRange(x[0]-60,x[corrdata->data_size()-1]+60);
    //plot->graph(0)->rescaleAxes(true);
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  3****"<<endl;
    plot->xAxis->setLabel("time");
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  4****"<<endl;
    QVector<double> x4(2),y4(2);
    x4[0] = x4[1] = y4[0] = y4[1] = 0;
    ui->widget->graph(3)->setData(x4,y4);
    ui->widget->graph(4)->setData(x4,y4);
    ui->widget->graph(3)->setVisible(false);
    ui->widget->graph(4)->setVisible(false);
    tracer->setVisible(false);
    tracerText->setVisible(false);

    plot->replot();
}
// 散布図のデータを設定する(古い順に並んだ x,y)
void MainWindow::set_scatter_data(QCustomPlot *plot,const QVector<double> &x,const QVector<double> &y)
{
    plot->graph(0)->setData(x,y);
    scatter_xy[plot] = qMakePair(x,y);
    apply_gradient(plot);
}

// Gradient チェックONなら、最古=赤 → 最新=青 のグラデーションで表示する。
// 1点ごとの色指定はできないため、色ごとのグラフ(GRADIENT_BINS個)に点を振り分ける。
void MainWindow::apply_gradient(QCustomPlot *plot)
{
    const int GRADIENT_BINS = 50;
    bool on = ui->checkBox_gradient->isChecked();

    // graph(0..2) は通常表示用。グラデーション用のグラフは、その後ろ(最初に作った時点の末尾)に追加する
    if(on && !gradient_first.contains(plot)){
        gradient_first[plot] = plot->graphCount();
    }
    if(!gradient_first.contains(plot)){
        plot->graph(0)->setVisible(true);
        return;
    }
    const int FIRST = gradient_first[plot];

    if(on){
        while(plot->graphCount() < FIRST + GRADIENT_BINS){
            double t = (plot->graphCount() - FIRST) / (double)(GRADIENT_BINS - 1);
            QColor col(qRound(255 * (1 - t)), 0, qRound(255 * t));
            QCPGraph *g = plot->addGraph();
            g->setPen(QPen(col));
            g->setLineStyle(QCPGraph::lsNone);
            g->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, col, Qt::white, 5));
        }
    }
    plot->graph(0)->setVisible(!on);
    for(int b=0;b<GRADIENT_BINS && FIRST + b < plot->graphCount();b++){
        plot->graph(FIRST + b)->setVisible(on);
    }

    if(on && scatter_xy.contains(plot)){
        const QVector<double> &x = scatter_xy[plot].first;
        const QVector<double> &y = scatter_xy[plot].second;
        int n = x.size();
        QVector<QVector<double>> bx(GRADIENT_BINS), by(GRADIENT_BINS);
        for(int i=0;i<n;i++){
            double t = (n > 1) ? i / (double)(n - 1) : 1.0;
            int b = qRound(t * (GRADIENT_BINS - 1));
            bx[b].append(x[i]);
            by[b].append(y[i]);
        }
        for(int b=0;b<GRADIENT_BINS;b++){
            plot->graph(FIRST + b)->setData(bx[b], by[b]);
        }
    }
}

void MainWindow::on_checkBox_gradient_toggled(bool)
{
    for(auto it = scatter_xy.begin(); it != scatter_xy.end(); ++it){
        apply_gradient(it.key());
        it.key()->replot();
    }
}

void MainWindow::plot_data(QCustomPlot *plot,int n)
{
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    QFont legendFont = font();
    legendFont.setPointSize(9);

    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(corrdata->sig_name(n),i);
        y[i] = corrdata->value(corrdata->sig_name(0),i);
    }
    set_scatter_data(plot,x,y);

    QVector<double> x2(1),y2(1);
    x2[0] = x[corrdata->data_size()-2];
    y2[0] = y[corrdata->data_size()-2];
    plot->graph(1)->setData(x2,y2);

    QVector<double> x3(1),y3(1);
    x3[0] = x[corrdata->data_size()-1];
    y3[0] = y[corrdata->data_size()-1];
    plot->graph(2)->setData(x3,y3);

    if(ui->checkBox_th->checkState()==Qt::Checked){
        plot->xAxis->setRange(corrdata->get_min(n,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(n,0.5,0,ui->V_threshold->text().toDouble()));
        plot->yAxis->setRange(corrdata->get_min(0,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(0,0.5,0,ui->V_threshold->text().toDouble()));
    }else{
        plot->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
        plot->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));
    }

    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);

    QCPItemText *micValue = new QCPItemText(plot);
    micValue->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    micValue->position->setType(QCPItemPosition::ptAxisRectRatio);
    micValue->position->setCoords(0.8, 0); // place position at center/top of axis rect
    micValue->setText(QString("mic : %1").arg(corrdata->mic(n),0,'f',3));
    micValue->setFont(legendFont);
    micValue->setPen(QPen(Qt::white));
    micValue->setBrush(QBrush(QColor(255,255,255,230)));
    plot->graph(1)->setVisible(true);
    plot->graph(2)->setVisible(true);

    plot->replot();

}
void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    cerr<<column<<item->text(column).toStdString()<<endl;
    cerr<<ui->listWidget->count()<<endl;
    cerr<<item->childCount()<<endl;
    if(item->childCount()!=0)return;

    if(ui->radioButton_2->isChecked()){
        for(int i= 0;i<ui->listWidget_2->count();++i){
            cerr<<ui->listWidget_2->item(i)->text().toStdString()<<endl;
            if(ui->listWidget_2->item(i)->text().toStdString().compare(item->text(column).toStdString())==0)return;
        }
        ui->listWidget_2->addItem(item->text(column));
    }else{
        for(int i= 0;i<ui->listWidget->count();++i){
            cerr<<ui->listWidget->item(i)->text().toStdString()<<endl;
            if(ui->listWidget->item(i)->text().toStdString().compare(item->text(column).toStdString())==0)return;
        }
        ui->listWidget->addItem(item->text(column));
    }
    corrdata->rank_init();
}
void MainWindow::create_tree()
{
    ui->treeWidget->setColumnCount(1);
    ui->treeWidget->setHeaderLabel(tr("Signal"));
    QList<QTreeWidgetItem*> items;
    QStringList list = {"sr_safety_topup","sr_mon_xsbt","sr_mon_img","xfel_mon_bpm","xfel_mon_xsbt","xfel_mon_msbpm","xfel_mon_csr","xfel_mon_blm","xfel_mon_ct","xfel_mon_timing","xfel_llrf_238","xfel_llrf_476","xfel_llrf_lb","xfel_llrf_sb_1"};
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
        cerr<<"*Data size = "<<corrdata->data_size()<<endl;

    if(ui->checkBox_add->checkState()!=Qt::Checked
            && corrdata->data_size()>ui->spinBox->value()){
        corrdata->erase(corrdata->data_size()-ui->spinBox->value());
        if(cnt>0)
            cerr<<"***Data size = "<<corrdata->data_size()<<endl;
    }


    if(ui->checkBox_th->checkState()==Qt::Checked){
        corrdata->calc_mic(ui->V_threshold->text().toDouble());
    }else{
        corrdata->calc_mic();
    }
    if(cnt>0)
        for(int j=0;j<corrdata->sig_size();j++){
            cerr<<" "<<j<<"  "<<
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
    plot_trend(ui->widget);
    if(cnt>0)
        corrdata->outfile("/home/xfel/xfelopr/users/ytajiri/data/corr.csv");


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

    const QIcon saveIcon = QIcon::fromTheme("document-save", QIcon(":/images/save.png"));
    QAction *saveAct = new QAction(saveIcon, tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::save);
    fileMenu->addAction(saveAct);
    //    fileToolBar->addAction(saveAct);

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
    cerr<<beg<<"   "<<end<<endl;
    corrdata->calc_mic(beg,end);
    cerr<<beg<<"   "<<end<<endl;

    plot_data(ui->widget_1,corrdata->get_ranking(1),beg,end);
    plot_data(ui->widget_2,corrdata->get_ranking(2),beg,end);
    plot_data(ui->widget_3,corrdata->get_ranking(3),beg,end);
    plot_data(ui->widget_7,corrdata->get_ranking(4),beg,end);
    plot_data(ui->widget_8,corrdata->get_ranking(5),beg,end);
    plot_data(ui->widget_9,corrdata->get_ranking(6),beg,end);
    cerr<<beg<<"   "<<end<<endl;

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
    cerr<<"m = "<<m<<endl;
    QVector<double> x(m),y(m);
    m=0;
    for(int i=0;i<corrdata->data_size();i++){
        strptime(corrdata->endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&buf);
        if(beg < mktime(&buf) && mktime(&buf) < end){
            x[m] = corrdata->value(corrdata->sig_name(n),i);
            y[m] = corrdata->value(corrdata->sig_name(0),i);
            m++;
        }
    }
    set_scatter_data(plot,x,y);

    if(ui->checkBox_th->checkState()==Qt::Checked){
        plot->xAxis->setRange(corrdata->get_min(n,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(n,0.5,0,ui->V_threshold->text().toDouble()));
        plot->yAxis->setRange(corrdata->get_min(0,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(0,0.5,0,ui->V_threshold->text().toDouble()));
    }else{
        plot->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
        plot->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));
    }

    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);

    QCPItemText *micValue = new QCPItemText(plot);
    micValue->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    micValue->position->setType(QCPItemPosition::ptAxisRectRatio);
    micValue->position->setCoords(0.8, 0); // place position at center/top of axis rect
    micValue->setText(QString("mic : %1").arg(corrdata->mic(n),0,'f',3));
    micValue->setFont(legendFont);
    micValue->setPen(QPen(Qt::white));
    micValue->setBrush(QBrush(QColor(255,255,255,230)));
    plot->graph(1)->setVisible(false);
    plot->graph(2)->setVisible(false);

    plot->replot();

}
