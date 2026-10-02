/*
 * API-29: EN_addlink leaves a new pump's diameter and a new link's flow,
 * status and setting unset.
 *
 * API-29_booster.inp: pump PU1 lifts water from reservoir R1 to J1, then
 * pipes P1 (12 in) and P2 (8 in, the last link) carry it on to J2 and J3.
 * EN_addlink puts a new link in the last slot of the link arrays, which
 * after a deletion still holds the deleted link's values. Two checks:
 *
 * 1. Turn P1 into a 5 hp pump with EN_setlinktype (which deletes the pipe
 *    and adds a pump), solve and write the report, which has a Diameter
 *    column. A pump has no diameter: the parser sets 0, and PU1 reports
 *    0.00. The new pump must report 0.00 too.
 * 2. In a new project: solve, delete P2 and add a pressure-reducing valve
 *    V9 from J1 to J3. Before the next run V9 has no results: its flow
 *    must read 0, its status active (2) and its setting 0, the values
 *    EN_addlink gives a new PRV.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Return the Diameter column of a link's first line in the link table */
static double reported_diameter(const char *filename, const char *link)
{
    char line[256], id[64];
    double diam = -1.0;
    int in_table = 0;
    FILE *f = fopen(filename, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "Link Results") != NULL) in_table = 1;
        if (in_table && sscanf(line, "%63s %lf", id, &diam) == 2 &&
            strcmp(id, link) == 0) break;
        diam = -1.0;
    }
    if (f != NULL) fclose(f);
    return diam;
}

int main(void)
{
    EN_Project ph;
    int p1, p2, v9;
    double d_pu1, d_p1, q2, q9, status9, setting9;

    /* 1. A pump made with EN_setlinktype */
    EN_createproject(&ph);
    EN_open(ph, "API-29_booster.inp", "API-29.rpt", "");
    EN_getlinkindex(ph, "P1", &p1);
    EN_setlinktype(ph, &p1, EN_PUMP, EN_UNCONDITIONAL);
    EN_setlinkvalue(ph, p1, EN_PUMP_POWER, 5.0);
    EN_solveH(ph);
    EN_solveQ(ph);
    EN_report(ph);
    EN_close(ph);

    d_pu1 = reported_diameter("API-29.rpt", "PU1");
    d_p1 = reported_diameter("API-29.rpt", "P1");
    printf("1. Diameter in the report: PU1 %.2f in, P1 (now a pump) %.2f in "
           "(expected 0.00 for both)\n", d_pu1, d_p1);

    /* 2. A valve added after a run and a deletion */
    EN_open(ph, "API-29_booster.inp", "API-29_2.rpt", "");
    EN_solveH(ph);
    EN_getlinkindex(ph, "P2", &p2);
    EN_getlinkvalue(ph, p2, EN_FLOW, &q2);
    EN_deletelink(ph, p2, EN_UNCONDITIONAL);
    EN_addlink(ph, "V9", EN_PRV, "J1", "J3", &v9);
    EN_getlinkvalue(ph, v9, EN_FLOW, &q9);
    EN_getlinkvalue(ph, v9, EN_STATUS, &status9);
    EN_getlinkvalue(ph, v9, EN_SETTING, &setting9);
    printf("2. P2 carried %.2f gpm. New PRV V9 (link %d) before a run:\n", q2, v9);
    printf("   flow %.2f gpm, status %.0f, setting %.2f psi "
           "(expected 0.00 gpm, status 2 = active, 0.00 psi)\n", q9, status9, setting9);
    EN_close(ph);
    EN_deleteproject(ph);

    if (d_pu1 != 0.0 || d_p1 != 0.0 || q9 != 0.0 || status9 != 2.0 || setting9 != 0.0) {
        printf("FAIL: the new links carry values left over from other links\n");
        return 1;
    }
    printf("PASS: new links start with no diameter (pumps) and no results\n");
    return 0;
}
