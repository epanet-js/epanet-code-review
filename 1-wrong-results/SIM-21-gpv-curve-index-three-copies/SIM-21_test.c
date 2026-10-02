/*
 * SIM-21: a GPV's curve index is stored in three places and only one is
 * kept current.
 *
 * In SIM-21_gpv.inp, GPV V1 carries 100 gpm and uses curve C2 (20 ft).
 * C1 gives 5 ft and C3 gives 80 ft. Each case reopens the file:
 *   1. Delete C1. C2 moves to index 1; V1 should still lose 20 ft, also
 *      after saving and reopening the project.
 *   2. Give V1 a simple control, then assign C3 with EN_GPV_CURVE. V1
 *      should lose 80 ft.
 *   3. Delete C2, V1's own curve. V1 then has no curve, so the solver
 *      should refuse to run (error 110) rather than use another curve.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int failures = 0;

/* Solves the project; returns the error code and V1's head loss */
static int solve(EN_Project ph, double *headloss)
{
    int v1, err;

    EN_getlinkindex(ph, "V1", &v1);
    err = EN_solveH(ph);
    *headloss = 0.0;
    if (err == 0) EN_getlinkvalue(ph, v1, EN_HEADLOSS, headloss);
    return err;
}

static void check(const char *what, double expected, double actual)
{
    int bad = fabs(actual - expected) > 0.01;
    printf("%-46s %8.2f %8.2f   %s\n", what, expected, actual,
           bad ? "<-- wrong" : "");
    failures += bad;
}

int main(void)
{
    EN_Project ph;
    int v1, index, control, err;
    double hl, curve;

    EN_createproject(&ph);
    printf("%-46s %8s %8s\n", "V1 head loss (ft)", "Expected", "Actual");

    /* 1. Delete another curve: the remaining curves move down one index */
    EN_open(ph, "SIM-21_gpv.inp", "SIM-21.rpt", "");
    EN_getcurveindex(ph, "C1", &index);
    EN_deletecurve(ph, index);
    solve(ph, &hl);
    check("1. after deleting C1", 20.0, hl);
    EN_saveinpfile(ph, "SIM-21_saved.inp");
    EN_close(ph);
    EN_open(ph, "SIM-21_saved.inp", "SIM-21.rpt", "");
    solve(ph, &hl);
    check("1. after deleting C1, saved and reopened", 20.0, hl);
    EN_close(ph);

    /* 2. A control on the GPV, then a new curve */
    EN_open(ph, "SIM-21_gpv.inp", "SIM-21.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);
    EN_addcontrol(ph, EN_TIMER, v1, EN_SET_OPEN, 0, 0, &control);
    EN_getcurveindex(ph, "C3", &index);
    EN_setlinkvalue(ph, v1, EN_GPV_CURVE, index);
    solve(ph, &hl);
    check("2. with a control, after assigning C3", 80.0, hl);
    EN_close(ph);

    /* 3. Delete the GPV's own curve */
    EN_open(ph, "SIM-21_gpv.inp", "SIM-21.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);
    EN_getcurveindex(ph, "C2", &index);
    EN_deletecurve(ph, index);
    EN_getlinkvalue(ph, v1, EN_GPV_CURVE, &curve);
    err = solve(ph, &hl);
    printf("3. after deleting C2: EN_GPV_CURVE = %.0f, EN_solveH returned %d, "
           "head loss %.2f\n", curve, err, hl);
    if (err != 110) {
        printf("   <-- wrong: the solver ran V1 with another curve\n");
        failures++;
    }
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 4 cases used the wrong curve for the GPV\n", failures);
        return 1;
    }
    printf("PASS: the GPV always uses the curve that EN_GPV_CURVE reports\n");
    return 0;
}
