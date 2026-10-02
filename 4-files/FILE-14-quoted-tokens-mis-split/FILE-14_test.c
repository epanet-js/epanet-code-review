/*
 * FILE-14: a quoted token corrupts the rest of the line.
 *
 * gettokens() in input2.c lets a token be enclosed in double quotes, so that
 * it can contain spaces (map labels in [LABELS] are written this way). It
 * keeps a count of the characters left on the line, but after a quoted
 * token it reduces the count by the length of the token's first word, while
 * the scan moves past the whole quoted token. The two then disagree:
 *   1. FILE-14_quoted-id.inp writes junction J1 as "J1". The count ends one
 *      short, so the last token on the line keeps its newline, and the
 *      pattern "PAT1\n" is not found (error 205).
 *   2. FILE-14_long-label.inp has a 923-character label with spaces. The
 *      count ends far too long, so the scan runs past the end of the line
 *      and the 1,025-byte line buffer; AddressSanitizer stops it there.
 *
 * Correct behaviour: both files open with no error.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Open one file, print the outcome and any error lines from the report */
static int open_file(const char *inpfile)
{
    EN_Project ph;
    char line[512];
    FILE *f;
    int err;

    printf("%s\n", inpfile);
    EN_createproject(&ph);
    err = EN_open(ph, inpfile, "FILE-14.rpt", "");
    EN_close(ph);
    EN_deleteproject(ph);
    printf("  EN_open returned %d (expected 0)\n", err);

    f = fopen("FILE-14.rpt", "r");
    while (f && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "Error")) printf("  report: %s\n", line);
    }
    if (f) fclose(f);
    return err;
}

int main(void)
{
    int failed = 0;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    failed += open_file("FILE-14_quoted-id.inp") != 0;
    failed += open_file("FILE-14_long-label.inp") != 0;

    if (failed) {
        printf("FAIL: %d of 2 files with quoted tokens did not load\n", failed);
        return 1;
    }
    printf("PASS: both files with quoted tokens load\n");
    return 0;
}
