/*
 * CRASH-30: time and option setters convert out-of-range values to integers
 * with undefined behaviour.
 *
 * Part 1 passes values that are too large for an int, or NaN, to setters of
 * integer-valued options and properties, using Net1. Each converts the
 * double with (int) or ROUND() before any range check, which is undefined
 * behaviour in C; UndefinedBehaviorSanitizer stops at the first call. It
 * also sets PATTERN START to LONG_MAX seconds, which EN_settimeparam accepts
 * and timestep() later overflows. Correct: each call returns an error code.
 *
 * Part 2 sets TRIALS to INT_MAX, a valid int, with UNBALANCED CONTINUE 10,
 * through the API and from CRASH-30_trials-int-max.inp. hydsolve() adds the
 * two and overflows. Correct: the solve gives the same pressure as with the
 * default trials.
 */
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "epanet2_2.h"

static int failures = 0;

/* Print the result of a call that should be refused */
static void expect_error(const char *call, int err)
{
    printf("  %-50s returned %3d%s\n", call, err, err ? "" : "   <-- accepted");
    if (err == 0) failures++;
}

/* Solve a single period and return the pressure at a node */
static double pressure(EN_Project ph, const char *node)
{
    int k;
    double p;

    EN_settimeparam(ph, EN_DURATION, 0);
    EN_solveH(ph);
    EN_getnodeindex(ph, node, &k);
    EN_getnodevalue(ph, k, EN_PRESSURE, &p);
    return p;
}

/* Compare a solve at TRIALS = INT_MAX with one at the default trials */
static void check_trials(EN_Project ph, const char *what, const char *node)
{
    double p_max, p_ref;

    p_max = pressure(ph, node);
    EN_setoption(ph, EN_TRIALS, 200);
    p_ref = pressure(ph, node);
    printf("  %-50s %s pressure %.2f psi (%.2f with 200 trials)%s\n", what, node,
           p_max, p_ref, fabs(p_max - p_ref) < 0.01 ? "" : "   <-- wrong");
    if (fabs(p_max - p_ref) >= 0.01) failures++;
}

int main(void)
{
    EN_Project ph;
    int j10, t2, pump9, pipe10;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-30_Net1.inp", "CRASH-30.rpt", "");
    EN_getnodeindex(ph, "10", &j10);
    EN_getnodeindex(ph, "2", &t2);
    EN_getlinkindex(ph, "9", &pump9);
    EN_getlinkindex(ph, "10", &pipe10);

    printf("Part 1: out-of-range values, each should be refused\n");
    expect_error("EN_setoption(EN_TRIALS, 1e10)", EN_setoption(ph, EN_TRIALS, 1e10));
    expect_error("EN_setoption(EN_CHECKFREQ, NAN)", EN_setoption(ph, EN_CHECKFREQ, NAN));
    expect_error("EN_setoption(EN_UNBALANCED, 1e300)",
                 EN_setoption(ph, EN_UNBALANCED, 1e300));
    expect_error("EN_setoption(EN_DEMANDPATTERN, 1e300)",
                 EN_setoption(ph, EN_DEMANDPATTERN, 1e300));
    expect_error("EN_setnodevalue(junction 10, EN_PATTERN, 1e300)",
                 EN_setnodevalue(ph, j10, EN_PATTERN, 1e300));
    expect_error("EN_setnodevalue(tank 2, EN_MIXMODEL, NAN)",
                 EN_setnodevalue(ph, t2, EN_MIXMODEL, NAN));
    expect_error("EN_setlinkvalue(pump 9, EN_PUMP_HCURVE, 1e300)",
                 EN_setlinkvalue(ph, pump9, EN_PUMP_HCURVE, 1e300));
    expect_error("EN_setlinkvalue(pipe 10, EN_INITSTATUS, 1e300)",
                 EN_setlinkvalue(ph, pipe10, EN_INITSTATUS, 1e300));
    expect_error("EN_settimeparam(EN_PATTERNSTART, LONG_MAX)",
                 EN_settimeparam(ph, EN_PATTERNSTART, LONG_MAX));
    EN_close(ph);
    EN_deleteproject(ph);

    printf("Part 2: TRIALS = INT_MAX with UNBALANCED CONTINUE 10\n");
    EN_createproject(&ph);
    EN_open(ph, "CRASH-30_Net1.inp", "CRASH-30.rpt", "");
    EN_setoption(ph, EN_TRIALS, INT_MAX);
    EN_setoption(ph, EN_UNBALANCED, 10);
    check_trials(ph, "set through EN_setoption (Net1):", "10");
    EN_close(ph);
    EN_deleteproject(ph);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-30_trials-int-max.inp", "CRASH-30.rpt", "");
    check_trials(ph, "read from CRASH-30_trials-int-max.inp:", "J1");
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures > 0) {
        printf("FAIL: %d out-of-range values were accepted or gave a wrong result\n",
               failures);
        return 1;
    }
    printf("PASS: out-of-range values are refused; TRIALS = INT_MAX solves normally\n");
    return 0;
}
