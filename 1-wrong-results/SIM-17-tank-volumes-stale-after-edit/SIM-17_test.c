/*
 * SIM-17: EN_MINLEVEL and EN_setcurve leave tank volumes stale.
 *
 * Three tanks each drain 200 gpm into their own junction. We run one hour
 * and record how far each level falls. Then we edit the tanks:
 *   T1 (a cylinder): EN_setnodevalue(EN_MINLEVEL) raises the minimum level
 *       from 0 to 5 ft;
 *   T2: EN_setcurve doubles every volume on its volume curve;
 *   T3: EN_setcurvevalue doubles the volume at the top of its curve (the
 *       same change, one point at a time);
 * and run the same hour again.
 *
 * Correct behaviour: the tank's initial volume (EN_INITVOLUME) matches the
 * volume at its initial level, and the level falls by the same amount as
 * before (T1, same area) or by half as much (T2 and T3, twice the area).
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define NTANKS 3

static const char *TANK_ID[NTANKS] = {"T1", "T2", "T3"};

/* Runs one hour; returns each tank's volume at t = 0 and its level change */
static void run_one_hour(EN_Project ph, double vol_t0[], double change[])
{
    long t, tstep;
    double level0[NTANKS], level;
    int k, index;

    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    for (k = 0; k < NTANKS; k++) {
        EN_getnodeindex(ph, TANK_ID[k], &index);
        EN_getnodevalue(ph, index, EN_TANKVOLUME, &vol_t0[k]);
        EN_getnodevalue(ph, index, EN_TANKLEVEL, &level0[k]);   /* initial level */
    }
    EN_nextH(ph, &tstep);
    EN_runH(ph, &t);   /* t = 3600 s */
    for (k = 0; k < NTANKS; k++) {
        double head, elev;
        EN_getnodeindex(ph, TANK_ID[k], &index);
        EN_getnodevalue(ph, index, EN_HEAD, &head);
        EN_getnodevalue(ph, index, EN_ELEVATION, &elev);
        level = head - elev;
        change[k] = level - level0[k];
    }
    EN_closeH(ph);
}

int main(void)
{
    EN_Project ph;
    double vol_t0[NTANKS], base_change[NTANKS], change[NTANKS], init_vol[NTANKS];
    double x[2] = {0, 20}, y[2] = {0, 80000};   /* the curve with doubled volumes */
    int k, index, c2, c3, wrong = 0;

    EN_createproject(&ph);
    EN_open(ph, "SIM-17_tanks.inp", "SIM-17.rpt", "");
    run_one_hour(ph, vol_t0, base_change);

    /* The edits */
    EN_getnodeindex(ph, "T1", &index);
    EN_setnodevalue(ph, index, EN_MINLEVEL, 5.0);
    EN_getcurveindex(ph, "C2", &c2);
    EN_setcurve(ph, c2, x, y, 2);
    EN_getcurveindex(ph, "C3", &c3);
    EN_setcurvevalue(ph, c3, 2, 20.0, 80000.0);

    for (k = 0; k < NTANKS; k++) {
        EN_getnodeindex(ph, TANK_ID[k], &index);
        EN_getnodevalue(ph, index, EN_INITVOLUME, &init_vol[k]);
    }
    run_one_hour(ph, vol_t0, change);

    printf("       EN_INITVOLUME  volume at      level change in the hour (ft)\n");
    printf("Tank       (ft3)      t = 0 (ft3)   before edit  after edit  expected\n");
    for (k = 0; k < NTANKS; k++) {
        /* T1 keeps its area; T2 and T3 have twice the area */
        double expected = (k == 0) ? base_change[k] : base_change[k] / 2.0;
        int bad = fabs(init_vol[k] - vol_t0[k]) > 0.01 ||
                  fabs(change[k] - expected) > 0.01;
        printf("%-4s %12.1f %13.1f %12.3f %11.3f %9.3f   %s\n", TANK_ID[k],
               init_vol[k], vol_t0[k], base_change[k], change[k], expected,
               bad ? "<-- wrong" : "");
        wrong += bad;
    }

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d of 3 tanks keep a stale initial volume after the edit\n", wrong);
        return 1;
    }
    printf("PASS: every tank's volumes follow the edit\n");
    return 0;
}
