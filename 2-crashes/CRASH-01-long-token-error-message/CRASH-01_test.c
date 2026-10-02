/*
 * CRASH-01: an invalid token over 255 characters overruns the error-message
 * buffers.
 *
 * Each input file has a [TITLE] line and a junction whose ID is too long
 * (error 252). inperrmsg() in input2.c copies the bad token into a 256-byte
 * buffer and formats the message into the 256-byte Project.Msg, both with no
 * length check:
 *   - a 240-character ID fits the first buffer, but the message does not.
 *     It runs on into Project.Title, so the title is overwritten. No
 *     sanitizer sees this, because Title is part of the same struct.
 *   - a 300-character ID overflows the first buffer on the stack, which
 *     AddressSanitizer reports.
 *
 * Correct behaviour: both files are rejected with error 200, the error
 * message is cut to fit, and the title is unchanged.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Open one file (EN_openX keeps the project open despite input errors),
   print the outcome and return 1 if EPANET behaved correctly */
static int check_file(const char *inpfile, const char *expected_title)
{
    EN_Project ph;
    char line1[EN_MAXMSG + 1] = "", line2[EN_MAXMSG + 1], line3[EN_MAXMSG + 1];
    int err, title_ok;

    printf("%s\n", inpfile);
    EN_createproject(&ph);
    err = EN_openX(ph, inpfile, "CRASH-01.rpt", "");
    EN_gettitle(ph, line1, line2, line3);
    EN_close(ph);
    EN_deleteproject(ph);

    title_ok = strcmp(line1, expected_title) == 0;
    printf("  EN_openX returned %d (expected 200)\n", err);
    printf("  title is \"%s\"%s\n", line1, title_ok ? "" : "   <-- overwritten");
    return err == 200 && title_ok;
}

int main(void)
{
    int ok = 1;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    ok &= check_file("CRASH-01_id-240-chars.inp",
                     "CRASH-01: an invalid ID of 240 characters");
    ok &= check_file("CRASH-01_id-300-chars.inp",
                     "CRASH-01: an invalid ID of 300 characters");

    if (!ok) {
        printf("FAIL: the error message for a long token overran its buffer\n");
        return 1;
    }
    printf("PASS: long tokens are reported without overrunning any buffer\n");
    return 0;
}
