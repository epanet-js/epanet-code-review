/*
 * SIM-06: the demand charge is multiplied by the price twice and compounds
 * across runs.
 *
 * The network is EPANET's Net1, which has one pump, with a demand charge of
 * 2.0 per kW of peak demand. We run the analysis three times in one project:
 *   run 1  EN_solveH + EN_solveQ + EN_report
 *   run 2  EN_solveQ + EN_report again, on the same hydraulics
 *   run 3  demand multiplier 0.5, then a fresh EN_solveH + EN_solveQ + EN_report
 * After each run we read the peak kW and the demand charge from the energy
 * table of the report, and the demand charge from the binary output file.
 *
 * Correct behaviour: every run reports Demand Charge = 2.0 x peak kW, in the
 * report and in the output file.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "epanet2_2.h"

#define DCOST 2.0   /* Demand Charge in the input file */

/* Reads the pump's peak kW and the demand charge from a report file */
static void read_report(const char *rptfile, double *peak_kw, double *charge)
{
    FILE *f = fopen(rptfile, "r");
    char line[256], id[32];
    double v[6];

    *peak_kw = *charge = -1.0;
    while (fgets(line, sizeof line, f)) {
        /* Pump row: ID, usage, efficiency, kWh/Mgal, avg kW, peak kW, cost/day */
        if (sscanf(line, "%31s %lf %lf %lf %lf %lf %lf",
                   id, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 7
            && strcmp(id, "9") == 0)
            *peak_kw = v[4];
        if (strstr(line, "Demand Charge:"))
            sscanf(strchr(line, ':') + 1, "%lf", charge);
    }
    fclose(f);
}

/* Reads the demand charge from the binary output file. It is the 4-byte value
   just before the time series, which is followed by a 28-byte epilogue. */
static double read_outfile_charge(EN_Project ph, const char *outfile)
{
    int nnodes, nlinks;
    long nperiods, size;
    float charge = -1.0f;
    FILE *f = fopen(outfile, "rb");

    EN_getcount(ph, EN_NODECOUNT, &nnodes);
    EN_getcount(ph, EN_LINKCOUNT, &nlinks);
    EN_gettimeparam(ph, EN_PERIODS, &nperiods);

    /* Each reporting period holds 4 node and 8 link values of 4 bytes */
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, size - 28 - nperiods * 4L * (4 * nnodes + 8 * nlinks) - 4, SEEK_SET);
    if (fread(&charge, sizeof charge, 1, f) != 1) charge = -1.0f;
    fclose(f);
    return charge;
}

int main(void)
{
    const char *what[3] = {
        "first run", "EN_solveQ again", "new EN_solveH"
    };
    EN_Project ph;
    int run, wrong = 0;

    EN_createproject(&ph);
    EN_open(ph, "SIM-06_demand-charge.inp", "SIM-06.rpt", "SIM-06.out");

    printf("Run  What               Peak kW   Expected   Report   Output file\n");
    for (run = 1; run <= 3; run++) {
        char rptfile[64], cmd[80];
        double peak_kw, charge, out_charge, expected;

        /* Write each run's report to its own file, so it is complete on disk */
        snprintf(rptfile, sizeof rptfile, "SIM-06_run%d.rpt", run);
        snprintf(cmd, sizeof cmd, "FILE %s", rptfile);
        EN_setreport(ph, cmd);

        if (run == 3) EN_setoption(ph, EN_DEMANDMULT, 0.5);
        if (run != 2) EN_solveH(ph);
        EN_solveQ(ph);
        out_charge = read_outfile_charge(ph, "SIM-06.out");
        EN_report(ph);
        read_report(rptfile, &peak_kw, &charge);

        /* The report rounds the peak to 0.01 kW */
        expected = DCOST * peak_kw;
        printf("%3d  %-16s %8.2f %10.2f %8.2f %13.2f\n",
               run, what[run - 1], peak_kw, expected, charge, out_charge);
        if (fabs(charge - expected) > 0.02 || fabs(out_charge - expected) > 0.02)
            wrong++;
    }

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: the demand charge is wrong in %d of 3 runs "
               "(expected %.1f x peak kW each time)\n", wrong, DCOST);
        return 1;
    }
    printf("PASS: every run charges %.1f x peak kW, in the report and the output file\n",
           DCOST);
    return 0;
}
