/*
 * SIM-14: pressure-driven demand (PDA) in kPa with a narrow pressure range.
 *
 * SIM-14_pda-kpa.inp has three junctions with 50 L/s demands fed by a 12 m
 * reservoir that cannot supply them all. The PDA minimum pressure is 20 kPa
 * and the required pressure 20.2 kPa, a range of 0.2 kPa (0.067 ft).
 *
 * Correct behaviour: the solver converges, and each junction's demand
 * follows the PDA formula D = Dfull ((p - Pmin) / (Preq - Pmin))^0.5 at its
 * own pressure. The same network with a 5 kPa range (Preq = 25 kPa) is run
 * as a control.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define PMIN  20.0   /* kPa */
#define DFULL 50.0   /* L/s */

/* Solves the network for one required pressure; returns 1 if all is well */
static int solve(EN_Project ph, double preq)
{
    long t;
    int code, ok = 1;
    double trials;

    EN_setdemandmodel(ph, EN_PDA, PMIN, preq, 0.5);
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    code = EN_runH(ph, &t);
    EN_getstatistic(ph, EN_ITERATIONS, &trials);

    printf("Required pressure %.1f kPa: EN_runH returned %d after %.0f trials\n",
           preq, code, trials);
    printf("  Node   Pressure   Demand   PDA formula\n");
    printf("           (kPa)    (L/s)       (L/s)\n");
    for (int i = 1; i <= 3; i++) {
        double p, d, formula;

        EN_getnodevalue(ph, i, EN_PRESSURE, &p);
        EN_getnodevalue(ph, i, EN_DEMAND, &d);
        if (p <= PMIN)
            formula = 0.0;
        else if (p >= preq)
            formula = DFULL;
        else
            formula = DFULL * sqrt((p - PMIN) / (preq - PMIN));

        int bad = fabs(d - formula) > 0.1;
        printf("  J%d   %9.3f  %7.2f   %9.2f   %s\n", i, p, d, formula,
               bad ? "<-- off the demand curve" : "");
        if (bad) ok = 0;
    }
    EN_closeH(ph);
    printf("\n");
    return ok && code == 0;
}

int main(void)
{
    EN_Project ph;

    EN_createproject(&ph);
    EN_open(ph, "SIM-14_pda-kpa.inp", "SIM-14.rpt", "");

    int control = solve(ph, 25.0);    /* 5 kPa range */
    int narrow = solve(ph, 20.2);     /* 0.2 kPa range, as in the file */

    EN_close(ph);
    EN_deleteproject(ph);

    if (!control) {
        printf("FAIL: the control case with a 5 kPa range did not converge\n");
        return 1;
    }
    if (!narrow) {
        printf("FAIL: with a 0.2 kPa pressure range the solver does not converge and "
               "demands are off the PDA curve\n");
        return 1;
    }
    printf("PASS: the solver converges and demands follow the PDA curve for both "
           "pressure ranges\n");
    return 0;
}
