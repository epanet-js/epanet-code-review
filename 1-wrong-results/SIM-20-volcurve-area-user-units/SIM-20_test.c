/*
 * SIM-20: EN_VOLCURVE stores the tank area in user units.
 *
 * SI network: tank T1 is a cylinder of 100 m2 (11.284 m diameter) that
 * drains 5 L/s. Volume curve VC describes the same 100 m2 cylinder.
 * We assign VC to T1 with EN_setnodevalue(EN_VOLCURVE) and read the
 * diameter back. Then we delete the curve, which turns T1 back into a
 * cylinder with the curve's nominal area, and run one hour.
 *
 * Correct behaviour: the diameter stays 11.284 m (as with EN_settankdata
 * or an input file), and the level changes by -(5 L/s x 3600 s) / 100 m2
 * = -0.18 m.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int t1, vc;
    long t, tstep;
    double diam_before, diam_after, level0, head, elev, change;

    EN_createproject(&ph);
    EN_open(ph, "SIM-20_si-tank.inp", "SIM-20.rpt", "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_getcurveindex(ph, "VC", &vc);

    /* Assign the volume curve; it has the same 100 m2 area */
    EN_getnodevalue(ph, t1, EN_TANKDIAM, &diam_before);
    EN_setnodevalue(ph, t1, EN_VOLCURVE, vc);
    EN_getnodevalue(ph, t1, EN_TANKDIAM, &diam_after);
    printf("T1 diameter: %.3f m as a cylinder, %.3f m after EN_VOLCURVE "
           "(expected 11.284)\n", diam_before, diam_after);

    /* Delete the curve: T1 becomes a cylinder with the curve's nominal area */
    EN_deletecurve(ph, vc);
    EN_getnodevalue(ph, t1, EN_TANKLEVEL, &level0);
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    EN_nextH(ph, &tstep);
    EN_runH(ph, &t);   /* t = 3600 s */
    EN_getnodevalue(ph, t1, EN_HEAD, &head);
    EN_getnodevalue(ph, t1, EN_ELEVATION, &elev);
    EN_closeH(ph);
    change = (head - elev) - level0;
    printf("After EN_deletecurve, T1's level changes by %+.3f m in one hour "
           "(expected -0.180)\n", change);

    EN_close(ph);
    EN_deleteproject(ph);

    if (fabs(diam_after - 11.284) > 0.001 || fabs(change + 0.180) > 0.001) {
        printf("FAIL: EN_VOLCURVE gave T1 an area of %.2f m2 instead of 100 m2\n",
               3.14159265 * diam_after * diam_after / 4.0);
        return 1;
    }
    printf("PASS: EN_VOLCURVE gives T1 the curve's area of 100 m2\n");
    return 0;
}
