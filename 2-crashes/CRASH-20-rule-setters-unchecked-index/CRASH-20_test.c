/*
 * CRASH-20: rule premise and action setters accept any object index.
 *
 * The network has 3 nodes and 2 links. The test adds rule R1 (IF tank T1
 * level > 15 THEN close P2 ELSE open P2), then calls the premise and action
 * setters with a node index of 4, a link index of 3, and a premise that
 * tests the flow of a node. It then runs the hydraulics.
 *
 * Correct behaviour: each call returns an error (203 undefined node, 204
 * undefined link, 251 invalid parameter code), the rule is unchanged and
 * the run completes. Without the fix every call returns 0, and the rule
 * check during EN_solveH reads past the end of the node arrays.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

static int wrong = 0;   /* calls that did not return the expected error */

static void check(const char *call, int err, int expected)
{
    printf("%-46s returned %3d (expected %d)\n", call, err, expected);
    if (err != expected) wrong++;
}

int main(void)
{
    EN_Project ph;
    int err, nnodes, nlinks, t1;
    char rule[120];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-20_tank.inp", "CRASH-20.rpt", "");
    EN_getcount(ph, EN_NODECOUNT, &nnodes);
    EN_getcount(ph, EN_LINKCOUNT, &nlinks);
    EN_getnodeindex(ph, "T1", &t1);

    strcpy(rule, "RULE R1\nIF TANK T1 LEVEL > 15\n"
                 "THEN PIPE P2 STATUS = CLOSED\nELSE PIPE P2 STATUS = OPEN");
    EN_addrule(ph, rule);

    /* A link variable (flow) for a node */
    check("EN_setpremise(node T1, EN_R_FLOW)",
          EN_setpremise(ph, 1, 1, 1, EN_R_NODE, t1, EN_R_FLOW, EN_R_GT, 0, 15.0), 251);

    /* Object indexes one past the last node or link */
    check("EN_setpremise(node 4, EN_R_LEVEL)",
          EN_setpremise(ph, 1, 1, 1, EN_R_NODE, nnodes + 1, EN_R_LEVEL, EN_R_GT, 0, 15.0),
          203);
    check("EN_setpremiseindex(node 4)",
          EN_setpremiseindex(ph, 1, 1, nnodes + 1), 203);
    check("EN_setthenaction(link 3)",
          EN_setthenaction(ph, 1, 1, nlinks + 1, EN_R_IS_CLOSED, EN_MISSING), 204);
    check("EN_setelseaction(link 3)",
          EN_setelseaction(ph, 1, 1, nlinks + 1, EN_R_IS_OPEN, EN_MISSING), 204);

    /* The rule is evaluated at every hydraulic time step */
    err = EN_solveH(ph);
    printf("EN_solveH returned %d\n", err);

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong > 0) {
        printf("FAIL: %d of 5 calls accepted an invalid premise or action\n", wrong);
        return 1;
    }
    printf("PASS: invalid premises and actions are rejected\n");
    return 0;
}
