/*
 * CRASH-19: EN_addrule edits the caller's string, so a string literal
 * faults.
 *
 * EN_addrule splits the rule text into lines by writing '\0' over each
 * newline, and puts the newline back after the line has been parsed. If
 * a line has an error it returns at once, so the newline is never put
 * back. If the text is a string literal, the first write faults.
 *
 * The test first passes a writable copy of a rule whose THEN clause names
 * a link that does not exist, and compares the copy with the original
 * text. It then passes a valid rule as a string literal.
 *
 * Correct behaviour: the first call returns 204 (undefined link) and
 * leaves the text unchanged; the second call returns 0.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Link P9 does not exist */
#define BAD_RULE "RULE R1\nIF TANK T1 LEVEL > 15\n" \
                 "THEN PIPE P9 STATUS = CLOSED\nPRIORITY 2"

int main(void)
{
    EN_Project ph;
    int err1, err2, unchanged, nrules;
    char text[100];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-19_tank.inp", "CRASH-19.rpt", "");

    /* A writable copy of a rule with an error in its third line */
    strcpy(text, BAD_RULE);
    err1 = EN_addrule(ph, text);
    unchanged = strcmp(text, BAD_RULE) == 0;
    printf("Rule with an undefined link: EN_addrule returned %d\n", err1);
    printf("  caller's text is %zu characters (was %zu): %s\n", strlen(text),
           strlen(BAD_RULE), unchanged ? "unchanged" : "CHANGED");

    /* A valid rule passed as a string literal, in read-only memory */
    err2 = EN_addrule(ph, "RULE R2\nIF TANK T1 LEVEL > 15\nTHEN PIPE P2 STATUS = CLOSED");
    EN_getcount(ph, EN_RULECOUNT, &nrules);
    printf("Valid rule as a string literal: EN_addrule returned %d, rule count %d\n",
           err2, nrules);

    EN_close(ph);
    EN_deleteproject(ph);

    if (!unchanged || err2 != 0 || nrules != 1) {
        printf("FAIL: EN_addrule changed the caller's string\n");
        return 1;
    }
    printf("PASS: EN_addrule leaves the caller's string alone\n");
    return 0;
}
