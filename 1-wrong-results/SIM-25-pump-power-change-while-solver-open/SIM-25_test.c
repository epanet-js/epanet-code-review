/*
 * SIM-25: changing a pump's power while the solver is open has no effect.
 *
 * In SIM-25_power-pumps.inp, pumps PU1 and PU2 each lift a fixed 1 cfs
 * from a reservoir at head 0. PU1 has a constant power of 10 hp (88.14 ft
 * at 1 cfs); PU2 has a head curve (100 ft at 1 cfs). With the hydraulic
 * solver open, the test sets EN_PUMP_POWER to 20 hp on PU1 and to 10 hp on
 * PU2, re-solves, and then restarts the run with EN_initH.
 *
 * Correct behaviour: from the next EN_runH on, PU1 adds 176.28 ft and PU2,
 * now a constant-power pump, adds 88.14 ft (head = 8.814 hp / cfs).
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int pu1, pu2, failures = 0;

/* Re-solves the current period and checks both pumps' head gain */
static void check(EN_Project ph, const char *step, double gain1, double gain2)
{
    long t;
    double hl1, hl2;

    EN_runH(ph, &t);
    EN_getlinkvalue(ph, pu1, EN_HEADLOSS, &hl1);    /* negative: a gain */
    EN_getlinkvalue(ph, pu2, EN_HEADLOSS, &hl2);

    int bad = fabs(-hl1 - gain1) > 0.01 || fabs(-hl2 - gain2) > 0.01;
    printf("%-28s %7.2f %8.2f   %7.2f %8.2f   %s\n", step, gain1, -hl1, gain2, -hl2,
           bad ? "<-- wrong" : "");
    failures += bad;
}

int main(void)
{
    EN_Project ph;

    EN_createproject(&ph);
    EN_open(ph, "SIM-25_power-pumps.inp", "SIM-25.rpt", "");
    EN_getlinkindex(ph, "PU1", &pu1);
    EN_getlinkindex(ph, "PU2", &pu2);

    printf("                             PU1 gain (ft)       PU2 gain (ft)\n");
    printf("                             Expect   Actual     Expect   Actual\n");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    check(ph, "As loaded", 88.14, 100.0);

    EN_setlinkvalue(ph, pu1, EN_PUMP_POWER, 20.0);
    EN_setlinkvalue(ph, pu2, EN_PUMP_POWER, 10.0);
    check(ph, "PU1 20 hp, PU2 10 hp", 176.28, 88.14);

    EN_initH(ph, EN_NOSAVE);
    check(ph, "after EN_initH", 176.28, 88.14);
    EN_closeH(ph);

    /* For comparison: the same values after reopening the solver */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    check(ph, "after EN_closeH, EN_openH", 176.28, 88.14);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 4 solves ignore a power change made while the solver "
               "was open\n", failures);
        return 1;
    }
    printf("PASS: a pump power set while the solver is open is used from the next "
           "EN_runH\n");
    return 0;
}
