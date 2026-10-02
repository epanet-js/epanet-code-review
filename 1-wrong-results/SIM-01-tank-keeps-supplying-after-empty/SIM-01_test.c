/*
 * SIM-01: a draining tank keeps supplying water after it is empty.
 *
 * Tank T1 starts 3 ft above its minimum level and drains into junction J1.
 * We run the hydraulics step by step, add up the volume that leaves the tank,
 * and compare it with the volume the tank actually lost.
 *
 * Correct behaviour: the tank cannot deliver more water than it held
 * (allowing for the 1-second rounding of the time step), and its final
 * volume is not below the minimum volume.
 *
 * The demand is swept from 190 to 215 gpm, because whether the bug shows up
 * depends on how the time-to-empty rounds to whole seconds.
 */
#include <stdio.h>
#include "epanet2_2.h"

#define GAL_PER_FT3 7.480519

typedef struct {
    double excess;      /* volume delivered beyond what the tank lost, ft3 */
    double final_vol;   /* tank volume at the end of the run, ft3 */
    double one_second;  /* volume the demand draws in one second, ft3 */
} Result;

static Result drain_tank(double demand_gpm)
{
    EN_Project ph;
    int j1, t1;
    long t, tstep;
    double v0, q_tank, delivered = 0.0;
    Result r;

    EN_createproject(&ph);
    EN_open(ph, "SIM-01_tank-drains.inp", "SIM-01.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "T1", &t1);
    EN_setnodevalue(ph, j1, EN_BASEDEMAND, demand_gpm);
    EN_getnodevalue(ph, t1, EN_INITVOLUME, &v0);

    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);

        /* A tank's EN_DEMAND is its net inflow (negative while draining) */
        EN_getnodevalue(ph, t1, EN_DEMAND, &q_tank);
        EN_nextH(ph, &tstep);

        /* gpm x seconds -> ft3 */
        delivered += -q_tank * tstep / 60.0 / GAL_PER_FT3;
    } while (tstep > 0);

    EN_getnodevalue(ph, t1, EN_TANKVOLUME, &r.final_vol);
    r.excess = delivered - (v0 - r.final_vol);
    r.one_second = demand_gpm / 60.0 / GAL_PER_FT3;

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    return r;
}

int main(void)
{
    int too_much = 0, below_min = 0;

    printf("Demand   Excess delivered   Final volume\n");
    printf(" (gpm)        (ft3)             (ft3)\n");

    for (double demand = 190; demand <= 215; demand += 1.0) {
        Result r = drain_tank(demand);
        /* Minimum volume is 0 in this network */
        int extra = r.excess > r.one_second;
        int below = r.final_vol < -1e-6;

        printf("%6.0f   %12.2f      %12.4f   %s\n", demand, r.excess, r.final_vol,
               extra ? "<-- delivered water it did not have" :
               below ? "<-- ended below its minimum volume" : "");
        too_much += extra;
        below_min += below;
    }

    if (too_much || below_min) {
        printf("FAIL: out of 26 runs, the tank delivered water it did not have in %d "
               "and ended below its minimum volume in %d\n", too_much, below_min);
        return 1;
    }
    printf("PASS: the tank never delivers more than it held\n");
    return 0;
}
