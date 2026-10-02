/*
 * CRASH-25: EN_setflowunits does not check its argument.
 *
 * The flow unit codes run from EN_CFS (0) to EN_CMS (10). We open a GPM
 * network and call EN_setflowunits with 11, one past the last code.
 *
 * Correct behaviour: the call is rejected with error 251 (invalid parameter
 * code), as EN_init does for the same argument, and the project keeps its
 * GPM units. The unpatched library accepts the code and indexes the table
 * of flow-unit names with it, reading past its end.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err, units;

    EN_createproject(&ph);
    EN_open(ph, "CRASH-25_network.inp", "CRASH-25.rpt", "");

    printf("EN_setflowunits(ph, 11) ...\n");
    fflush(stdout);
    err = EN_setflowunits(ph, 11);
    EN_getflowunits(ph, &units);
    printf("  returned %d (expected 251); flow units code is now %d (expected %d, GPM)\n",
           err, units, EN_GPM);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 251 || units != EN_GPM) {
        printf("FAIL: an out-of-range flow units code was accepted\n");
        return 1;
    }
    printf("PASS: the out-of-range code is rejected and the units are unchanged\n");
    return 0;
}
