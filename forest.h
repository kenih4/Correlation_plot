#ifndef FOREST_H
#define FOREST_H

#include <vector>

// ランダムフォレスト回帰 + OOB(out-of-bag)permutation importance
//   X : X[j][i] = 説明変数 j のサンプル i の値 (p 個 × n サンプル)
//   y : 目的変数 (n サンプル)
struct ForestResult{
    std::vector<double> importance; // 変数ごとの重要度。permutationで増えたOOB誤差 / yの分散 (大きいほど重要。1を超えることもある)
    double oob_r2;                  // OOB予測の決定係数。0以下ならモデルに予測力がなく、重要度は信用できない
    int n;                          // 使用したサンプル数
};

ForestResult forest_importance(const std::vector<std::vector<double> > &X,
                               const std::vector<double> &y,
                               int ntrees = 150,
                               unsigned seed = 12345);

#endif // FOREST_H
