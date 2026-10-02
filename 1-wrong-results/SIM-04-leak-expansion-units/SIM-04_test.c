/*
 * SIM-04: which unit of head does the leak expansion rate use?
 *
 * Each input file has one pipe, 100 length units long, from a reservoir to
 * junction J1. Its only leakage parameter is an expansion rate of 1, so the
 * leak area is 1 sq mm for every unit of pressure head in which it is read.
 *
 * The test computes the FAVAD leakage, Q = 0.6 A sqrt(2 g h), for two
 * readings of the rate: sq mm per metre of head, and sq mm per unit of head
 * (per ft in US units), and shows which one EPANET uses. In SI units the two
 * readings agree; in US units they differ by a factor of 0.3048.
 *
 * The code uses per metre in both unit systems, which is the design the
 * maintainers stated in GitHub issue #899. The test passes when it sees that.
 * The fix is to the documentation, so the test passes before and after it.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define M_PER_FT     0.3048
#define GPM_PER_M3S  15850.32

/* FAVAD leakage in m3/s for a leak area in sq mm and a pressure head in m */
static double favad(double area_mm2, double head_m)
{
    return 0.6 * area_mm2 * 1.0e-6 * sqrt(2.0 * 9.81 * head_m);
}

/* Returns 1 if EPANET's leakage matches the "per metre" reading */
static int run(const char *inpfile, int us_units)
{
    EN_Project ph;
    int j1, p1;
    double head, leak, head_m, per_metre, per_unit, to_user;

    EN_createproject(&ph);
    EN_open(ph, inpfile, "SIM-04.rpt", "");
    EN_solveH(ph);
    EN_getnodeindex(ph, "J1", &j1);
    EN_getlinkindex(ph, "P1", &p1);
    EN_getnodevalue(ph, j1, EN_HEAD, &head);         /* J1 is at elevation 0 */
    EN_getlinkvalue(ph, p1, EN_LINK_LEAKAGE, &leak);
    EN_close(ph);
    EN_deleteproject(ph);

    /* Leak area = rate (1) x head, in metres or in the file's head units */
    head_m = us_units ? head * M_PER_FT : head;
    to_user = us_units ? GPM_PER_M3S : 1000.0;       /* m3/s -> gpm or L/s */
    per_metre = favad(1.0 * head_m, head_m) * to_user;
    per_unit = favad(1.0 * head, head_m) * to_user;

    printf("%-5s  %7.2f %-2s  %10.4f %-3s  %13.4f  %20.4f\n",
           us_units ? "US" : "SI", head, us_units ? "ft" : "m", leak,
           us_units ? "gpm" : "L/s", per_metre, per_unit);
    return fabs(leak - per_metre) < 0.01 * per_metre;
}

int main(void)
{
    /* Column headings */
    printf("%38s%s\n", "", "Expected leakage if the rate is per");
    printf("%s%s\n", "Units  Head at J1   EPANET leakage    ",
           "metre of head  unit of head (ft or m)");
    int us = run("SIM-04_us-leak.inp", 1);
    int si = run("SIM-04_si-leak.inp", 0);

    if (us && si) {
        printf("PASS: the expansion rate is read as sq mm per metre of head in both "
               "unit systems\n");
        return 0;
    }
    printf("FAIL: the expansion rate is not read as sq mm per metre of head "
           "(US %s, SI %s)\n", us ? "yes" : "no", si ? "yes" : "no");
    return 1;
}
