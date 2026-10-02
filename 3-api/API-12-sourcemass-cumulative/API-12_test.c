/*
 * API-12: EN_SOURCEMASS returns cumulative mass x 60, not a rate.
 *
 * EN_SOURCEMASS is documented as the source's computed mass inflow, and the
 * toolkit's units table gives source mass injection in mass/minute. The test
 * network has a constant 60 mg/min mass booster at junction J1, with water
 * flowing through J1 all the time. It reads EN_SOURCEMASS every hour.
 *
 * Correct behaviour: 60 mg/min at every hour after the start. (At 0:00 no
 * water has passed the source yet, so the value is 0.)
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int j1, wrong = 0;
    long t, tstep;
    double mass;

    EN_createproject(&ph);
    EN_open(ph, "API-12_mass-source.inp", "API-12.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);

    printf("Hour   EN_SOURCEMASS   Expected (mg/min)\n");
    EN_solveH(ph);
    EN_openQ(ph);
    EN_initQ(ph, EN_NOSAVE);
    do {
        EN_runQ(ph, &t);
        EN_getnodevalue(ph, j1, EN_SOURCEMASS, &mass);
        if (t > 0 && t % 3600 == 0) {
            int bad = fabs(mass - 60.0) > 1e-6;
            printf("%4ld   %13.1f   %17.1f   %s\n", t / 3600, mass, 60.0,
                   bad ? "<-- wrong" : "");
            wrong += bad;
        }
        EN_nextQ(ph, &tstep);
    } while (tstep > 0);
    EN_closeQ(ph);

    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong > 0) {
        printf("FAIL: EN_SOURCEMASS grows with time instead of giving "
               "the 60 mg/min rate\n");
        return 1;
    }
    printf("PASS: EN_SOURCEMASS gives the source's mass inflow rate\n");
    return 0;
}
