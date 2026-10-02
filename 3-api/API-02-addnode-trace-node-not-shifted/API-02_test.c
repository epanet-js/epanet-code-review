/*
 * API-02: EN_addnode does not shift the water quality trace node.
 *
 * In API-02_two-sources.inp reservoir River supplies junction J1 and
 * reservoir Lake supplies junction J2. The project traces water from Lake,
 * so J1 should show 0 % and J2 100 %.
 *
 * We add junction J3 (and pipe P4 to connect it). Junctions come first in
 * the node list, so River and Lake move up one place and the trace node
 * index must move with them.
 *
 * Correct behaviour: the trace node is still Lake after EN_addnode, and the
 * trace results are unchanged.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Run hydraulics and water quality, print the trace node and the percentage
   of water from it at J1 and J2. Returns 1 if the trace node is Lake. */
static int run_trace(EN_Project ph)
{
    int qual_type, trace_node, j1, j2;
    double pct1, pct2;
    char id[EN_MAXID + 1];

    EN_getqualtype(ph, &qual_type, &trace_node);
    EN_getnodeid(ph, trace_node, id);

    EN_solveH(ph);
    EN_solveQ(ph);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "J2", &j2);
    EN_getnodevalue(ph, j1, EN_QUALITY, &pct1);
    EN_getnodevalue(ph, j2, EN_QUALITY, &pct2);

    printf("  trace node is node %d (%s); J1 gets %.0f %% and J2 %.0f %% from it\n",
           trace_node, id, pct1, pct2);
    return strcmp(id, "Lake") == 0;
}

int main(void)
{
    EN_Project ph;
    int j3, p4, ok;

    EN_createproject(&ph);
    EN_open(ph, "API-02_two-sources.inp", "API-02.rpt", "");

    printf("Before adding a junction (expected: Lake, J1 0 %%, J2 100 %%):\n");
    run_trace(ph);

    EN_addnode(ph, "J3", EN_JUNCTION, &j3);
    EN_addlink(ph, "P4", EN_PIPE, "J2", "J3", &p4);
    printf("After adding junction J3 as node %d:\n", j3);
    ok = run_trace(ph);

    EN_close(ph);
    EN_deleteproject(ph);

    if (!ok) {
        printf("FAIL: after EN_addnode the trace runs from a different node, "
               "with no error\n");
        return 1;
    }
    printf("PASS: the trace node is still Lake after EN_addnode\n");
    return 0;
}
