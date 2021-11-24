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
    ui->widget_1->addGraph();
    ui->widget_1->graph(0)->setPen(QPen(Qt::red));
    ui->widget_1->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_1->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 3));

    ui->widget_2->addGraph();
    ui->widget_2->graph(0)->setPen(QPen(Qt::red));
    ui->widget_2->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_2->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 3));

    ui->widget_3->addGraph();
    ui->widget_3->graph(0)->setPen(QPen(Qt::red));
    ui->widget_3->graph(0)->setLineStyle(QCPGraph::lsNone);
    ui->widget_3->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 3));

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    ui->dateTimeEdit->setDateTime(QDateTime::currentDateTime());
    ui->dateTimeEdit->setDisplayFormat(tr("yyyy/MM/dd HH:mm:ss"));
    corrdata = new Corr_data(50);
    corrdata->add_signal("xfel_mon_bpm_xsbt_1_x/position");
    corrdata->add_signal("xfel_mon_bpm_xsbt_3_x/position");
    corrdata->add_signal("xfel_mon_bpm_xsbt_5_x/position");
    corrdata->add_signal("sr_mon_bpm_xsbt_25_x/position");
    corrdata->add_signal("sr_mon_bpm_xsbt_25_y/position");
    corrdata->add_signal("xfel_mon_csr_bc1_adc/voltage");
    corrdata->add_signal("xfel_mon_csr_bc2_adc/voltage");
    corrdata->add_signal("xfel_mon_csr_bc3_adc/voltage");


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
    corrdata->calc_mic();
    for(int j=0;j<corrdata->sig_size();j++){
        cerr<<" "<<j<<"  "<<
              corrdata->sig_name(j)<<"  "<<
              corrdata->mic(j)<<endl;
    }
    QVector<double> x(corrdata->data_size()),y(corrdata->data_size());
    int n;
    n = corrdata->get_ranking(1);
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(corrdata->sig_name(n),i);
        y[i] = corrdata->value(corrdata->sig_name(0),i);
    }
    ui->widget_1->graph(0)->setData(x,y);
    ui->widget_1->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
    ui->widget_1->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));
    ui->widget_1->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    ui->widget_1->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    ui->widget_1->graph(0)->setName(QString::number(corrdata->mic(n)));
    ui->widget_1->replot();

    n = corrdata->get_ranking(2);
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(corrdata->sig_name(n),i);
        y[i] = corrdata->value(corrdata->sig_name(0),i);
    }
    ui->widget_2->graph(0)->setData(x,y);
    ui->widget_2->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
    ui->widget_2->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));
    ui->widget_2->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    ui->widget_2->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    ui->widget_2->graph(0)->setName(QString::number(corrdata->mic(n)));
    ui->widget_2->replot();

    n = corrdata->get_ranking(3);
    for(int i=0;i<corrdata->data_size();i++){
        x[i] = corrdata->value(corrdata->sig_name(n),i);
        y[i] = corrdata->value(corrdata->sig_name(0),i);
    }
    ui->widget_3->graph(0)->setData(x,y);
    ui->widget_3->xAxis->setRange(corrdata->get_min(n,0.5),corrdata->get_max(n,0.5));
    ui->widget_3->yAxis->setRange(corrdata->get_min(0,0.5),corrdata->get_max(0,0.5));
    ui->widget_3->xAxis->setLabel(tr(corrdata->sig_name(n).c_str()));
    ui->widget_3->yAxis->setLabel(tr(corrdata->sig_name(0).c_str()));
    ui->widget_3->graph(0)->setName(QString::number(corrdata->mic(n)));
    ui->widget_3->replot();

    delete corrdata;
}
