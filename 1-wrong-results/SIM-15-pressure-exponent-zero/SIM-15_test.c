/*
 * SIM-15: PRESSURE EXPONENT 0 is accepted and gives NaN results.
 *
 * SIM-15_pexp-zero.inp is a two-junction network with pressure-driven
 * demand and [OPTIONS] PRESSURE EXPONENT 0. EN_setdemandmodel rejects an
 * exponent <= 0 with error 208.
 *
 * Correct behaviour: EN_open rejects the file (error 200, with error 213
 * "invalid option value" in the report). Without the check the file loads,
 * the solver divides by the exponent, and every head and flow is NaN while
 * EN_solveH returns 0.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err, nnodes, nan_count = 0;

    EN_createproject(&ph);
    err = EN_open(ph, "SIM-15_pexp-zero.inp", "SIM-15.rpt", "");
    printf("EN_open returned %d\n", err);

    if (err == 0) {
        /* The file was accepted, so run it and look at the results */
        err = EN_solveH(ph);
        printf("EN_solveH returned %d\n", err);

        EN_getcount(ph, EN_NODECOUNT, &nnodes);
        for (int i = 1; i <= nnodes; i++) {
            char id[EN_MAXID + 1];
            double head, demand;

            EN_getnodeid(ph, i, id);
            EN_getnodevalue(ph, i, EN_HEAD, &head);
            EN_getnodevalue(ph, i, EN_DEMAND, &demand);
            printf("  %-3s head = %f  demand = %f\n", id, head, demand);
            if (isnan(head) || isnan(demand)) nan_count++;
        }
        EN_close(ph);
        EN_deleteproject(ph);
        printf("FAIL: PRESSURE EXPONENT 0 was accepted; %d of %d nodes have NaN "
               "results\n", nan_count, nnodes);
        return 1;
    }

    EN_deleteproject(ph);
    if (err != 200) {
        printf("FAIL: expected input error 200, got %d\n", err);
        return 1;
    }
    printf("PASS: PRESSURE EXPONENT 0 is rejected as an input error\n");
    return 0;
}
