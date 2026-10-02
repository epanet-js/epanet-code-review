/*
 * SIM-10: a PATTERN START that is not a multiple of PATTERN STEP misses the
 * pattern boundaries.
 *
 * The network is EPANET's Net1 with Pattern Start 0:30 and Pattern Timestep
 * 2:00, so the demand pattern moves to its next period at 1:30, 3:30, 5:30,
 * ... of simulation time. The hydraulic step is 1:00. We run the hydraulics
 * step by step and record the time and the demand at junction 11 of every
 * solution.
 *
 * Correct behaviour: the solver takes a time step that ends at each pattern
 * boundary, so the new demand applies from that time, not from the next
 * whole hour.
 */
#include <stdio.h>
#include "epanet2_2.h"

#define MAXSTEPS 200

int main(void)
{
    EN_Project ph;
    long step_time[MAXSTEPS], t, tstep, pstep, pstart, dur, b;
    double demand[MAXSTEPS];
    int n11, nsteps = 0, late = 0, nbounds = 0, i;

    EN_createproject(&ph);
    EN_open(ph, "SIM-10_pattern-start.inp", "SIM-10.rpt", "");
    EN_getnodeindex(ph, "11", &n11);
    EN_gettimeparam(ph, EN_PATTERNSTEP, &pstep);
    EN_gettimeparam(ph, EN_PATTERNSTART, &pstart);
    EN_gettimeparam(ph, EN_DURATION, &dur);

    /* Step through the hydraulics and record every solution */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        if (nsteps < MAXSTEPS) {
            step_time[nsteps] = t;
            EN_getnodevalue(ph, n11, EN_DEMAND, &demand[nsteps]);
            nsteps++;
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    /* Pattern periods start at k*pstep - pstart in simulation time */
    printf("Pattern period   First solution     Junction 11 demand\n");
    printf("starts at        at or after it     there (gpm)\n");
    for (b = pstep - pstart; b < dur; b += pstep) {
        for (i = 0; i < nsteps && step_time[i] < b; i++)
            ;
        nbounds++;
        if (step_time[i] != b) late++;
        if (nbounds <= 4) {
            printf("%5ld:%02ld          %5ld:%02ld           %6.1f   %s\n",
                   b / 3600, b % 3600 / 60, step_time[i] / 3600,
                   step_time[i] % 3600 / 60, demand[i],
                   step_time[i] != b ? "<-- late" : "");
        }
    }
    printf("...\n");

    if (late) {
        printf("FAIL: %d of %d demand pattern changes are applied late, "
               "at the next whole step\n", late, nbounds);
        return 1;
    }
    printf("PASS: all %d demand pattern changes are applied when the pattern "
           "period starts\n", nbounds);
    return 0;
}
