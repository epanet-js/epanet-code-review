/*
 * API-07: EN_deletelink does not decrement the pipe count for check-valve pipes.
 *
 * API-07_cv-pipe.inp has three pipes; P3 has a check valve. We delete P3,
 * run the network and write a full report to a second report file with
 * EN_report. The pipe count is not available through EN_getcount; its only
 * use is the "Number of Pipes" line of the report's input summary, so the
 * test reads it from there.
 *
 * Correct behaviour: after P3 is deleted the summary reports 2 pipes.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Return the number on the "Number of Pipes" line of a report file, or -1 */
static int pipes_in_summary(const char *filename)
{
    char line[256];
    int n = -1;
    FILE *f = fopen(filename, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "Number of Pipes") != NULL) {
            sscanf(strrchr(line, '.') + 1, "%d", &n);
        }
    }
    if (f != NULL) fclose(f);
    return n;
}

int main(void)
{
    EN_Project ph;
    int p3, type, npipes;

    EN_createproject(&ph);
    EN_open(ph, "API-07_cv-pipe.inp", "API-07.rpt", "");

    EN_getlinkindex(ph, "P3", &p3);
    EN_getlinktype(ph, p3, &type);
    printf("Deleting P3 (link type %d, EN_CVPIPE = %d)\n", type, EN_CVPIPE);
    EN_deletelink(ph, p3, EN_UNCONDITIONAL);

    /* A second report file gets the input summary as well as the results */
    EN_setreport(ph, "FILE API-07_after.rpt");
    EN_solveH(ph);
    EN_solveQ(ph);
    EN_report(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    /* EN_open wrote the first summary to the primary report file */
    printf("Summary written by EN_open:       %d pipes\n",
           pipes_in_summary("API-07.rpt"));
    npipes = pipes_in_summary("API-07_after.rpt");
    printf("Summary written after the delete: %d pipes (expected 2)\n", npipes);

    if (npipes != 2) {
        printf("FAIL: deleting a check-valve pipe left the pipe count at %d\n", npipes);
        return 1;
    }
    printf("PASS: the pipe count drops when a check-valve pipe is deleted\n");
    return 0;
}
