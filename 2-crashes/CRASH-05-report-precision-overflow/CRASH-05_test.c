/*
 * CRASH-05: PRECISION 7 or more overflows a 16-byte report buffer.
 *
 * CRASH-05_precision12.inp asks for heads with 12 decimal places. The test
 * runs the network and calls EN_report. The report writer formats each
 * value into char s1[16]; a head of about 800 ft with 12 decimals needs
 * 17 bytes. Under AddressSanitizer the unpatched library stops in
 * writenodetable() with a stack-buffer-overflow. Without the sanitizer the
 * test finds the 16-character values in the report.
 *
 * Correct behaviour: EN_report returns 0 and every value in the report fits
 * the buffer (at most 15 characters).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    char line[1024], longest[64] = "";
    FILE *f;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, "CRASH-05_precision12.inp", "CRASH-05.rpt", "CRASH-05.out");
    if (!err) err = EN_solveH(ph);
    if (!err) err = EN_solveQ(ph);
    if (!err) err = EN_report(ph);   /* the overflow happens in here */
    EN_close(ph);
    EN_deleteproject(ph);
    printf("EN_report returned %d\n", err);
    if (err) {
        printf("FAIL: the report was not written (error %d)\n", err);
        return 1;
    }

    /* Find the longest number written in the report */
    f = fopen("CRASH-05.rpt", "r");
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        char *tok, *end;
        for (tok = strtok(line, " \t\n"); tok != NULL; tok = strtok(NULL, " \t\n")) {
            strtod(tok, &end);
            if (*end == '\0' && strchr(tok, '.') != NULL &&
                strlen(tok) > strlen(longest) && strlen(tok) < sizeof(longest)) {
                strcpy(longest, tok);
            }
        }
    }
    if (f != NULL) fclose(f);

    printf("Longest value in the report: %s (%d characters, buffer holds 15)\n",
           longest, (int)strlen(longest));
    if (strlen(longest) > 15) {
        printf("FAIL: the report wrote a %d-character value into a 16-byte buffer\n",
               (int)strlen(longest));
        return 1;
    }
    printf("PASS: EN_report succeeds and every value fits the buffer\n");
    return 0;
}
