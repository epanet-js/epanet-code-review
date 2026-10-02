/*
 * CRASH-27: EN_addrule leaves rule labels of 31 characters or more
 * unterminated.
 *
 * newrule() copies a rule's label with strncpy(label, token, 31), which
 * writes no terminating zero for a label of 31 characters or more. The
 * Rule entry that EN_addrule adds comes from realloc and is not cleared,
 * so the label runs on into whatever follows it.
 *
 * The test adds a rule whose label is 31 characters long (the longest ID
 * EPANET allows) and reads it back with EN_getruleID into a buffer of
 * EN_MAXID + 1 characters, the size the API documents.
 *
 * Correct behaviour: EN_getruleID returns the 31-character label.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#define LABEL "PUMP_STATION_7_NIGHT_TARIFF_OFF"   /* 31 characters */

int main(void)
{
    EN_Project ph;
    int err;
    char rule[150], id[EN_MAXID + 1];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-27_tank.inp", "CRASH-27.rpt", "");

    strcpy(rule, "RULE " LABEL "\nIF TANK T1 LEVEL > 15\n"
                 "THEN PIPE P2 STATUS = CLOSED");
    err = EN_addrule(ph, rule);
    printf("EN_addrule returned %d for a %d-character label\n", err,
           (int)strlen(LABEL));

    err = EN_getruleID(ph, 1, id);
    printf("EN_getruleID returned %d: %d characters\n", err, (int)strlen(id));
    printf("  expected: %s\n", LABEL);
    printf("  read:     %s\n", id);

    EN_close(ph);
    EN_deleteproject(ph);

    if (strcmp(id, LABEL) != 0) {
        printf("FAIL: the rule label was not terminated\n");
        return 1;
    }
    printf("PASS: the rule label reads back unchanged\n");
    return 0;
}
