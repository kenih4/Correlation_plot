#include "corr_data.h"

Corr_data::Corr_data(int data_num,int f)
{
    size = data_num;
    data.resize(size);
    dflg = f;
}

Corr_data::~Corr_data(){

}
void Corr_data::get_tag(string endtime)
{
    string sep5u = "sr_mon_screen_sep5u_otr/intensity";
    string route = "xfel_tmg_sw_master_ema_route_inf_shm/current";
    string ssbt = "sr_safety_topup_intlk_beam_ssbt1/charge";
    Db_f_data *get_inf_sep5u;
    Db_i_data *get_inf_route;
    Db_f_data *get_inf_ssbt;
    int get_inf_cnt_sep5u = 0,get_inf_cnt_route = 0,get_inf_cnt_ssbt = 0;
    int status;
    char out[256];
    uint64_t btime;

    status = db_get_data_num_f(sep5u.c_str(),endtime.c_str(),size,&get_inf_sep5u,&get_inf_cnt_sep5u);
    if(status==0 && get_inf_cnt_sep5u>0){
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            cerr<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
            db_unixtime_us_to_date((get_inf_sep5u+i)->unixtime_us,out);
            status = db_get_data_num_i(route.c_str(),out,60,&get_inf_route,&get_inf_cnt_route);
            if(status==0 && get_inf_cnt_route>0){
                for(int j=0;j<get_inf_cnt_route;j++){
                    if((get_inf_route+j)->idata==769){
                        data[i].sacla_tag=(get_inf_route+j)->evno;
                        db_unixtime_us_to_date((get_inf_route+get_inf_cnt_route-1)->unixtime_us,out);
                        data[i].beg_time = out;
                        db_unixtime_us_add_sec(get_inf_route->unixtime_us,2.0,&btime);
                        db_unixtime_us_to_date(btime,out);
                        data[i].end_time = out;
                        status = db_get_data_f(ssbt.c_str(),data[i].beg_time.c_str(),data[i].end_time.c_str(),&get_inf_ssbt,&get_inf_cnt_ssbt);
                        if(status == 0 && get_inf_cnt_ssbt>0){
                            for(int n=0;n<get_inf_cnt_ssbt;n++){
                                if((get_inf_ssbt+n)->fdata > 0.0){
                                    data[i].sr_tag = (get_inf_ssbt+n)->evno;
                                    break;
                                }
                            }
                            free(get_inf_ssbt);
                        }
                        break;
                    }
                }
                free(get_inf_route);
            }
        }
        free(get_inf_sep5u);
    }
}
void Corr_data::get_tag(string begtime,string endtime)
{
    string sep5u = "sr_mon_screen_sep5u_otr/intensity";
    string route = "xfel_tmg_sw_master_ema_route_inf_shm/current";
    string ssbt = "sr_safety_topup_intlk_beam_ssbt1/charge";
    Db_f_data *get_inf_sep5u;
    Db_i_data *get_inf_route;
    Db_f_data *get_inf_ssbt;
    int get_inf_cnt_sep5u = 0,get_inf_cnt_route = 0,get_inf_cnt_ssbt = 0;
    int status;
    char out[256];
    uint64_t btime;

    status = db_get_data_f(sep5u.c_str(),begtime.c_str(),endtime.c_str(),&get_inf_sep5u,&get_inf_cnt_sep5u);
    if(status==0 && get_inf_cnt_sep5u>0){
        size = get_inf_cnt_sep5u;
        data.resize(size);
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            cerr<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
            db_unixtime_us_to_date((get_inf_sep5u+i)->unixtime_us,out);
            status = db_get_data_num_i(route.c_str(),out,60,&get_inf_route,&get_inf_cnt_route);
            if(status==0 && get_inf_cnt_route>0){
                for(int j=0;j<get_inf_cnt_route;j++){
                    if((get_inf_route+j)->idata==769){
                        data[i].sacla_tag=(get_inf_route+j)->evno;
                        db_unixtime_us_to_date((get_inf_route+get_inf_cnt_route-1)->unixtime_us,out);
                        data[i].beg_time = out;
                        db_unixtime_us_add_sec(get_inf_route->unixtime_us,2.0,&btime);
                        db_unixtime_us_to_date(btime,out);
                        data[i].end_time = out;
                        status = db_get_data_f(ssbt.c_str(),data[i].beg_time.c_str(),data[i].end_time.c_str(),&get_inf_ssbt,&get_inf_cnt_ssbt);
                        if(status == 0 && get_inf_cnt_ssbt>0){
                            for(int n=0;n<get_inf_cnt_ssbt;n++){
                                if((get_inf_ssbt+n)->fdata > 0.0){
                                    data[i].sr_tag = (get_inf_ssbt+n)->evno;
                                    break;
                                }
                            }
                            free(get_inf_ssbt);
                        }
                        break;
                    }
                }
                free(get_inf_route);
            }
        }
        free(get_inf_sep5u);
    }
    sort(data.begin(),data.end());
}

void Corr_data::db_get_sync_data()
{
    int status;
    DB_SIGNAL_STRUCT siglist;
    DB_SIGNAL_DATA* tData;
    Db_f_data *fdata;

    status = db_init_signal_list(&siglist);
    if(status == 0){
        for(auto itr = sig.begin();itr != sig.end(); ++itr){
            status = db_add_signal_list(&siglist,itr->name.c_str());
            if(status != 0){
                printf("err db_add_signal_list :: %s\n",itr->name.c_str());
                sig.erase(itr);
                itr--;
            }
        }
    }
    cerr<<"num_siglist  =  "<<siglist.num_signal<<endl;
    for(int n=0;n<size;n++){
        status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
        if(status == 0){
            for(int i=0;i<(int)sig.size();i++){
                for(int j=0;j<siglist.num_signal;j++){
                    if(sig[i].name.compare(siglist.signal_list[j].sig_name)==0){
                        tData = &siglist.signal_list[j];
                        fdata = (Db_f_data*)tData->data;
                        for(int m=0;m<tData->num_data;m++){
                            if(fdata[m].evno == data[n].sacla_tag||
                                    fdata[m].evno == data[n].sr_tag){
                                        if((fdata[m].fdata>1e8||sig[i].name.find("sr_safety")!=std::string::npos)&&sig[i].name.find("efficiency")==std::string::npos){

                                    data[n].value[sig[i].name] = fdata[m].fdata*1.60219e-10;
                                }else{
                                    data[n].value[sig[i].name] = fdata[m].fdata;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    db_release_signal_list_all(&siglist);
    sort(data.begin(),data.end());

    status = db_init_signal_list(&siglist);
    if(status == 0){
        for(auto itr = sig_d.begin();itr != sig_d.end(); ++itr){
            status = db_add_signal_list(&siglist,itr->name.c_str());
            if(status != 0){
                printf("err db_add_signal_list :: %s\n",itr->name.c_str());
                sig_d.erase(itr);
                itr--;
            }
        }
    }
    cerr<<"num_siglist  =  "<<siglist.num_signal<<endl;
    for(int n=0;n<size;n++){
        status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
        if(status == 0){
            for(int i=0;i<(int)sig_d.size();i++){
                for(int j=0;j<siglist.num_signal;j++){
                    if(sig_d[i].name.compare(siglist.signal_list[j].sig_name)==0){
                        tData = &siglist.signal_list[j];
                        fdata = (Db_f_data*)tData->data;
                        for(int m=0;m<tData->num_data;m++){
                            if(fdata[m].evno == data[n].sacla_tag||
                                    fdata[m].evno == data[n].sr_tag){
                                    if((fdata[m].fdata>1e8||sig_d[i].name.find("sr_safety")!=std::string::npos)&&sig_d[i].name.find("efficiency")==std::string::npos){

                                    data[n].denominator[sig_d[i].name] = fdata[m].fdata*1.60219e-10;
                                }else{
                                    data[n].denominator[sig_d[i].name] = fdata[m].fdata;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    db_release_signal_list_all(&siglist);

}

void Corr_data::calc_mic()
{
    MINE *mine;
    double *x,*y;
    int n;
    double alpha=0.85;
    for(int i=0;i<(int)sig.size();i++){
        n = size;
        x = new double [n];
        y = new double [n];
        if(n>=2500){
            alpha = 0.55;
        }else if(n>=1000){
            alpha = 0.6;
        }else if(n>=500){
            alpha = 0.65;
        }else if(n>=250){
            alpha = 0.7;
        }else if(n>=50){
            alpha = 0.75;
        }else if(n>=25){
            alpha = 0.8;
        }else{
            alpha = 0.85;
        }
        mine = new MINE(alpha, 15, EST_MIC_APPROX);
        for(int j=0;j<n;j++){
            x[j] = value(sig[0].name,j);
            y[j] = value(sig[i].name,j);
        }
        mine->compute_score(x,y,n);
        sig[i].coefficient = mine->mic();
        delete mine;
        delete [] x;
        delete [] y;
    }
}
void Corr_data::calc_mic(double th)
{
    MINE *mine;
    double *x,*y;
    int n = 0,m = 0;
    double alpha=0.85;
    for(int i=0;i<(int)size;i++){
        if(value(sig[0].name,i)>th){
            n++;
        }
    }
    for(int i=0;i<(int)sig.size();i++){
        x = new double [n];
        y = new double [n];
        if(m>=2500){
            alpha = 0.55;
        }else if(n>=1000){
            alpha = 0.6;
        }else if(n>=500){
            alpha = 0.65;
        }else if(n>=250){
            alpha = 0.7;
        }else if(n>=50){
            alpha = 0.75;
        }else if(n>=25){
            alpha = 0.8;
        }else{
            alpha = 0.85;
        }
        mine = new MINE(alpha, 15, EST_MIC_APPROX);

        m = 0;
        for(int j=0;j<(int)size;j++){
            if( value(sig[0].name,j)>th){
                x[m] = value(sig[0].name,j);
                y[m] = value(sig[i].name,j);
                m++;
            }
        }


        mine->compute_score(x,y,n);
        sig[i].coefficient = mine->mic();
        delete mine;
        delete [] x;
        delete [] y;
    }
}
void Corr_data::calc_mic(int beg,int end)
{
    MINE *mine;
    double *x,*y;
    int n = 0,m = 0;
    double alpha=0.85;
    struct tm buf;
    for(int i=0;i<(int)size;i++){
        strptime(endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&buf);
        if(beg < mktime(&buf) && mktime(&buf) < end){
            n++;
        }
    }
    for(int i=0;i<(int)sig.size();i++){
        x = new double [n];
        y = new double [n];
        if(m>=2500){
            alpha = 0.55;
        }else if(n>=1000){
            alpha = 0.6;
        }else if(n>=500){
            alpha = 0.65;
        }else if(n>=250){
            alpha = 0.7;
        }else if(n>=50){
            alpha = 0.75;
        }else if(n>=25){
            alpha = 0.8;
        }else{
            alpha = 0.85;
        }
        mine = new MINE(alpha, 15, EST_MIC_APPROX);

        m = 0;
        for(int j=0;j<(int)size;j++){
            strptime(endtime(j).c_str(),"%Y/%m/%d %H:%M:%S",&buf);
            if(beg < mktime(&buf) && mktime(&buf) < end){
                x[m] = value(sig[0].name,j);
                y[m] = value(sig[i].name,j);
                m++;
            }
        }


        mine->compute_score(x,y,n);
        sig[i].coefficient = mine->mic();
        delete mine;
        delete [] x;
        delete [] y;
    }
}
double Corr_data::get_max(int n)
{

    double ret = value(sig[n].name,0);
    for(int i=1;i<size;i++){
        if(ret<value(sig[n].name,i))ret = value(sig[n].name,i);
    }
    return ret;
}

double Corr_data::get_min(int n)
{
    double ret = value(sig[n].name,0);
    for(int i=1;i<size;i++){
        if(ret>value(sig[n].name,i))ret = value(sig[n].name,i);
    }
    return ret;
}

double Corr_data::get_max(int n,double ratio)
{
    double max = value(sig[n].name,0);
    double min = value(sig[n].name,0);
    double ret;
    for(int i=1;i<size;i++){
        if(max<value(sig[n].name,i))max = value(sig[n].name,i);
        if(min>value(sig[n].name,i))min = value(sig[n].name,i);
    }
    ret = max + (max - min) * ratio;
    return ret;
}

double Corr_data::get_min(int n,double ratio)
{
    double max = value(sig[n].name,0);
    double min = value(sig[n].name,0);
    double ret;
    for(int i=1;i<size;i++){
        if(max<value(sig[n].name,i))max = value(sig[n].name,i);
        if(min>value(sig[n].name,i))min = value(sig[n].name,i);
    }
    ret = min - (max - min) * ratio;
    return ret;
}

double Corr_data::get_max(int n,double ratio,int a,double th)
{
    double max = value(sig[n].name,0);
    double min = value(sig[n].name,0);
    double ret;
    for(int i=1;i<size;i++){
        if(value(sig[a].name,i)>th){
            if(max<value(sig[n].name,i))max = value(sig[n].name,i);
            if(min>value(sig[n].name,i))min = value(sig[n].name,i);
        }
    }
    ret = max + (max - min) * ratio;
    return ret;
}

double Corr_data::get_min(int n,double ratio,int a,double th)
{
    double max = value(sig[n].name,0);
    double min = value(sig[n].name,0);
    double ret;
    for(int i=1;i<size;i++){
        if(value(sig[a].name,i)>th){
            if(max<value(sig[n].name,i))max = value(sig[n].name,i);
            if(min>value(sig[n].name,i))min = value(sig[n].name,i);
        }
    }
    ret = min - (max - min) * ratio;
    return ret;
}

int Corr_data::get_ranking(int ranking,bool flg){
    int ret;
    if(flg == false && rank.size() == sig.size()){
        for(int i=0;i<(int)sig.size();i++){
            if(rank[i] == ranking)ret = i;
        }

    }else{
        ret = get_ranking(ranking);
    }
    return ret;
}
int Corr_data::get_ranking(int ranking)
{
    int ret = 0;
    rank.resize((int)sig.size());

    for(int i=0;i<(int)sig.size();i++){
        rank[i] = 0;
        for(int j=0;j<(int)sig.size();j++){
            if(i!=j && sig[i].coefficient<sig[j].coefficient){
                rank[i]++;
            }else if(i>j && sig[i].coefficient==sig[j].coefficient){
                rank[i]++;
            }
        }
    }
    for(int i=0;i<(int)sig.size();i++){
        //cerr<<"i="<<i<<" coeff = "<<sig[i].coefficient<<" rank = "<<rank[i]<<endl;
        if(rank[i] == ranking)ret = i;
    }

    return ret;
}

void Corr_data::append()
{
    size+=1;
    data.resize(size);
}

void Corr_data::erase()
{
    data.erase(data.begin());
    size-=1;
}

void Corr_data::erase(int n)
{
    for(int i=0;i<n;i++){
        data.erase(data.begin());
        size-=1;
    }
}
int Corr_data::add_tag(string strtime)
{
    string endtime,begtime;
    string sep5u = "sr_mon_screen_sep5u_otr/intensity";
    string route = "xfel_tmg_sw_master_ema_route_inf_shm/current";
    string ssbt = "sr_safety_topup_intlk_beam_ssbt1/charge";
    Db_f_data *get_inf_sep5u;
    Db_i_data *get_inf_route;
    Db_f_data *get_inf_ssbt;
    int get_inf_cnt_sep5u = 0,get_inf_cnt_route = 0,get_inf_cnt_ssbt = 0;
    int status;
    char out[256];
    uint64_t btime;
    int org_size;
    struct tm beg,end,buf;
    sort(data.begin(),data.end());
    static int cnt = 1;
    endtime = data.front().beg_time;
    begtime = data.back().end_time;
    cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;

    strptime(strtime.c_str(),"%Y/%m/%d %H:%M:%S",&buf);
    strptime(endtime.c_str(),"%Y/%m/%d %H:%M:%S",&end);
    strptime(begtime.c_str(),"%Y/%m/%d %H:%M:%S",&beg);
    cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<"  "<<strtime.c_str()<<endl;
    cerr<<mktime(&beg)<<"   "<<mktime(&end)<<"   "<<mktime(&buf)<<endl;

    if(mktime(&buf)<mktime(&end)){
        cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;
        begtime = strtime;
        if(mktime(&end)-mktime(&buf)>600){
            time_t buf_t = mktime(&end)-600*cnt;
            char bufstr[256];
            strftime(bufstr,sizeof (bufstr),"%Y/%m/%d %H:%M:%S",localtime(&buf_t));
            begtime=bufstr;
        }
        cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;
    }else if(mktime(&buf)>mktime(&beg)){
        cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;
        endtime = strtime;
        if(mktime(&buf)-mktime(&beg)>600){
            time_t buf_t = mktime(&beg)+600*cnt;
            char bufstr[256];
            strftime(bufstr,sizeof (bufstr),"%Y/%m/%d %H:%M:%S",localtime(&buf_t));
            endtime=bufstr;
        }
    }else{
        return 0;
    }

    cerr<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;

    org_size = size;
    //    if(strtime<data.front().beg_time){
    //        endtime = data.front().beg_time;
    //        begtime = strtime;
    //    }else if(strtime>data.back().end_time){
    //        endtime = strtime;
    //        begtime = data.back().end_time;
    //    }else{
    //        return 0;
    //    }


    status = db_get_data_f(sep5u.c_str(),begtime.c_str(),endtime.c_str(),&get_inf_sep5u,&get_inf_cnt_sep5u);
    if(status==0 && get_inf_cnt_sep5u>0){
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            cerr<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
            db_unixtime_us_to_date((get_inf_sep5u+i)->unixtime_us,out);
            status = db_get_data_num_i(route.c_str(),out,60,&get_inf_route,&get_inf_cnt_route);
            if(status==0 && get_inf_cnt_route>0){
                append();
                for(int j=0;j<get_inf_cnt_route;j++){
                    if((get_inf_route+j)->idata==769){
                        data[i+org_size].sacla_tag=(get_inf_route+j)->evno;
                        db_unixtime_us_to_date((get_inf_route+get_inf_cnt_route-1)->unixtime_us,out);
                        data[i+org_size].beg_time = out;
                        db_unixtime_us_add_sec(get_inf_route->unixtime_us,2.0,&btime);
                        db_unixtime_us_to_date(btime,out);
                        data[i+org_size].end_time = out;
                        status = db_get_data_f(ssbt.c_str(),data[i+org_size].beg_time.c_str(),data[i+org_size].end_time.c_str(),&get_inf_ssbt,&get_inf_cnt_ssbt);
                        if(status == 0 && get_inf_cnt_ssbt>0){
                            for(int n=0;n<get_inf_cnt_ssbt;n++){
                                if((get_inf_ssbt+n)->fdata > 0.0){
                                    data[i+org_size].sr_tag = (get_inf_ssbt+n)->evno;
                                    break;
                                }
                            }
                            free(get_inf_ssbt);
                        }
                        break;
                    }
                }
                cerr<<"beg = "<<data[i+org_size].beg_time<<
                      "  end = "<<data[i+org_size].end_time<<
                      "  sacla = "<<data[i+org_size].sacla_tag<<
                      "  sr = "<<data[i+org_size].sr_tag<<endl;
                if(data[i+org_size].sr_tag==0){
                    data.erase(data.end()-1);
                    size-=1;
                    get_inf_cnt_sep5u-=1;
                }
                free(get_inf_route);
            }
        }
        free(get_inf_sep5u);
    }
    sort(data.begin(),data.end());
    if(get_inf_cnt_sep5u==0){
        cerr<<get_inf_cnt_sep5u<<endl;
        cnt++;
    }else{
        cnt = 0;
    }
    return get_inf_cnt_sep5u;
}

void Corr_data::add_db_get_sync_data()
{
    int status;
    DB_SIGNAL_STRUCT siglist;
    DB_SIGNAL_DATA* tData;
    Db_f_data *fdata;

    for(int n=0;n<size;n++){
        status = db_init_signal_list(&siglist);
        if(status == 0){
            for(auto itr = sig.begin();itr != sig.end(); ++itr){
                if(data[n].value.count(itr->name)==0){
                    status = db_add_signal_list(&siglist,itr->name.c_str());
                    if(status != 0){
                        printf("err db_add_signal_list :: %s\n",itr->name.c_str());
                        sig.erase(itr);
                        itr--;
                    }
                }
            }
        }
        if(siglist.num_signal>0){
            status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
            if(status == 0){
                for(int i=0;i<(int)sig.size();i++){
                    for(int j=0;j<siglist.num_signal;j++){
                        if(sig[i].name.compare(siglist.signal_list[j].sig_name)==0){
                            tData = &siglist.signal_list[j];
                            fdata = (Db_f_data*)tData->data;
                            for(int m=0;m<tData->num_data;m++){
                                if(fdata[m].evno == data[n].sacla_tag||
                                        fdata[m].evno == data[n].sr_tag){
                                        if((fdata[m].fdata>1e8||sig[i].name.find("sr_safety")!=std::string::npos)&&sig[i].name.find("efficiency")==std::string::npos){

                                        data[n].value[sig[i].name] = fdata[m].fdata*1.60219e-10;
                                    }else{
                                        data[n].value[sig[i].name] = fdata[m].fdata;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            db_release_signal_list_all(&siglist);
        }
    }

    for(int n=0;n<size;n++){
        status = db_init_signal_list(&siglist);
        if(status == 0){
            for(auto itr = sig_d.begin();itr != sig_d.end(); ++itr){
                if(data[n].denominator.count(itr->name)==0){
                    status = db_add_signal_list(&siglist,itr->name.c_str());
                    if(status != 0){
                        printf("err db_add_signal_list :: %s\n",itr->name.c_str());
                        sig_d.erase(itr);
                        itr--;
                    }
                }
            }
        }
        if(siglist.num_signal>0){
            status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
            if(status == 0){
                for(int i=0;i<(int)sig_d.size();i++){
                    for(int j=0;j<siglist.num_signal;j++){
                        if(sig_d[i].name.compare(siglist.signal_list[j].sig_name)==0){
                            tData = &siglist.signal_list[j];
                            fdata = (Db_f_data*)tData->data;
                            for(int m=0;m<tData->num_data;m++){
                                if(fdata[m].evno == data[n].sacla_tag||
                                        fdata[m].evno == data[n].sr_tag){
                                    if((fdata[m].fdata>1e8||sig_d[i].name.find("sr_safety")!=std::string::npos)&&sig_d[i].name.find("efficiency")==std::string::npos){
                                        data[n].denominator[sig_d[i].name] = fdata[m].fdata*1.60219e-10;
                                    }else{
                                        data[n].denominator[sig_d[i].name] = fdata[m].fdata;

                                    }
                                }
                            }
                        }
                    }
                }
            }

            db_release_signal_list_all(&siglist);
        }
    }
}
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <regex>

void Corr_data::outfile(string fname)
{
    QFile fp(fname.c_str());
    if(fp.open(QIODevice::WriteOnly)){
        QTextStream out(&fp);
        out << "sacla_tag"<<",";
        out << "sr_tag"<<",";
        out << "beg_time"<<",";
        out << "end_time"<<",";
        for(int j=0;j<(int)sig_d.size();j++){
            out<<"("<<sig[0].name.c_str()<<")"<<"/"<<"("<<sig_d[j].name.c_str()<<")"<<"*100"<<",";
        }
        for(int j=0;j<(int)sig.size();j++){
            out << sig[j].name.c_str()<<",";
        }
        for(int j=0;j<(int)sig_d.size();j++){
            out << sig_d[j].name.c_str()<<",";
        }
        out << endl;
        for(int i=0;i<size;i++){
            out << data[i].sacla_tag<<",";
            out << data[i].sr_tag<<",";
            out << regex_replace(data[i].beg_time,regex(" "),"-").c_str()<<",";
            out << regex_replace(data[i].end_time,regex(" "),"-").c_str()<<",";
            for(int j=0;j<(int)sig_d.size();j++){
                out<<data[i].value[sig[0].name]/data[i].denominator[sig_d[j].name]*100<<",";
            }
            for(int j=0;j<(int)sig.size();j++){
                out << data[i].value[sig[j].name]<<",";
            }
            for(int j=0;j<(int)sig_d.size();j++){
                out << data[i].denominator[sig_d[j].name]<<",";
            }
            out << endl;
        }
        fp.close();
    }else{
        qWarning().noquote()<<"Cannot open file  "<<fname.c_str()<<endl;
    }

}
