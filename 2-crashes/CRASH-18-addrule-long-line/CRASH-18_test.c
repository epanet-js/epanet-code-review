/*
 * CRASH-18: EN_addrule with a line of 1,024 characters or more overflows
 * the stack.
 *
 * EN_addrule copies each line of the rule text into char line2[1025] with
 * strcpy, then appends a newline. A line of 1,023 characters fits exactly;
 * a line of 1,024 characters writes one byte past the end.
 *
 * The test adds two rules whose last line is a comment: 1,023 characters
 * long in rule R1, 1,024 characters long in rule R2.
 *
 * Correct behaviour: R1 is added, and R2 is rejected with error 214 (too
 * many characters in input line). Without the fix, AddressSanitizer
 * reports a stack-buffer-overflow in EN_addrule.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "epanet2_2.h"

/* Builds a rule whose last line is a comment of the given length */
static char *rule_with_comment(const char *label, int comment_length)
{
    char *text = malloc(200 + comment_length);
    int n;

    n = snprintf(text, 200, "RULE %s\nIF TANK T1 LEVEL > 15\n"
                            "THEN PIPE P2 STATUS = CLOSED\n", label);

    /* The comment: ';' followed by 'x' characters, with no final newline */
    text[n] = ';';
    memset(text + n + 1, 'x', comment_length - 1);
    text[n + comment_length] = '\0';
    return text;
}

int main(void)
{
    EN_Project ph;
    int err1, err2, nrules;
    char *text;

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-18_tank.inp", "CRASH-18.rpt", "");

    text = rule_with_comment("R1", 1023);
    err1 = EN_addrule(ph, text);
    printf("Rule R1, last line 1023 characters: EN_addrule returned %d\n", err1);
    free(text);

    text = rule_with_comment("R2", 1024);
    err2 = EN_addrule(ph, text);
    printf("Rule R2, last line 1024 characters: EN_addrule returned %d\n", err2);
    free(text);

    EN_getcount(ph, EN_RULECOUNT, &nrules);
    printf("Rule count: %d\n", nrules);
    EN_close(ph);
    EN_deleteproject(ph);

    if (err1 != 0 || err2 != 214 || nrules != 1) {
        printf("FAIL: a 1024-character line was not rejected\n");
        return 1;
    }
    printf("PASS: a 1023-character line is accepted, a 1024-character one rejected\n");
    return 0;
}
