#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qdatetime.h>

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

    //    ui->listWidget->addItem("sr_safety_topup_intlk_beam/efficiency");
    ui->listWidget->addItem("sr_safety_topup_intlk_beam_sr_dcct1/charge_diff");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_3_x/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_5_x/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_25_x/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_25_y/position");
    ui->listWidget->addItem("xfel_mon_msbpm_bc1_x/position");
    ui->listWidget->addItem("xfel_mon_msbpm_bc2_x/position");
    ui->listWidget->addItem("xfel_mon_msbpm_bc3_x/position");

    ui->listWidget_2->addItem("sr_mon_bpm_xsbt_20_sum/charge");
    ui->listWidget_2->addItem("sr_mon_bpm_xsbt_21_sum/charge");
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

}

MainWindow::~MainWindow()
{
    delete ui;
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
    plot->graph(0)->setData(x,y);

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

    plot->replot();
}
void MainWindow::plot_data(QCustomPlot *plot,int n)
{
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    QFont legendFont = font();
    legendFont.setPointSize(9);

    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  1****"<<endl;
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(corrdata->sig_name(n),i);
        y[i] = corrdata->value(corrdata->sig_name(0),i);
        //        y[i] = corrdata->value(corrdata->sig_name(0),i)/(corrdata->denominator(corrdata->sig_d_name(0),i)*1e-9/(1.60219e-19))*100;
    }
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  2****"<<endl;
    plot->graph(0)->setData(x,y);

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
        plot->xAxis->setRange(corrdata->get_min(n,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(n,0.5,0,ui->V_threshold->text().toDouble()));
        plot->yAxis->setRange(corrdata->get_min(0,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(0,0.5,0,ui->V_threshold->text().toDouble()));
    }else{
        plot->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
        plot->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));

    }

    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  3****"<<endl;
    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    if(ui->checkBox_calc->checkState()==Qt::Checked){
        plot->yAxis->setLabel("calc efficiency");
    }
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
    //    cerr<<"****  "<<corrdata->sig_name(n)<<"  4****"<<endl;
    QCPItemText *micValue = new QCPItemText(plot);
    micValue->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
    micValue->position->setType(QCPItemPosition::ptAxisRectRatio);
    micValue->position->setCoords(0.8, 0); // place position at center/top of axis rect
    micValue->setText(QString("mic : %1").arg(corrdata->mic(n),0,'f',3));
    micValue->setFont(legendFont);
    micValue->setPen(QPen(Qt::white));
    micValue->setBrush(QBrush(QColor(255,255,255,230)));

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
    QStringList list = {"sr_safety_topup","sr_mon_xsbt","xfel_mon_bpm","xfel_mon_xsbt","xfel_mon_msbpm","xfel_mon_csr","xfel_mon_blm"};
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
