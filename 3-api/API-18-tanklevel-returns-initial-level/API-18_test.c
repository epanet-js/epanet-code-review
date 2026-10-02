/*
 * API-18: EN_TANKLEVEL returns the initial level but is documented as current.
 *
 * Tank T1 starts at a level of 10 ft and drains 200 gpm for one hour.
 * Reservoir R1 has a head of 50 ft.
 *   A. During the run we read T1's EN_TANKLEVEL and its current level
 *      (EN_HEAD - EN_ELEVATION). EN_TANKLEVEL is the initial level, as the
 *      maintainers ruled in PR #633; only the documentation said otherwise.
 *   B. We set R1's EN_TANKLEVEL to 60. For a reservoir the setter treats the
 *      value as the reservoir's head, so we read EN_TANKLEVEL back.
 *
 * Correct behaviour: in A, EN_TANKLEVEL stays 10 ft while the tank drains
 * (this holds with and without the fix; the fix corrects the documentation).
 * In B, EN_TANKLEVEL reads back the 60 that was set.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int t1, r1, err, wrong_tank = 0, wrong_res = 0;
    long t, tstep;
    double level_param, head, elev, r1_level, r1_elev;

    EN_createproject(&ph);
    EN_open(ph, "API-18_network.inp", "API-18.rpt", "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_getnodeindex(ph, "R1", &r1);

    /* A. A draining tank */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        EN_getnodevalue(ph, t1, EN_TANKLEVEL, &level_param);
        EN_getnodevalue(ph, t1, EN_HEAD, &head);
        EN_getnodevalue(ph, t1, EN_ELEVATION, &elev);
        printf("A. t = %ld s: T1 EN_TANKLEVEL %.3f ft, current level %.3f ft\n",
               t, level_param, head - elev);
        if (fabs(level_param - 10.0) > 1e-9) wrong_tank++;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);

    /* B. A reservoir */
    err = EN_setnodevalue(ph, r1, EN_TANKLEVEL, 60.0);
    EN_getnodevalue(ph, r1, EN_TANKLEVEL, &r1_level);
    EN_getnodevalue(ph, r1, EN_ELEVATION, &r1_elev);
    printf("B. R1: EN_setnodevalue(EN_TANKLEVEL, 60) returned %d; EN_TANKLEVEL reads %g"
           " (expected 60), EN_ELEVATION reads %g\n", err, r1_level, r1_elev);
    if (fabs(r1_level - 60.0) > 1e-9) wrong_res++;

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong_tank) {
        printf("FAIL: EN_TANKLEVEL changed during the run\n");
        return 1;
    }
    if (wrong_res) {
        printf("FAIL: a reservoir's EN_TANKLEVEL does not read back the value set\n");
        return 1;
    }
    printf("PASS: EN_TANKLEVEL is the initial level of a tank and the head of a "
           "reservoir, for both set and get\n");
    return 0;
}
