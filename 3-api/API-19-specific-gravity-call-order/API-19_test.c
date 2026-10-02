/*
 * API-19: EN_SP_GRAVITY does not rescale stored emitter and PDA values.
 *
 * The network has a PRV set to 40 psi, an emitter of 10 gpm/psi^0.5,
 * pressure-driven demand with a required pressure of 20 psi, and a control
 * on a pressure of 30 psi. API-19_sg2.inp has specific gravity 2;
 * API-19_sg1.inp is the same network with specific gravity 1.
 *
 * We open API-19_sg1.inp and call EN_setoption(EN_SP_GRAVITY, 2), then compare
 * the model with API-19_sg2.inp.
 *
 * Correct behaviour: the two models are the same. In an input file every
 * pressure value is in the units of the final specific gravity, so changing
 * the gravity through the API should leave values entered in psi unchanged.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

typedef struct {
    double emitter;     /* J2 emitter coefficient, gpm/psi^0.5 */
    double preq;        /* PDA required pressure, psi */
    double prv;         /* V1 initial setting, psi */
    double control;     /* control level on J2, psi */
    double pressure;    /* J2 pressure after a hydraulic run, psi */
} Model;

static Model read_model(EN_Project ph)
{
    Model m;
    int j2, v1, ctype, link, node, type;
    double setting, pmin, pexp;

    EN_getnodeindex(ph, "J2", &j2);
    EN_getlinkindex(ph, "V1", &v1);

    EN_getnodevalue(ph, j2, EN_EMITTER, &m.emitter);
    EN_getdemandmodel(ph, &type, &pmin, &m.preq, &pexp);
    EN_getlinkvalue(ph, v1, EN_INITSETTING, &m.prv);
    EN_getcontrol(ph, 1, &ctype, &link, &setting, &node, &m.control);

    EN_solveH(ph);
    EN_getnodevalue(ph, j2, EN_PRESSURE, &m.pressure);
    return m;
}

static int compare(const char *name, double expected, double actual)
{
    int bad = fabs(expected - actual) > 1e-3;
    printf("%-30s %10.3f %10.3f   %s\n", name, expected, actual,
           bad ? "<-- differs" : "");
    return bad;
}

int main(void)
{
    EN_Project ph;
    Model file, api;
    int differ = 0;

    /* Reference: specific gravity 2 read from the input file */
    EN_createproject(&ph);
    EN_open(ph, "API-19_sg2.inp", "API-19.rpt", "");
    file = read_model(ph);
    EN_close(ph);

    /* Same network with specific gravity 1, changed to 2 through the API */
    EN_open(ph, "API-19_sg1.inp", "API-19.rpt", "");
    EN_setoption(ph, EN_SP_GRAVITY, 2.0);
    api = read_model(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    printf("                               SG 2 in    SG 1 file\n");
    printf("                               the file   + EN_SP_GRAVITY 2\n");
    differ += compare("J2 emitter coeff. (gpm/psi^n)", file.emitter, api.emitter);
    differ += compare("PDA required pressure (psi)", file.preq, api.preq);
    differ += compare("V1 PRV setting (psi)", file.prv, api.prv);
    differ += compare("Control level on J2 (psi)", file.control, api.control);
    differ += compare("J2 pressure after run (psi)", file.pressure, api.pressure);

    if (differ) {
        printf("FAIL: %d of 5 values differ from the same network read with "
               "specific gravity 2\n", differ);
        return 1;
    }
    printf("PASS: changing the specific gravity keeps the pressure values "
           "entered in psi\n");
    return 0;
}
