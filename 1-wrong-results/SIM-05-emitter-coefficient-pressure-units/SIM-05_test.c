/*
 * SIM-05: emitter coefficients are re-converted with the pressure-unit factor.
 *
 * Junction J1 has an emitter with coefficient 10. For each flow unit system
 * (GPM and LPS) and each pressure unit, we
 *   1. call EN_setoption(EN_EMITEXPON, 0.5), which re-sets the emitter
 *      exponent to the value it already has, and read the coefficient back;
 *   2. save the project with EN_saveinpfile, reopen the saved file and read
 *      the coefficient again.
 *
 * Correct behaviour: an emitter coefficient is always per psi (US units) or
 * per metre (SI units), whatever the pressure units, so it stays 10 after
 * both steps.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static const char *PRESS_NAME[] = {"PSI", "KPA", "METERS", "BAR", "FEET"};

static double emitter_coeff(EN_Project ph)
{
    int j1;
    double k;
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodevalue(ph, j1, EN_EMITTER, &k);
    return k;
}

/* Opens the network in the given flow and pressure units, with J1's
   emitter coefficient set to 10 in those units */
static EN_Project open_network(int flow_units, int press_units)
{
    EN_Project ph;
    int j1;

    EN_createproject(&ph);
    EN_open(ph, "SIM-05_emitter.inp", "SIM-05.rpt", "");
    EN_setflowunits(ph, flow_units);
    EN_setoption(ph, EN_PRESS_UNITS, press_units);
    EN_getnodeindex(ph, "J1", &j1);
    EN_setnodevalue(ph, j1, EN_EMITTER, 10.0);
    return ph;
}

static void close_network(EN_Project ph)
{
    EN_close(ph);
    EN_deleteproject(ph);
}

static double emitter_flow(EN_Project ph)
{
    int j1;
    double q;
    EN_solveH(ph);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodevalue(ph, j1, EN_EMITTERFLOW, &q);
    return q;
}

int main(void)
{
    int flow_units[] = {EN_GPM, EN_LPS};
    int wrong = 0;
    double q_before, q_after;
    EN_Project ph;

    printf("Flow  Pressure   coefficient after         coefficient after\n");
    printf("units units      EN_setoption(EMITEXPON)   save and reopen"
           "   (expected 10)\n");

    for (int f = 0; f < 2; f++) {
        for (int u = EN_PSI; u <= EN_FEET; u++) {
            double k_expon, k_saved;
            EN_Project saved;

            /* Step 1: re-set the exponent to its current value */
            ph = open_network(flow_units[f], u);
            EN_setoption(ph, EN_EMITEXPON, 0.5);
            k_expon = emitter_coeff(ph);
            close_network(ph);

            /* Step 2: save and reopen */
            ph = open_network(flow_units[f], u);
            EN_saveinpfile(ph, "SIM-05_saved.inp");
            close_network(ph);
            EN_createproject(&saved);
            EN_open(saved, "SIM-05_saved.inp", "SIM-05_saved.rpt", "");
            k_saved = emitter_coeff(saved);
            close_network(saved);

            int bad = fabs(k_expon - 10.0) > 1e-6 || fabs(k_saved - 10.0) > 1e-6;
            printf("%-5s %-8s %14.4f %25.4f   %s\n", f == 0 ? "GPM" : "LPS",
                   PRESS_NAME[u], k_expon, k_saved, bad ? "<-- wrong" : "");
            wrong += bad;
        }
    }

    /* What this does to the results: GPM project with pressures in kPa */
    ph = open_network(EN_GPM, EN_KPA);
    q_before = emitter_flow(ph);
    EN_setoption(ph, EN_EMITEXPON, 0.5);
    q_after = emitter_flow(ph);
    close_network(ph);
    printf("\nGPM, kPa: J1 emitter flow %.2f gpm before EN_setoption(EN_EMITEXPON, 0.5),"
           " %.2f gpm after\n", q_before, q_after);

    if (wrong) {
        printf("FAIL: the emitter coefficient changed in %d of 10 unit combinations\n",
               wrong);
        return 1;
    }
    printf("PASS: the emitter coefficient stays 10 in every unit combination\n");
    return 0;
}
