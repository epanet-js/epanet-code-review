/*
 * FILE-03: FILLTIME and DRAINTIME rule values are written in a form the
 * parser rejects.
 *
 * FILE-03_filltime-rules.inp has two rules on tank T1: "FILLTIME BELOW 1.5"
 * and "DRAINTIME > 2.25", both in hours as the manual describes. The test
 * opens the file, saves it with EN_saveinpfile, prints the premises that
 * were written and opens the saved file again.
 *
 * Correct behaviour: the saved file opens and both premise values are the
 * same as before (EN_getpremise returns them in seconds).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Value tested by the first premise of a rule (seconds for FILLTIME) */
static double premise_value(EN_Project ph, int rule)
{
    int logop, object, index, variable, relop, status;
    double value = -1.0;

    EN_getpremise(ph, rule, 1, &logop, &object, &index, &variable, &relop,
                  &status, &value);
    return value;
}

int main(void)
{
    EN_Project ph;
    int err;
    double before[2], after[2] = {-1.0, -1.0};
    char line[256];
    FILE *f;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-03_filltime-rules.inp", "FILE-03.rpt", "");
    printf("EN_open(original) returned %d\n", err);
    before[0] = premise_value(ph, 1);
    before[1] = premise_value(ph, 2);
    EN_saveinpfile(ph, "FILE-03_saved.inp");
    EN_close(ph);

    /* Show the premises as they were saved */
    f = fopen("FILE-03_saved.inp", "r");
    while (f && fgets(line, sizeof line, f)) {
        if (strncmp(line, "IF", 2) == 0) printf("  saved: %s", line);
    }
    if (f) fclose(f);

    err = EN_open(ph, "FILE-03_saved.inp", "FILE-03.rpt", "");
    printf("EN_open(saved) returned %d\n", err);
    if (err == 0) {
        after[0] = premise_value(ph, 1);
        after[1] = premise_value(ph, 2);
    }
    EN_close(ph);
    EN_deleteproject(ph);

    printf("FILLTIME premise:  %.0f s before, %.0f s after\n", before[0], after[0]);
    printf("DRAINTIME premise: %.0f s before, %.0f s after\n", before[1], after[1]);

    if (err != 0 || after[0] != before[0] || after[1] != before[1]) {
        printf("FAIL: the saved FILLTIME and DRAINTIME rules do not load\n");
        return 1;
    }
    printf("PASS: the saved rules load with the same values\n");
    return 0;
}
