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

Corr_data::Corr_data(int data_num)
{
    size = data_num;
    data.resize(size);
    Signal_param s = {target_name_virtual(),0,0,1,0,0,0,0};     // sig[0]: 基準信号の合成値(仮想)
    sig.push_back(s);
}

Corr_data::~Corr_data(){

}
// タグが決まらなかったショット(route 769 が無い: 時間窓が空、または sr_tag が無い)を捨てる。
// 残すと、時刻が不定(エポック)になってトレンドグラフの横軸が何十年にも広がり、
// 信号の値もすべて 0 になって、散布図の (0,0) に点が出てしまう。自動更新(add_tag)も、同じ理由でこのようなショットは捨てている
void Corr_data::drop_untagged()
{
    int before = (int)data.size();
    data.erase(remove_if(data.begin(),data.end(),[](const Data &d){
        return d.sacla_tag==0 || d.sr_tag==0 || d.beg_time.empty() || d.end_time.empty();
    }),data.end());
    int dropped = before - (int)data.size();
    if(dropped>0){
        size = (int)data.size();
        ver++;
        cerr << "[get] dropped " << dropped << " shot(s) whose sacla_tag/sr_tag could not be determined (see [tag] lines with 'not found')" << endl;
    }
}
void Corr_data::get_tag(string endtime)
{
    string sep5u = "sr_mon_screen_sep5u_otr/intensity";
    string route = "xfel_tmg_sw_master_ema_route_inf_shm/current";
    string ssbt = "sr_safety_topup_intlk_beam_ssbt1/charge";
    Db_i_data *get_inf_route;
    Db_f_data *get_inf_ssbt;
    int get_inf_cnt_sep5u = 0,get_inf_cnt_route = 0,get_inf_cnt_ssbt = 0;
    int status;
    char out[256];
    uint64_t btime;

    // ショットの検索: endtime 以前の最新 size 件(新しい順)を、100件ずつに分けてさかのぼって取る。
    // 一度に多く(300件など)要求すると、DB API が失敗することがあるため。DB の応答は、ターミナルに出す
    vector<Db_f_data> shots;
    {
        const int CHUNK = 100;
        string cur_end = endtime;
        int remaining = (int)data.size();
        uint64_t last_oldest = ~(uint64_t)0;            // これまでに取ったショットのうち、いちばん古い時刻(us)
        while(remaining>0){
            int req = min(CHUNK,remaining);
            Db_f_data *arr = NULL;
            int cnt = 0;
            status = db_get_data_num_f(sep5u.c_str(),cur_end.c_str(),req,&arr,&cnt);
            cerr << "[get] " << sep5u << "  end=" << cur_end << "  num=" << req
                 << "  -> status=" << status << "  count=" << cnt << endl;
            if(status!=0 || cnt<=0 || arr==NULL)break;
            if(cnt>req)cnt = req;                       // 要求より多く返されても、使うのは要求した分だけ
            int pushed = 0;
            for(int k=0;k<cnt;k++){
                // 前のまとまりと重ならないように、これまでより古いショットだけを取る
                if((uint64_t)arr[k].unixtime_us < last_oldest){
                    shots.push_back(arr[k]);
                    pushed++;
                }
            }
            free(arr);
            if(pushed==0)break;
            remaining -= pushed;
            last_oldest = (uint64_t)shots.back().unixtime_us;
            if(cnt<req)break;                           // もう古いデータが無い
            // 次は、いちばん古かったショットの少し前から。時刻の文字列はミリ秒までなので、切り捨てでも
            // 四捨五入でも、同じショットをもう一度拾わないように、5 ms 引いておく
            db_unixtime_us_to_date(last_oldest-5000,out);
            cur_end = out;
        }
    }
    found_shots = (int)shots.size();
    get_inf_cnt_sep5u = found_shots;
    if(found_shots==0){
        cerr << "[get] WARNING: no shots were found for " << sep5u << " (see the status/count above)" << endl;
    }else if(found_shots<(int)data.size()){
        cerr << "[get] only " << found_shots << " of " << data.size() << " requested shots are available; the rest is dropped" << endl;
        data.resize(found_shots);                       // 空のエントリが残らないように、見つかった分だけにする
        size = found_shots;
    }
    if(get_inf_cnt_sep5u>0){
        for(int i=0;i<get_inf_cnt_sep5u;i++){
            DLOG<<shots[i].evno << "  " << shots[i].tdata<<endl;
            db_unixtime_us_to_date(shots[i].unixtime_us,out);
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
            log_tag(shots[i].evno,data[i],
                    data[i].sacla_tag==0 ? "(route 769 not found)" : (data[i].sr_tag==0 ? "(sr_tag not found)" : ""),
                    off_sacla,off_sr);
            report(0,i+1,get_inf_cnt_sep5u);
        }
    }
    drop_untagged();
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
            report(0,i+1,get_inf_cnt_sep5u);
        }
        free(get_inf_sep5u);
    }
    drop_untagged();
    sort(data.begin(),data.end());
}

// ===== 基準信号(Target) =====
vector<string> Corr_data::fetch_names()
{
    vector<string> v;
    for(const TSig &t : tsig)v.push_back(t.name);
    for(size_t i=1;i<sig.size();i++){
        if(find_if(tsig.begin(),tsig.end(),[&](const TSig &t){return t.name==sig[i].name;})==tsig.end())v.push_back(sig[i].name);
    }
    return v;
}

// DB に登録できなかった信号を捨てる
void Corr_data::drop_signal(const string &name)
{
    for(size_t i=1;i<sig.size();i++)if(sig[i].name==name){sig.erase(sig.begin()+i);break;}
    for(size_t i=0;i<tsig.size();i++)if(tsig[i].name==name){tsig.erase(tsig.begin()+i);tstats_valid = false;break;}
}

// db_get_data_multi の結果から、ショット n の時間窓に対応するイベントの値を data[n].value に入れる
void Corr_data::store_values(int n,DB_SIGNAL_STRUCT &siglist)
{
    for(int j=0;j<siglist.num_signal;j++){
        DB_SIGNAL_DATA *tData = &siglist.signal_list[j];
        const string name = siglist.signal_list[j].sig_name;
        Db_f_data *fdata = (Db_f_data*)tData->data;
        for(int m=0;m<tData->num_data;m++){
            if(fdata[m].evno == eff_sacla(n) || fdata[m].evno == eff_sr(n)){
                if((fdata[m].fdata>1e8||name.find("sr_safety")!=std::string::npos)&&name.find("efficiency")==std::string::npos){
                    data[n].value[name] = fdata[m].fdata*1.60219e-10;
                }else{
                    data[n].value[name] = fdata[m].fdata;
                }
            }
        }
    }
}

bool Corr_data::target_ready(int n)
{
    if(tsig.empty())return false;
    for(const TSig &t : tsig)if(data[n].value.count(t.name)==0)return false;
    return true;
}

int Corr_data::eff_norm()
{
    if(normalize_mode==NORM_AUTO)return tsig.size()==1 ? NORM_NONE : NORM_ZSCORE;
    return normalize_mode;
}

string Corr_data::target_label()
{
    if(tsig.empty())return "(no target)";
    if(tsig.size()==1 && eff_norm()==NORM_NONE && tsig[0].weight==1.0)return tsig[0].name;
    if(tsig.size()==1 && eff_norm()==NORM_NONE)return boost::str(boost::format("%g x %s") % tsig[0].weight % tsig[0].name);
    return string("Target (") + (combine_mode==COMBINE_MIN ? "min" : "sum") + (eff_norm()==NORM_ZSCORE ? " of z-scores)" : ")");
}

// Get 時点の平均・標準偏差(基準信号ごと)と、min 合成の best(s_k·z_k の最大値)を求めて固定する
void Corr_data::compute_target_stats()
{
    const int nt = (int)tsig.size();
    tmean.assign(nt,0.0);  tstd.assign(nt,1.0);  tbest.assign(nt,0.0);
    vector<int> rows;
    for(int n=0;n<(int)data.size();n++)if(target_ready(n))rows.push_back(n);
    if(rows.empty()){tstats_valid = false;return;}
    for(int k=0;k<nt;k++){
        double s = 0;
        for(int n : rows)s += data[n].value[tsig[k].name];
        double mean = s/rows.size(),var = 0;
        for(int n : rows){double d = data[n].value[tsig[k].name]-mean;var += d*d;}
        double sd = sqrt(var/rows.size());
        tmean[k] = mean;
        tstd[k] = sd>0 ? sd : 1.0;
        double best = -1e300;
        for(int n : rows){
            double z = eff_norm()==NORM_ZSCORE ? (data[n].value[tsig[k].name]-mean)/tstd[k] : data[n].value[tsig[k].name];
            double u = (tsig[k].weight<0 ? -1.0 : 1.0)*z;
            if(u>best)best = u;
        }
        tbest[k] = best;
    }
    tstats_valid = true;
}

// すべてのショットの合成値(sig[0]=TARGET の値)を計算し直す
void Corr_data::recompute_target()
{
    const int nt = (int)tsig.size();
    if(nt==0)return;
    if(!tstats_valid)compute_target_stats();
    if(!tstats_valid)return;
    const bool zs = eff_norm()==NORM_ZSCORE;
    const bool use_min = combine_mode==COMBINE_MIN && nt>1;
    const string tn = target_name_virtual();
    for(int n=0;n<(int)data.size();n++){
        if(!target_ready(n)){data[n].value.erase(tn);continue;}
        double sum = 0,mn = 1e300;
        for(int k=0;k<nt;k++){
            double w = tsig[k].weight;
            if(w==0)continue;
            double x = data[n].value[tsig[k].name];
            double z = zs ? (x-tmean[k])/tstd[k] : x;
            sum += w*z;
            double u = (w<0 ? -1.0 : 1.0)*z;
            mn = min(mn,fabs(w)*(u-tbest[k]));
        }
        data[n].value[tn] = use_min ? (mn<1e299 ? mn : 0.0) : sum;
    }
    ver++;
}

bool Corr_data::set_targets(const vector<Target> &t,int combine,int normalize)
{
    bool changed = (combine!=combine_mode || normalize!=normalize_mode || t.size()!=tsig.size());
    for(size_t k=0;!changed && k<t.size();k++)changed = (t[k].name!=tsig[k].name || t[k].weight!=tsig[k].weight);
    if(!changed)return false;
    combine_mode = combine;
    normalize_mode = normalize;
    tsig.clear();
    for(const Target &x : t)tsig.push_back({x.name,x.weight});
    for(size_t i=sig.size();i-->1;){        // 基準信号になった信号は、説明変数から外す
        for(const TSig &x : tsig)if(x.name==sig[i].name){sig.erase(sig.begin()+i);break;}
    }
    tstats_valid = false;
    recompute_target();         // 値がまだ無ければ何もしない(Get / 自動更新で値が入ってから計算する)
    return true;
}

void Corr_data::db_get_sync_data()
{
    int status;
    DB_SIGNAL_STRUCT siglist;

    const vector<string> names = fetch_names();
    status = db_init_signal_list(&siglist);
    if(status == 0){
        for(const string &name : names){
            status = db_add_signal_list(&siglist,name.c_str());
            if(status != 0){
                printf("err db_add_signal_list :: %s\n",name.c_str());
                drop_signal(name);
            }
        }
    }
    DLOG<<"num_siglist  =  "<<siglist.num_signal<<endl;
    for(int n=0;n<size;n++){
        status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
        if(status == 0)store_values(n,siglist);
        report(1,n+1,size);
    }
    db_release_signal_list_all(&siglist);
    sort(data.begin(),data.end());

    tstats_valid = false;       // Get 時点の平均・標準偏差を、ここで固定する
    recompute_target();
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

    // 値(value)の個数が増えたかでデータ更新を検出する
    auto stored = [this]()->size_t{
        size_t t = 0;
        for(size_t k=0;k<data.size();k++)t += data[k].value.size();
        return t;
    };
    size_t stored_before = stored();

    // 値が足りないショットを、DB APIを呼ばずに先に調べる(足りなければ何も呼ばない)
    const vector<string> names = fetch_names();
    vector<int> need_sig;
    for(int n=0;n<size;n++){
        bool miss = false;
        for(size_t i=0;i<names.size() && !miss;i++)miss = (data[n].value.count(names[i])==0);
        if(miss)need_sig.push_back(n);
    }

    for(size_t q=0;q<need_sig.size();q++){
        int n = need_sig[q];
        status = db_init_signal_list(&siglist);
        if(status == 0){
            for(const string &name : names){
                if(data[n].value.count(name)==0){
                    status = db_add_signal_list(&siglist,name.c_str());
                    if(status != 0){
                        printf("err db_add_signal_list :: %s\n",name.c_str());
                        drop_signal(name);
                    }
                }
            }
        }
        if(siglist.num_signal>0){
            status = db_get_data_multi(&siglist,data[n].beg_time.c_str(),data[n].end_time.c_str(),1,0);
            if(status == 0)store_values(n,siglist);
            db_release_signal_list_all(&siglist);
        }
    }
    // 新しく値が入ったショットの合成値を計算する(平均・標準偏差は Get 時点のまま)
    if(!need_sig.empty() || !tstats_valid)recompute_target();
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
        for(int j=0;j<(int)sig.size();j++){
            out << sig[j].name.c_str()<<",";
        }
        for(int j=0;j<(int)tsig.size();j++){
            out << tsig[j].name.c_str()<<",";
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
            for(int j=0;j<(int)sig.size();j++){
                out << data[i].value[sig[j].name]<<",";
            }
            for(int j=0;j<(int)tsig.size();j++){
                out << data[i].value[tsig[j].name]<<",";
            }
            out << Qt::endl;
        }
        fp.close();
    }else{
        qWarning().noquote()<<"Cannot open file  "<<fname.c_str()<<Qt::endl;
    }

}
