/*
 * API-20: reads before a run return solver scratch values.
 *
 * Tank T1 starts at a level of 10 ft (head 110 ft, 19,635 ft3) and feeds
 * junctions J1 and J2. Nothing has been run yet when we
 *   A. read T1's EN_TANKVOLUME and EN_HEAD, and
 *   B. give J1 an emitter with EN_setnodevalue(EN_EMITTER) and read its
 *      EN_EMITTERFLOW.
 * Then, as a check for issue #795, we start a run, give J2 an emitter
 * during it (the emitter exponent is 1.1) and run one more step.
 *
 * Correct behaviour: before a run, T1 reads its initial volume and head,
 * and J1's emitter flow is 0 because nothing has been computed. The emitter
 * added during the run gives a finite flow.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int t1, j1, j2, wrong = 0;
    long t, tstep;
    double vol, init_vol, head, qe1, qe2;

    EN_createproject(&ph);
    EN_open(ph, "API-20_network.inp", "API-20.rpt", "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "J2", &j2);

    /* A. Tank values before any hydraulics */
    EN_getnodevalue(ph, t1, EN_TANKVOLUME, &vol);
    EN_getnodevalue(ph, t1, EN_INITVOLUME, &init_vol);
    EN_getnodevalue(ph, t1, EN_HEAD, &head);
    printf("A. Before a run: T1 EN_TANKVOLUME %.1f ft3 (expected %.1f), "
           "EN_HEAD %.2f ft (expected 110.00)\n", vol, init_vol, head);
    if (fabs(vol - init_vol) > 0.1 || fabs(head - 110.0) > 0.01) wrong++;

    /* B. Emitter flow right after adding an emitter */
    EN_setnodevalue(ph, j1, EN_EMITTER, 10.0);
    EN_getnodevalue(ph, j1, EN_EMITTERFLOW, &qe1);
    printf("B. Before a run: J1 EN_EMITTERFLOW %.2f gpm after setting EN_EMITTER "
           "(expected 0.00)\n", qe1);
    if (qe1 != 0.0) wrong++;

    /* Issue #795: an emitter added during a run must not give NaN */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    EN_setnodevalue(ph, j2, EN_EMITTER, 10.0);
    EN_nextH(ph, &tstep);
    EN_runH(ph, &t);
    EN_getnodevalue(ph, j2, EN_EMITTERFLOW, &qe2);
    EN_closeH(ph);
    printf("   During a run (t = %ld s): J2 EN_EMITTERFLOW %.2f gpm after adding "
           "an emitter (expected finite)\n", t, qe2);
    if (!isfinite(qe2)) wrong++;

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d of 3 reads returned a scratch value instead of a result\n",
               wrong);
        return 1;
    }
    printf("PASS: reads before a run return the initial state, and 0 for flows\n");
    return 0;
}
