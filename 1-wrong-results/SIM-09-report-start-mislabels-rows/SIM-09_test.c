/*
 * SIM-09: a REPORT START that is not a multiple of REPORT STEP mislabels
 * every reported row.
 *
 * The network is EPANET's Net1 with Report Start 0:30 and Report Step 1:00,
 * so results are reported at 0:30, 1:30, 2:30, ... We run the hydraulics
 * step by step and record the times at which a solution is computed and the
 * pressure at junction 11. Then we run the full analysis and read the
 * reported pressures from the binary output file.
 *
 * Correct behaviour: a hydraulic solution is computed at every reporting
 * time, and each reported value is the solution at the time it is labelled.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"
#include "epanet_output.h"

#define MAXSTEPS 200

static long step_time[MAXSTEPS];      /* times of the hydraulic solutions, s */
static double step_p11[MAXSTEPS];     /* pressure at junction 11, psi */
static int nsteps = 0;

/* Returns the index of the first hydraulic solution at or after time t */
static int solution_at_or_after(long t)
{
    int i;
    for (i = 0; i < nsteps; i++)
        if (step_time[i] >= t) return i;
    return -1;
}

int main(void)
{
    EN_Project ph;
    ENR_Handle out;
    int n11, nvalues, nperiods, rstart, rstep, k, wrong = 0;
    long t, tstep;
    float *p;

    EN_createproject(&ph);
    EN_open(ph, "SIM-09_report-start.inp", "SIM-09.rpt", "SIM-09.out");
    EN_getnodeindex(ph, "11", &n11);

    /* Step through the hydraulics and record every solution */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        if (nsteps < MAXSTEPS) {
            step_time[nsteps] = t;
            EN_getnodevalue(ph, n11, EN_PRESSURE, &step_p11[nsteps]);
            nsteps++;
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);

    /* Full run, then read the reported pressures from the output file */
    EN_solveH(ph);
    EN_solveQ(ph);
    EN_report(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    ENR_init(&out);
    ENR_open(out, "SIM-09.out");
    ENR_getTimes(out, ENR_numPeriods, &nperiods);
    ENR_getTimes(out, ENR_reportStart, &rstart);
    ENR_getTimes(out, ENR_reportStep, &rstep);

    printf("Report    Reported   Solution at    First solution at or after it\n");
    printf("time      pressure   that time      (time, pressure)\n");
    for (k = 0; k < nperiods; k++) {
        long label = rstart + (long)k * rstep;
        int i = solution_at_or_after(label);
        int exact = i >= 0 && step_time[i] == label;
        double reported;

        ENR_getNodeAttribute(out, k, ENR_pressure, &p, &nvalues);
        reported = p[n11 - 1];
        ENR_free((void **)&p);
        if (i < 0) {
            wrong++;
            continue;
        }

        if (!exact || fabs(reported - step_p11[i]) > 0.01) wrong++;
        if (k < 4) {
            printf("%2ld:%02ld   %8.2f    %-12s   %2ld:%02ld  %7.2f\n",
                   label / 3600, label % 3600 / 60, reported,
                   exact ? "yes" : "none", step_time[i] / 3600,
                   step_time[i] % 3600 / 60, step_p11[i]);
        }
    }
    printf("...\n");
    ENR_close(&out);

    if (wrong) {
        printf("FAIL: %d of %d reported rows hold a solution from a later time "
               "than their label\n", wrong, nperiods);
        return 1;
    }
    printf("PASS: all %d reported rows hold the solution at the time they are labelled\n",
           nperiods);
    return 0;
}
