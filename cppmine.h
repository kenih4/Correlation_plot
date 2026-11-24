#ifndef _CPPMINE_H
#define _CPPMINE_H

#include "mine.h"

class MINE {

  private:

    mine_parameter param;
    mine_problem prob;
    mine_score *score;

  public:

    /* Constructor */
    MINE(double alpha, double c, int est);

    /* Destructor. */
    ~MINE();

    /* See mine.h */
    void compute_score(double *x, double *y, int n);

    /* See mine.h */
    double mic();
};

#endif /* _CPPMINE_H */