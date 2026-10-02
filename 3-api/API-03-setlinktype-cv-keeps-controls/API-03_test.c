/*
 * API-03: EN_setlinktype from PIPE to CVPIPE keeps the link's controls and rules.
 *
 * In API-03_controlled-pipe.inp pipe P2 is closed by a simple control (at 2 h)
 * and by rule 1. Rule 2 only reads P2's flow and sets pipe P1.
 * We give P2 a check valve with EN_setlinktype(EN_CVPIPE, EN_UNCONDITIONAL),
 * save the project with EN_saveinpfile and open the saved file again.
 *
 * A check valve cannot be controlled: the input file parser, EN_addcontrol
 * and EN_addrule all refuse it with error 207. Correct behaviour: after the
 * type change no control or rule action sets P2, and the saved file opens.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Count the simple controls and rule actions that set link 'link' */
static int count_settings_of(EN_Project ph, int link)
{
    int i, k, n = 0, count, type, lnk, node, npremises, nthen, nelse, status;
    double setting, level, priority;

    EN_getcount(ph, EN_CONTROLCOUNT, &count);
    for (i = 1; i <= count; i++) {
        EN_getcontrol(ph, i, &type, &lnk, &setting, &node, &level);
        if (lnk == link) n++;
    }
    EN_getcount(ph, EN_RULECOUNT, &count);
    for (i = 1; i <= count; i++) {
        EN_getrule(ph, i, &npremises, &nthen, &nelse, &priority);
        for (k = 1; k <= nthen; k++) {
            EN_getthenaction(ph, i, k, &lnk, &status, &setting);
            if (lnk == link) n++;
        }
        for (k = 1; k <= nelse; k++) {
            EN_getelseaction(ph, i, k, &lnk, &status, &setting);
            if (lnk == link) n++;
        }
    }
    return n;
}

static void print_counts(EN_Project ph, int p2)
{
    int ncontrols, nrules;

    EN_getcount(ph, EN_CONTROLCOUNT, &ncontrols);
    EN_getcount(ph, EN_RULECOUNT, &nrules);
    printf("  %d control(s), %d rule(s); %d of them set P2\n",
           ncontrols, nrules, count_settings_of(ph, p2));
}

int main(void)
{
    EN_Project ph;
    int p2, err, settings;
    char line[256];
    FILE *f;

    EN_createproject(&ph);
    EN_open(ph, "API-03_controlled-pipe.inp", "API-03.rpt", "");
    EN_getlinkindex(ph, "P2", &p2);
    printf("Before the type change:\n");
    print_counts(ph, p2);

    err = EN_setlinktype(ph, &p2, EN_CVPIPE, EN_UNCONDITIONAL);
    printf("EN_setlinktype(P2, EN_CVPIPE, EN_UNCONDITIONAL) returned %d\n", err);
    print_counts(ph, p2);
    settings = count_settings_of(ph, p2);

    EN_saveinpfile(ph, "API-03_saved.inp");
    EN_close(ph);

    /* Open the saved file in the same project */
    err = EN_open(ph, "API-03_saved.inp", "API-03_reload.rpt", "");
    printf("Opening the saved file returned %d\n", err);
    EN_close(ph);
    EN_deleteproject(ph);

    /* Show the parser's "attempt to control a CV" messages, if any */
    f = fopen("API-03_reload.rpt", "r");
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "207") != NULL) printf("  %s\n", line);
    }
    if (f != NULL) fclose(f);

    if (settings > 0 || err != 0) {
        printf("FAIL: %d control(s) or rule action(s) still set check valve pipe P2, "
               "and opening the saved file returned %d\n", settings, err);
        return 1;
    }
    printf("PASS: no control or rule sets the check valve pipe, "
           "and the saved file opens\n");
    return 0;
}
