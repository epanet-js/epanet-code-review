/*
 * API-21: controls de-activated with link index 0 still cut time steps.
 *
 * Net1 (1-hour hydraulic step) gets one extra control: a TIMER control that
 * opens pump 9 at 0:30. It is then de-activated with EN_setcontrol and a
 * link index of 0, which EN_setcontrol accepts for that purpose. We list the
 * times of the hydraulic solutions in the first 4 hours.
 *
 * Correct behaviour: a de-activated control has no effect, so the solutions
 * are at 0:00, 1:00, 2:00, 3:00 and 4:00, as without the control.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int pump, idx, type, link, node, extra = 0;
    long t, tstep;
    double setting, level;

    EN_createproject(&ph);
    EN_open(ph, "API-21_Net1.inp", "API-21.rpt", "");
    EN_getlinkindex(ph, "9", &pump);

    /* TIMER control: open pump 9 at 1800 s, then de-activate it */
    EN_addcontrol(ph, EN_TIMER, pump, 1.0, 0, 1800.0, &idx);
    EN_setcontrol(ph, idx, EN_TIMER, 0, 1.0, 0, 1800.0);
    EN_getcontrol(ph, idx, &type, &link, &setting, &node, &level);
    printf("Control %d after EN_setcontrol: link index %d\n", idx, link);

    printf("Hydraulic solutions in the first 4 hours:");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        if (t <= 4 * 3600) {
            printf(" %ld:%02ld", t / 3600, t % 3600 / 60);
            if (t % 3600 != 0) extra++;
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    printf("\n");
    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (extra) {
        printf("FAIL: the de-activated control still cut %d time step(s) short\n", extra);
        return 1;
    }
    printf("PASS: the de-activated control does not change the time steps\n");
    return 0;
}
