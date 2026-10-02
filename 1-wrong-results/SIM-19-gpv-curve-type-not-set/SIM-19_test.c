/*
 * SIM-19: GPV curves set through the API are never typed as head loss
 * curves, so EN_setflowunits does not convert them.
 *
 * SIM-19_gpv.inp has a GPV, V1, between J1 and J2 (100 gpm). The test
 * assigns it curve C2, (0,0)-(1000 gpm, 20 ft), with EN_setlinkvalue
 * (EN_GPV_CURVE), then switches the project to LPS and saves and reopens it.
 *
 * Correct behaviour: C2 becomes a head loss curve (EN_HLOSS_CURVE), as a
 * curve named on a [VALVES] line does. EN_setflowunits converts it to
 * (0,0)-(63.09 L/s, 6.096 m), so the head loss stays 2 ft = 0.6096 m. The
 * saved file keeps the curve type.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

static int failures = 0;

static void check(const char *what, double expected, double actual)
{
    int bad = fabs(actual - expected) > 0.001 * fabs(expected) + 1e-6;
    printf("%-44s %10.4f %10.4f   %s\n", what, expected, actual,
           bad ? "<-- wrong" : "");
    failures += bad;
}

static double gpv_headloss(EN_Project ph, int v1)
{
    double hl;

    EN_solveH(ph);
    EN_getlinkvalue(ph, v1, EN_HEADLOSS, &hl);
    return hl;
}

int main(void)
{
    EN_Project ph;
    int v1, c2, type;
    double x, y;

    EN_createproject(&ph);
    EN_open(ph, "SIM-19_gpv.inp", "SIM-19.rpt", "");
    EN_getlinkindex(ph, "V1", &v1);
    EN_getcurveindex(ph, "C2", &c2);
    printf("%-44s %10s %10s\n", "", "Expected", "Actual");

    /* Assign C2 to the GPV through the toolkit */
    EN_setlinkvalue(ph, v1, EN_GPV_CURVE, c2);
    EN_getcurvetype(ph, c2, &type);
    check("C2 curve type (EN_HLOSS_CURVE = 3)", EN_HLOSS_CURVE, type);
    check("GPM: V1 head loss (ft)", 2.0, gpv_headloss(ph, v1));

    /* Switch to SI units: C2 should be converted like any head loss curve */
    EN_setflowunits(ph, EN_LPS);
    EN_getcurvevalue(ph, c2, 2, &x, &y);
    check("LPS: C2 second point, flow (L/s)", 63.0902, x);
    check("LPS: C2 second point, head loss (m)", 6.096, y);
    check("LPS: V1 head loss (m)", 0.6096, gpv_headloss(ph, v1));

    /* Save and reopen: the curve type should survive */
    EN_saveinpfile(ph, "SIM-19_saved.inp");
    EN_close(ph);
    EN_open(ph, "SIM-19_saved.inp", "SIM-19.rpt", "");
    EN_getcurveindex(ph, "C2", &c2);
    EN_getcurvetype(ph, c2, &type);
    check("Saved and reopened: C2 curve type", EN_HLOSS_CURVE, type);
    EN_close(ph);
    EN_deleteproject(ph);

    if (failures) {
        printf("FAIL: %d of 6 checks wrong; the API-assigned GPV curve is not a "
               "head loss curve and is not converted\n", failures);
        return 1;
    }
    printf("PASS: the API-assigned GPV curve is typed, converted and saved as a "
           "head loss curve\n");
    return 0;
}
