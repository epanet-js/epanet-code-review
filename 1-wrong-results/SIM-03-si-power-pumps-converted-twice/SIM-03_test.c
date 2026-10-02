/*
 * SIM-03: constant-power pumps in SI input files are converted from kW twice.
 *
 * SIM-03_si-pump.inp has a 10 kW constant-power pump lifting 10 L/s from a
 * reservoir at head 0 to junction J1. SIM-03_us-pump.inp is the same with
 * a 10 hp pump lifting 1 cfs. The head at J1 is the pump's head gain.
 *
 * Correct behaviour:
 *   - the SI pump adds P / (rho g Q) = 101.9 m, and EN_PUMP_POWER reads 10 kW
 *   - setting the same power through EN_setlinkvalue gives the same result
 *   - saving and reopening the SI file changes neither the power nor the head
 *   - EN_setflowunits(EN_LPS) on the US project changes the units, not the
 *     physics: 88.14 ft becomes 26.87 m, and 10 hp reads as 7.457 kW
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int failures = 0;

/* Print one expected/actual row; values must agree within 0.5 % */
static void check(const char *what, double expected, double actual)
{
    int bad = fabs(actual - expected) > 0.005 * fabs(expected);
    printf("%-50s %10.3f %10.3f   %s\n", what, expected, actual,
           bad ? "<-- wrong" : "");
    failures += bad;
}

/* Solve one hydraulic period and return the head at J1 (the head gain) */
static double head_gain(EN_Project ph)
{
    int j1;
    long t;
    double h;

    EN_getnodeindex(ph, "J1", &j1);
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    EN_getnodevalue(ph, j1, EN_HEAD, &h);
    EN_closeH(ph);
    return h;
}

static double pump_power(EN_Project ph)
{
    int pu1;
    double kw;

    EN_getlinkindex(ph, "PU1", &pu1);
    EN_getlinkvalue(ph, pu1, EN_PUMP_POWER, &kw);
    return kw;
}

int main(void)
{
    EN_Project ph;
    int pu1;
    const double si_head = 10.0 / (9.81 * 0.010);   /* m, for 10 kW at 10 L/s */
    const double us_head = 8.814 * 10.0 / 1.0;      /* ft, for 10 hp at 1 cfs */

    EN_createproject(&ph);
    printf("%-50s %10s %10s\n", "", "Expected", "Actual");

    /* 1. The SI file as loaded */
    EN_open(ph, "SIM-03_si-pump.inp", "SIM-03.rpt", "");
    check("SI file: EN_PUMP_POWER (kW)", 10.0, pump_power(ph));
    check("SI file: head gain (m)", si_head, head_gain(ph));

    /* 2. The same 10 kW set through the API */
    EN_getlinkindex(ph, "PU1", &pu1);
    EN_setlinkvalue(ph, pu1, EN_PUMP_POWER, 10.0);
    check("SI file, 10 kW set through the API: head gain (m)", si_head, head_gain(ph));
    EN_close(ph);

    /* 3. Save the SI file as loaded, and reopen it */
    EN_open(ph, "SIM-03_si-pump.inp", "SIM-03.rpt", "");
    EN_saveinpfile(ph, "SIM-03_saved.inp");
    EN_close(ph);
    EN_open(ph, "SIM-03_saved.inp", "SIM-03.rpt", "");
    check("SI file saved and reopened: EN_PUMP_POWER (kW)", 10.0, pump_power(ph));
    check("SI file saved and reopened: head gain (m)", si_head, head_gain(ph));
    EN_close(ph);

    /* 4. The US file, then the same project switched to SI units */
    EN_open(ph, "SIM-03_us-pump.inp", "SIM-03.rpt", "");
    check("US file: head gain (ft)", us_head, head_gain(ph));
    EN_setflowunits(ph, EN_LPS);
    check("US file switched to LPS: head gain (m)", us_head * 0.3048, head_gain(ph));
    check("US file switched to LPS: EN_PUMP_POWER (kW)", 10.0 * 0.7457, pump_power(ph));
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 8 checks wrong; SI pump power is converted from kW "
               "twice\n", failures);
        return 1;
    }
    printf("PASS: SI and US pumps give the correct head, and the power survives "
           "the API, a save and a change of units\n");
    return 0;
}
