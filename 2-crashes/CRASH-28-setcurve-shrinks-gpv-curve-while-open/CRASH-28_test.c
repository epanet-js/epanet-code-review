/*
 * CRASH-28: EN_setcurve can shrink a GPV's curve to one point while the
 * solver is open.
 *
 * In CRASH-28_gpv.inp, GPV V1 carries 1000 gpm on curve C1, which has two
 * points; C2 has one point. EN_openH checks that a GPV's curve has at least
 * two points (CRASH-06), but only once. With the solver open, the test:
 *   1. replaces C1 with a single point using EN_setcurve, and re-solves
 *   2. assigns the one-point curve C2 to V1 with EN_GPV_CURVE, and re-solves
 *
 * Correct behaviour: both calls are refused with an error, the valve keeps
 * its two-point curve, and the re-solve gives the same 10 ft head loss.
 * Without the fix the sanitizer build stops in curvecoeff() at step 1.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int v1, failures = 0;

/* Re-solves the current period and prints V1's head loss */
static void solve(EN_Project ph, const char *step, int err)
{
    long t;
    double hl;

    EN_runH(ph, &t);
    EN_getlinkvalue(ph, v1, EN_HEADLOSS, &hl);
    printf("%-36s call returned %3d   V1 head loss %6.2f ft\n", step, err, hl);

    /* The call must be refused and the 10 ft head loss kept */
    if (err == 0 || fabs(hl - 10.0) > 0.01) failures++;
}

int main(void)
{
    EN_Project ph;
    int c1, c2;
    long t;
    double hl, x[1] = {500}, y[1] = {10};

    EN_createproject(&ph);
    EN_open(ph, "CRASH-28_gpv.inp", "CRASH-28.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);
    EN_getcurveindex(ph, "C1", &c1);
    EN_getcurveindex(ph, "C2", &c2);

    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    EN_getlinkvalue(ph, v1, EN_HEADLOSS, &hl);
    printf("%-36s                     V1 head loss %6.2f ft\n", "As loaded", hl);

    solve(ph, "1. EN_setcurve(C1, one point)", EN_setcurve(ph, c1, x, y, 1));
    solve(ph, "2. EN_GPV_CURVE = C2 (one point)",
          EN_setlinkvalue(ph, v1, EN_GPV_CURVE, c2));

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 2 calls gave a GPV a one-point curve while the solver "
               "was open\n", failures);
        return 1;
    }
    printf("PASS: a GPV cannot be given a one-point curve while the solver is "
           "open\n");
    return 0;
}
