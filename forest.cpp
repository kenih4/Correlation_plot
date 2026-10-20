#include "forest.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace {

typedef std::vector<std::vector<double> > Matrix;

struct Node{
    int feat;       // 分割に使う変数 (-1 なら葉)
    double thr;     // x <= thr なら left
    int left;
    int right;
    double val;     // 葉の予測値
    Node() : feat(-1), thr(0), left(-1), right(-1), val(0) {}
};

class Tree{
public:
    Tree(const Matrix &X, const std::vector<double> &y, int mtry, int minLeaf, int maxDepth)
        : X_(X), y_(y), mtry_(mtry), minLeaf_(minLeaf), maxDepth_(maxDepth) {}

    void build(std::vector<int> idx, std::mt19937 &gen){
        nodes_.clear();
        grow(idx, 0, gen);
    }

    double predict(const Matrix &X, int i) const{
        int k = 0;
        while(nodes_[k].feat >= 0){
            k = (X[nodes_[k].feat][i] <= nodes_[k].thr) ? nodes_[k].left : nodes_[k].right;
        }
        return nodes_[k].val;
    }

private:
    int grow(std::vector<int> &idx, int depth, std::mt19937 &gen){
        int self = (int)nodes_.size();
        nodes_.push_back(Node());

        int n = (int)idx.size();
        double sum = 0, sumsq = 0;
        for(int k = 0; k < n; k++){
            double v = y_[idx[k]];
            sum += v;
            sumsq += v * v;
        }
        nodes_[self].val = sum / n;
        double sse = sumsq - sum * sum / n;
        if(depth >= maxDepth_ || n < 2 * minLeaf_ || sse <= 1e-12 * (1.0 + sumsq)){
            return self;
        }

        // 候補の変数をランダムに mtry 個選ぶ
        int p = (int)X_.size();
        std::vector<int> feats(p);
        std::iota(feats.begin(), feats.end(), 0);
        std::shuffle(feats.begin(), feats.end(), gen);

        double bestGain = 0;
        int bestFeat = -1;
        double bestThr = 0;
        std::vector<int> order(idx);
        for(int f = 0; f < mtry_ && f < p; f++){
            int j = feats[f];
            const std::vector<double> &col = X_[j];
            std::sort(order.begin(), order.end(), [&col](int a, int b){ return col[a] < col[b]; });
            double ls = 0, lss = 0;
            for(int k = 0; k + 1 < n; k++){
                double v = y_[order[k]];
                ls += v;
                lss += v * v;
                int nl = k + 1, nr = n - nl;
                if(nl < minLeaf_ || nr < minLeaf_)continue;
                if(col[order[k]] == col[order[k + 1]])continue;     // 同じ値の間では分割できない
                double rs = sum - ls, rss = sumsq - lss;
                double sse_split = (lss - ls * ls / nl) + (rss - rs * rs / nr);
                double gain = sse - sse_split;
                if(gain > bestGain){
                    bestGain = gain;
                    bestFeat = j;
                    bestThr = 0.5 * (col[order[k]] + col[order[k + 1]]);
                }
            }
        }
        if(bestFeat < 0)return self;

        std::vector<int> li, ri;
        for(int k = 0; k < n; k++){
            if(X_[bestFeat][idx[k]] <= bestThr)li.push_back(idx[k]);
            else ri.push_back(idx[k]);
        }
        nodes_[self].feat = bestFeat;
        nodes_[self].thr = bestThr;
        int l = grow(li, depth + 1, gen);
        int r = grow(ri, depth + 1, gen);
        nodes_[self].left = l;
        nodes_[self].right = r;
        return self;
    }

    const Matrix &X_;
    const std::vector<double> &y_;
    int mtry_, minLeaf_, maxDepth_;
    std::vector<Node> nodes_;
};

} // namespace

ForestResult forest_importance(const Matrix &Xin, const std::vector<double> &y, int ntrees, unsigned seed)
{
    ForestResult res;
    int p = (int)Xin.size();
    int n = (int)y.size();
    res.importance.assign(p, 0.0);
    res.oob_r2 = 0;
    res.n = n;
    if(p == 0 || n < 10)return res;

    // permutation 用に作業用コピーを持つ
    Matrix X(Xin);
    std::mt19937 gen(seed);

    double ymean = std::accumulate(y.begin(), y.end(), 0.0) / n;
    double yvar = 0;
    for(int i = 0; i < n; i++)yvar += (y[i] - ymean) * (y[i] - ymean);
    yvar /= n;
    if(yvar <= 0)return res;

    int mtry = std::max(1, p / 3);
    std::vector<double> oobSum(n, 0.0);
    std::vector<int> oobCnt(n, 0);
    std::vector<double> impSum(p, 0.0);
    int treesUsed = 0;

    for(int t = 0; t < ntrees; t++){
        // ブートストラップ標本と OOB
        std::vector<int> idx(n);
        std::vector<char> inBag(n, 0);
        std::uniform_int_distribution<int> pick(0, n - 1);
        for(int k = 0; k < n; k++){
            idx[k] = pick(gen);
            inBag[idx[k]] = 1;
        }
        std::vector<int> oob;
        for(int i = 0; i < n; i++)if(!inBag[i])oob.push_back(i);
        if(oob.size() < 5)continue;

        Tree tree(Xin, y, mtry, 3, 14);
        tree.build(idx, gen);

        double base = 0;
        for(size_t k = 0; k < oob.size(); k++){
            double pr = tree.predict(X, oob[k]);
            oobSum[oob[k]] += pr;
            oobCnt[oob[k]]++;
            base += (pr - y[oob[k]]) * (pr - y[oob[k]]);
        }
        base /= oob.size();

        // 変数 j の OOB サンプルの値をシャッフルして誤差の増加を見る
        for(int j = 0; j < p; j++){
            std::vector<double> saved(oob.size());
            for(size_t k = 0; k < oob.size(); k++)saved[k] = X[j][oob[k]];
            std::vector<double> shuffled(saved);
            std::shuffle(shuffled.begin(), shuffled.end(), gen);
            for(size_t k = 0; k < oob.size(); k++)X[j][oob[k]] = shuffled[k];
            double perm = 0;
            for(size_t k = 0; k < oob.size(); k++){
                double pr = tree.predict(X, oob[k]);
                perm += (pr - y[oob[k]]) * (pr - y[oob[k]]);
            }
            perm /= oob.size();
            impSum[j] += perm - base;
            for(size_t k = 0; k < oob.size(); k++)X[j][oob[k]] = saved[k];
        }
        treesUsed++;
    }
    if(treesUsed == 0)return res;

    for(int j = 0; j < p; j++)res.importance[j] = impSum[j] / treesUsed / yvar;

    double sse = 0, sst = 0;
    int cnt = 0;
    for(int i = 0; i < n; i++){
        if(oobCnt[i] == 0)continue;
        double pr = oobSum[i] / oobCnt[i];
        sse += (y[i] - pr) * (y[i] - pr);
        sst += (y[i] - ymean) * (y[i] - ymean);
        cnt++;
    }
    res.oob_r2 = (cnt > 0 && sst > 0) ? 1.0 - sse / sst : 0.0;
    return res;
}
