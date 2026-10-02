/*
 * WQ-04: average reaction rates are low when REPORT START is not 0.
 *
 * The binary output file ends with four network-wide averages: bulk, wall
 * and tank reaction rates and the source mass inflow rate, in mass/hr. The
 * quality solver adds to these totals only from REPORT START on, but the
 * averages divide them by the whole duration.
 *
 * The network has steady flow through two pipes, a zero-order bulk decay of
 * 0.1 mg/L/day and a 60 mg/min mass source, so the true rates do not change
 * during the 24 h run. It is run with REPORT START 0 and 12:00, and the
 * averages are read back with the output-file reader.
 *
 * Correct behaviour: both runs give the same averages, and they match the
 * rates worked out by hand from the pipe volume and the source strength.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"
#include "epanet_output.h"

#define LITRES_PER_FT3 28.3168
#define PI 3.14159265358979

/* True if a is within 1 % of b (allows for rounding in the solver) */
static int near(double a, double b)
{
    return fabs(a - b) <= 0.01 * fabs(b);
}

/* Runs the network with the given report start (s) and reads the averages */
static void run(long rstart, const char *outfile, float *bulk, float *source)
{
    EN_Project ph;
    ENR_Handle out;
    float *w;
    int n;

    EN_createproject(&ph);
    EN_open(ph, "WQ-04_steady-decay.inp", "WQ-04.rpt", outfile);
    EN_settimeparam(ph, EN_REPORTSTART, rstart);
    EN_solveH(ph);
    EN_solveQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    /* Index 0 is the bulk reaction rate, 3 the source inflow rate */
    ENR_init(&out);
    ENR_open(out, outfile);
    ENR_getNetReacts(out, &w, &n);
    *bulk = w[0];
    *source = w[3];
    ENR_free((void **)&w);
    ENR_close(&out);
}

int main(void)
{
    float bulk0, src0, bulk12, src12;
    double volume, exp_bulk, exp_src;

    /* Two 1000 ft pipes, 1 ft across (ft3): every litre in them loses 0.1 mg/day */
    volume = 2 * 1000.0 * PI / 4.0;
    exp_bulk = 0.1 / 24.0 * volume * LITRES_PER_FT3;
    exp_src = 60.0 * 60.0;

    run(0, "WQ-04_start0.out", &bulk0, &src0);
    run(12 * 3600, "WQ-04_start12.out", &bulk12, &src12);

    printf("Average rate (mg/hr)        Bulk reaction   Source inflow\n");
    printf("Expected                   %14.1f %15.1f\n", exp_bulk, exp_src);
    printf("REPORT START 0:00          %14.1f %15.1f\n", bulk0, src0);
    printf("REPORT START 12:00         %14.1f %15.1f\n", bulk12, src12);

    if (!near(bulk0, exp_bulk) || !near(src0, exp_src) ||
        !near(bulk12, exp_bulk) || !near(src12, exp_src)) {
        printf("FAIL: with REPORT START 12:00 the averages are %.0f %% of the true rates\n",
               100.0 * bulk12 / exp_bulk);
        return 1;
    }
    printf("PASS: the averages do not depend on REPORT START\n");
    return 0;
}
