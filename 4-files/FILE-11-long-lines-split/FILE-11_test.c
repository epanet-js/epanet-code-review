/*
 * FILE-11: lines over 1,023 characters are split in two.
 *
 * readdata() in input2.c reads the file with fgets(line, MAXLINE, ...),
 * which returns at most 1,023 characters. The rest of a longer line comes
 * back from the next fgets call and is parsed as a line of its own. The
 * "line too long" check (error 214) compares strlen(line) with MAXLINE and
 * so can never fire.
 *
 *   1. FILE-11_long-comment.inp: junction J1 has a 1,100-character comment.
 *      Its tail is read as a new [JUNCTIONS] line and gives a spurious
 *      error. Correct: the file loads (the comment is cut, which is harmless).
 *   2. FILE-11_long-pattern.inp: pattern PAT1 has 36 factors on a line of
 *      1,085 characters. The line is split inside the 34th factor, and its
 *      tail is read as a second pattern. Correct: error 214 (the documented
 *      limit is 1,024 characters per line), so EN_open returns 200.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Print the report lines that mention an error */
static void print_errors(const char *rptfile)
{
    char line[512];
    FILE *f = fopen(rptfile, "r");

    while (f && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "Error")) printf("  report: %.70s\n", line);
    }
    if (f) fclose(f);
}

int main(void)
{
    EN_Project ph;
    int err1, err2, npats = 0, pat, len = 0;

    /* Case 1: a long comment */
    EN_createproject(&ph);
    err1 = EN_open(ph, "FILE-11_long-comment.inp", "FILE-11.rpt", "");
    EN_close(ph);
    EN_deleteproject(ph);
    printf("FILE-11_long-comment.inp: EN_open returned %d (expected 0)\n", err1);
    print_errors("FILE-11.rpt");

    /* Case 2: a long data line */
    EN_createproject(&ph);
    err2 = EN_open(ph, "FILE-11_long-pattern.inp", "FILE-11.rpt", "");
    printf("FILE-11_long-pattern.inp: EN_open returned %d (expected 200)\n", err2);
    if (err2 == 0) {
        /* Loaded: show what became of the line */
        EN_getcount(ph, EN_PATCOUNT, &npats);
        EN_getpatternindex(ph, "PAT1", &pat);
        EN_getpatternlen(ph, pat, &len);
        printf("  %d patterns (the file has 1); PAT1 has %d factors (the line has 36)\n",
               npats, len);
        if (npats > 1) {
            char id[EN_MAXID + 1];
            EN_getpatternid(ph, 2, id);
            printf("  pattern 2 is named \"%s\", from the tail of the line\n", id);
        }
    }
    EN_close(ph);
    EN_deleteproject(ph);
    print_errors("FILE-11.rpt");

    if (err1 != 0 || err2 != 200) {
        printf("FAIL: lines over 1,023 characters are split and the tail is "
               "parsed as a new line\n");
        return 1;
    }
    printf("PASS: a long comment is ignored and a long data line gives error 214\n");
    return 0;
}
