/*
 * SIM-13: PCV curve extrapolation uses 1 instead of 100 %.
 *
 * A positional control valve's curve gives Kv/Kvo (in %) against % open.
 * Above the curve's last point EPANET extrapolates linearly to (100, 100).
 * The network has three PCVs, each passing 1000 gpm:
 *   V1  setting 90 %, curve (0,0) (50,20) (80,50): extrapolate to Kv ratio 75 %
 *   V2  setting 90 %, the same curve plus (100,100): interpolate to 75 %
 *   V3  setting 50 %, curve (0,0) (1,1): extrapolate to 50 %
 *
 * Correct behaviour: V1 and V2 have the same head loss, 2.22 ft, and V3 has
 * four times the fully open head loss, 5.00 ft.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    const char *valve[] = {"V1", "V2", "V3"};
    const double kv_ratio[] = {0.75, 0.75, 0.50};   /* expected Kv / Kvo */

    /* Fully open loss: 0.02517 Km Q^2 / D^4 with Q = 1000 gpm in cfs, D = 1 ft */
    const double q_cfs = 1000.0 / 448.831;
    const double h_open = 0.02517 * 10.0 * q_cfs * q_cfs;

    EN_Project ph;
    int wrong = 0;

    EN_createproject(&ph);
    EN_open(ph, "SIM-13_pcv-curves.inp", "SIM-13.rpt", "");
    EN_solveH(ph);

    printf("Fully open head loss: %.4f ft\n\n", h_open);
    printf("Valve  Kv ratio   Expected head loss   Actual head loss\n");
    printf("                         (ft)                (ft)\n");
    for (int i = 0; i < 3; i++) {
        int v;
        double hl;
        double expected = h_open / (kv_ratio[i] * kv_ratio[i]);

        EN_getlinkindex(ph, valve[i], &v);
        EN_getlinkvalue(ph, v, EN_HEADLOSS, &hl);

        int bad = fabs(hl - expected) > 0.01;
        printf("%-5s  %6.0f %%   %14.4f   %18.4f   %s\n", valve[i], 100 * kv_ratio[i],
               expected, hl, bad ? "<-- wrong" : "");
        wrong += bad;
    }

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d PCV(s) set above their curve's last point have the wrong "
               "head loss\n", wrong);
        return 1;
    }
    printf("PASS: extrapolating a PCV curve to (100, 100) gives the right head loss\n");
    return 0;
}
