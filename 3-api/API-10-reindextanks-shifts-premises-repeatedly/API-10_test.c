/*
 * API-10: reindextanks shifts rule premises once per tank.
 *
 * API-10_duplicate-junction.inp lists junction J1 twice. EN_openX accepts
 * the file with error 200 and keeps one J1, so the Node array has one
 * unused slot before the reservoir and tanks. reindextanks() moves R1, T1
 * and T2 down by one slot and should shift rule premises by one as well.
 *
 * Rule RA tests the level of tank T2. Correct behaviour: after EN_openX the
 * premise still refers to T2.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err, t2, logop, object, index, variable, relop, status;
    double value;
    char id[EN_MAXID + 1] = "";

    EN_createproject(&ph);
    err = EN_openX(ph, "API-10_duplicate-junction.inp", "API-10.rpt", "");
    printf("EN_openX returned %d (200: errors in the file, project still opened)\n", err);

    EN_getnodeindex(ph, "T2", &t2);
    EN_getpremise(ph, 1, 1, &logop, &object, &index, &variable, &relop,
                  &status, &value);
    EN_getnodeid(ph, index, id);
    printf("Tank T2 is node %d\n", t2);
    printf("Rule RA premise refers to node %d (%s)\n", index, id);

    EN_close(ph);
    EN_deleteproject(ph);

    if (index != t2) {
        printf("FAIL: the premise moved from T2 to %s\n", id);
        return 1;
    }
    printf("PASS: the premise still refers to T2\n");
    return 0;
}
