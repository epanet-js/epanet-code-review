/*
 * CRASH-26: a rule premise that compares a node variable with a status
 * reads past the link arrays.
 *
 * Only links have a status, but the [RULES] parser accepts OPEN, CLOSED or
 * ACTIVE as the value of any premise. Rule R1 in CRASH-26_level-is-open.inp
 * reads "IF TANK T1 LEVEL IS OPEN". T1 is node 3 and the network has two
 * links, so the rule check reads LinkStatus[3], one past the end.
 *
 * The test opens the file and runs the hydraulics. It then opens the file
 * with EN_openX (which keeps the network) and passes the same premise to
 * EN_addrule.
 *
 * Correct behaviour: EN_open returns 200 (error in the input file) and
 * EN_addrule returns 202 (illegal numeric value).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err_open, err_add;
    char rule[100];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 1. The input file */
    EN_createproject(&ph);
    err_open = EN_open(ph, "CRASH-26_level-is-open.inp", "CRASH-26.rpt", "");
    printf("EN_open returned %d (expected 200)\n", err_open);
    if (err_open == 0) {
        /* Rules are checked at every hydraulic time step */
        printf("EN_solveH returned %d\n", EN_solveH(ph));
    }
    EN_close(ph);

    /* 2. The toolkit: EN_openX keeps the network but drops the bad rule */
    EN_openX(ph, "CRASH-26_level-is-open.inp", "CRASH-26.rpt", "");
    strcpy(rule, "RULE R2\nIF TANK T1 LEVEL IS OPEN\nTHEN PIPE P2 STATUS = CLOSED");
    err_add = EN_addrule(ph, rule);
    printf("EN_addrule returned %d (expected 202)\n", err_add);
    EN_close(ph);
    EN_deleteproject(ph);

    if (err_open != 200 || err_add != 202) {
        printf("FAIL: a status was accepted for a node variable\n");
        return 1;
    }
    printf("PASS: a status for a node variable is rejected\n");
    return 0;
}
