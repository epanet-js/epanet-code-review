/*
 * WQ-03: FIFO and LIFO tanks treat filling as overflow.
 *
 * Reservoir R1 (1.0 mg/L) fills tank T1, which starts at 0 mg/L and is full
 * after about 40 minutes. There are no demands, so water can only leave the
 * network by overflowing the tank. The test runs the network with each tank
 * mixing model, with the tank's overflow option off and on, and along both
 * paths: the normal one (EN_solveH, then the quality solver reading the saved
 * hydraulics) and with hydraulics and quality stepped together. It reads the
 * mass balance from each run's report file.
 *
 * Correct behaviour:
 * - A tank that cannot overflow loses no mass: Mass Outflow is about 0.
 * - A tank that can overflow is full of R1's water at the end, after about
 *   26 tank volumes have passed through it, so every mixing model ends with
 *   the same Final Mass. The completely mixed (MIXED) tank is the reference.
 * - Both paths give the same results.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "epanet2_2.h"

typedef struct {
    double inflow;   /* Mass Inflow from the report, mg */
    double outflow;  /* Mass Outflow, mg */
    double final;    /* Final Mass, mg */
} MassBalance;

/* Reads the number after a label such as "Mass Outflow:" in a report file */
static double report_value(const char *rptfile, const char *label)
{
    char line[256], *p;
    double v = NAN;
    FILE *f = fopen(rptfile, "r");

    while (f && fgets(line, sizeof line, f)) {
        p = strstr(line, label);
        if (p) sscanf(p + strlen(label), "%lf", &v);
    }
    if (f) fclose(f);
    return v;
}

/* Runs the network once and returns its water quality mass balance */
static MassBalance run(int mixmodel, int overflow, int stepped)
{
    EN_Project ph;
    int t1;
    long t, tstep;
    char rpt[64];
    MassBalance mb;

    snprintf(rpt, sizeof rpt, "WQ-03_%d_%d_%d.rpt", mixmodel, overflow, stepped);
    EN_createproject(&ph);
    EN_open(ph, "WQ-03_filling-tank.inp", rpt, "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_setnodevalue(ph, t1, EN_MIXMODEL, mixmodel);
    EN_setnodevalue(ph, t1, EN_CANOVERFLOW, overflow);

    if (!stepped) {
        /* Normal path: all hydraulics first, then quality from the saved file */
        EN_solveH(ph);
        EN_solveQ(ph);
    } else {
        /* Hydraulics and quality stepped together */
        EN_openH(ph);
        EN_initH(ph, EN_NOSAVE);
        EN_openQ(ph);
        EN_initQ(ph, EN_NOSAVE);
        do {
            EN_runH(ph, &t);
            EN_runQ(ph, &t);
            EN_nextH(ph, &tstep);
            EN_nextQ(ph, &tstep);
        } while (tstep > 0);
        EN_closeQ(ph);
        EN_closeH(ph);
    }
    EN_close(ph);
    EN_deleteproject(ph);

    mb.inflow = report_value(rpt, "Mass Inflow:");
    mb.outflow = report_value(rpt, "Mass Outflow:");
    mb.final = report_value(rpt, "Final Mass:");
    return mb;
}

int main(void)
{
    const int models[] = {EN_MIX1, EN_FIFO, EN_LIFO};
    const char *names[] = {"MIXED", "FIFO", "LIFO"};
    const char *paths[] = {"normal", "stepped"};
    MassBalance mb, ref;
    int m, overflow, stepped, wrong = 0;

    printf("Model  Overflow  Path      Mass inflow  Mass outflow   Final mass   (mg)\n");
    for (overflow = 0; overflow <= 1; overflow++) {
        ref = run(EN_MIX1, overflow, 0);
        for (m = 0; m < 3; m++) {
            for (stepped = 0; stepped <= 1; stepped++) {
                const char *note = "";

                mb = run(models[m], overflow, stepped);
                if (!overflow && mb.outflow > 0.001 * mb.inflow)
                    note = "<-- lost mass from a tank that cannot overflow";
                else if (overflow && fabs(mb.final - ref.final) > 0.01 * ref.final)
                    note = "<-- final mass differs from the MIXED tank";
                if (note[0] != '\0') wrong++;

                printf("%-5s  %-8s  %-7s  %12.4g  %12.4g  %11.4g   %s\n", names[m],
                       overflow ? "yes" : "no", paths[stepped], mb.inflow,
                       mb.outflow, mb.final, note);
            }
        }
    }

    if (wrong > 0) {
        printf("FAIL: %d of 12 runs book filling as overflow\n", wrong);
        return 1;
    }
    printf("PASS: every model and path books overflow only when the tank overflows\n");
    return 0;
}
