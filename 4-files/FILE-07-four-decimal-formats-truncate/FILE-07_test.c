/*
 * FILE-07: fixed four-decimal formats truncate small values.
 *
 * FILE-07_small-values.inp is a small booster in CMS (m3/s) units with four
 * values below 0.001: a pump curve flow of 0.00045, an FCV setting of
 * 0.00025, a control setting of 0.00012 and a pattern factor of 0.00005.
 * The test reads them, saves the project with EN_saveinpfile, reopens the
 * saved file and reads them again.
 *
 * Correct behaviour: each value comes back unchanged (to 1 part in a million).
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define NVALUES 4

/* Reads the four small values from an open project */
static void read_values(EN_Project ph, double v[])
{
    int pump, curve, valve, pattern, type, link, node;
    double flow, head, level;

    EN_getlinkindex(ph, "PU1", &pump);
    EN_getheadcurveindex(ph, pump, &curve);
    EN_getcurvevalue(ph, curve, 1, &flow, &head);
    v[0] = flow;

    EN_getlinkindex(ph, "V1", &valve);
    EN_getlinkvalue(ph, valve, EN_INITSETTING, &v[1]);

    EN_getcontrol(ph, 1, &type, &link, &v[2], &node, &level);

    EN_getpatternindex(ph, "DAY", &pattern);
    EN_getpatternvalue(ph, pattern, 3, &v[3]);
}

int main(void)
{
    const char *what[NVALUES] = {"pump curve flow (m3/s)", "FCV setting (m3/s)",
                                 "control setting (m3/s)", "pattern factor"};
    double before[NVALUES], after[NVALUES];
    EN_Project ph;
    int i, err, changed = 0;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-07_small-values.inp", "FILE-07.rpt", "");
    if (!err) read_values(ph, before);
    if (!err) err = EN_saveinpfile(ph, "FILE-07_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);

    EN_createproject(&ph);
    if (!err) err = EN_open(ph, "FILE-07_saved.inp", "FILE-07_saved.rpt", "");
    if (!err) read_values(ph, after);
    EN_close(ph);
    EN_deleteproject(ph);
    if (err) {
        printf("FAIL: could not open, save or reopen the network (error %d)\n", err);
        return 1;
    }

    printf("Value                     Input file   Saved and reloaded\n");
    for (i = 0; i < NVALUES; i++) {
        int bad = fabs(after[i] - before[i]) > 1e-6 * fabs(before[i]);
        printf("%-24s  %10.6g   %10.6g   %s\n", what[i], before[i], after[i],
               bad ? "<-- changed" : "");
        changed += bad;
    }

    if (changed) {
        printf("FAIL: %d of %d small values changed when the file was saved\n",
               changed, NVALUES);
        return 1;
    }
    printf("PASS: small values survive saving and reloading\n");
    return 0;
}
