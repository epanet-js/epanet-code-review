/*
 * CRASH-07: a network with an isolated component overflows SortedNodes in a
 * water quality run.
 *
 * The network is a reservoir feeding one junction, plus a separate pipe J2-J3
 * that is not connected to anything. It has 4 nodes, 2 links and 1 tank or
 * reservoir. The quality solver sizes its sorted-node array for
 * Nlinks + Ntanks + 1 = 4 entries, but the topological sort stores one entry
 * per node, at positions 1 to Nnodes = 4.
 *
 * Correct behaviour: EN_solveQ completes without touching memory outside its
 * arrays. Without the fix, AddressSanitizer reports a heap-buffer-overflow in
 * sortnodes(); without sanitizers the write goes unnoticed.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int nnodes, nlinks, ntanks, errH, errQ;

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-07_isolated-pipe.inp", "CRASH-07.rpt", "");

    EN_getcount(ph, EN_NODECOUNT, &nnodes);
    EN_getcount(ph, EN_LINKCOUNT, &nlinks);
    EN_getcount(ph, EN_TANKCOUNT, &ntanks);
    printf("Nodes %d, links %d, tanks and reservoirs %d\n", nnodes, nlinks, ntanks);
    printf("SortedNodes entries needed:     Nnodes + 1          = %d\n", nnodes + 1);
    printf("SortedNodes entries at 83e25bb: Nlinks + Ntanks + 1 = %d\n",
           nlinks + ntanks + 1);

    /* The isolated pipe only gives hydraulic warnings (J2 disconnected) */
    errH = EN_solveH(ph);
    printf("EN_solveH returned %d\n", errH);

    /* Under AddressSanitizer the unpatched library aborts in here */
    errQ = EN_solveQ(ph);
    printf("EN_solveQ returned %d\n", errQ);

    EN_close(ph);
    EN_deleteproject(ph);

    if (errH > 100 || errQ > 100) {
        printf("FAIL: the run stopped with an error\n");
        return 1;
    }
    printf("PASS: the quality run completes on a network with an isolated component\n");
    return 0;
}
