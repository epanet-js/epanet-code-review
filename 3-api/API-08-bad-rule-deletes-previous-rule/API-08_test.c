/*
 * API-08: a bad EN_addrule string deletes the previous rule.
 *
 * When EN_addrule finds an error it deletes rule number Nrules, assuming
 * that it is the rule being added. If the error comes before a valid RULE
 * clause, no rule was added, so the previous rule is deleted instead. A
 * string that starts with DISABLED is accepted and disables the previous
 * rule.
 *
 * The network has one rule, R1. The test passes three bad strings to
 * EN_addrule, then adds a valid rule R4, and lists the rules after each
 * call.
 *
 * Correct behaviour: each bad string returns an error and changes nothing,
 * so the project ends with R1 (enabled) and R4.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Writes the rule count and IDs as "2 [R1 R4]"; a disabled rule is marked (off) */
static void list_rules(EN_Project ph, char *out)
{
    int i, n = 0, enabled;
    char id[EN_MAXID + 1];

    EN_getcount(ph, EN_RULECOUNT, &n);
    snprintf(out, 20, "%2d [", n);
    for (i = 1; i <= n; i++) {
        EN_getruleID(ph, i, id);
        EN_getruleenabled(ph, i, &enabled);
        if (i > 1) strcat(out, " ");
        strcat(out, id);
        if (!enabled) strcat(out, "(off)");
    }
    strcat(out, "]");
}

/* Adds a rule from a writable copy of text (see CRASH-19) and reports it */
static void add(EN_Project ph, const char *what, const char *text)
{
    char buf[200], rules[200];
    int err;

    strcpy(buf, text);
    err = EN_addrule(ph, buf);
    list_rules(ph, rules);
    printf("%-28s returned %3d   rules: %s\n", what, err, rules);
}

int main(void)
{
    EN_Project ph;
    char rules[200];

    EN_createproject(&ph);
    EN_open(ph, "API-08_one-rule.inp", "API-08.rpt", "");
    list_rules(ph, rules);
    printf("%-28s                rules: %s\n", "At start", rules);

    add(ph, "DISABLED on its own", "DISABLED\n");
    add(ph, "No RULE clause",
        "IF TANK T1 LEVEL < 5\nTHEN PIPE P2 STATUS = OPEN\n");
    add(ph, "Two labels on the RULE line",
        "RULE R3 EXTRA\nIF TANK T1 LEVEL < 5\nTHEN PIPE P2 STATUS = OPEN\n");
    add(ph, "Valid rule R4",
        "RULE R4\nIF TANK T1 LEVEL < 2\nTHEN PIPE P1 STATUS = OPEN\n");

    list_rules(ph, rules);
    EN_close(ph);
    EN_deleteproject(ph);

    if (strcmp(rules, " 2 [R1 R4]") != 0) {
        printf("FAIL: bad rule strings changed the existing rules\n");
        return 1;
    }
    printf("PASS: bad rule strings leave the existing rules alone\n");
    return 0;
}
