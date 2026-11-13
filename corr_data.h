#ifndef CORR_DATA_H
#define CORR_DATA_H
#include <vector>
#include <functional>
#include <algorithm>
#include <iostream>
#include <string.h>
#include <boost/format.hpp>
#include <map>
#include "db_common.h"
#include <cstdlib>
#include <cmath>
#include "cppmine.h"
#include "forest.h"

using namespace std;

// 環境変数 CORR_DEBUG が設定されているときだけ詳細ログを出す(出さないときは引数を評価しない)
inline bool corr_debug(){
    static const bool d = getenv("CORR_DEBUG")!=NULL;
    return d;
}
#define DLOG if(!corr_debug()){}else cerr

typedef struct Data{
    int64_t sacla_tag;
    int64_t sr_tag;
    string beg_time;
    string end_time;
    map <string,double>value;
    bool operator<(const Data &right)const {
        return sacla_tag == right.sacla_tag ? sr_tag < right.sr_tag : sacla_tag < right.sacla_tag;
    }
}Data;

typedef struct Signal_param{
    string name;
    double coefficient;
    double rho;         // Spearman順位相関係数(基準信号との間。符号付き)
    double pval;        // rho の並べ替え検定のp値
    double mic_null;    // 無相関のときのMICの95パーセンタイル(同じデータ数)
    double importance;  // ランダムフォレストの重要度(基準信号は最大値)。重要度順の表示のときだけ計算する
    double gap;         // 閾値以上/未満の2群の平均の差 / 標準偏差(基準信号は最大値)。平均の差順の表示のときだけ計算する
    double list_score;  // Plot Signal リストの順番による得点(上にあるほど大きい。基準信号は最大値)。リスト順の表示のときだけ使う
    bool operator==(const Signal_param &other) const {
        return name == other.name;
    }
}Signal_param;

typedef struct Importance{
    string name;
    double importance;  // permutation importance (yの分散に対する割合)
    double rho;         // 基準信号とのSpearman順位相関(向きの目安)
    double mic;         // 基準信号とのMIC
}Importance;

class Corr_data
{
public:
    Corr_data(int data_num);
    ~Corr_data();
    void get_tag(string endtime);
    void drop_untagged();       // タグが決まらなかったショットを捨てる
    void get_tag(string begtime,string endtime);
    struct TSig{string name;double weight;};   // 基準信号そのもの(DB から取る)
    // 説明変数を追加する(基準信号と同じ名前のものは、相関を見る意味がないので追加しない)
    void add_signal(string signal){
        if(signal==target_name_virtual())return;
        for(const TSig &t : tsig)if(t.name==signal)return;
        Signal_param s = {signal,0,0,1,0,0,0,0};
        if(find(sig.begin(),sig.end(),s)==sig.end()){
            sig.push_back(s);
        }
    };
    // ===== 基準信号(Target) =====
    // 基準信号は複数にでき、それぞれに重みを付ける。sig[0] は、それらを合成した値を持つ仮想の信号(TARGET_NAME。DB からは取らない)で、
    // MIC・Highlight・トレンドなど、これまで sig[0] を基準信号として使っていた処理は、そのままこの合成値を使う。
    // sig[1..] は説明変数(Plot Signal リスト)。基準信号そのもの(tsig)は、相関ランキングには出ない。
    //   合成: sum = Σ w_k·z_k      min = min_k |w_k|·(s_k·z_k − best_k)  (s_k=sign(w_k)、best_k は s_k·z_k の最大値。重み付きチェビシェフ)
    //   z_k : 正規化した値。平均と標準偏差は Get 時点のもの(自動更新で変わらない)
    enum Combine{COMBINE_SUM=0,COMBINE_MIN=1};
    enum Normalize{NORM_AUTO=0,NORM_ZSCORE=1,NORM_NONE=2};   // AUTO: 基準信号が1つなら正規化しない(従来と同じ値)、2つ以上なら z スコア
    struct Target{string name;double weight;};
    static const char *target_name_virtual(){return "[Target]";}
    // 基準信号のリストと合成の方法を設定する。変わったときは(データがあれば)合成値を計算し直し、true を返す
    bool set_targets(const vector<Target> &t,int combine,int normalize);
    int target_count(){return (int)tsig.size();}
    const string &target_name(int k){return (k>=0 && k<(int)tsig.size()) ? tsig[k].name : empty_name;}
    double target_weight(int k){return (k>=0 && k<(int)tsig.size()) ? tsig[k].weight : 0.0;}
    string target_label();      // 合成値の表示名(基準信号が1つで重み1、正規化なしなら、その信号名そのもの)
    void db_get_sync_data();
    int data_size(){return size;};
    int sig_size(){return sig.size();};
    // 範囲外の番号でも落ちないようにする(空の名前を返す)
    const string &sig_name(int n){return (n>=0 && n<(int)sig.size()) ? sig[n].name : empty_name;};
    double value(const string &signame,int n){
        if(n<0 || n>=(int)data.size())return 0.0;       // ショット番号が範囲外
        if(signame==target_name_virtual()){             // 合成値は、まだ無いときに 0 を作って残さない
            auto it = data[n].value.find(signame);
            return it!=data[n].value.end() ? it->second : 0.0;
        }
        return data[n].value[signame];
    };
    string begtime(int n){return data[n].beg_time;};
    string endtime(int n){return data[n].end_time;};
    int64_t sacla_tag(int n){return data[n].sacla_tag;};
    int64_t sr_tag(int n){return data[n].sr_tag;};
    void calc_mic();
    void calc_mic(double th);
    void calc_mic(int beg,int end);
    double mic(int n){return sig[n].coefficient;};
    double rho(int n){return sig[n].rho;};
    double pval(int n){return sig[n].pval;};
    double mic_null(int n){return sig[n].mic_null;};
    double get_max(int n);
    double get_min(int n);
    double get_max(int n,double ratio);
    double get_min(int n,double ratio);
    double get_max(int n,double ratio,int a,double th);
    // get_min/get_max と同じ値を、1回の走査でまとめて返す
    void get_range(int n,double ratio,double &lo,double &hi);
    void get_range(int n,double ratio,int a,double th,double &lo,double &hi);
    double get_min(int n,double ratio,int a,double th);
    int get_ranking(int ranking);
    int get_ranking(int ranking,bool flg);
    void append();
    void erase();
    void erase(int n);
    int add_tag(string strtime);
    void add_db_get_sync_data();
    void rank_init(){rank.resize(0);};
    // タグのオフセット: 各信号のデータ点を選ぶとき、タグ(sacla_tag, sr_tag)にこの値を足したイベント番号と照合する。
    // タグが見つかっていない(0)ショットには足さない。タグ自体の決定(get_tag, add_tag)は変えない
    void set_tag_offset(long long sacla,long long sr){off_sacla = sacla;off_sr = sr;};
    long long tag_offset_sacla(){return off_sacla;};
    long long tag_offset_sr(){return off_sr;};
    // 取得の進み具合を知らせるコールバック(Get の進捗表示用)。progress(段階, 済んだ数, 全体の数)
    //   段階 0: タグの決定(ショットごと)、1: 信号の値の取得(ショットごと)
    void set_progress(std::function<void(int,int,int)> f){progress_cb = f;};
    int tag_count(){return found_shots;};      // 直近の get_tag で見つかったショット数(0 なら、DB から1件も取れなかった)
    unsigned data_version(){return ver;};   // データが変わるたびに増える(変化の検出用)
    void set_stats(bool f){stats_flg = f;};
    void set_detrend(bool f){detrend_flg = f;};
    static void detrend(double *v,int n);
    // 基準信号 sig[0] を目的変数、他の全信号を説明変数としてランダムフォレストの重要度を求める。
    // 相関と同じ条件(Threshold、Detrend)でショットを選ぶ。成功したら true。
    bool calc_importance(bool use_th,double th,vector<Importance> &out,double &oob_r2,int &nrows,int beg=-1,int end=-1);
    // 散布図の並び順: MIC順 / 重要度順 / 閾値で分けた2群の平均の差順 / Plot Signal リストの順
    enum RankMode{RANK_MIC=0,RANK_IMPORTANCE=1,RANK_GAP=2,RANK_LIST=3};
    void set_rank_mode(int m){rank_mode = m;imp_valid = false;gap_valid = false;};
    // MIC(と rho / p値)の計算は重いので、並び順が MIC か Importance のときだけ行う(Gap / List のときは省く)。
    // Importance ダイアログなど、MIC が必要な処理では set_mic_force(true) で一時的に計算させる
    bool mic_enabled(){return rank_mode==RANK_MIC || rank_mode==RANK_IMPORTANCE || mic_force;}
    void set_mic_force(bool f){mic_force = f;}
    bool rank_importance(){return rank_mode==RANK_IMPORTANCE && imp_valid;};
    bool rank_gap(){return rank_mode==RANK_GAP && gap_valid;};
    void update_importance(bool use_th,double th,int beg=-1,int end=-1);
    // 基準信号の値が hl_th 以上/未満の2群に分け、各信号の値の平均の差(標準偏差で割る)を求める
    void update_gap(bool use_th,double th,double hl_th,int beg=-1,int end=-1);
    double gap(int n){return sig[n].gap;};
    // Plot Signal リストの並び(上から順の信号名)を渡す。リストに無い信号は最後になる
    void update_list_order(const vector<string> &names);
    double importance(int n){return sig[n].importance;};
    // 信号どうしの相関行列(行優先 N×N)。use_mic=false: Spearman(符号付き)、true: MIC。cb が false を返したら中止(空を返す)
    vector<double> corr_matrix(const vector<string> &names,bool use_mic,std::function<bool(int,int)> cb);
    int used_shots(){int k = 0;for(const Data &d : data)if(!d.beg_time.empty())k++;return k;}
    void outfile(string);
private:
    static double mic_alpha(int n);
    static double mic_null95(int n);
    void calc_stats(int i,double *x,double *y,int n);
    int size;
    vector <Signal_param> sig;      // sig[0] は合成した基準値(仮想)、sig[1..] は説明変数
    vector <TSig> tsig;             // 基準信号そのもの(DB から取る)
    int combine_mode = COMBINE_SUM;
    int normalize_mode = NORM_AUTO;
    bool tstats_valid = false;
    vector<double> tmean,tstd,tbest;    // 基準信号ごとの、Get 時点の平均・標準偏差と、min 合成の best
    vector<string> fetch_names();       // DB から取る信号名(基準信号 + 説明変数)
    void drop_signal(const string &name);
    void store_values(int n,DB_SIGNAL_STRUCT &siglist);
    bool target_ready(int n);           // ショット n に、すべての基準信号の値があるか
    int eff_norm();
    void compute_target_stats();
    void recompute_target();
    vector<Data> data;
    vector<int> rank;
    unsigned ver = 0;
    int found_shots = 0;
    std::function<void(int,int,int)> progress_cb;
    void report(int stage,int done,int total){if(progress_cb)progress_cb(stage,done,total);}
    string empty_name;
    long long off_sacla = 0;    // sacla_tag のオフセット
    long long off_sr = 0;       // sr_tag のオフセット
    long long eff_sacla(int n){return data[n].sacla_tag!=0 ? data[n].sacla_tag+off_sacla : 0;};   // オフセット込みで照合するタグ
    long long eff_sr(int n){return data[n].sr_tag!=0 ? data[n].sr_tag+off_sr : 0;};
    void calc_mic_rows(const vector<int> &rows);
    void set_self_stats();      // 基準信号自身の指標(MIC=1など)。計算せず固定値にする
    int rank_mode = RANK_MIC;   // 順位の決め方
    bool gap_valid = false;     // 平均の差が計算できているか
    bool imp_valid = false;     // 重要度が計算できているか(データ不足のときはMIC順に戻す)
    double score(int i){
        if(rank_mode==RANK_IMPORTANCE && imp_valid)return sig[i].importance;
        if(rank_mode==RANK_GAP && gap_valid)return sig[i].gap;
        if(rank_mode==RANK_LIST)return sig[i].list_score;
        return sig[i].coefficient;
    }
    bool mic_force = false;
    bool detrend_flg = false;   // 相関を取る前に移動平均を引いてゆっくりした変動を除くか
    bool stats_flg = false;    // rho / p値 / MICの無相関95%点を計算するか
};

#endif // CORR_DATA_H
