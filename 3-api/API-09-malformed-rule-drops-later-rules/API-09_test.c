/*
 * API-09: one malformed rule in a file drops every rule after it.
 *
 * EN_openX opens an input file that has errors, skips the bad items and
 * keeps the rest. For [RULES] it is meant to drop only the malformed rule.
 *
 * API-09_five-rules.inp has rules R1 to R5. R2 has a misspelt status and
 * the RULE line of rule 4 has a label with a space. API-09_stray-clause.inp
 * has an IF clause before its first RULE line, then rules R1 and R2.
 *
 * Correct behaviour: EN_openX returns 200 and keeps every valid rule:
 * R1, R3 and R5 from the first file, R1 and R2 from the second.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Opens a file with EN_openX and lists its rules; returns 1 if they match */
static int check(const char *inpfile, const char *rptfile, const char *expected)
{
    EN_Project ph;
    int err, i, nrules = 0;
    char id[EN_MAXID + 1], loaded[200] = "";

    EN_createproject(&ph);
    err = EN_openX(ph, inpfile, rptfile, "");
    EN_getcount(ph, EN_RULECOUNT, &nrules);
    for (i = 1; i <= nrules; i++) {
        EN_getruleID(ph, i, id);
        strcat(loaded, " ");
        strcat(loaded, id);
    }
    EN_close(ph);
    EN_deleteproject(ph);

    printf("%s: EN_openX returned %d, rule count %d\n", inpfile, err, nrules);
    printf("  expected:%s\n", expected);
    printf("  loaded:  %s\n", loaded);
    return strcmp(loaded, expected) == 0;
}

int main(void)
{
    int ok = 1;

    ok &= check("API-09_five-rules.inp", "API-09_five-rules.rpt", " R1 R3 R5");
    ok &= check("API-09_stray-clause.inp", "API-09_stray-clause.rpt", " R1 R2");

    if (!ok) {
        printf("FAIL: valid rules after a malformed one were dropped\n");
        return 1;
    }
    printf("PASS: only the malformed rules were dropped\n");
    return 0;
}
