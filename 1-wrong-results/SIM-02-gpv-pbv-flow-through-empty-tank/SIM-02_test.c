/*
 * SIM-02: GPV and PBV ignore TEMPCLOSED and pass flow out of an empty tank.
 *
 * The network has three copies of the same layout: an empty tank T feeds
 * junction J through a valve V, and reservoir R feeds J through pipe P.
 * The valve is a GPV in the first copy, a PBV in the second and a TCV in
 * the third. J has a demand of 200 gpm.
 *
 * The tank is at its minimum level, so the solver marks the valve
 * temporarily closed. Correct behaviour: no flow through the valve, and
 * the reservoir supplies the 200 gpm demand through P on its own.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

int main(void)
{
    const char *valve[] = {"V1", "V2", "V3"};
    const char *pipe[] = {"P1", "P2", "P3"};
    const char *type[] = {"GPV", "PBV", "TCV"};
    int wrong = 0;
    long t;
    EN_Project ph;

    EN_createproject(&ph);
    EN_open(ph, "SIM-02_empty-tanks.inp", "SIM-02.rpt", "");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_runH(ph, &t);

    printf("Valve  Type   Valve flow   Pipe flow from reservoir   Expected pipe flow\n");
    printf("                 (gpm)              (gpm)                    (gpm)\n");
    for (int i = 0; i < 3; i++) {
        int v, p;
        double qvalve, qpipe;

        EN_getlinkindex(ph, valve[i], &v);
        EN_getlinkindex(ph, pipe[i], &p);
        EN_getlinkvalue(ph, v, EN_FLOW, &qvalve);
        EN_getlinkvalue(ph, p, EN_FLOW, &qpipe);

        /* With the valve closed, the pipe must carry the whole 200 gpm demand */
        int bad = fabs(qpipe - 200.0) > 0.1;
        printf("%-5s  %-4s  %10.1f   %20.1f   %22.1f   %s\n", valve[i], type[i],
               qvalve, qpipe, 200.0, bad ? "<-- wrong" : "");
        wrong += bad;
    }

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d valve(s) passed flow out of an empty tank; the reservoir "
               "pipe does not carry the demand\n", wrong);
        return 1;
    }
    printf("PASS: no valve passes flow out of an empty tank; each reservoir "
           "supplies the 200 gpm demand\n");
    return 0;
}
