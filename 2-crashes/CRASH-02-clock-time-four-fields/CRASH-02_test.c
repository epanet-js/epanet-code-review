/*
 * CRASH-02: a clock time with four or more fields writes past a 3-element array.
 *
 * The input file sets DURATION to 1:2:3:4. hour() in input2.c splits a clock
 * time at the colons into double y[3], but its loop allows a fourth field and
 * writes it to y[3], past the end of the array. Under AddressSanitizer the
 * EN_open call below aborts with a stack-buffer-overflow.
 *
 * Correct behaviour: a time with more than three fields is not a valid
 * hh:mm:ss time, so EN_open rejects the file with error 200 (the report
 * shows error 213, invalid option value, for the [TIMES] line).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Print the report lines that mention an error */
static void print_errors(const char *rptfile)
{
    char line[512];
    FILE *f = fopen(rptfile, "r");

    if (f == NULL) return;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "Error")) printf("  report: %s\n", line);
    }
    fclose(f);
}

int main(void)
{
    EN_Project ph;
    long duration;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, "CRASH-02_four-field-duration.inp", "CRASH-02.rpt", "");
    printf("EN_open returned %d (expected 200)\n", err);

    if (err == 0) {
        /* The file was accepted: show what the bad time was read as */
        EN_gettimeparam(ph, EN_DURATION, &duration);
        printf("DURATION 1:2:3:4 was read as %ld s\n", duration);
    }
    EN_close(ph);
    EN_deleteproject(ph);
    print_errors("CRASH-02.rpt");

    if (err != 200) {
        printf("FAIL: the four-field clock time 1:2:3:4 was accepted\n");
        return 1;
    }
    printf("PASS: the four-field clock time is rejected with an input error\n");
    return 0;
}
