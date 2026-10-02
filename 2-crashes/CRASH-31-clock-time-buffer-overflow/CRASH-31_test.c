/*
 * CRASH-31: a run of 1,000,000 hours or more overflows the 13-byte clock-time
 * buffers.
 *
 * clocktime() in report.c formats a time as "h:mm:ss" into Report.Atime,
 * which holds 12 characters and a terminator. From 1,000,000 hours on the
 * text is "1000000:00:00", 13 characters, so the terminator lands in the
 * next field of the struct, Rpt1Fname, the report file's name. No sanitizer
 * sees that. The status report also copies the text into a 13-byte array on
 * the stack, which AddressSanitizer reports.
 *
 * CRASH-31_million-hours.inp runs for 1,000,000 hours in 100,000-hour steps.
 * The test solves it twice: with the status report turned off, then with it
 * on, as in the file. Correct behaviour: both runs complete, EN_copyreport
 * can still reopen the report file by name, and the report and the saved
 * input file show the time 1000000:00:00.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Return 1 if the file has a line containing the text */
static int file_has(const char *file, const char *text)
{
    char line[512];
    int found = 0;
    FILE *f = fopen(file, "r");

    while (f && !found && fgets(line, sizeof line, f))
        found = strstr(line, text) != NULL;
    if (f) fclose(f);
    return found;
}

/* Solve the network and copy its report; return 1 if all went well */
static int run(int status_report)
{
    EN_Project ph;
    int err_solve, err_copy, err_save, ok;

    printf("Status report %s:\n", status_report ? "on" : "off");
    EN_createproject(&ph);
    EN_open(ph, "CRASH-31_million-hours.inp", "CRASH-31.rpt", "");
    if (!status_report) EN_setstatusreport(ph, EN_NO_REPORT);
    err_solve = EN_solveH(ph);

    /* EN_copyreport reopens the report file by its stored name */
    err_copy = EN_copyreport(ph, "CRASH-31_copy.rpt");
    err_save = EN_saveinpfile(ph, "CRASH-31_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);

    printf("  EN_solveH returned %d, EN_copyreport returned %d (expected 0, 0)\n",
           err_solve, err_copy);
    ok = err_solve == 0 && err_copy == 0;
    if (status_report) {
        int line_ok = file_has("CRASH-31_copy.rpt", "1000000:00:00: Balanced");
        printf("  Report has the status line for 1000000:00:00: %s\n",
               line_ok ? "yes" : "no");
        ok = ok && line_ok;
    }
    if (err_save == 0) {
        int saved_ok = file_has("CRASH-31_saved.inp", "1000000:00:00");
        printf("  Saved input file has DURATION 1000000:00:00: %s\n",
               saved_ok ? "yes" : "no");
        ok = ok && saved_ok;
    }
    return ok;
}

int main(void)
{
    int ok;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    ok = run(0);
    ok = run(1) && ok;

    if (!ok) {
        printf("FAIL: the clock time for 1,000,000 hours did not fit its buffer\n");
        return 1;
    }
    printf("PASS: a run of 1,000,000 hours is reported and saved correctly\n");
    return 0;
}
