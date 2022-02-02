#include "mainwindow.h"
#include "ui_mainwindow.h"

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

    ui->widget_1->addGraph();
    ui->widget_1->graph(0)->setPen(QPen(Qt::red));
    ui->widget_1->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_1->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));

    ui->widget_2->addGraph();
    ui->widget_2->graph(0)->setPen(QPen(Qt::red));
    ui->widget_2->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_2->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));

    ui->widget_3->addGraph();
    ui->widget_3->graph(0)->setPen(QPen(Qt::red));
    ui->widget_3->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_3->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));

    ui->listWidget->addItem("sr_safety_topup_intlk_beam/efficiency");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_3_x/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_3_y/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_5_x/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_5_y/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_7_x/position");
    ui->listWidget->addItem("xfel_mon_bpm_xsbt_7_y/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_15_x/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_15_y/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_16_x/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_16_y/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_25_x/position");
    ui->listWidget->addItem("sr_mon_bpm_xsbt_25_y/position");
    ui->listWidget->addItem("xfel_mon_msbpm_bc3_x/position");



    create_tree();
    ui->V_threshold->setText(QString::number(0));
    timer = new QTimer(this);
    timer->setInterval(6000);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_data()));
    timer->start();
    corrdata = new Corr_data(ui->spinBox->value());
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    delete corrdata;
    corrdata = new Corr_data(ui->spinBox->value());

    for(int i= 0;i<ui->listWidget->count();++i){
        corrdata->add_signal(ui->listWidget->item(i)->text().toStdString().c_str());
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

    plot->graph(0)->setData(x,y);
    if(ui->checkBox_th->checkState()==Qt::Checked){
        plot->xAxis->setRange(corrdata->get_min(n,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(n,0.5,0,ui->V_threshold->text().toDouble()));
        plot->yAxis->setRange(corrdata->get_min(0,0.5,0,ui->V_threshold->text().toDouble()),corrdata->get_max(0,0.5,0,ui->V_threshold->text().toDouble()));
    }else{
        plot->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
        plot->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));

    }
    plot->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    plot->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    plot->graph(0)->setName(QString("mic : %1").arg(corrdata->mic(n)));
    plot->legend->setVisible(true);
    plot->legend->setFont(legendFont);
    plot->legend->setBrush(QBrush(QColor(255,255,255,230)));
    plot->axisRect()->insetLayout()->setInsetAlignment(0,Qt::AlignTop|Qt::AlignRight);
    plot->replot();

}
void MainWindow::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    cerr<<column<<item->text(column).toStdString()<<endl;
    cerr<<ui->listWidget->count()<<endl;
    cerr<<item->childCount()<<endl;
    if(item->childCount()!=0)return;
    for(int i= 0;i<ui->listWidget->count();++i){
        cerr<<ui->listWidget->item(i)->text().toStdString()<<endl;
        if(ui->listWidget->item(i)->text().toStdString().compare(item->text(column).toStdString())==0)return;
    }
    ui->listWidget->addItem(item->text(column));

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
    QListWidgetItem* currentItem = ui->listWidget->takeItem( ui->listWidget->currentRow());
    if ( NULL != currentItem )
    {
        delete ( currentItem );
    }
}

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

    plot_data(ui->widget_1,corrdata->get_ranking(1));
    plot_data(ui->widget_2,corrdata->get_ranking(2));
    plot_data(ui->widget_3,corrdata->get_ranking(3));

}
