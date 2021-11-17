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

typedef struct{
    int64_t sacla_tag;
    int64_t sr_tag;
    string beg_time;
    string end_time;
    map <string,double>value;
}Data;

typedef struct{
    string name;
    double coefficient;
    int priority;
}Signal_param;

class Corr_data
{
public:
    Corr_data(int data_num);
    ~Corr_data();
    void get_tag(string endtime);
    void get_tag(string begtime,string endtime);
    void add_signal(string signal){sig.push_back({signal,0,0});};
    void db_get_sync_data();
    int data_size(){return size;};
    int sig_size(){return sig.size();};
    string sig_name(int n){return sig[n].name;};
    double value(string signame,int n){return data[n].value[signame];};
    string begtime(int n){return data[n].beg_time;};
    string endtime(int n){return data[n].end_time;};
    int64_t sacla_tag(int n){return data[n].sacla_tag;};
    int64_t sr_tag(int n){return data[n].sr_tag;};
    void calc_mic();
    double mic(int n){return sig[n].coefficient;};
private:
    int size;
    vector <Signal_param> sig;
    vector<Data> data;
};

#endif // CORR_DATA_H
