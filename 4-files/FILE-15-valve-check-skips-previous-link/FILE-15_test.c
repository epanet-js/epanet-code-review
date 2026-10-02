/*
 * FILE-15: the illegal-valve check skips the link read just before the valve.
 *
 * Both input files have two PRVs, V1 (J1 -> J2) and V2 (J3 -> J2), that end
 * at the same node. EPANET does not allow this: two PRVs sharing a downstream
 * node give error 220 ("illegal valve connection to another valve"), as
 * EN_addlink does for the same valve. In the first file the two valves are on
 * consecutive lines; in the second, a pipe is read between them.
 *
 * Correct behaviour: both files are rejected with error 200 (error 220 in
 * the report). With the bug, the consecutive version loads without error,
 * because valvedata() tells valvecheck() to skip link index Nlinks, which
 * is V1, the link read before V2.
 */
#include <stdio.h>
#include "epanet2_2.h"

/* Open a file and return the error code from EN_open */
static int open_file(const char *inpfile)
{
    EN_Project ph;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, inpfile, "FILE-15.rpt", "");
    EN_close(ph);
    EN_deleteproject(ph);
    printf("%-30s EN_open returned %3d (expected 200)\n", inpfile, err);
    return err;
}

int main(void)
{
    int err_consecutive = open_file("FILE-15_consecutive-prvs.inp");
    int err_separated = open_file("FILE-15_separated-prvs.inp");

    if (err_consecutive != 200 || err_separated != 200) {
        printf("FAIL: the illegal PRV pair is accepted when the valves are on "
               "consecutive lines\n");
        return 1;
    }
    printf("PASS: the illegal PRV pair is rejected in both files\n");
    return 0;
}
