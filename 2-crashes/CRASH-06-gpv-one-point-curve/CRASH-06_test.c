/*
 * CRASH-06: a one-point GPV head loss curve reads out of bounds.
 *
 * CRASH-06_one-point-curve.inp has one GPV whose head loss curve C1 has a
 * single point. The solver interpolates on the segment of the curve that
 * brackets the flow, and a one-point curve has no segment, so curvecoeff()
 * reads X[-1] or X[1] of a one-element array.
 *
 * Correct behaviour: EN_solveH refuses to run (error 110, with the reason
 * in the report). Without the fix the sanitizer build stops with a heap
 * buffer overflow inside curvecoeff().
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err, v1;
    double headloss;

    EN_createproject(&ph);
    EN_open(ph, "CRASH-06_one-point-curve.inp", "CRASH-06.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);

    err = EN_solveH(ph);
    printf("EN_solveH returned %d\n", err);

    if (err == 0) {
        /* Only reached without sanitizers: the result is built on garbage */
        EN_getlinkvalue(ph, v1, EN_HEADLOSS, &headloss);
        printf("V1 head loss = %f ft\n", headloss);
        EN_close(ph);
        EN_deleteproject(ph);
        printf("FAIL: a one-point GPV curve was used for interpolation\n");
        return 1;
    }

    EN_close(ph);
    EN_deleteproject(ph);
    if (err != 110) {
        printf("FAIL: expected error 110, got %d\n", err);
        return 1;
    }
    printf("PASS: a one-point GPV curve is reported as an error (110) and the "
           "solver does not run\n");
    return 0;
}
