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

using namespace std;

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
    bool operator==(const Signal_param &other) const {
        return name == other.name;
    }
}Signal_param;

class Corr_data
{
public:
    Corr_data(int data_num);
    ~Corr_data();
    void get_tag(string endtime);
    void get_tag(string begtime,string endtime);
    void add_signal(string signal){
        Signal_param s = {signal,0};
        if(find(sig.begin(),sig.end(),s)==sig.end()){
        sig.push_back(s);
        }
    };
    void add_signal_d(string signal){
        Signal_param s = {signal,0};
        if(find(sig_d.begin(),sig_d.end(),s)==sig_d.end()){
        sig_d.push_back(s);
        }
    };
    void db_get_sync_data();
    int data_size(){return size;};
    int sig_size(){return sig.size();};
    string sig_name(int n){return sig[n].name;};
    string sig_d_name(int n){return sig_d[n].name;};
    double value(string signame,int n){return data[n].value[signame];};
    double denominator(string signame,int n){return data[n].denominator[signame];};
    string begtime(int n){return data[n].beg_time;};
    string endtime(int n){return data[n].end_time;};
    int64_t sacla_tag(int n){return data[n].sacla_tag;};
    int64_t sr_tag(int n){return data[n].sr_tag;};
    void calc_mic();
    void calc_mic(double th);
    double mic(int n){return sig[n].coefficient;};
    double get_max(int n);
    double get_min(int n);
    double get_max(int n,double ratio);
    double get_min(int n,double ratio);
    double get_max(int n,double ratio,int a,double th);
    double get_min(int n,double ratio,int a,double th);
    int get_ranking(int ranking);
    int get_ranking(int ranking,bool flg);
    void append();
    void erase();
    void erase(int n);
    int add_tag(string strtime);
    void add_db_get_sync_data();
    void rank_init(){rank.resize(0);};
private:
    int size;
    vector <Signal_param> sig;
    vector <Signal_param> sig_d;
    vector<Data> data;
    vector<int> rank;
};

#endif // CORR_DATA_H
