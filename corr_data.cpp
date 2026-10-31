#include "corr_data.h"

// データ取得時にタグを決めたショットの情報を、ターミナル(標準エラー)に1行で出す。
//   evno : 基準にした sr_mon_screen_sep5u_otr/intensity のイベント番号
//   window: ショットの検索時間窓 beg_time ~ end_time
//   sacla_tag, sr_tag : 決まったタグ(0 は見つからなかったことを示す)
// タグのオフセットを指定しているときは、その旨と、実際に照合に使うタグを明示する。
static void log_tag(long long evno,const Data &d,const char *note,long long off_sacla,long long off_sr)
{
    cerr << "[tag] evno=" << evno
         << "  window=" << (d.beg_time.empty() ? "-" : d.beg_time) << " ~ " << (d.end_time.empty() ? "-" : d.end_time)
         << "  sacla_tag=" << d.sacla_tag
         << "  sr_tag=" << d.sr_tag;
    if(off_sacla!=0 || off_sr!=0){
        cerr << "  [OFFSET SPECIFIED: sacla_tag" << showpos << off_sacla << noshowpos;
        if(d.sacla_tag!=0)cerr << " -> " << d.sacla_tag+off_sacla;
        else cerr << " (not applied: tag not found)";
        cerr << ", sr_tag" << showpos << off_sr << noshowpos;
        if(d.sr_tag!=0)cerr << " -> " << d.sr_tag+off_sr;
        else cerr << " (not applied: tag not found)";
        cerr << "]";
    }
    if(note && note[0])cerr << "  " << note;
    cerr << endl;
}

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
    if(status==0 && get_inf_cnt_sep5u>(int)data.size())get_inf_cnt_sep5u = (int)data.size();     // 要求より多く返されても data の外へは書かない
    if(status==0 && get_inf_cnt_sep5u>0){
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            DLOG<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
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
            log_tag((get_inf_sep5u+i)->evno,data[i],
                    data[i].sacla_tag==0 ? "(route 769 not found)" : (data[i].sr_tag==0 ? "(sr_tag not found)" : ""),
                    off_sacla,off_sr);
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
            DLOG<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
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
            log_tag((get_inf_sep5u+i)->evno,data[i],
                    data[i].sacla_tag==0 ? "(route 769 not found)" : (data[i].sr_tag==0 ? "(sr_tag not found)" : ""),
                    off_sacla,off_sr);
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
    DLOG<<"num_siglist  =  "<<siglist.num_signal<<endl;
    for(int n=0;n<size;n++){
        status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
        if(status == 0){
            for(int i=0;i<(int)sig.size();i++){
                for(int j=0;j<siglist.num_signal;j++){
                    if(sig[i].name.compare(siglist.signal_list[j].sig_name)==0){
                        tData = &siglist.signal_list[j];
                        fdata = (Db_f_data*)tData->data;
                        for(int m=0;m<tData->num_data;m++){
                            if(fdata[m].evno == eff_sacla(n)||
                                    fdata[m].evno == eff_sr(n)){
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
    DLOG<<"num_siglist  =  "<<siglist.num_signal<<endl;
    for(int n=0;n<size;n++){
        status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
        if(status == 0){
            for(int i=0;i<(int)sig_d.size();i++){
                for(int j=0;j<siglist.num_signal;j++){
                    if(sig_d[i].name.compare(siglist.signal_list[j].sig_name)==0){
                        tData = &siglist.signal_list[j];
                        fdata = (Db_f_data*)tData->data;
                        for(int m=0;m<tData->num_data;m++){
                            if(fdata[m].evno == eff_sacla(n)||
                                    fdata[m].evno == eff_sr(n)){
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

#include <random>

// 時間順に並んだ v から移動平均(中心、端は窓を狭める)を引いて、ゆっくりしたドリフトを除く
void Corr_data::detrend(double *v,int n)
{
    if(n<6)return;
    int w = n/10;
    if(w<5)w = 5;
    if(w>25)w = 25;
    int h = w/2;
    vector<double> ps(n+1,0.0);
    for(int i=0;i<n;i++)ps[i+1] = ps[i]+v[i];
    vector<double> out(n);
    for(int i=0;i<n;i++){
        int lo = max(0,i-h), hi = min(n-1,i+h);
        out[i] = v[i]-(ps[hi+1]-ps[lo])/(hi-lo+1);
    }
    for(int i=0;i<n;i++)v[i] = out[i];
}

// データ数に応じたMINEのalpha(従来のcalc_micと同じ値)
double Corr_data::mic_alpha(int n)
{
    if(n>=2500)return 0.55;
    if(n>=1000)return 0.6;
    if(n>=500)return 0.65;
    if(n>=250)return 0.7;
    if(n>=50)return 0.75;
    if(n>=25)return 0.8;
    return 0.85;
}

// 無相関(独立な一様乱数)のときのMICの95パーセンタイル。
// 値はデータ数だけで決まるので、丸めたデータ数ごとに1度だけ計算してキャッシュする。
double Corr_data::mic_null95(int n)
{
    static map<int,double> cache;
    int key = n;
    if(n>=100){
        int unit = 1;
        while(n/unit>=100)unit*=10;
        key = (n/unit)*unit;    // 上位2桁に丸める
    }
    if(key<4)return 1.0;
    auto it = cache.find(key);
    if(it!=cache.end())return it->second;

    const int R = 60;
    std::mt19937 gen(12345);
    std::uniform_real_distribution<double> u(0.0,1.0);
    vector<double> mics;
    vector<double> x(key),y(key);
    for(int r=0;r<R;r++){
        for(int i=0;i<key;i++){x[i]=u(gen);y[i]=u(gen);}
        MINE m(mic_alpha(key),15,EST_MIC_APPROX);
        m.compute_score(x.data(),y.data(),key);
        mics.push_back(m.mic());
    }
    sort(mics.begin(),mics.end());
    double v = mics[(int)(0.95*(R-1)+0.5)];
    cache[key] = v;
    return v;
}

// 平均順位(同値は平均)
static vector<double> rank_avg(const double *v,int n)
{
    vector<int> idx(n);
    for(int i=0;i<n;i++)idx[i]=i;
    sort(idx.begin(),idx.end(),[v](int a,int b){return v[a]<v[b];});
    vector<double> r(n);
    int i=0;
    while(i<n){
        int j=i;
        while(j+1<n && v[idx[j+1]]==v[idx[i]])j++;
        double avg = (i+j)/2.0+1.0;
        for(int k=i;k<=j;k++)r[idx[k]]=avg;
        i=j+1;
    }
    return r;
}

// 基準信号(x)と信号(y)のSpearman順位相関と並べ替え検定のp値、MICの無相関95%点を求める
void Corr_data::calc_stats(int i,double *x,double *y,int n)
{
    sig[i].rho = 0;
    sig[i].pval = 1;
    sig[i].mic_null = 0;
    if(!stats_flg || n<4)return;

    vector<double> rx = rank_avg(x,n);
    vector<double> ry = rank_avg(y,n);
    double mean = (n+1)/2.0;
    double sx=0,sy=0,sxy=0;
    for(int k=0;k<n;k++){
        sx  += (rx[k]-mean)*(rx[k]-mean);
        sy  += (ry[k]-mean)*(ry[k]-mean);
        sxy += (rx[k]-mean)*(ry[k]-mean);
    }
    if(sx<=0 || sy<=0)return;   // どちらかが一定値
    double rho = sxy/sqrt(sx*sy);
    sig[i].rho = rho;

    const int B = 1000;
    std::mt19937 gen(12345);
    vector<double> yp(ry);
    int ge = 0;
    for(int b=0;b<B;b++){
        shuffle(yp.begin(),yp.end(),gen);
        double s = 0;
        for(int k=0;k<n;k++)s += (rx[k]-mean)*(yp[k]-mean);
        if(fabs(s/sqrt(sx*sy))>=fabs(rho)-1e-12)ge++;
    }
    sig[i].pval = (ge+1.0)/(B+1.0);
    sig[i].mic_null = mic_null95(n);
}

bool Corr_data::calc_importance(bool use_th,double th,vector<Importance> &out,double &oob_r2,int &nrows,int beg,int end)
{
    out.clear();
    oob_r2 = 0;
    nrows = 0;
    int p = (int)sig.size() - 1;
    if(p < 1 || size < 2)return false;

    // 使うショット: Threshold オンなら基準信号が閾値を超えたもの。値が有限でないものは除く
    vector<int> rows;
    bool useRange = end > beg;     // 時間範囲(calc_mic(beg,end)と同じ条件: 区間の端は含まない)
    struct tm tmv;
    for(int i=0;i<size;i++){
        if(useRange){
            memset(&tmv,0,sizeof(tmv));
            tmv.tm_isdst = -1;
            if(strptime(endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&tmv)==NULL)continue;
            time_t t = mktime(&tmv);
            if(!(beg < t && t < end))continue;
        }
        double y0 = value(sig[0].name,i);
        if(!std::isfinite(y0))continue;
        if(use_th && !(y0>th))continue;
        bool ok = true;
        for(int j=1;j<=p && ok;j++)ok = std::isfinite(value(sig[j].name,i));
        if(ok)rows.push_back(i);
    }
    int n = (int)rows.size();
    nrows = n;
    if(n < 20)return false;

    vector<double> y(n);
    vector<vector<double> > X(p,vector<double>(n));
    for(int k=0;k<n;k++){
        y[k] = value(sig[0].name,rows[k]);
        for(int j=0;j<p;j++)X[j][k] = value(sig[j+1].name,rows[k]);
    }
    if(detrend_flg){
        detrend(y.data(),n);
        for(int j=0;j<p;j++)detrend(X[j].data(),n);
    }

    ForestResult r = forest_importance(X,y);
    oob_r2 = r.oob_r2;

    vector<double> ry = rank_avg(y.data(),n);
    double mean = (n+1)/2.0;
    for(int j=0;j<p;j++){
        Importance im;
        im.name = sig[j+1].name;
        im.importance = r.importance[j];
        im.mic = sig[j+1].coefficient;
        vector<double> rx = rank_avg(X[j].data(),n);
        double sx=0,sy=0,sxy=0;
        for(int k=0;k<n;k++){
            sx  += (rx[k]-mean)*(rx[k]-mean);
            sy  += (ry[k]-mean)*(ry[k]-mean);
            sxy += (rx[k]-mean)*(ry[k]-mean);
        }
        im.rho = (sx>0 && sy>0) ? sxy/sqrt(sx*sy) : 0;
        out.push_back(im);
    }
    sort(out.begin(),out.end(),[](const Importance &a,const Importance &b){return a.importance>b.importance;});
    return true;
}

void Corr_data::update_list_order(const vector<string> &names)
{
    if(rank_mode!=RANK_LIST || sig.empty())return;
    for(size_t i=1;i<sig.size();i++){
        double score = -1e6 - (double)i;        // リストに無い信号は最後(元の並び順)
        for(size_t k=0;k<names.size();k++){
            if(names[k]==sig[i].name){score = -(double)k;break;}
        }
        sig[i].list_score = score;
    }
    sig[0].list_score = 1e30;       // 基準信号が常に先頭(0位)になるようにする
}

void Corr_data::update_gap(bool use_th,double th,double hl_th,int beg,int end)
{
    gap_valid = false;
    if(rank_mode!=RANK_GAP || sig.size()<2)return;
    for(size_t i=0;i<sig.size();i++)sig[i].gap = 0;

    // 使うショット(相関と同じ条件: Threshold、時間範囲)
    vector<int> rows;
    bool useRange = end > beg;
    struct tm tmv;
    for(int i=0;i<size;i++){
        if(useRange){
            memset(&tmv,0,sizeof(tmv));
            tmv.tm_isdst = -1;
            if(strptime(endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&tmv)==NULL)continue;
            time_t t = mktime(&tmv);
            if(!(beg < t && t < end))continue;
        }
        double y0 = value(sig[0].name,i);
        if(!std::isfinite(y0))continue;
        if(use_th && !(y0>th))continue;
        rows.push_back(i);
    }
    int n = (int)rows.size();
    if(n < 4)return;        // ショット不足: MIC順のまま

    // 閾値で2群に分ける(判定は Detrend 前の元の値で行う。散布図の縦線と同じ)
    vector<char> hi(n);
    int n_hi = 0;
    for(int k=0;k<n;k++){
        hi[k] = value(sig[0].name,rows[k]) >= hl_th ? 1 : 0;
        n_hi += hi[k];
    }
    int n_lo = n - n_hi;
    if(n_hi < 2 || n_lo < 2)return;

    vector<double> x(n);
    for(int j=1;j<(int)sig.size();j++){
        bool ok = true;
        for(int k=0;k<n && ok;k++){
            x[k] = value(sig[j].name,rows[k]);
            ok = std::isfinite(x[k]);
        }
        if(!ok)continue;
        if(detrend_flg)detrend(x.data(),n);
        double sum = 0,sum_hi = 0;
        for(int k=0;k<n;k++){
            sum += x[k];
            if(hi[k])sum_hi += x[k];
        }
        double mean = sum/n;
        double var = 0;
        for(int k=0;k<n;k++)var += (x[k]-mean)*(x[k]-mean);
        double sd = sqrt(var/n);
        if(sd<=0)continue;
        double m_hi = sum_hi/n_hi,m_lo = (sum-sum_hi)/n_lo;
        sig[j].gap = fabs(m_hi-m_lo)/sd;
    }
    sig[0].gap = 1e30;      // 基準信号が常に先頭(0位)になるようにする
    gap_valid = true;
}

void Corr_data::update_importance(bool use_th,double th,int beg,int end)
{
    imp_valid = false;
    if(rank_mode!=RANK_IMPORTANCE)return;
    vector<Importance> imp;
    double r2 = 0;
    int nrows = 0;
    for(size_t i=0;i<sig.size();i++)sig[i].importance = 0;
    if(!calc_importance(use_th,th,imp,r2,nrows,beg,end))return;   // ショット不足: MIC順のまま
    for(size_t k=0;k<imp.size();k++){
        for(size_t i=1;i<sig.size();i++){
            if(sig[i].name==imp[k].name)sig[i].importance = imp[k].importance;
        }
    }
    sig[0].importance = 1e30;   // 基準信号が常に先頭(0位)になるようにする
    imp_valid = true;
}

void Corr_data::set_self_stats()
{
    if(sig.empty())return;
    sig[0].coefficient = 1.0;   // 順位付けで常に先頭(0位)になるようにする
    sig[0].rho = 1.0;
    sig[0].pval = 0.0;
    sig[0].mic_null = 0.0;
}
// rows に挙げたショットだけを使って、基準信号 sig[0] と各信号のMICなどを計算する。
// 基準信号側の値(x)は全信号で共通なので1回だけ作る(Detrend も1回)。MINEは x,y を書き換えない。
void Corr_data::calc_mic_rows(const vector<int> &rows)
{
    if(sig.empty())return;
    set_self_stats();
    int n = (int)rows.size();
    if(n<2){
        for(int i=1;i<(int)sig.size();i++){
            sig[i].coefficient = 0;
            sig[i].rho = 0;
            sig[i].pval = 1;
            sig[i].mic_null = 0;
        }
        return;
    }
    vector<double> x(n),y(n);
    const string &name0 = sig[0].name;
    for(int k=0;k<n;k++)x[k] = value(name0,rows[k]);
    if(detrend_flg)detrend(x.data(),n);
    double alpha = mic_alpha(n);
    for(int i=1;i<(int)sig.size();i++){
        const string &nm = sig[i].name;
        for(int k=0;k<n;k++)y[k] = value(nm,rows[k]);
        if(detrend_flg)detrend(y.data(),n);
        MINE mine(alpha, 15, EST_MIC_APPROX);
        mine.compute_score(x.data(),y.data(),n);
        sig[i].coefficient = mine.mic();
        calc_stats(i,x.data(),y.data(),n);
    }
}

void Corr_data::calc_mic()
{
    vector<int> rows(size);
    for(int i=0;i<size;i++)rows[i] = i;
    calc_mic_rows(rows);
}

void Corr_data::calc_mic(double th)
{
    vector<int> rows;
    if(sig.empty())return;
    const string &name0 = sig[0].name;
    for(int i=0;i<size;i++){
        if(value(name0,i)>th)rows.push_back(i);
    }
    calc_mic_rows(rows);
}

void Corr_data::calc_mic(int beg,int end)
{
    vector<int> rows;
    struct tm buf;
    for(int i=0;i<size;i++){
        memset(&buf,0,sizeof(buf));
        buf.tm_isdst = -1;
        if(strptime(endtime(i).c_str(),"%Y/%m/%d %H:%M:%S",&buf)==NULL)continue;
        time_t t = mktime(&buf);
        if(beg < t && t < end)rows.push_back(i);
    }
    calc_mic_rows(rows);
}

// get_min/get_max と同じ値(最小/最大から比率 ratio だけ広げた範囲)を1回の走査で求める
void Corr_data::get_range(int n,double ratio,double &lo,double &hi)
{
    lo = hi = 0;
    if(size<1)return;
    const string &nm = sig[n].name;
    double mx = value(nm,0),mn = mx;
    for(int i=1;i<size;i++){
        double v = value(nm,i);
        if(mx<v)mx = v;
        if(mn>v)mn = v;
    }
    lo = mn - (mx - mn) * ratio;
    hi = mx + (mx - mn) * ratio;
}

void Corr_data::get_range(int n,double ratio,int a,double th,double &lo,double &hi)
{
    lo = hi = 0;
    if(size<1)return;
    const string &nm = sig[n].name;
    const string &na = sig[a].name;
    double mx = value(nm,0),mn = mx;
    for(int i=1;i<size;i++){
        if(value(na,i)>th){
            double v = value(nm,i);
            if(mx<v)mx = v;
            if(mn>v)mn = v;
        }
    }
    lo = mn - (mx - mn) * ratio;
    hi = mx + (mx - mn) * ratio;
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
    int ret = 0;        // 該当する順位が無いとき(信号の数より大きい順位など)は 0(基準信号)を返す
    if(flg == false && rank.size() == sig.size()){
        for(int i=0;i<(int)sig.size();i++){
            if(rank[i] == ranking)ret = i;
        }

    }else{
        ret = get_ranking(ranking);
    }
    if(ret<0 || ret>=(int)sig.size())ret = 0;       // 念のため範囲内に収める
    return ret;
}
int Corr_data::get_ranking(int ranking)
{
    int ret = 0;
    rank.resize((int)sig.size());

    for(int i=0;i<(int)sig.size();i++){
        rank[i] = 0;
        for(int j=0;j<(int)sig.size();j++){
            if(i!=j && score(i)<score(j)){
                rank[i]++;
            }else if(i>j && score(i)==score(j)){
                rank[i]++;
            }
        }
    }
    for(int i=0;i<(int)sig.size();i++){
        //DLOG<<"i="<<i<<" coeff = "<<sig[i].coefficient<<" rank = "<<rank[i]<<endl;
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
    ver++;
}

void Corr_data::erase(int n)
{
    if(n>(int)data.size())n = (int)data.size();
    if(n<=0)return;
    data.erase(data.begin(),data.begin()+n);    // 1回でまとめて詰める
    size-=n;
    ver++;
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
    if(!is_sorted(data.begin(),data.end()))sort(data.begin(),data.end());
    static int cnt = 1;

    // 基準になる時刻は、beg_time/end_time が入っている(有効な)エントリだけから取る。
    // get_tag で埋まらなかった空のエントリ(sacla_tag=0)は sort で先頭に来るため、除外しないと時刻が不定になる。
    string first_beg,last_end;
    for(size_t k=0;k<data.size();k++){
        if(!data[k].beg_time.empty()){first_beg = data[k].beg_time;break;}
    }
    for(size_t k=data.size();k>0;k--){
        if(!data[k-1].end_time.empty()){last_end = data[k-1].end_time;break;}
    }
    DLOG<<last_end.c_str()<<"  "<<first_beg.c_str()<<"  "<<strtime.c_str()<<endl;

    auto parse_time = [](const string &s,time_t &t)->bool{
        struct tm tmv;
        memset(&tmv,0,sizeof(tmv));
        tmv.tm_isdst = -1;
        if(strptime(s.c_str(),"%Y/%m/%d %H:%M:%S",&tmv)==NULL)return false;
        t = mktime(&tmv);
        return true;
    };
    auto format_time = [](time_t t)->string{
        char bufstr[256];
        strftime(bufstr,sizeof (bufstr),"%Y/%m/%d %H:%M:%S",localtime(&t));
        return string(bufstr);
    };

    time_t t_buf = 0,t_end = 0,t_beg = 0;
    if(!parse_time(strtime,t_buf))return 0;
    bool have_ref = !first_beg.empty() && !last_end.empty()
            && parse_time(first_beg,t_end) && parse_time(last_end,t_beg);
    DLOG<<t_beg<<"   "<<t_end<<"   "<<t_buf<<endl;

    if(!have_ref){
        // 有効なデータが1件もない: 直近600秒を探す
        begtime = format_time(t_buf-600);
        endtime = strtime;
    }else if(t_buf<t_end){
        begtime = strtime;
        endtime = first_beg;
        if(t_end-t_buf>600){
            begtime = format_time(t_end-600*cnt);
        }
        DLOG<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;
    }else if(t_buf>t_beg){
        begtime = last_end;
        endtime = strtime;
        if(t_buf-t_beg>600){
            endtime = format_time(t_beg+600*cnt);
        }
    }else{
        return 0;
    }

    DLOG<<begtime.c_str()<<"  "<<endtime.c_str()<<endl;

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
    int added = 0;  // 実際にdataへ追加できたショット数
    if(status==0 && get_inf_cnt_sep5u>0){
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            DLOG<<(get_inf_sep5u+i)->evno << "  " << (get_inf_sep5u+i)->tdata<<endl;
            db_unixtime_us_to_date((get_inf_sep5u+i)->unixtime_us,out);
            status = db_get_data_num_i(route.c_str(),out,60,&get_inf_route,&get_inf_cnt_route);
            if(status==0 && get_inf_cnt_route>0){
                append();
                // 除外されたショットがあっても位置がずれないよう、常に末尾に書き込む
                Data &d = data.back();
                for(int j=0;j<get_inf_cnt_route;j++){
                    if((get_inf_route+j)->idata==769){
                        d.sacla_tag=(get_inf_route+j)->evno;
                        db_unixtime_us_to_date((get_inf_route+get_inf_cnt_route-1)->unixtime_us,out);
                        d.beg_time = out;
                        db_unixtime_us_add_sec(get_inf_route->unixtime_us,2.0,&btime);
                        db_unixtime_us_to_date(btime,out);
                        d.end_time = out;
                        status = db_get_data_f(ssbt.c_str(),d.beg_time.c_str(),d.end_time.c_str(),&get_inf_ssbt,&get_inf_cnt_ssbt);
                        if(status == 0 && get_inf_cnt_ssbt>0){
                            for(int n=0;n<get_inf_cnt_ssbt;n++){
                                if((get_inf_ssbt+n)->fdata > 0.0){
                                    d.sr_tag = (get_inf_ssbt+n)->evno;
                                    break;
                                }
                            }
                            free(get_inf_ssbt);
                        }
                        break;
                    }
                }
                log_tag((get_inf_sep5u+i)->evno,d,
                        d.sr_tag==0 ? "(sr_tag not found: dropped, retried next time)" : "",
                        off_sacla,off_sr);
                if(d.sr_tag==0){
                    data.pop_back();
                    size-=1;
                }else{
                    added++;
                }
                free(get_inf_route);
            }else{
                cerr << "[tag] evno=" << (get_inf_sep5u+i)->evno << "  route data not found (skipped, retried next time)" << endl;
            }
        }
        free(get_inf_sep5u);
    }
    sort(data.begin(),data.end());
    if(added>0)ver++;
    if(added==0){
        DLOG<<added<<endl;
        cnt++;
    }else{
        cnt = 0;
    }
    return added;
}

void Corr_data::add_db_get_sync_data()
{
    int status;
    DB_SIGNAL_STRUCT siglist;
    DB_SIGNAL_DATA* tData;
    Db_f_data *fdata;

    // 値(value/denominator)の個数が増えたかでデータ更新を検出する
    auto stored = [this]()->size_t{
        size_t t = 0;
        for(size_t k=0;k<data.size();k++)t += data[k].value.size()+data[k].denominator.size();
        return t;
    };
    size_t stored_before = stored();

    // 値が足りないショットを、DB APIを呼ばずに先に調べる(足りなければ何も呼ばない)
    vector<int> need_sig,need_den;
    for(int n=0;n<size;n++){
        bool miss = false;
        for(size_t i=0;i<sig.size() && !miss;i++)miss = (data[n].value.count(sig[i].name)==0);
        if(miss)need_sig.push_back(n);
        miss = false;
        for(size_t i=0;i<sig_d.size() && !miss;i++)miss = (data[n].denominator.count(sig_d[i].name)==0);
        if(miss)need_den.push_back(n);
    }
    if(need_sig.empty() && need_den.empty())return;

    for(size_t q=0;q<need_sig.size();q++){
        int n = need_sig[q];
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
                                if(fdata[m].evno == eff_sacla(n)||
                                        fdata[m].evno == eff_sr(n)){
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

    for(size_t q=0;q<need_den.size();q++){
        int n = need_den[q];
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
                                if(fdata[m].evno == eff_sacla(n)||
                                        fdata[m].evno == eff_sr(n)){
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
    if(stored()!=stored_before)ver++;
}
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <regex>

void Corr_data::outfile(string fname)
{
    QDir().mkpath(QFileInfo(fname.c_str()).absolutePath());     // 保存先ディレクトリが無ければ作る
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
        out << Qt::endl;
        for(int i=0;i<size;i++){
            out << data[i].sacla_tag<<",";
            out << data[i].sr_tag<<",";
            string bt = data[i].beg_time, et = data[i].end_time;
            replace(bt.begin(),bt.end(),' ','-');
            replace(et.begin(),et.end(),' ','-');
            out << bt.c_str()<<",";
            out << et.c_str()<<",";
            for(int j=0;j<(int)sig_d.size();j++){
                out<<data[i].value[sig[0].name]/data[i].denominator[sig_d[j].name]*100<<",";
            }
            for(int j=0;j<(int)sig.size();j++){
                out << data[i].value[sig[j].name]<<",";
            }
            for(int j=0;j<(int)sig_d.size();j++){
                out << data[i].denominator[sig_d[j].name]<<",";
            }
            out << Qt::endl;
        }
        fp.close();
    }else{
        qWarning().noquote()<<"Cannot open file  "<<fname.c_str()<<Qt::endl;
    }

}
