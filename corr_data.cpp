#include "corr_data.h"

Corr_data::Corr_data(int data_num)
{
    sig.push_back({"sr_safety_topup_intlk_beam/efficiency",0,0});
    size = data_num;
    data.resize(size);
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

void Corr_data::calc_mic()
{
    MINE *mine;
    double *x,*y;
    int n;
    for(int i=0;i<(int)sig.size();i++){
        mine = new MINE(0.6, 15, EST_MIC_APPROX);
        n = size;
        x = new double [n];
        y = new double [n];
        for(int j=0;j<n;j++){
            x[j] = data[j].value[sig[0].name];
            y[j] = data[j].value[sig[i].name];
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
    double ret = data[0].value[sig[n].name];
    for(int i=1;i<size;i++){
        if(ret<data[i].value[sig[n].name])ret = data[i].value[sig[n].name];
    }
    return ret;
}

double Corr_data::get_min(int n)
{
    double ret = data[0].value[sig[n].name];
    for(int i=1;i<size;i++){
        if(ret>data[i].value[sig[n].name])ret = data[i].value[sig[n].name];
    }
    return ret;
}
