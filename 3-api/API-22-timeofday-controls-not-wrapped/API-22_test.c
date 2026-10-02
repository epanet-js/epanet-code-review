/*
 * API-22: TIMEOFDAY controls added through the API are not reduced modulo
 * 24 hours.
 *
 * Net1 is started at 8:00 AM, and a TIMEOFDAY control that closes pipe 111
 * at midnight is added with EN_addcontrol, giving the clock time as 24:00
 * (86400 s). The input-file parser accepts "CLOCKTIME 24:00" and reduces it
 * to 0:00. We run the hydraulics and note when pipe 111 closes, then do the
 * same after saving the project with EN_saveinpfile and opening the file.
 *
 * Correct behaviour: in both runs the pipe closes 16 hours into the run, at
 * midnight.
 */
#include <stdio.h>
#include "epanet2_2.h"

/* Runs the hydraulics; returns the time pipe 111 first closes, or -1 */
static long time_pipe_closes(EN_Project ph)
{
    int pipe;
    long t, tstep, closed_at = -1;
    double status;

    EN_getlinkindex(ph, "111", &pipe);
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        EN_getlinkvalue(ph, pipe, EN_STATUS, &status);
        if (status == EN_CLOSED && closed_at < 0) closed_at = t;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);
    return closed_at;
}

static void print_result(const char *what, long closed_at)
{
    if (closed_at < 0)
        printf("%-34s pipe 111 never closes\n", what);
    else
        printf("%-34s pipe 111 closes at %2ld:%02ld into the run\n", what,
               closed_at / 3600, closed_at % 3600 / 60);
}

int main(void)
{
    EN_Project ph;
    int pipe, idx;
    long api_run, file_run;

    EN_createproject(&ph);
    EN_open(ph, "API-22_Net1.inp", "API-22.rpt", "");
    EN_settimeparam(ph, EN_STARTTIME, 8 * 3600);           /* start at 8:00 AM */
    EN_getlinkindex(ph, "111", &pipe);
    EN_addcontrol(ph, EN_TIMEOFDAY, pipe, 0.0, 0, 24 * 3600.0, &idx);  /* 0 = closed */

    api_run = time_pipe_closes(ph);
    print_result("Control added with EN_addcontrol:", api_run);

    /* The same project, saved and read back by the input-file parser */
    EN_saveinpfile(ph, "API-22_saved.inp");
    EN_close(ph);
    EN_open(ph, "API-22_saved.inp", "API-22_saved.rpt", "");
    file_run = time_pipe_closes(ph);
    print_result("Same control from the saved file:", file_run);
    EN_close(ph);
    EN_deleteproject(ph);

    if (api_run != 16 * 3600) {
        printf("FAIL: the TIMEOFDAY control for 24:00 added through the API "
               "never fires\n");
        return 1;
    }
    printf("PASS: the TIMEOFDAY control for 24:00 fires at midnight\n");
    return 0;
}
