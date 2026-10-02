/*
 * FILE-19: the documented default for TRIALS is 40; the code uses 200.
 *
 * doc/toolkit-input.dox says "TRIALS ... The default is 40." The code sets
 * MaxIter to MAXITER, which is 200 (src/input1.c), and has done so since
 * EPANET 2.00.12; the EPANET 2.2 Users Manual also gives 200.
 *
 * The test opens a network whose [OPTIONS] section does not set TRIALS and
 * reads the value EPANET uses. It passes when that value is 200, the
 * intended default. The fix corrects the documentation only, so the test
 * passes with and without it; it records the value the documentation
 * should state.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    double trials = 0.0;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-19_no-trials-option.inp", "FILE-19.rpt", "");
    EN_getoption(ph, EN_TRIALS, &trials);
    EN_close(ph);
    EN_deleteproject(ph);

    printf("EN_open returned %d\n", err);
    printf("Default TRIALS used by EPANET:   %.0f\n", trials);
    printf("Default stated in toolkit-input.dox at 83e25bb: 40\n");

    if (err != 0 || trials != 200.0) {
        printf("FAIL: the default TRIALS is %.0f, not 200\n", trials);
        return 1;
    }
    printf("PASS: the default TRIALS is 200, as the corrected documentation states\n");
    return 0;
}
