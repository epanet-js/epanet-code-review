/*
 * API-04: EN_settankdata with diameter 0 turns a tank into a reservoir.
 *
 * API-04_two-tanks.inp has two identical tanks, T1 and T2 (50 ft diameter,
 * bottom at 100 ft, initial level 10 ft), both draining to junction J1.
 * We call EN_settankdata on T1 with a diameter of 0 and no volume curve, and
 * EN_setnodevalue(EN_TANKDIAM, 0) on T2, then run the network for 4 hours.
 *
 * The maintainers keep this conversion for backward compatibility (#647):
 * a [TANKS] line with diameter 0 also becomes a reservoir. The fix for this
 * issue documents it. The test checks the documented behaviour: T1 becomes
 * a reservoir with a fixed head of 110 ft, the change cannot be undone
 * (error 263), and EN_setnodevalue rejects a diameter of 0 (error 209).
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int t1, t2, type, err_diam, err_data, err_again;
    double head1, head2;

    EN_createproject(&ph);
    EN_open(ph, "API-04_two-tanks.inp", "API-04.rpt", "");
    EN_getnodeindex(ph, "T1", &t1);
    EN_getnodeindex(ph, "T2", &t2);

    err_diam = EN_setnodevalue(ph, t2, EN_TANKDIAM, 0.0);
    printf("EN_setnodevalue(T2, EN_TANKDIAM, 0) returned %d\n", err_diam);

    /* elev, initlvl, minlvl, maxlvl, diam, minvol, volcurve */
    err_data = EN_settankdata(ph, t1, 100, 10, 0, 20, 0.0, 0, "");
    EN_getnodetype(ph, t1, &type);
    printf("EN_settankdata(T1, diameter 0) returned %d; T1 is now type %d "
           "(EN_TANK = %d, EN_RESERVOIR = %d)\n", err_data, type, EN_TANK, EN_RESERVOIR);

    /* Try to restore the original 50 ft diameter */
    err_again = EN_settankdata(ph, t1, 100, 10, 0, 20, 50.0, 0, "");
    printf("EN_settankdata(T1, diameter 50) returned %d\n", err_again);

    /* Run 4 hours: T2 drains, T1 now supplies at a fixed head */
    EN_solveH(ph);
    EN_getnodevalue(ph, t1, EN_HEAD, &head1);
    EN_getnodevalue(ph, t2, EN_HEAD, &head2);
    printf("Head after 4 hours: T1 %.2f ft, T2 %.2f ft (both started at 110 ft)\n",
           head1, head2);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err_diam == 209 && err_data == 0 && type == EN_RESERVOIR && err_again == 263) {
        printf("PASS: as documented, diameter 0 turned T1 into a reservoir for good, "
               "and EN_setnodevalue refused it with 209\n");
        return 0;
    }
    printf("FAIL: the behaviour differs from the documentation\n");
    return 1;
}
