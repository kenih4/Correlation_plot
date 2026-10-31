#ifndef CORR_DATA_H
#define CORR_DATA_H
#include <vector>
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
    map <string,double>denominator;
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
    Corr_data(int data_num,int f);
    ~Corr_data();
    void get_tag(string endtime);
    void get_tag(string begtime,string endtime);
    void add_signal(string signal){
        Signal_param s = {signal,0,0,1,0,0,0,0};
        if(find(sig.begin(),sig.end(),s)==sig.end()){
            sig.push_back(s);
        }
    };
    void add_signal_d(string signal){
        Signal_param s = {signal,0,0,1,0,0,0,0};
        if(find(sig_d.begin(),sig_d.end(),s)==sig_d.end()){
            sig_d.push_back(s);
        }
    };
    void db_get_sync_data();
    int data_size(){return size;};
    int sig_size(){return sig.size();};
    // 範囲外の番号でも落ちないようにする(空の名前を返す)
    const string &sig_name(int n){return (n>=0 && n<(int)sig.size()) ? sig[n].name : empty_name;};
    const string &sig_d_name(int n){return (n>=0 && n<(int)sig_d.size()) ? sig_d[n].name : empty_name;};
    double value(const string &signame,int n){
        if(n<0 || n>=(int)data.size())return 0.0;       // ショット番号が範囲外
        // Calc efficiency(dflg)は、基準信号を分母信号(sig_d の先頭)で割る。信号や分母が無ければ元の値を返す
        if(dflg != 0 && !sig.empty() && !sig_d.empty() && signame==sig[0].name){
            return data[n].value[signame]/data[n].denominator[sig_d[0].name]*100;
        }else{
            return data[n].value[signame];
        }
    };
    double denominator(string signame,int n){return data[n].denominator[signame];};
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
    bool rank_importance(){return rank_mode==RANK_IMPORTANCE && imp_valid;};
    bool rank_gap(){return rank_mode==RANK_GAP && gap_valid;};
    void update_importance(bool use_th,double th,int beg=-1,int end=-1);
    // 基準信号の値が hl_th 以上/未満の2群に分け、各信号の値の平均の差(標準偏差で割る)を求める
    void update_gap(bool use_th,double th,double hl_th,int beg=-1,int end=-1);
    double gap(int n){return sig[n].gap;};
    // Plot Signal リストの並び(上から順の信号名)を渡す。リストに無い信号は最後になる
    void update_list_order(const vector<string> &names);
    double importance(int n){return sig[n].importance;};
    void set_flg(int f){dflg = f;DLOG<<dflg<<endl;};
    void outfile(string);
private:
    static double mic_alpha(int n);
    static double mic_null95(int n);
    void calc_stats(int i,double *x,double *y,int n);
    int size;
    vector <Signal_param> sig;
    vector <Signal_param> sig_d;
    vector<Data> data;
    vector<int> rank;
    int dflg;
    unsigned ver = 0;
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
    bool detrend_flg = false;   // 相関を取る前に移動平均を引いてゆっくりした変動を除くか
    bool stats_flg = false;    // rho / p値 / MICの無相関95%点を計算するか
};

#endif // CORR_DATA_H
