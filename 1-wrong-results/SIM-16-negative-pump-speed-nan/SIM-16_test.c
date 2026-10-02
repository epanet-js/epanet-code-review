/*
 * SIM-16: a negative pump-speed pattern factor gives NaN for the rest of
 * the run.
 *
 * Pump PU1 lifts water from R1 to junction J1 (100 gpm). Its speed pattern
 * is 1, -1, 1 over three hourly periods. Reservoir R2, behind a check
 * valve, supplies J1 when the pump is off.
 *
 * Correct behaviour: no NaN anywhere. A negative speed has no physical
 * meaning, so the pump is closed in the second hour and R2 supplies J1.
 * The third hour (speed 1 again) repeats the first.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int pu1, j1, nan_count = 0;
    long t, tstep;
    double head[3] = {0, 0, 0}, pumpflow[3] = {0, 0, 0};
    int period = 0;

    EN_createproject(&ph);
    EN_open(ph, "SIM-16_negative-speed.inp", "SIM-16.rpt", "");
    EN_getlinkindex(ph, "PU1", &pu1);
    EN_getnodeindex(ph, "J1", &j1);

    printf("Hour   Speed   Pump flow (gpm)   Head at J1 (ft)\n");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        double speed, flow, h;

        EN_runH(ph, &t);
        EN_getlinkvalue(ph, pu1, EN_SETTING, &speed);
        EN_getlinkvalue(ph, pu1, EN_FLOW, &flow);
        EN_getnodevalue(ph, j1, EN_HEAD, &h);
        printf("%4ld   %5.1f   %15.2f   %15.2f\n", t / 3600, speed, flow, h);

        if (isnan(flow) || isnan(h)) nan_count++;
        if (period < 3) {
            head[period] = h;
            pumpflow[period] = flow;
            period++;
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (nan_count) {
        printf("FAIL: %d of 3 periods have NaN results, including the period after "
               "the speed is back to 1\n", nan_count);
        return 1;
    }
    if (fabs(pumpflow[1]) > 0.01) {
        printf("FAIL: the pump is not closed while its speed is negative\n");
        return 1;
    }
    if (fabs(head[2] - head[0]) > 0.01) {
        printf("FAIL: the third hour (speed 1) does not repeat the first\n");
        return 1;
    }
    printf("PASS: no NaN; the pump is off at the negative speed and recovers "
           "afterwards\n");
    return 0;
}
