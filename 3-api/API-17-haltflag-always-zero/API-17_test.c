/*
 * API-17: EN_gettimeparam(EN_HALTFLAG) always returns 0.
 *
 * Net1 is run step by step with UNBALANCED STOP and only 1 trial allowed,
 * so the first hydraulic solution is unbalanced and EPANET halts the run:
 * EN_nextH returns a time step of 0 at t = 0 instead of running for 24 h.
 *
 * Correct behaviour: after the run is halted, EN_HALTFLAG reads 1.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    long t, tstep, dur, halt = -1;
    int err;

    EN_createproject(&ph);
    EN_open(ph, "API-17_Net1.inp", "API-17.rpt", "");
    EN_setoption(ph, EN_UNBALANCED, -1);   /* -1 = stop if unbalanced */
    EN_setoption(ph, EN_TRIALS, 1);
    EN_gettimeparam(ph, EN_DURATION, &dur);

    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        err = EN_runH(ph, &t);
        EN_nextH(ph, &tstep);
        EN_gettimeparam(ph, EN_HALTFLAG, &halt);
        printf("t = %ld s: EN_runH returns %d, next time step %ld s, EN_HALTFLAG = %ld\n",
               t, err, tstep, halt);
    } while (tstep > 0);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    printf("The run stopped at t = %ld s of %ld s\n", t, dur);
    if (halt != 1) {
        printf("FAIL: the run was halted at t = %ld s but EN_HALTFLAG is %ld\n", t, halt);
        return 1;
    }
    printf("PASS: EN_HALTFLAG is 1 after the run was halted\n");
    return 0;
}
