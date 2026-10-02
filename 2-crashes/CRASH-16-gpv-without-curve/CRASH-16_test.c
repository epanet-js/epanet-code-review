/*
 * CRASH-16: a GPV with no head loss curve crashes the hydraulic solver.
 *
 * Part 1 opens CRASH-16_network.inp, adds a GPV from J1 to J2 with
 * EN_addlink and does not assign it a curve, so its curve index is 0.
 * Part 2 opens CRASH-16_gpv-line-without-curve.inp, whose [VALVES] line
 * has no curve field; the parser accepts it with the same curve index 0.
 *
 * Correct behaviour: EN_solveH refuses to run (error 110, with the reason
 * in the report) instead of reading the unused curve slot Curve[0].
 * Without the fix the sanitizer build stops inside curvecoeff().
 */
#include <stdio.h>
#include "epanet2_2.h"

/* Solves the project and returns 1 if the solver correctly refused to run */
static int refuses_to_run(EN_Project ph)
{
    int err = EN_solveH(ph);

    printf("  EN_solveH returned %d\n", err);
    return err == 110;
}

int main(void)
{
    EN_Project ph;
    int v1, ok1, ok2;
    double curve;

    EN_createproject(&ph);

    printf("Part 1: GPV added with EN_addlink\n");
    EN_open(ph, "CRASH-16_network.inp", "CRASH-16.rpt", "");
    EN_addlink(ph, "V1", EN_GPV, "J1", "J2", &v1);
    EN_getlinkvalue(ph, v1, EN_GPV_CURVE, &curve);
    printf("  EN_GPV_CURVE of the new GPV = %.0f (no curve)\n", curve);
    ok1 = refuses_to_run(ph);
    EN_close(ph);

    printf("Part 2: GPV line without a curve in the input file\n");
    EN_open(ph, "CRASH-16_gpv-line-without-curve.inp", "CRASH-16.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);
    EN_getlinkvalue(ph, v1, EN_GPV_CURVE, &curve);
    printf("  EN_GPV_CURVE of V1 = %.0f (no curve)\n", curve);
    ok2 = refuses_to_run(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (!ok1 || !ok2) {
        printf("FAIL: the solver ran a GPV that has no head loss curve\n");
        return 1;
    }
    printf("PASS: a GPV without a curve is reported as an error (110) and the "
           "solver does not run\n");
    return 0;
}
