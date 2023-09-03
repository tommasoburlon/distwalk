#include "distrib.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

__thread struct drand48_data rnd_buf;

double expon(double lambda) {
    double x;
    drand48_r(&rnd_buf, &x);
    return (-log(1 - x) / lambda);
}

/* X~unif[0,1] -> E[X] = 1/2, sx^2 = E[(X-1/2)^2] = \int_0^1 x^2 dx - 1/4 = 1/3 - 1/4 = 1/12
 * Y = X_1 + ... + X_n -> my = n * mx, sy^2 = n * sx^2, sy = sqrt(n) * sx
 * E[(Y - my)^2] = E[Y^2] - my^2
 * = E[(X_1 + ... + X_n)*(X_1 + ... + X_n)] - my^2
 * = n * (mx^2 + sx^2) + (n^2 - n)*mx^2 - n^2 * mx^2
 * = n * sx^2
 */
double gaussian(double avg, double std) {
    double y = 0.0;
    const int N = 4;
    for (int i = 0; i < N*N; i++) {
        double x;
        drand48_r(&rnd_buf, &x);
        y += x - 0.5;
    }
    // y has mean 0, var N*N/12
    y *= sqrt(12)/N; // now y ~ N(0,1)
    return y * std + avg;
}

void pd_init(long int seed) {
    srand48_r(seed, &rnd_buf);
}

int pd_parse(pd_spec_t *p, const char *s) {
    *p = (pd_spec_t) { .prob = FIXED, .val = NAN, .min = NAN, .max = NAN };
    if (sscanf(s, "unif(%lf,%lf)", &p->min, &p->max) == 2) {
        p->prob = UNIF;
        return 1;
    } else if (sscanf(s, "unif(%lf)", &p->max) == 1) {
        p->min = 0;
        p->prob = UNIF;
        return 1;
    } else if (sscanf(s, "exp(%lf)", &p->val) == 1) {
        p->prob = EXPON;
        return 1;
    } else if (sscanf(s, "norm(%lf,%lf)", &p->val, &p->std) == 2) {
        p->min = 0;
        p->prob = NORM;
        return 1;
    } else if (sscanf(s, "%lf", &p->val) == 1) {
        p->prob = FIXED;
        return 1;
    } else {
        return 0;
    }
}

double pd_sample(pd_spec_t *p) {
    double val;
    switch (p->prob) {
    case FIXED:
        val = p->val;
        break;
    case UNIF:
        double x;
        drand48_r(&rnd_buf, &x);
        // no need to check boundaries in this case
        return p->min + (p->max - p->min) * x;
    case EXPON:
        val = expon(1.0 / (p->val));
        break;
    case NORM:
        val = gaussian(p->val, p->std);
        break;
    default:
        fprintf(stderr, "Unexpected prob type: %d\n", p->prob);
        exit(1);
    }
    if (!isnan(p->min) && val < p->min)
        val = p->min;
    if (!isnan(p->max) && val > p->max)
        val = p->max;
    return val;
}

static char s[64];

// stringify spec into static array and return it
char *pd_str(pd_spec_t *p) {
    switch (p->prob) {
    case FIXED:
        sprintf(s, "%g", p->val);
        break;
    case UNIF:
        sprintf(s, "unif(%g,%g)", p->min, p->max);
        break;
    case EXPON:
        sprintf(s, "exp(%g)", p->val);
        break;
    case NORM:
        sprintf(s, "norm(%g,%g)", p->val, p->std);
        break;
    default:
        fprintf(stderr, "Unexpected prob type: %d\n", p->prob);
        exit(1);
    }
    return s;
}
