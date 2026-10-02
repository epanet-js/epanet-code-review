/*
 * CRASH-22: EN_setcurvevalue reads past the curve before range-checking the
 * point index.
 *
 * The test builds a project with EN_init and gives it a 3-point curve:
 * (0, 100), (1000, 80), (2000, 50). The function's comment in epanet.c says
 * that a point index past the end of the curve adds a new point. We call
 * EN_setcurvevalue with point index 5 (two past the end) twice:
 *   - x = 500 is below the last point's x, so it must be refused with
 *     error 230 (x-values must increase);
 *   - x = 3000 is valid, so it must be added as point 4.
 * Without the fix, the calls compare x with X[3], which lies past the
 * 3-point array. AddressSanitizer reports a heap-buffer-overflow.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int c, npts, err_low, err_high;
    double x[] = {0, 1000, 2000};
    double y[] = {100, 80, 50};
    double x4, y4;

    /* Unbuffered, so the output is not lost if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_init(ph, "CRASH-22.rpt", "", EN_GPM, EN_HW);
    EN_addcurve(ph, "C1");
    EN_getcurveindex(ph, "C1", &c);
    EN_setcurve(ph, c, x, y, 3);

    err_low = EN_setcurvevalue(ph, c, 5, 500.0, 60.0);
    printf("EN_setcurvevalue(C1, point 5, x = 500) returned %d (expected 230)\n",
           err_low);

    err_high = EN_setcurvevalue(ph, c, 5, 3000.0, 20.0);
    printf("EN_setcurvevalue(C1, point 5, x = 3000) returned %d (expected 0)\n",
           err_high);

    EN_getcurvelen(ph, c, &npts);
    EN_getcurvevalue(ph, c, 4, &x4, &y4);
    printf("C1 now has %d points; point 4 is (%.0f, %.0f) (expected 4 points, "
           "point 4 = (3000, 20))\n", npts, x4, y4);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err_low != 230 || err_high != 0 || npts != 4 || x4 != 3000.0) {
        printf("FAIL: EN_setcurvevalue checked a point index past the end of the "
               "curve against the wrong point\n");
        return 1;
    }
    printf("PASS: a point index past the end is checked against the last point "
           "and appended\n");
    return 0;
}
