/*
 * CRASH-08: xstrcpy leaves over-long names unterminated.
 *
 * xstrcpy() in project.c stores a string in a buffer sized for at most n
 * characters. When the source is longer it copies n+1 characters with
 * strncpy, so the stored copy has no terminating null. Reading it back runs
 * past the end of the allocation, which AddressSanitizer reports.
 *
 * Two cases:
 *   1. The input file gives junction J1 a demand category whose name (the
 *      [DEMANDS] comment) has 45 characters; the limit is 31 (EN_MAXID).
 *   2. EN_setcomment gives pipe P1 a 300-character comment; the limit is
 *      255 (EN_MAXMSG).
 *
 * Correct behaviour: each string is cut to its limit and read back intact.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#define LONG_NAME "Residential customers - north pressure zone 5"

/* Compare a string read back from EPANET with the first `limit` characters
   of what was stored */
static int check(const char *what, const char *got, const char *stored, size_t limit)
{
    int ok = strlen(got) == limit && strncmp(got, stored, limit) == 0;

    printf("%s: %zu characters read back (expected %zu)%s\n", what, strlen(got),
           limit, ok ? "" : "   <-- not the stored text");
    return ok;
}

int main(void)
{
    EN_Project ph;
    char name[1024] = "";        /* larger than needed, in case of overrun */
    char comment[1024] = "";
    char long_comment[301];
    int j1, p1, ok = 1;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    if (EN_open(ph, "CRASH-08_long-demand-name.inp", "CRASH-08.rpt", "") != 0) {
        printf("FAIL: could not open the input file\n");
        return 1;
    }

    /* Case 1: the demand category name from the input file */
    EN_getnodeindex(ph, "J1", &j1);
    EN_getdemandname(ph, j1, 1, name);
    printf("Demand name: \"%s\"\n", name);
    ok &= check("Demand name", name, LONG_NAME, EN_MAXID);

    /* Case 2: a comment set through the API */
    memset(long_comment, 'c', 300);
    long_comment[300] = '\0';
    EN_getlinkindex(ph, "P1", &p1);
    EN_setcomment(ph, EN_LINK, p1, long_comment);
    EN_getcomment(ph, EN_LINK, p1, comment);
    ok &= check("Comment", comment, long_comment, EN_MAXMSG);

    EN_close(ph);
    EN_deleteproject(ph);

    if (!ok) {
        printf("FAIL: a string cut to its length limit was stored without a terminator\n");
        return 1;
    }
    printf("PASS: over-long strings are cut to their limit and stay terminated\n");
    return 0;
}
