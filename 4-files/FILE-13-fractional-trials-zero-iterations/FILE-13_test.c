/*
 * FILE-13: TRIALS below 1 gives zero solver iterations.
 *
 * Reservoir R1 (head 200 ft) feeds junction J1 (elevation 100 ft, 500 gpm)
 * through one pipe. The input file sets TRIALS 0.5. optionvalue() in
 * input3.c only checks that TRIALS is above 0, then stores (int)0.5 = 0, so
 * the hydraulic solver runs no trials at all and returns its initial guess.
 * EN_setoption(EN_TRIALS) rejects the same value with error 213.
 *
 * Correct behaviour: the file is rejected with error 200 (error 213 for the
 * TRIALS line in the report).
 */
#include <stdio.h>
#include "epanet2_2.h"

/* Solve the hydraulics for a single period and return J1's pressure */
static double solve_pressure(EN_Project ph)
{
    int j1;
    double p;

    EN_solveH(ph);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodevalue(ph, j1, EN_PRESSURE, &p);
    return p;
}

int main(void)
{
    EN_Project ph;
    double trials, p_bad, p_good;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-13_trials-0.5.inp", "FILE-13.rpt", "");
    printf("EN_open returned %d (expected 200)\n", err);

    if (err == 0) {
        EN_getoption(ph, EN_TRIALS, &trials);
        p_bad = solve_pressure(ph);

        /* The same network with the default 200 trials */
        EN_setoption(ph, EN_TRIALS, 200);
        p_good = solve_pressure(ph);

        printf("TRIALS stored as %.0f\n", trials);
        printf("J1 pressure with TRIALS 0.5: %8.2f psi\n", p_bad);
        printf("J1 pressure with TRIALS 200: %8.2f psi\n", p_good);
    }
    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 200) {
        printf("FAIL: TRIALS 0.5 was accepted and the solver ran no trials\n");
        return 1;
    }
    printf("PASS: TRIALS below 1 is rejected\n");
    return 0;
}
