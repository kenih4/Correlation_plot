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
    ui->widget_1->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, Qt::red, Qt::white, 5));


}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
   corrdata = new Corr_data(50);
   corrdata->add_signal("xfel_mon_bpm_xsbt_3_x/position");
   corrdata->add_signal("xfel_mon_bpm_xsbt_5_x/position");
   corrdata->add_signal("xfel_mon_csr_bc2_adc/voltage");
   corrdata->add_signal("xfel_mon_csr_bc3_adc/voltage");

   corrdata->get_tag("2021/11/15 15:00:00");
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
   for(int i=0;i<corrdata->data_size();i++){
       x[i] = corrdata->value(corrdata->sig_name(1),i);
       y[i] = corrdata->value(corrdata->sig_name(0),i);
   }
   ui->widget_1->graph(0)->setData(x,y);
   ui->widget_1->replot();

   delete corrdata;
}
