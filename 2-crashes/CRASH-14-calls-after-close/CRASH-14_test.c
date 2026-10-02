/*
 * CRASH-14: rule, control, ID and link-node functions work on freed memory
 * after EN_close.
 *
 * EN_close frees the network arrays but keeps the node, link, control and
 * rule counts. Most toolkit functions return 102 (no network data) once
 * the project is closed; the functions below have no such check, so their
 * index checks pass and they read or write the freed arrays.
 *
 * The test opens a network with one control and one rule, closes it and
 * calls each function. It then calls EN_close a second time, and calls
 * EN_addrule on a project that was never opened.
 *
 * Correct behaviour: every call returns 102, and the second EN_close is
 * harmless.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

static int wrong = 0;   /* calls that did not return 102 */

static void check(const char *call, int err)
{
    printf("%-22s returned %d\n", call, err);
    if (err != 102) wrong++;
}

int main(void)
{
    EN_Project ph;
    int n1, n2, n3, i1, i2, i3, i4, i5;   /* outputs that are not used */
    double x;
    char rule[100], id[EN_MAXID + 1];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-14_rule-and-control.inp", "CRASH-14.rpt", "");
    EN_close(ph);
    printf("After EN_close:\n");

    /* IDs and link end nodes */
    check("EN_setnodeid", EN_setnodeid(ph, 1, "NEWID"));
    check("EN_setlinkid", EN_setlinkid(ph, 1, "NEWID"));
    check("EN_setlinknodes", EN_setlinknodes(ph, 1, 2, 3));

    /* Simple controls */
    check("EN_deletecontrol", EN_deletecontrol(ph, 1));

    /* Rules */
    strcpy(rule, "RULE R2\nIF TANK T1 LEVEL > 5\nTHEN PIPE P2 STATUS = OPEN");
    check("EN_addrule", EN_addrule(ph, rule));
    check("EN_getrule", EN_getrule(ph, 1, &n1, &n2, &n3, &x));
    check("EN_getruleID", EN_getruleID(ph, 1, id));
    check("EN_getpremise", EN_getpremise(ph, 1, 1, &i1, &i2, &i3, &i4, &i5, &n1, &x));
    check("EN_setpremise", EN_setpremise(ph, 1, 1, 1, EN_R_NODE, 3, EN_R_LEVEL,
                                         EN_R_GT, 0, 10.0));
    check("EN_setpremiseindex", EN_setpremiseindex(ph, 1, 1, 2));
    check("EN_setpremisestatus", EN_setpremisestatus(ph, 1, 1, 0));
    check("EN_setpremisevalue", EN_setpremisevalue(ph, 1, 1, 10.0));
    check("EN_getthenaction", EN_getthenaction(ph, 1, 1, &i1, &i2, &x));
    check("EN_setthenaction", EN_setthenaction(ph, 1, 1, 2, EN_R_IS_OPEN, EN_MISSING));
    check("EN_getelseaction", EN_getelseaction(ph, 1, 1, &i1, &i2, &x));
    check("EN_setelseaction", EN_setelseaction(ph, 1, 1, 2, EN_R_IS_OPEN, EN_MISSING));
    check("EN_setrulepriority", EN_setrulepriority(ph, 1, 5.0));
    check("EN_deleterule", EN_deleterule(ph, 1));

    /* Closing twice frees the same arrays again */
    printf("EN_close again returned %d\n", EN_close(ph));
    EN_deleteproject(ph);

    /* A project that was never opened has no ID hash tables */
    printf("Before EN_open:\n");
    EN_createproject(&ph);
    strcpy(rule, "RULE R2\nIF TANK T1 LEVEL > 5\nTHEN PIPE P2 STATUS = OPEN");
    check("EN_addrule", EN_addrule(ph, rule));
    EN_deleteproject(ph);

    if (wrong > 0) {
        printf("FAIL: %d calls on a closed project did not return 102\n", wrong);
        return 1;
    }
    printf("PASS: every call on a closed project returns 102\n");
    return 0;
}
