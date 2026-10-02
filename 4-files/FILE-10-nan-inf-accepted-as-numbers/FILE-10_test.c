/*
 * FILE-10: nan, inf, hex and empty tokens are accepted as numbers.
 *
 * The input file has five parallel pipes from reservoir R1 to junction J1.
 * Each has one value that is not a valid decimal number: a diameter "nan",
 * a length "inf", a roughness "0x64", a minor loss "" (an empty quoted
 * token) and a diameter "12°" (a number followed by a non-ASCII character).
 * getfloat() in input2.c accepts all five, because strtod() parses nan, inf
 * and hex, and its end-of-token test (*endptr > 0) is false for an empty
 * token and for a trailing byte above 127 when char is signed. NaN then
 * passes the "diameter > 0" style range checks, which are ordered
 * comparisons.
 *
 * Correct behaviour: EN_open returns 200, with error 202 (illegal numeric
 * value) reported once for each of the five pipes.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Count the report lines that contain the given text */
static int count_lines(const char *rptfile, const char *text)
{
    char line[512];
    int n = 0;
    FILE *f = fopen(rptfile, "r");

    if (f == NULL) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, text)) n++;
    fclose(f);
    return n;
}

/* Print the value EPANET stored for one pipe property */
static void show(EN_Project ph, const char *pipe, int property, const char *what,
                 const char *token)
{
    int k;
    double v;

    EN_getlinkindex(ph, pipe, &k);
    EN_getlinkvalue(ph, k, property, &v);
    printf("  %s %-10s token %-6s stored as %g\n", pipe, what, token, v);
}

int main(void)
{
    EN_Project ph;
    int err, n202;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-10_bad-numbers.inp", "FILE-10.rpt", "");
    printf("EN_open returned %d (expected 200)\n", err);

    if (err == 0) {
        /* The file loaded: show what each bad token became */
        show(ph, "P1", EN_DIAMETER,  "diameter",  "nan");
        show(ph, "P2", EN_LENGTH,    "length",    "inf");
        show(ph, "P3", EN_ROUGHNESS, "roughness", "0x64");
        show(ph, "P4", EN_MINORLOSS, "minor loss", "\"\"");
        show(ph, "P5", EN_DIAMETER,  "diameter",  "12°");
    }
    EN_close(ph);
    EN_deleteproject(ph);

    n202 = count_lines("FILE-10.rpt", "Error 202");
    printf("Error 202 reported %d times (expected 5)\n", n202);

    if (err != 200 || n202 != 5) {
        printf("FAIL: tokens that are not decimal numbers were accepted\n");
        return 1;
    }
    printf("PASS: all five tokens are rejected as illegal numeric values\n");
    return 0;
}
