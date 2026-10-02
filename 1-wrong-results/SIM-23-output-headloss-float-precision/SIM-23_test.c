/*
 * SIM-23: output-file head loss is computed from heads rounded to 4-byte floats.
 *
 * The test runs Net2 (SIM-23_Net2.inp, 55 hours, hourly reports). While the
 * hydraulics run it computes each pipe's unit head loss, 1000 * |H1 - H2| / L,
 * from the solver's heads (EN_HEAD, double precision). EN_solveQ then writes
 * the binary output file from the scratch hydraulics file, and the test
 * reads the head loss back with the output reader (epanet_output.h).
 *
 * Correct behaviour: the output file holds the solver's head loss, rounded
 * only to a 4-byte float (relative error below 1e-6). Closed pipes and pipes
 * with no head loss are not compared.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "epanet2_2.h"
#include "epanet_output.h"

/* Prints a count and its share of the total */
static void print_share(const char *label, int n, int total)
{
    printf("%-32s %d (%.1f %%)\n", label, n, 100.0 * n / total);
}

int main(void)
{
    EN_Project ph;
    ENR_Handle out = NULL;
    long t, tstep, rstart, rstep, duration;
    int i, k, err, nlinks, nperiods, len, type;
    int compared = 0, over1 = 0, over10 = 0, zero = 0;
    int worstLink = 0, worstPeriod = 0;
    double *ref, worst = 0.0, worstRef = 0.0, worstOut = 0.0;
    char *isPipe, id[EN_MAXID + 1];
    float *headloss;

    EN_createproject(&ph);
    err = EN_open(ph, "SIM-23_Net2.inp", "SIM-23.rpt", "SIM-23.out");
    if (err) {
        printf("FAIL: could not open the network (error %d)\n", err);
        return 1;
    }
    EN_setqualtype(ph, EN_NONE, "", "", "");   /* quality plays no part here */
    EN_getcount(ph, EN_LINKCOUNT, &nlinks);
    EN_gettimeparam(ph, EN_REPORTSTART, &rstart);
    EN_gettimeparam(ph, EN_REPORTSTEP, &rstep);
    EN_gettimeparam(ph, EN_DURATION, &duration);
    nperiods = (int)((duration - rstart) / rstep) + 1;

    /* ref[k * (nlinks + 1) + i] = head loss of pipe i in reporting period k */
    ref = calloc((size_t)nperiods * (nlinks + 1), sizeof(double));
    isPipe = calloc(nlinks + 1, 1);
    for (i = 1; i <= nlinks; i++) {
        EN_getlinktype(ph, i, &type);
        isPipe[i] = (type == EN_PIPE || type == EN_CVPIPE);
    }

    /* Run the hydraulics, saving them for EN_solveQ, and record the head
       loss the solver has at each reporting time */
    EN_openH(ph);
    EN_initH(ph, EN_SAVE);
    do {
        EN_runH(ph, &t);
        if (t >= rstart && (t - rstart) % rstep == 0) {
            k = (int)((t - rstart) / rstep);
            for (i = 1; i <= nlinks; i++) {
                int n1, n2;
                double h1, h2, length, status;
                if (!isPipe[i]) continue;
                EN_getlinknodes(ph, i, &n1, &n2);
                EN_getnodevalue(ph, n1, EN_HEAD, &h1);
                EN_getnodevalue(ph, n2, EN_HEAD, &h2);
                EN_getlinkvalue(ph, i, EN_LENGTH, &length);
                EN_getlinkvalue(ph, i, EN_STATUS, &status);
                if (status != 0.0) {
                    ref[k * (nlinks + 1) + i] = 1000.0 * fabs(h1 - h2) / length;
                }
            }
        }
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);

    /* Write the output file the usual way */
    err = EN_solveQ(ph);
    if (err) {
        printf("FAIL: EN_solveQ returned %d\n", err);
        return 1;
    }

    /* Compare with the head loss stored in the output file */
    ENR_init(&out);
    ENR_open(out, "SIM-23.out");
    for (k = 0; k < nperiods; k++) {
        if (ENR_getLinkAttribute(out, k, ENR_headloss, &headloss, &len) != 0) break;
        for (i = 1; i <= nlinks && i <= len; i++) {
            double r = ref[k * (nlinks + 1) + i], o = headloss[i - 1], rel;
            if (!isPipe[i] || r < 1e-6) continue;   /* closed or no flow */
            rel = fabs(o - r) / r;
            compared++;
            if (o == 0.0f) zero++;
            if (rel > 0.01) over1++;
            if (rel > 0.10) over10++;
            if (rel > worst) {
                worst = rel;
                worstLink = i;
                worstPeriod = k;
                worstRef = r;
                worstOut = o;
            }
        }
        ENR_free((void **)&headloss);
    }
    ENR_close(&out);

    printf("Pipe head-loss values compared:  %d (%d reporting periods)\n",
           compared, nperiods);
    print_share("Off by more than 1 %:", over1, compared);
    print_share("Off by more than 10 %:", over10, compared);
    print_share("Stored as exactly 0:", zero, compared);
    if (worstLink > 0) {
        EN_getlinkid(ph, worstLink, id);
        printf("Worst: pipe %s at %ld:00, solver %.6f, output file %.6f ft/1000 ft "
               "(%.0f %% off)\n", id, (rstart + worstPeriod * rstep) / 3600,
               worstRef, worstOut, 100.0 * worst);
    }
    EN_close(ph);
    EN_deleteproject(ph);
    free(ref);
    free(isPipe);

    if (worst > 1e-6) {
        printf("FAIL: %.1f %% of the head losses in the output file are off by more "
               "than 1 %%\n", 100.0 * over1 / compared);
        return 1;
    }
    printf("PASS: the output file holds the solver's head loss for every pipe\n");
    return 0;
}
