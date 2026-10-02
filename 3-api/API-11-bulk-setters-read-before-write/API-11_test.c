/*
 * API-11: the bulk setters read every node before writing, so source and
 * tank properties cannot be set.
 *
 * The network has junctions J1 and J2 (nodes 1 and 2), reservoir R1 (3) and
 * tanks T1 and T2 (4 and 5), and no quality sources. We call
 * EN_setnodevalues with
 *   A. EN_TANKDIAM: new diameters for the two tanks; the entries for the
 *      junctions and the reservoir are placeholders;
 *   B. EN_SOURCEPAT with an invalid pattern index at node 3, which must
 *      fail and remove the sources it created at nodes 1 to 3;
 *   C. EN_SOURCEQUAL: a source strength for every node.
 *
 * Correct behaviour: A and C succeed and set the values; B returns 205 with
 * badIndex 3 and leaves no sources behind.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define NNODES 5

static int count_sources(EN_Project ph)
{
    int i, count = 0;
    double v;
    for (i = 1; i <= NNODES; i++) {
        /* Error 240 means the node has no source */
        if (EN_getnodevalue(ph, i, EN_SOURCEQUAL, &v) == 0) count++;
    }
    return count;
}

int main(void)
{
    EN_Project ph;
    double diams[NNODES] = {0, 0, 0, 60, 70};
    double pats[NNODES] = {0, 0, 99, 0, 0};          /* there is no pattern 99 */
    double quals[NNODES] = {0.5, 1.0, 1.5, 2.0, 2.5};
    double d4, d5, q2, q5;
    int err, bad, wrong = 0;

    EN_createproject(&ph);
    EN_open(ph, "API-11_network.inp", "API-11.rpt", "");

    /* A. A tank-only property */
    err = EN_setnodevalues(ph, EN_TANKDIAM, diams, &bad);
    EN_getnodevalue(ph, 4, EN_TANKDIAM, &d4);
    EN_getnodevalue(ph, 5, EN_TANKDIAM, &d5);
    printf("A. EN_TANKDIAM:   returned %3d, badIndex %d; T1 %.1f ft, T2 %.1f ft"
           "   (expected 0, 0; 60.0, 70.0)\n", err, bad, d4, d5);
    if (err != 0 || fabs(d4 - 60) > 1e-9 || fabs(d5 - 70) > 1e-9) wrong++;

    /* B. A source property with a bad value: everything must be undone */
    err = EN_setnodevalues(ph, EN_SOURCEPAT, pats, &bad);
    int sources = count_sources(ph);
    printf("B. EN_SOURCEPAT:  returned %3d, badIndex %d; nodes with a source: %d"
           "   (expected 205, 3; 0)\n", err, bad, sources);
    if (err != 205 || bad != 3 || sources != 0) wrong++;

    /* C. A source property */
    err = EN_setnodevalues(ph, EN_SOURCEQUAL, quals, &bad);
    EN_getnodevalue(ph, 2, EN_SOURCEQUAL, &q2);
    EN_getnodevalue(ph, 5, EN_SOURCEQUAL, &q5);
    printf("C. EN_SOURCEQUAL: returned %3d, badIndex %d; J2 %.1f, T2 %.1f"
           "               (expected 0, 0; 1.0, 2.5)\n", err, bad, q2, q5);
    if (err != 0 || fabs(q2 - 1.0) > 1e-9 || fabs(q5 - 2.5) > 1e-9) wrong++;

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d of 3 bulk assignments did not behave as expected\n", wrong);
        return 1;
    }
    printf("PASS: tank and source properties can be bulk-set, and a failure is undone\n");
    return 0;
}
