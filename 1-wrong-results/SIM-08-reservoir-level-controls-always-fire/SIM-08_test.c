/*
 * SIM-08: level controls on a reservoir always fire.
 *
 * Two reservoirs feed junction J1 through pipes P1 and P2. Their heads follow
 * time patterns, so their levels (head minus the Head value in the input file)
 * change at 3:00:
 *   RIVER rises from 0 ft to 20 ft;  control: LINK P1 CLOSED IF NODE RIVER ABOVE 10
 *   LAKE falls from 20 ft to 0 ft;   control: LINK P2 CLOSED IF NODE LAKE BELOW 10
 * A third reservoir, MAIN, keeps J1 supplied.
 *
 * Correct behaviour: P1 and P2 stay open until 3:00 and close at 3:00, when
 * the conditions first become true.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int river, lake, p1, p2, wrong = 0;
    long t, tstep;
    double base_river, base_lake;

    EN_createproject(&ph);
    EN_open(ph, "SIM-08_reservoir-levels.inp", "SIM-08.rpt", "");
    EN_getnodeindex(ph, "RIVER", &river);
    EN_getnodeindex(ph, "LAKE", &lake);
    EN_getlinkindex(ph, "P1", &p1);
    EN_getlinkindex(ph, "P2", &p2);

    /* A reservoir's EN_ELEVATION is the Head value from the input file */
    EN_getnodevalue(ph, river, EN_ELEVATION, &base_river);
    EN_getnodevalue(ph, lake, EN_ELEVATION, &base_lake);

    printf("Time   RIVER level  P1      expected   LAKE level  P2      expected\n");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        double h_river, h_lake, s1, s2;
        int open1, open2, want_open;

        EN_runH(ph, &t);
        EN_getnodevalue(ph, river, EN_HEAD, &h_river);
        EN_getnodevalue(ph, lake, EN_HEAD, &h_lake);
        EN_getlinkvalue(ph, p1, EN_STATUS, &s1);
        EN_getlinkvalue(ph, p2, EN_STATUS, &s2);
        open1 = s1 > 0;
        open2 = s2 > 0;

        /* Both conditions become true at 3:00 */
        want_open = t < 3 * 3600;
        printf("%2ld:00  %8.1f ft  %-6s  %-6s     %7.1f ft  %-6s  %s\n", t / 3600,
               h_river - base_river, open1 ? "OPEN" : "CLOSED",
               want_open ? "OPEN" : "CLOSED",
               h_lake - base_lake, open2 ? "OPEN" : "CLOSED",
               want_open ? "OPEN" : "CLOSED");
        wrong += (open1 != want_open) + (open2 != want_open);

        EN_nextH(ph, &tstep);
    } while (tstep > 0);

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d pipe statuses are wrong; the reservoir level controls fired "
               "while their conditions were false\n", wrong);
        return 1;
    }
    printf("PASS: both reservoir level controls fire at 3:00, when their conditions "
           "become true\n");
    return 0;
}
