/*
 * WQ-02: check-valve pipes have no volume and no reactions in water quality.
 *
 * Reservoir R1 (chlorine 1.0 mg/L, first-order bulk decay of -1/day) feeds
 * two identical 10,000 ft, 6 in pipes that each supply 100 gpm. P1 is an
 * ordinary pipe; P2 has a check valve, which never closes because the flow
 * is always forward. Everything except R1 starts at 0 mg/L.
 *
 * Correct behaviour: J1 and J2 have the same chlorine at every hour. The
 * water takes about 2.4 h to cross either pipe and loses about 10 % on the
 * way.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int j1, j2, p2, type, differ = 0;
    long t, tstep;
    double c1, c2;

    EN_createproject(&ph);
    EN_open(ph, "WQ-02_twin-pipes.inp", "WQ-02.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "J2", &j2);
    EN_getlinkindex(ph, "P2", &p2);
    EN_getlinktype(ph, p2, &type);
    printf("P2 link type: %d (EN_CVPIPE = %d)\n\n", type, EN_CVPIPE);

    printf("Hour   J1, open pipe   J2, check-valve pipe   (mg/L)\n");
    EN_solveH(ph);
    EN_openQ(ph);
    EN_initQ(ph, EN_NOSAVE);
    do {
        EN_runQ(ph, &t);
        EN_getnodevalue(ph, j1, EN_QUALITY, &c1);
        EN_getnodevalue(ph, j2, EN_QUALITY, &c2);
        if (t % 3600 == 0) {
            int bad = fabs(c1 - c2) > 1e-4;
            printf("%4ld   %13.4f   %20.4f   %s\n", t / 3600, c1, c2,
                   bad ? "<-- differs" : "");
            differ += bad;
        }
        EN_nextQ(ph, &tstep);
    } while (tstep > 0);
    EN_closeQ(ph);

    EN_close(ph);
    EN_deleteproject(ph);

    if (differ > 0) {
        printf("FAIL: the check-valve pipe gives different quality in %d of 7 hours\n",
               differ);
        return 1;
    }
    printf("PASS: the check-valve pipe gives the same quality as the open pipe\n");
    return 0;
}
