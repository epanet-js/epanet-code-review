/*
 * FILE-12: tokens past the 40th on a line are dropped.
 *
 * The input file defines a weekly demand pattern, WEEK, with all 168 hourly
 * factors on one line of 845 characters (the documented limit is 1024
 * characters per line). gettokens() in input2.c stops after MAXTOKS (40)
 * tokens and drops the rest with no error, so the pattern keeps only the
 * ID and its first 39 factors. The run then repeats those 39 hours instead
 * of following the week.
 *
 * Correct behaviour: the pattern has 168 factors, and factor 168 is 0.70.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err, pat, len = 0;
    double last = 0.0;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-12_weekly-pattern.inp", "FILE-12.rpt", "");
    printf("EN_open returned %d\n", err);

    EN_getpatternindex(ph, "WEEK", &pat);
    EN_getpatternlen(ph, pat, &len);
    if (len > 0) EN_getpatternvalue(ph, pat, len, &last);
    printf("Pattern WEEK: %d factors (expected 168), last factor %.2f (expected 0.70)\n",
           len, last);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 0 || len != 168) {
        printf("FAIL: %d of the 168 factors on the line were silently dropped\n",
               168 - len);
        return 1;
    }
    printf("PASS: all 168 factors on the line are read\n");
    return 0;
}
