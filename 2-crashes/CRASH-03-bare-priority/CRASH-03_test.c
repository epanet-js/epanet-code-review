/*
 * CRASH-03: a bare PRIORITY with no value dereferences a null token.
 *
 * Rule R1 in CRASH-03_bare-priority.inp ends with a PRIORITY line that has
 * no value. The test opens the file with EN_open, then opens it again with
 * EN_openX (which keeps the network) and passes a rule with the same bare
 * PRIORITY line to EN_addrule.
 *
 * Correct behaviour: EN_open returns 200 (errors in the input file) and
 * EN_addrule returns 250 (invalid format). Without the fix, newpriority()
 * passes a NULL token to strtod() and the program crashes in EN_open.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err_open, err_add, nrules;
    char rule[100];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 1. The input file */
    EN_createproject(&ph);
    err_open = EN_open(ph, "CRASH-03_bare-priority.inp", "CRASH-03.rpt", "");
    printf("EN_open returned %d (expected 200)\n", err_open);
    EN_close(ph);

    /* 2. The toolkit: EN_openX keeps the network and drops the bad rule */
    EN_openX(ph, "CRASH-03_bare-priority.inp", "CRASH-03.rpt", "");
    strcpy(rule, "RULE R2\nIF TANK T1 LEVEL > 15\n"
                 "THEN PIPE P2 STATUS = CLOSED\nPRIORITY");
    err_add = EN_addrule(ph, rule);
    EN_getcount(ph, EN_RULECOUNT, &nrules);
    printf("EN_addrule returned %d (expected 250), rule count %d\n", err_add, nrules);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err_open != 200 || err_add != 250 || nrules != 0) {
        printf("FAIL: a bare PRIORITY line was not rejected\n");
        return 1;
    }
    printf("PASS: a bare PRIORITY line is reported as an error\n");
    return 0;
}
