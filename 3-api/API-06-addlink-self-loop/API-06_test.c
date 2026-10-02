/*
 * API-06: EN_addlink accepts a link from a node to itself.
 *
 * API-06_three-junctions.inp is reservoir R1 feeding junctions J1, J2 and J3
 * in a chain. We add pipe P4 with J3 at both ends, then solve the hydraulics.
 *
 * Correct behaviour: EN_addlink returns error 222 ("same start and end nodes
 * for link"), as EN_setlinknodes and the input file parser already do.
 * Without the fix, EN_addlink returns 0 and EN_solveH (in EN_openH) reads
 * past a heap buffer while it orders the nodes for the sparse solver.
 * Under AddressSanitizer that is reported as a heap-buffer-overflow.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int p4, err;

    /* Unbuffered, so the output is not lost if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "API-06_three-junctions.inp", "API-06.rpt", "");

    err = EN_addlink(ph, "P4", EN_PIPE, "J3", "J3", &p4);
    printf("EN_addlink(P4 from J3 to J3) returned %d (expected 222)\n", err);

    /* With the self-loop in place, this is where the unpatched library
       overruns a heap buffer */
    printf("Solving hydraulics ...\n");
    printf("EN_solveH returned %d\n", EN_solveH(ph));

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 222) {
        printf("FAIL: EN_addlink accepted a pipe whose two ends are the same node\n");
        return 1;
    }
    printf("PASS: EN_addlink refuses a link from a node to itself with error 222\n");
    return 0;
}
