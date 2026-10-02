/*
 * API-26: EN_STATUS, EN_SETTING and EN_DEMAND read leftover values before
 * a run.
 *
 * The network has an open pipe P1, a closed pipe P2, a pump PU1 at speed
 * 0.9 and a PRV V1 set to 40 psi. J2's demand comes from [JUNCTIONS] and
 * J3's from [DEMANDS]. Before any hydraulics are run we read
 *   A. each link's EN_STATUS and EN_SETTING, and compare them with its
 *      EN_INITSTATUS and EN_INITSETTING;
 *   B. the EN_DEMAND of J2 and J3.
 * Finally we show what flows read between EN_initH and EN_runH.
 *
 * Correct behaviour: before a run the current status and setting are the
 * initial ones, and no demand has been computed (0).
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

/* Compares a link's current status and setting with its initial ones */
static int check_link(EN_Project ph, const char *id)
{
    int i;
    double status, init_status, setting, init_setting;

    EN_getlinkindex(ph, id, &i);
    EN_getlinkvalue(ph, i, EN_STATUS, &status);
    EN_getlinkvalue(ph, i, EN_INITSTATUS, &init_status);
    EN_getlinkvalue(ph, i, EN_SETTING, &setting);
    EN_getlinkvalue(ph, i, EN_INITSETTING, &init_setting);
    int ok = status == init_status && fabs(setting - init_setting) < 1e-9;
    printf("%-4s %6g %12g %9g %13g   %s\n", id, status, init_status, setting,
           init_setting, ok ? "" : "<-- differs");
    return ok;
}

static double read_node(EN_Project ph, const char *id, int property)
{
    int i;
    double v;
    EN_getnodeindex(ph, id, &i);
    EN_getnodevalue(ph, i, property, &v);
    return v;
}

static double read_link(EN_Project ph, const char *id, int property)
{
    int i;
    double v;
    EN_getlinkindex(ph, id, &i);
    EN_getlinkvalue(ph, i, property, &v);
    return v;
}

int main(void)
{
    EN_Project ph;
    int wrong = 0;
    long t;
    double d2, d3;

    EN_createproject(&ph);
    EN_open(ph, "API-26_network.inp", "API-26.rpt", "");

    /* A. Status 0 = closed, 1 = open, 2 = active (valves) */
    printf("Before a run:\n");
    printf("Link EN_STATUS EN_INITSTATUS EN_SETTING EN_INITSETTING\n");
    wrong += !check_link(ph, "P1");
    wrong += !check_link(ph, "P2");
    wrong += !check_link(ph, "PU1");
    wrong += !check_link(ph, "V1");

    /* B. Demands */
    d2 = read_node(ph, "J2", EN_DEMAND);
    d3 = read_node(ph, "J3", EN_DEMAND);
    printf("J2 EN_DEMAND %g gpm, J3 EN_DEMAND %g gpm (expected 0, 0)\n", d2, d3);
    wrong += (d2 != 0.0) + (d3 != 0.0);

    /* Between EN_initH and EN_runH, flows are the solver's starting values */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    printf("\nAfter EN_initH:  P1 EN_FLOW %8.2f gpm, J3 EN_EMITTERFLOW %8.2f gpm"
           "  (starting values)\n", read_link(ph, "P1", EN_FLOW),
           read_node(ph, "J3", EN_EMITTERFLOW));
    EN_runH(ph, &t);
    printf("After EN_runH:   P1 EN_FLOW %8.2f gpm, J3 EN_EMITTERFLOW %8.2f gpm"
           "  (results)\n", read_link(ph, "P1", EN_FLOW),
           read_node(ph, "J3", EN_EMITTERFLOW));
    EN_closeH(ph);

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d of 6 values read before a run are leftovers\n", wrong);
        return 1;
    }
    printf("PASS: before a run, links read their initial status and setting, "
           "and demands read 0\n");
    return 0;
}
