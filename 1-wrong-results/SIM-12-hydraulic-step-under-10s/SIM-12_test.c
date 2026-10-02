/*
 * SIM-12: a hydraulic step under 10 s hangs a water quality run and divides
 * by zero in rule timing.
 *
 * Both input files have a 5-second hydraulic step, a 1-minute duration and
 * no quality or rule time step, so EPANET sets those to 1/10 of the
 * hydraulic step.
 *   Part 1, SIM-12_quality.inp: a chlorine run. EN_solveQ never returns in
 *           unpatched EPANET; an alarm stops the test after 10 s.
 *   Part 2, SIM-12_rules.inp: a run with one rule. Unpatched EPANET divides
 *           by zero when timing the rule.
 *
 * Correct behaviour: the default quality and rule steps are at least 1 s,
 * EN_solveQ finishes, and the run with the rule reaches the 1-minute end.
 */
#include <stdio.h>
#include "epanet2_2.h"

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>

/* Print a message, then let the alarm end the process as usual (a hang) */
static void on_alarm(int sig)
{
    const char msg[] = "\nHANG: EN_solveQ is still running after 10 s "
                       "(a quality step of 0 s never advances)\n";
    write(STDOUT_FILENO, msg, sizeof msg - 1);
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

/* Opens a file and prints the time steps EPANET derived from it */
static EN_Project open_project(const char *inpfile, const char *rptfile)
{
    EN_Project ph;
    long hstep, qstep, rulestep;

    EN_createproject(&ph);
    EN_open(ph, inpfile, rptfile, "");
    EN_gettimeparam(ph, EN_HYDSTEP, &hstep);
    EN_gettimeparam(ph, EN_QUALSTEP, &qstep);
    EN_gettimeparam(ph, EN_RULESTEP, &rulestep);
    printf("%s: hydraulic step %ld s, quality step %ld s, rule step %ld s\n",
           inpfile, hstep, qstep, rulestep);
    return ph;
}

int main(void)
{
    EN_Project ph;
    long t, tstep, dur, last = 0;
    int err;

    /* Part 1: water quality run */
    ph = open_project("SIM-12_quality.inp", "SIM-12_quality.rpt");
    EN_solveH(ph);
    fflush(stdout);
#ifndef _WIN32
    signal(SIGALRM, on_alarm);
    alarm(10);
#endif
    err = EN_solveQ(ph);
#ifndef _WIN32
    alarm(0);
#endif
    printf("  EN_solveQ returned %d\n", err);
    EN_close(ph);
    EN_deleteproject(ph);

    /* Part 2: hydraulics with a rule, step by step */
    ph = open_project("SIM-12_rules.inp", "SIM-12_rules.rpt");
    EN_gettimeparam(ph, EN_DURATION, &dur);
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        last = t;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    printf("  last hydraulic solution at %ld s (duration %ld s)\n", last, dur);

    if (last != dur) {
        printf("FAIL: the run with a rule stopped at %ld s instead of %ld s\n",
               last, dur);
        return 1;
    }
    printf("PASS: the quality run finishes and the run with a rule reaches %ld s\n", dur);
    return 0;
}
