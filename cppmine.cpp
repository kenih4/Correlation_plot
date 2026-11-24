#include <cstdlib>
#include "mine.h"
#include "cppmine.h"

using namespace std;


MINE::MINE(double alpha, double c, int est)
{
  char *ret;

  param.alpha = alpha;
  param.c = c;
  param.est = est;
  score = NULL;

  ret = mine_check_parameter(&param);
  if (ret)
    throw ret;
}


MINE::~MINE()
{
  mine_free_score(&score);
}


void MINE::compute_score(double *x, double *y, int n)
{
  prob.x = x;
  prob.y = y;
  prob.n = n;

  mine_free_score(&score);
  score = mine_compute_score(&prob, &param);
  char ret[256] = "error in mine_compute_score()";
  if (score == NULL)
    throw ret;
}


double MINE::mic()
{
  char ret[256] = "no score computed";
  if (score == NULL)
    throw ret;

  return mine_mic(score);
}
