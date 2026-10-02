/*
 * FILE-16: [PIPES], [PUMPS] and [VALVES] lines with too few fields are
 * silently dropped.
 *
 * FILE-16_short-lines.inp has seven links. Three of their lines are too
 * short to define a link: P2 and PU2 have no end node, and V2 has no
 * valve type. Each has a complete twin in parallel, so the network still
 * solves without them.
 *
 * Correct behaviour: EN_open rejects the file with error 200, and the
 * report has an "Error 201: syntax error" line for each short line, as
 * EPANET 2.2 did.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Counts the lines of the report file that contain the given text */
static int count_lines(const char *fname, const char *text)
{
    char line[1024];
    int count = 0;
    FILE *f = fopen(fname, "r");

    if (f == NULL) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, text)) count++;
    fclose(f);
    return count;
}

int main(void)
{
    const char *links[] = {"P1", "P2", "P3", "PU1", "PU2", "V1", "V2"};
    EN_Project ph;
    int err, index, missing = 0;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-16_short-lines.inp", "FILE-16.rpt", "");
    printf("EN_open returned %d\n", err);

    if (err == 0) {
        /* The file was accepted: see which links made it into the project */
        for (int i = 0; i < 7; i++) {
            int found = EN_getlinkindex(ph, links[i], &index) == 0;
            printf("  %-4s %s\n", links[i], found ? "loaded" : "<-- not in the project");
            missing += !found;
        }
        printf("EN_solveH returned %d\n", EN_solveH(ph));
        EN_close(ph);
        EN_deleteproject(ph);
        printf("FAIL: %d of 7 link lines were dropped without any error\n", missing);
        return 1;
    }
    EN_close(ph);    /* also needed after a failed EN_open: closes the report */
    EN_deleteproject(ph);

    /* The report should explain each rejected line */
    int errors = count_lines("FILE-16.rpt", "Error 201");
    printf("Report lines with \"Error 201\": %d\n", errors);
    if (err != 200 || errors != 3) {
        printf("FAIL: expected error 200 and three Error 201 lines\n");
        return 1;
    }
    printf("PASS: each short link line is reported as error 201\n");
    return 0;
}
