/*
 * SIM-18: EN_setqualtype scales initial quality by 1/28.3 when switching to
 * CHEM, and never converts the limiting concentration (EN_CONCENLIMIT).
 *
 * Part A loads Net1 (chlorine; node 10 starts at 0.5 mg/L), sets a limiting
 * concentration of 2 mg/L, switches to water age and back to chlorine, and
 * then starts a chlorine run. Part B starts an empty project with EN_init (no
 * quality analysis), sets a junction's initial quality to 1 and switches to
 * chlorine.
 *
 * Correct behaviour: switching the analysis type does not change the values
 * a program reads back, and the run starts from the file's 0.5 mg/L.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int wrong = 0;

/* Prints one row and counts it if the value is not the expected one */
static void check(const char *step, const char *what, double actual, double expected)
{
    int bad = fabs(actual - expected) > 1e-6;
    printf("%-32s %-13s %9.4f %9.4f   %s\n", step, what, actual, expected,
           bad ? "<-- wrong" : "");
    wrong += bad;
}

int main(void)
{
    EN_Project ph;
    int n10, j;
    long t;
    double c0, climit, c;

    printf("%-32s %-13s %9s %9s\n", "Step", "Value", "Actual", "Expected");

    /* Part A: Net1, CHEM -> AGE -> CHEM */
    EN_createproject(&ph);
    EN_open(ph, "SIM-18_Net1.inp", "SIM-18.rpt", "");
    EN_getnodeindex(ph, "10", &n10);
    EN_setoption(ph, EN_CONCENLIMIT, 2.0);

    EN_getnodevalue(ph, n10, EN_INITQUAL, &c0);
    EN_getoption(ph, EN_CONCENLIMIT, &climit);
    check("Net1 as loaded (chlorine)", "INITQUAL(10)", c0, 0.5);
    check("", "CONCENLIMIT", climit, 2.0);

    EN_setqualtype(ph, EN_AGE, "", "", "");
    EN_getnodevalue(ph, n10, EN_INITQUAL, &c0);
    EN_getoption(ph, EN_CONCENLIMIT, &climit);
    check("after EN_setqualtype(EN_AGE)", "INITQUAL(10)", c0, 0.5);
    check("", "CONCENLIMIT", climit, 2.0);

    EN_setqualtype(ph, EN_CHEM, "Chlorine", "mg/L", "");
    EN_getnodevalue(ph, n10, EN_INITQUAL, &c0);
    EN_getoption(ph, EN_CONCENLIMIT, &climit);
    check("after EN_setqualtype(EN_CHEM)", "INITQUAL(10)", c0, 0.5);
    check("", "CONCENLIMIT", climit, 2.0);

    /* The run uses the stored value, so it starts from the wrong chlorine */
    EN_solveH(ph);
    EN_openQ(ph);
    EN_initQ(ph, EN_NOSAVE);
    EN_runQ(ph, &t);
    EN_getnodevalue(ph, n10, EN_QUALITY, &c);
    check("chlorine run, time 0", "QUALITY(10)", c, 0.5);
    EN_closeQ(ph);
    EN_close(ph);

    /* Part B: no quality analysis -> CHEM */
    EN_init(ph, "SIM-18.rpt", "", EN_GPM, EN_HW);
    EN_addnode(ph, "J1", EN_JUNCTION, &j);
    EN_setnodevalue(ph, j, EN_INITQUAL, 1.0);
    EN_getnodevalue(ph, j, EN_INITQUAL, &c0);
    check("EN_init, then INITQUAL = 1", "INITQUAL(J1)", c0, 1.0);

    EN_setqualtype(ph, EN_CHEM, "Chlorine", "mg/L", "");
    EN_getnodevalue(ph, j, EN_INITQUAL, &c0);
    check("after EN_setqualtype(EN_CHEM)", "INITQUAL(J1)", c0, 1.0);

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong > 0) {
        printf("FAIL: %d values changed when the quality type was switched\n", wrong);
        return 1;
    }
    printf("PASS: switching the quality type keeps the values in user units\n");
    return 0;
}
