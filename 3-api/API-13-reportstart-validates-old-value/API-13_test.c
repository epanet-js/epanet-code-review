/*
 * API-13: EN_settimeparam(EN_REPORTSTART) validates the old value.
 *
 * The network runs for 3 hours with STATISTIC AVERAGE, and its hydraulics
 * halt at 1 hour (TRIALS 3 with UNBALANCED STOP; the demand jumps at 1 h).
 *   A. We set REPORT START to 4 h (beyond the duration), then back to 0.
 *   B. We set REPORT START to 2 h, run hydraulics and quality, and read the
 *      average pressure that EPANET wrote to the output file. The run halts
 *      before 2 h, so no period is reported.
 *
 * Correct behaviour: in A, 4 h is refused with error 213 and 0 is accepted.
 * In B, the output file contains no NaN: with no reported period there is
 * nothing to average.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"
#include "epanet_output.h"

int main(void)
{
    EN_Project ph;
    ENR_Handle out = NULL;
    int err_late, err_zero, err_open, periods = 0, wrong = 0;
    long rstart;
    float *pressure = NULL;
    int len = 0;
    double avg = 0.0;

    /* A. Refuse a report start after the end, accept 0 */
    EN_createproject(&ph);
    EN_open(ph, "API-13_halts.inp", "API-13.rpt", "");
    err_late = EN_settimeparam(ph, EN_REPORTSTART, 4 * 3600);
    err_zero = EN_settimeparam(ph, EN_REPORTSTART, 0);
    EN_gettimeparam(ph, EN_REPORTSTART, &rstart);
    EN_close(ph);
    printf("A. REPORTSTART 4 h (duration 3 h): returned %d (expected 213)\n", err_late);
    printf("   REPORTSTART 0:                  returned %d (expected 0); "
           "REPORTSTART is now %ld s\n", err_zero, rstart);
    if (err_late != 213 || err_zero != 0) wrong++;

    /* B. A run that halts before the report start, with STATISTIC AVERAGE */
    EN_open(ph, "API-13_halts.inp", "API-13.rpt", "API-13.out");
    EN_settimeparam(ph, EN_REPORTSTART, 2 * 3600);
    EN_solveH(ph);
    EN_solveQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    /* ENR_open returns 436 (and frees the handle) for a file with no results */
    ENR_init(&out);
    err_open = ENR_open(out, "API-13.out");
    if (err_open < 400) {
        ENR_getTimes(out, ENR_numPeriods, &periods);
        ENR_getNodeAttribute(out, 0, ENR_pressure, &pressure, &len);
        avg = pressure[0];
        ENR_free((void **)&pressure);
        ENR_close(&out);
        printf("B. Output file: %d period; average pressure at J1 = %g psi"
               "   (expected no NaN)\n", periods, avg);
        if (isnan(avg)) wrong++;
    }
    else {
        printf("B. Output file: ENR_open returned %d, no results"
               "   (expected no NaN)\n", err_open);
    }

    if (wrong) {
        printf("FAIL: %d of 2 checks failed\n", wrong);
        return 1;
    }
    printf("PASS: the new report start is validated, and no NaN reaches the "
           "output file\n");
    return 0;
}
