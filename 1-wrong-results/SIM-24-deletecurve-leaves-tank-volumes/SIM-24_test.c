/*
 * SIM-24: EN_deletecurve leaves a tank's volumes computed from the deleted
 * curve.
 *
 * Tank T1 has volume curve VC (a narrow riser below 10 ft and a wide bowl
 * above) and starts at 10 ft, holding 1,000 ft3. It drains 20 gpm into J1.
 * We delete VC with EN_deletecurve. T1 then becomes a cylinder with the
 * curve's nominal area (8,000 ft3 over 20 ft = 400 ft2), as it does when
 * EN_TANKDIAM removes a volume curve. We run one hour.
 *
 * Correct behaviour: the tank's stored initial volume matches the volume of
 * that cylinder at the initial level, and the level falls by the volume
 * drawn (160.4 ft3) divided by 400 ft2, i.e. 0.40 ft.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define PI 3.14159265358979
#define GAL_PER_FT3 7.480519

int main(void)
{
    EN_Project ph;
    int t1, vc;
    long t, tstep;
    double diam, area, init_vol, vol_t0, level0, head, elev, change, expected;

    EN_createproject(&ph);
    EN_open(ph, "SIM-24_tank-curve.inp", "SIM-24.rpt", "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_getcurveindex(ph, "VC", &vc);
    EN_deletecurve(ph, vc);

    /* T1 is now a cylinder with the curve's nominal area */
    EN_getnodevalue(ph, t1, EN_TANKDIAM, &diam);
    area = PI * diam * diam / 4.0;
    EN_getnodevalue(ph, t1, EN_INITVOLUME, &init_vol);
    EN_getnodevalue(ph, t1, EN_TANKLEVEL, &level0);

    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);
    EN_getnodevalue(ph, t1, EN_TANKVOLUME, &vol_t0);   /* from the head */
    EN_nextH(ph, &tstep);
    EN_runH(ph, &t);   /* t = 3600 s */
    EN_getnodevalue(ph, t1, EN_HEAD, &head);
    EN_getnodevalue(ph, t1, EN_ELEVATION, &elev);
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    /* 20 gpm for one hour, spread over the cylinder's area */
    change = (head - elev) - level0;
    expected = -(20.0 * 60.0 / GAL_PER_FT3) / area;

    printf("After EN_deletecurve: T1 area %.1f ft2 (diameter %.2f ft)\n", area, diam);
    printf("T1 EN_INITVOLUME %.1f ft3, volume at the initial level %.1f ft3\n",
           init_vol, vol_t0);
    printf("T1 level change in one hour: %+.3f ft (expected %+.3f)\n",
           change, expected);

    if (fabs(init_vol - vol_t0) > 0.1 || fabs(change - expected) > 0.001) {
        printf("FAIL: T1 kept the volumes of the deleted curve, so its level is "
               "wrong by %.2f ft after one hour\n", fabs(change - expected));
        return 1;
    }
    printf("PASS: T1's volumes follow its new shape\n");
    return 0;
}
