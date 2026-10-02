/*
 * API-01: EN_addnode shifts rule premises after incrementing the junction count.
 *
 * API-01_tank-rule.inp has junctions J1 and J2 (nodes 1 and 2), reservoir R1
 * (node 3), tank T1 (node 4) and one rule:
 *     IF RESERVOIR R1 HEAD > 100 AND TANK T1 LEVEL > 15 THEN PIPE P3 STATUS IS CLOSED
 * We add junction J3 with EN_addnode. Junctions come first in the node list,
 * so R1 and T1 move up to nodes 4 and 5, and the rule premises must follow them.
 *
 * Correct behaviour: after the new junction is added, premise 1 still refers
 * to R1 and premise 2 to T1.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Print the node that premise number 'premise' of rule 1 refers to.
   Returns 1 if it is the expected node. */
static int check_premise(EN_Project ph, int premise, const char *expected)
{
    int logop, object, node, variable, relop, status;
    double value;
    char id[EN_MAXID + 1];

    EN_getpremise(ph, 1, premise, &logop, &object, &node, &variable, &relop,
                  &status, &value);
    EN_getnodeid(ph, node, id);
    printf("  premise %d refers to node %d (%s), expected %s\n",
           premise, node, id, expected);
    return strcmp(id, expected) == 0;
}

int main(void)
{
    EN_Project ph;
    int j3, ok1, ok2;

    EN_createproject(&ph);
    EN_open(ph, "API-01_tank-rule.inp", "API-01.rpt", "");

    printf("Before adding a junction:\n");
    check_premise(ph, 1, "R1");
    check_premise(ph, 2, "T1");

    EN_addnode(ph, "J3", EN_JUNCTION, &j3);
    printf("After adding junction J3 (now node %d):\n", j3);
    ok1 = check_premise(ph, 1, "R1");
    ok2 = check_premise(ph, 2, "T1");

    EN_close(ph);
    EN_deleteproject(ph);

    if (!ok1 || !ok2) {
        printf("FAIL: after EN_addnode the rule tests the head of the new junction "
               "instead of reservoir R1\n");
        return 1;
    }
    printf("PASS: both rule premises still refer to R1 and T1\n");
    return 0;
}
