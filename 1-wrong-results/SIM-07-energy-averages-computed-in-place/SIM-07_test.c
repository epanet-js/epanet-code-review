/*
 * SIM-07: energy averages are computed in place, so a second EN_solveQ
 * corrupts them.
 *
 * Net1 has one pump. We solve the hydraulics once, then run EN_solveQ and
 * EN_report twice on the same hydraulic results. After each run we read the
 * pump's energy statistics from the report and from the binary output file.
 *
 * Correct behaviour: nothing changed between the two runs, so both runs
 * report the same usage factor, efficiency, kWh/Mgal, average kW, peak kW and
 * cost per day, in the report and in the output file.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "epanet2_2.h"
#include "epanet_output.h"

static const char *names[6] = {
    "Usage factor (%)", "Avg. efficiency (%)", "kWh/Mgal",
    "Avg. kW", "Peak kW", "Cost/day"
};

/* Reads the six energy statistics of pump 9 from a report file */
static void read_report(const char *rptfile, double v[6])
{
    FILE *f = fopen(rptfile, "r");
    char line[256], id[32];

    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%31s %lf %lf %lf %lf %lf %lf",
                   id, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 7
            && strcmp(id, "9") == 0)
            break;
    }
    fclose(f);
}

/* Reads the same six statistics from the binary output file */
static void read_outfile(const char *outfile, double v[6])
{
    ENR_Handle h;
    int link, n, i;
    float *x;

    ENR_init(&h);
    ENR_open(h, outfile);
    ENR_getEnergyUsage(h, 1, &link, &x, &n);
    for (i = 0; i < 6; i++) v[i] = x[i];
    ENR_free((void **)&x);
    ENR_close(&h);
}

int main(void)
{
    EN_Project ph;
    double rpt[2][6], out[2][6];
    int run, i, changed = 0;

    EN_createproject(&ph);
    EN_open(ph, "SIM-07_Net1.inp", "SIM-07.rpt", "SIM-07.out");
    EN_setoption(ph, EN_GLOBALPRICE, 0.10);   /* $ per kWh, so the cost is not 0 */
    EN_setreport(ph, "ENERGY YES");
    EN_setreport(ph, "STATUS NO");
    EN_setreport(ph, "NODES NONE");
    EN_setreport(ph, "LINKS NONE");

    EN_solveH(ph);
    for (run = 0; run < 2; run++) {
        char rptfile[64], cmd[80];

        /* Write each run's report to its own file, so it is complete on disk */
        snprintf(rptfile, sizeof rptfile, "SIM-07_run%d.rpt", run + 1);
        snprintf(cmd, sizeof cmd, "FILE %s", rptfile);
        EN_setreport(ph, cmd);

        EN_solveQ(ph);
        read_outfile("SIM-07.out", out[run]);
        EN_report(ph);
        read_report(rptfile, rpt[run]);
    }
    EN_close(ph);
    EN_deleteproject(ph);

    printf("%-20s %8s  %8s  %8s\n", "", "Run 1", "Run 2", "Run 2");
    printf("%-20s %8s  %8s  %8s\n", "", "report", "report", "out file");
    for (i = 0; i < 6; i++) {
        int same = fabs(rpt[1][i] - rpt[0][i]) <= 0.01
                && fabs(out[1][i] - out[0][i]) <= 0.01;
        printf("%-20s %8.2f  %8.2f  %8.2f   %s\n", names[i],
               rpt[0][i], rpt[1][i], out[1][i], same ? "" : "<-- changed");
        changed += !same;
    }

    if (changed) {
        printf("FAIL: %d of 6 energy statistics changed when EN_solveQ was run again "
               "on the same hydraulics\n", changed);
        return 1;
    }
    printf("PASS: the second EN_solveQ reports the same energy statistics "
           "as the first\n");
    return 0;
}
