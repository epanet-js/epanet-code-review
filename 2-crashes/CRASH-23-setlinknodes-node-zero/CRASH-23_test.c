/*
 * CRASH-23: EN_setlinknodes accepts node index 0.
 *
 * CRASH-23_parallel-pipes.inp: reservoir R1 feeds J1, and J1 feeds J2 through
 * two parallel pipes, P2 and P3. J2 lies above R1's head, so the solution has
 * negative pressures. Node indices start at 1. We call
 *     EN_setlinknodes(P3, 0, J2)
 * and then solve the hydraulics.
 *
 * Correct behaviour: node 0 is rejected with error 203 ("undefined node"),
 * as the other node functions do, and P3 keeps its nodes. Without the fix
 * the call returns 0 and P3 starts at node 0, an unused slot. The solver's
 * warning (negative pressures) makes EPANET check for disconnected nodes,
 * and that check counts node 0 as one more node than its list can hold:
 * AddressSanitizer reports a heap-buffer-overflow.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int p3, j1, j2, n1, n2, err;

    /* Unbuffered, so the output is not lost if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-23_parallel-pipes.inp", "CRASH-23.rpt", "");
    EN_getlinkindex(ph, "P3", &p3);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "J2", &j2);

    err = EN_setlinknodes(ph, p3, 0, j2);
    EN_getlinknodes(ph, p3, &n1, &n2);
    printf("EN_setlinknodes(P3, 0, J2) returned %d (expected 203)\n", err);
    printf("P3 now runs from node %d to node %d (J1 is %d, J2 is %d)\n",
           n1, n2, j1, j2);

    printf("Solving hydraulics ...\n");
    printf("EN_solveH returned %d (6 = negative pressures warning)\n", EN_solveH(ph));

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 203) {
        printf("FAIL: EN_setlinknodes accepted node index 0\n");
        return 1;
    }
    printf("PASS: node index 0 is rejected with error 203\n");
    return 0;
}
