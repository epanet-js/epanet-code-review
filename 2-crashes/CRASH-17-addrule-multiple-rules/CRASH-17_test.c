/*
 * CRASH-17: EN_addrule with two RULE clauses overflows the rule array.
 *
 * EN_addrule grows the Rule array by one entry, but the parser starts a
 * new rule, and moves to the next entry, at every RULE clause. The test
 * opens a network that has one rule (R1) and passes rules AA and BB to
 * EN_addrule in one string. It then adds AA and BB in two separate calls.
 *
 * Correct behaviour: the two-rule string is rejected with error 250
 * (invalid format) and the rule count stays at 1; the separate calls add
 * one rule each. Without the fix, AddressSanitizer reports a heap buffer
 * overflow in newrule().
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#define RULE_AA "RULE AA\nIF TANK T1 LEVEL > 15\nTHEN PIPE P1 STATUS = CLOSED\n"
#define RULE_BB "RULE BB\nIF TANK T1 LEVEL < 5\nTHEN PIPE P1 STATUS = OPEN\n"

static int rule_count(EN_Project ph)
{
    int n = 0;
    EN_getcount(ph, EN_RULECOUNT, &n);
    return n;
}

int main(void)
{
    EN_Project ph;
    int err_both, err_aa, err_bb, n_after_both, n_final;
    char text[200];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-17_one-rule.inp", "CRASH-17.rpt", "");
    printf("Rules at start: %d\n", rule_count(ph));

    /* Both rules in one call (a writable copy: see CRASH-19) */
    strcpy(text, RULE_AA RULE_BB);
    err_both = EN_addrule(ph, text);
    n_after_both = rule_count(ph);
    printf("EN_addrule(AA and BB) returned %d, rule count %d\n", err_both, n_after_both);

    /* One rule per call */
    strcpy(text, RULE_AA);
    err_aa = EN_addrule(ph, text);
    strcpy(text, RULE_BB);
    err_bb = EN_addrule(ph, text);
    n_final = rule_count(ph);
    printf("EN_addrule(AA), EN_addrule(BB) returned %d and %d, rule count %d\n",
           err_aa, err_bb, n_final);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err_both != 250 || n_after_both != 1 || err_aa || err_bb || n_final != 3) {
        printf("FAIL: two rules in one EN_addrule call were not rejected cleanly\n");
        return 1;
    }
    printf("PASS: two rules in one call are rejected; one per call works\n");
    return 0;
}
