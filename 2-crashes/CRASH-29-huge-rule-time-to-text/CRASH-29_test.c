/*
 * CRASH-29: rule time values of 2^31 seconds or more are converted with
 * undefined behaviour.
 *
 * checktime() compares a SYSTEM TIME premise as (long)value every time the
 * rule is checked, and EN_saveinpfile writes it with gettimetxt(), which
 * starts with (int)seconds / 3600. Converting a double that does not fit
 * is undefined behaviour.
 *
 * The test adds two rules that close pipe P1 and should never fire in the
 * 6-hour run: LATE tests TIME >= 600000 hours (2.16e9 s, past the range of
 * an int) and NEVER tests TIME > 1e300 hours (past the range of a 64-bit
 * long). It runs the hydraulics, then deletes NEVER, saves the project,
 * opens the saved file and reads LATE's time back.
 *
 * Correct behaviour: the run completes with P1 open, and LATE's time reads
 * back unchanged. Without the fix, UndefinedBehaviorSanitizer
 * stops in checktime().
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#define LATE_SECONDS (600000.0 * 3600.0)

/* Value tested by the first premise of rule 1 (seconds for a time) */
static double premise_value(EN_Project ph)
{
    int logop, object, index, variable, relop, status;
    double value = -1.0;

    EN_getpremise(ph, 1, 1, &logop, &object, &index, &variable, &relop,
                  &status, &value);
    return value;
}

/* Adds a rule from a writable copy of its text (see CRASH-19) */
static int add_rule(EN_Project ph, const char *text)
{
    char rule[100];

    strcpy(rule, text);
    return EN_addrule(ph, rule);
}

int main(void)
{
    EN_Project ph;
    int err1, err2, err, p1;
    double status, after = -1.0;

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-29_tank.inp", "CRASH-29.rpt", "");
    EN_getlinkindex(ph, "P1", &p1);

    err1 = add_rule(ph, "RULE LATE\nIF SYSTEM TIME >= 600000\n"
                        "THEN PIPE P1 STATUS = CLOSED");
    err2 = add_rule(ph, "RULE NEVER\nIF SYSTEM TIME > 1e300\n"
                        "THEN PIPE P1 STATUS = CLOSED");
    printf("EN_addrule returned %d and %d, LATE time %.0f s\n", err1, err2,
           premise_value(ph));

    /* The rules are checked at every rule time step */
    err = EN_solveH(ph);
    EN_getlinkvalue(ph, p1, EN_STATUS, &status);
    printf("EN_solveH returned %d; at the end P1 is %s\n", err,
           status == EN_OPEN ? "open" : "closed");

    /* Save LATE only: 1e300 hours does not fit the 20-character buffer of
       the rule writer unless the CRASH-09 fix is applied */
    EN_deleterule(ph, 2);
    err = EN_saveinpfile(ph, "CRASH-29_saved.inp");
    printf("EN_saveinpfile returned %d\n", err);
    EN_close(ph);

    err = EN_open(ph, "CRASH-29_saved.inp", "CRASH-29.rpt", "");
    printf("EN_open(saved file) returned %d\n", err);
    if (err == 0) after = premise_value(ph);
    printf("LATE time: expected %.0f s, read back %.0f s\n", LATE_SECONDS, after);
    EN_close(ph);
    EN_deleteproject(ph);

    if (status != EN_OPEN || after != LATE_SECONDS) {
        printf("FAIL: the very large rule times were not handled\n");
        return 1;
    }
    printf("PASS: rule times of 2^31 s or more are checked, saved and read back\n");
    return 0;
}
