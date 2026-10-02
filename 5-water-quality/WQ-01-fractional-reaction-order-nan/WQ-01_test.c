/*
 * WQ-01: a reaction order between 0 and 1 gives NaN across the network.
 *
 * Reservoir R1 (chlorine 1.0 mg/L) fills tank T1 through junction J1 for
 * three hours; then R1's head drops and T1 drains back through J1. Everything
 * except R1 starts at 0 mg/L. The bulk reaction is of order 0.5.
 *
 * The rate of an n-th order reaction is kb * c^n. EPANET evaluates it as
 * c * c^(n-1), which for c = 0 and n < 1 is 0 * inf = NaN.
 *
 * Correct behaviour: every concentration is finite and the mass balance
 * ratio is close to 1. Order 1 is run as well, for comparison.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

/* Runs the network with the given bulk order; returns 1 if any value is NaN */
static int run(double order)
{
    EN_Project ph;
    int j1, t1, p1, bad = 0;
    long t, tstep;
    double cj1, ct1, cp1, ratio;

    EN_createproject(&ph);
    EN_open(ph, "WQ-01_half-order.inp", "WQ-01.rpt", "");
    EN_setoption(ph, EN_BULKORDER, order);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "T1", &t1);
    EN_getlinkindex(ph, "P1", &p1);

    printf("Bulk order %.1f\n", order);
    printf("  Hour    J1 (mg/L)   T1 (mg/L)   P1 (mg/L)\n");

    EN_solveH(ph);
    EN_openQ(ph);
    EN_initQ(ph, EN_NOSAVE);
    do {
        EN_runQ(ph, &t);
        EN_getnodevalue(ph, j1, EN_QUALITY, &cj1);
        EN_getnodevalue(ph, t1, EN_QUALITY, &ct1);
        EN_getlinkvalue(ph, p1, EN_LINKQUAL, &cp1);
        if (t % 3600 == 0)
            printf("  %4ld   %9.4f   %9.4f   %9.4f\n", t / 3600, cj1, ct1, cp1);
        if (isnan(cj1) || isnan(ct1) || isnan(cp1)) bad = 1;
        EN_nextQ(ph, &tstep);
    } while (tstep > 0);

    EN_getstatistic(ph, EN_MASSBALANCE, &ratio);
    printf("  Mass balance ratio: %.4f\n\n", ratio);
    if (isnan(ratio)) bad = 1;

    EN_closeQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    return bad;
}

int main(void)
{
    int bad;

    run(1.0);
    bad = run(0.5);

    if (bad > 0) {
        printf("FAIL: order 0.5 gives NaN concentrations\n");
        return 1;
    }
    printf("PASS: order 0.5 gives finite concentrations\n");
    return 0;
}
