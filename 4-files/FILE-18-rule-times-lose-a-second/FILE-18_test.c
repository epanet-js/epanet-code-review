/*
 * FILE-18: clock times in rule premises are truncated and lose a second.
 *
 * In FILE-18_rule-times.inp, rule TIMED closes pipe P1 when SYSTEM TIME
 * reaches 8:00:01 and rule CLOCK closes pipe P2 when the CLOCKTIME
 * reaches 9:00:01 AM. Rules are checked every 360 s, so the first check
 * at or after each time is 8:06 and 9:06.
 *
 * The test prints the times the parser stored, runs the hydraulics step
 * by step to find when each pipe closes, then saves the project and
 * prints the saved premises.
 *
 * Correct behaviour: the times are stored as 28,801 and 32,401 s, P1
 * closes at 8:06 and P2 at 9:06, and the saved file keeps the :01.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Time value of the first premise of a rule, in seconds */
static double premise_time(EN_Project ph, int rule)
{
    int logop, object, index, variable, relop, status;
    double value = -1.0;

    EN_getpremise(ph, rule, 1, &logop, &object, &index, &variable, &relop,
                  &status, &value);
    return value;
}

static void print_hms(const char *what, long t, long expected)
{
    printf("%s closes at %ld:%02ld:%02ld (expected %ld:%02ld:%02ld)\n", what,
           t / 3600, t % 3600 / 60, t % 60,
           expected / 3600, expected % 3600 / 60, expected % 60);
}

int main(void)
{
    EN_Project ph;
    int p1, p2;
    long t, tstep, closed1 = -1, closed2 = -1;
    double status, x1, x2;
    char line[256];
    FILE *f;

    EN_createproject(&ph);
    EN_open(ph, "FILE-18_rule-times.inp", "FILE-18.rpt", "");
    EN_getlinkindex(ph, "P1", &p1);
    EN_getlinkindex(ph, "P2", &p2);

    x1 = premise_time(ph, 1);
    x2 = premise_time(ph, 2);
    printf("TIMED premise stored as %.17g s (expected 28801)\n", x1);
    printf("CLOCK premise stored as %.17g s (expected 32401)\n", x2);

    /* Run the hydraulics and note when each pipe is first closed */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        EN_getlinkvalue(ph, p1, EN_STATUS, &status);
        if (status == EN_CLOSED && closed1 < 0) closed1 = t;
        EN_getlinkvalue(ph, p2, EN_STATUS, &status);
        if (status == EN_CLOSED && closed2 < 0) closed2 = t;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);
    print_hms("P1", closed1, 29160);
    print_hms("P2", closed2, 32760);

    /* Show the premises as they are saved */
    EN_saveinpfile(ph, "FILE-18_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);
    f = fopen("FILE-18_saved.inp", "r");
    while (f && fgets(line, sizeof line, f)) {
        if (strncmp(line, "IF", 2) == 0) printf("  saved: %s", line);
    }
    if (f) fclose(f);

    if (x1 != 28801 || x2 != 32401 || closed1 != 29160 || closed2 != 32760) {
        printf("FAIL: the rule times lost a second and the rules fired early\n");
        return 1;
    }
    printf("PASS: rule times are kept to the second\n");
    return 0;
}
