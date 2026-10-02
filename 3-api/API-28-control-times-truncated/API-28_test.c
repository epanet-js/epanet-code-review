/*
 * API-28: control times added through the API are truncated, not rounded.
 *
 * A host program keeps its schedule in hours and converts to seconds when it
 * adds the controls to Net1:
 *   TIMER      close pipe 111 after 8.2 hours        8.2 * 3600
 *   TIMEOFDAY  close pipe 122 at 4:20 AM             (4 + 20/60.0) * 3600
 * In double precision both products are a hair below the whole second
 * (29519.999999999996 and 15599.999999999998). We read the stored control
 * times back with EN_getcontrol and run the hydraulics to see when each pipe
 * closes. Then we try times that cannot be stored at all: 1e300 s and NaN.
 *
 * Correct behaviour: the times are rounded to the nearest second, so the
 * pipes close at 8:12:00 and 4:20:00, and the impossible times are refused
 * with error 202. (Unpatched, converting 1e300 to a long is undefined
 * behaviour, which UndefinedBehaviorSanitizer reports as a crash.)
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

typedef struct {
    const char *pipe_id;
    int type;            /* EN_TIMER or EN_TIMEOFDAY */
    double seconds;      /* time passed to EN_addcontrol */
    long expected;       /* the whole second meant */
    int pipe, control;
    long stored;         /* time stored, from EN_getcontrol */
    long closed_at;      /* first solution with the pipe closed */
} Schedule;

static void print_time(long t)
{
    printf("%2ld:%02ld:%02ld", t / 3600, t % 3600 / 60, t % 60);
}

int main(void)
{
    Schedule s[2] = {
        { "111", EN_TIMER,     8.2 * 3600,               29520, 0, 0, 0, -1 },
        { "122", EN_TIMEOFDAY, (4 + 20 / 60.0) * 3600,   15600, 0, 0, 0, -1 }
    };
    EN_Project ph;
    long t, tstep;
    int i, type, link, node, idx, err_big, err_nan, wrong = 0;
    double setting, level, status;

    EN_createproject(&ph);
    EN_open(ph, "API-28_Net1.inp", "API-28.rpt", "");
    for (i = 0; i < 2; i++) {
        EN_getlinkindex(ph, s[i].pipe_id, &s[i].pipe);
        EN_addcontrol(ph, s[i].type, s[i].pipe, 0.0, 0, s[i].seconds, &s[i].control);
        EN_getcontrol(ph, s[i].control, &type, &link, &setting, &node, &level);
        s[i].stored = (long)level;
    }

    /* Find when each pipe first shows as closed */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        for (i = 0; i < 2; i++) {
            EN_getlinkvalue(ph, s[i].pipe, EN_STATUS, &status);
            if (status == EN_CLOSED && s[i].closed_at < 0) s[i].closed_at = t;
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);

    printf("Control    Time passed in (s)   Stored   Pipe closes  Expected\n");
    for (i = 0; i < 2; i++) {
        printf("%-9s  %-18.17g   %6ld   ", s[i].type == EN_TIMER ? "TIMER" : "TIMEOFDAY",
               s[i].seconds, s[i].stored);
        print_time(s[i].closed_at);
        printf("     ");
        print_time(s[i].expected);
        printf("\n");
        if (s[i].stored != s[i].expected || s[i].closed_at != s[i].expected) wrong++;
    }

    /* Times that do not fit in a long must be refused (error 202) */
    fflush(stdout);
    err_big = EN_addcontrol(ph, EN_TIMER, s[0].pipe, 0.0, 0, 1e300, &idx);
    printf("EN_addcontrol with a time of 1e300 s returns %d (expected 202)\n", err_big);
    err_nan = EN_addcontrol(ph, EN_TIMER, s[0].pipe, 0.0, 0, NAN, &idx);
    printf("EN_addcontrol with a time of NaN returns %d (expected 202)\n", err_nan);
    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong || err_big != 202 || err_nan != 202) {
        printf("FAIL: %d of 2 controls act one second early, and %d of 2 impossible "
               "times were accepted\n", wrong, (err_big != 202) + (err_nan != 202));
        return 1;
    }
    printf("PASS: both control times are rounded to the second meant, and "
           "impossible times are refused\n");
    return 0;
}
