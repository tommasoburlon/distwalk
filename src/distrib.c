#include "distrib.h"
#include "cw_debug.h"

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

// Only works for small and integer values of 'k'
double distr_gamma(double avg, double std) {
    // avg = k * scale, std^2 = k * scale^2
    double scale = std*std / avg;
    int k = lrint(avg / scale);
    check(fabs(k - avg/scale) < 0.00001, "Using Gamma with non-integer k: %g", avg/scale);
    double x = 0.0;
    for (int i = 0; i < k; i++)
        x += expon(1.0 / scale);
    return x;
}

void pd_init(long int seed) {
    srand48_r(seed, &rnd_buf);
}

int pd_parse(pd_spec_t *p, char *s) {
    *p = (pd_spec_t) { .prob = FIXED, .val = NAN, .std = NAN, .min = NAN, .max = NAN };
    char *tok = strsep(&s, ":");
    check(tok, "Wrong value/distribution syntax\n");
    if (strcmp(tok, "unif") == 0)
        p->prob = UNIF;
    else if (strcmp(tok, "exp") == 0)
        p->prob = EXPON;
    else if (strcmp(tok, "norm") == 0)
        p->prob = NORM;
    else if (strcmp(tok, "gamma") == 0)
        p->prob = GAMMA;
    else if (sscanf(tok, "%lf", &p->val) == 1)
        p->prob = FIXED;
    else {
        fprintf(stderr, "Wrong value/distribution syntax: %s\n", tok);
        exit(EXIT_FAILURE);
    }
    double k = NAN, scale = NAN; // for Gamma
    while ((tok = strsep(&s, ",")) != NULL) {
        printf("Processing tok: %s\n", tok);
        if (sscanf(tok, "min=%lf", &p->min) == 1
            || sscanf(tok, "max=%lf", &p->max) == 1
            || sscanf(tok, "std=%lf", &p->std) == 1
            || sscanf(tok, "k=%lf", &k) == 1
            || sscanf(tok, "scale=%lf", &scale) == 1
            || sscanf(tok, "avg=%lf", &p->val) == 1
            || sscanf(tok, "%lf", &p->val) == 1)
                continue;
        fprintf(stderr, "Unrecognized token in value/distribution syntax: %s\n", tok);
        exit(EXIT_FAILURE);
    }
    if (p->prob == GAMMA && !isnan(k) && !isnan(scale)) {
        p->val = k * scale;
        p->std = sqrt(k) * scale;
    }
    check(p->prob != FIXED || !isnan(p->val));
    check(p->prob != UNIF || (!isnan(p->min) && !isnan(p->max)));
    check(p->prob != EXPON || (!isnan(p->val) && isnan(p->std)));
    check(p->prob != NORM || (!isnan(p->val) && !isnan(p->std)));
    check(p->prob != GAMMA || (!isnan(p->val) && !isnan(p->std)));
    return 1;
}

double pd_sample(pd_spec_t *p) {
    double val;
 retry:
    switch (p->prob) {
    case FIXED:
        // no need to check boundaries in this case
        return p->val;
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
    case GAMMA:
        val = distr_gamma(p->val, p->std);
        break;
    default:
        fprintf(stderr, "Unexpected prob type: %d\n", p->prob);
        exit(EXIT_FAILURE);
    }
    if ((!isnan(p->min) && val < p->min)
        || (!isnan(p->max) && val > p->max))
        goto retry;
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
        sprintf(s, "unif:");
        break;
    case EXPON:
        sprintf(s, "exp:%g", p->val);
        break;
    case NORM:
        sprintf(s, "norm:%g", p->val);
        break;
    case GAMMA:
        double scale = p->std * p->std / p->val;
        int k = lrint(p->val / scale);
        sprintf(s, "gamma:%g,k=%d,scale=%g", p->val, k, scale);
        break;
    default:
        fprintf(stderr, "Unexpected prob type: %d\n", p->prob);
        exit(EXIT_FAILURE);
    }
    if (!isnan(p->std))
        sprintf(s + strlen(s), ",std=%g", p->std);
    if (!isnan(p->min))
        sprintf(s + strlen(s), ",min=%g", p->min);
    if (!isnan(p->max))
        sprintf(s + strlen(s), ",max=%g", p->max);
    return s;
}
