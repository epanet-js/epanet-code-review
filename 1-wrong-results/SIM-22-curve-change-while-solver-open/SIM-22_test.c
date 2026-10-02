/*
 * SIM-22: changing a GPV or pump curve while the solver is open has no
 * effect.
 *
 * In SIM-22_pump-gpv.inp, pump PU1 (curve PC1, 50 ft at 100 gpm) and GPV
 * V1 (curve GC1, 10 ft at 100 gpm) carry a fixed 100 gpm in series.
 * With the hydraulic solver open, the test:
 *   1. assigns V1 curve GC2 (30 ft) and PU1 curve PC2 (80 ft), and re-solves
 *   2. replaces PC2's data with EN_setcurve (120 ft), and re-solves
 *   3. restarts the run with EN_initH
 *   4. closes the solver, saves the project, reopens and solves it
 *
 * Correct behaviour: every change is used from the next EN_runH on, survives
 * EN_initH, and is saved.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int pu1, v1, failures = 0;

/* Re-solves the current period and checks the pump gain and GPV loss */
static void check(EN_Project ph, const char *step, double pump_ft, double gpv_ft)
{
    long t;
    double pump_hl, gpv_hl;

    EN_runH(ph, &t);
    EN_getlinkvalue(ph, pu1, EN_HEADLOSS, &pump_hl);   /* negative: a gain */
    EN_getlinkvalue(ph, v1, EN_HEADLOSS, &gpv_hl);

    int bad = fabs(-pump_hl - pump_ft) > 0.01 || fabs(gpv_hl - gpv_ft) > 0.01;
    printf("%-30s %6.1f %8.2f   %6.1f %8.2f   %s\n", step, pump_ft, -pump_hl,
           gpv_ft, gpv_hl, bad ? "<-- wrong" : "");
    failures += bad;
}

int main(void)
{
    EN_Project ph;
    int pc2, gc2;
    double x[1] = {100}, y[1] = {120};

    EN_createproject(&ph);
    EN_open(ph, "SIM-22_pump-gpv.inp", "SIM-22.rpt", "");
    EN_getlinkindex(ph, "PU1", &pu1);
    EN_getlinkindex(ph, "V1", &v1);
    EN_getcurveindex(ph, "PC2", &pc2);
    EN_getcurveindex(ph, "GC2", &gc2);

    printf("                               Pump gain (ft)    GPV loss (ft)\n");
    printf("                               Expect  Actual   Expect  Actual\n");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    check(ph, "As loaded", 50, 10);

    EN_setlinkvalue(ph, v1, EN_GPV_CURVE, gc2);
    EN_setheadcurveindex(ph, pu1, pc2);
    check(ph, "1. GC2 and PC2 assigned", 80, 30);

    EN_setcurve(ph, pc2, x, y, 1);
    check(ph, "2. PC2 changed to 120 ft", 120, 30);

    EN_initH(ph, EN_NOSAVE);
    check(ph, "3. after EN_initH", 120, 30);
    EN_closeH(ph);

    EN_saveinpfile(ph, "SIM-22_saved.inp");
    EN_close(ph);
    EN_open(ph, "SIM-22_saved.inp", "SIM-22.rpt", "");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    check(ph, "4. saved and reopened", 120, 30);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 4 steps ignore a curve change made while the solver "
               "was open\n", failures);
        return 1;
    }
    printf("PASS: curve changes made while the solver is open are used, kept "
           "after EN_initH and saved\n");
    return 0;
}
