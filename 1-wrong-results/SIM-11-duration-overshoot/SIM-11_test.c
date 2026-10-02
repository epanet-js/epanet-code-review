/*
 * SIM-11: a duration that is not a multiple of the hydraulic step is
 * overshot.
 *
 * The network is EPANET's Net1 run for 1:30 with a 1:00 hydraulic step.
 * We step through the hydraulics and list the solution times, step through
 * the water quality solver and add up how far it gets, and read the pump's
 * usage factor (percent of the run it was on) from the binary output file.
 *
 * Correct behaviour: the last hydraulic step is shortened to end at 1:30,
 * the water quality solver reaches 1:30, and the usage factor of the pump
 * is at most 100 %.
 */
#include <stdio.h>
#include "epanet2_2.h"
#include "epanet_output.h"

static void print_time(long t)
{
    printf(" %ld:%02ld", t / 3600, t % 3600 / 60);
}

int main(void)
{
    EN_Project ph;
    ENR_Handle out;
    long dur, t, tstep, last_h = 0, qual_end = 0;
    int link, n, wrong = 0;
    float *energy, usage;

    EN_createproject(&ph);
    EN_open(ph, "SIM-11_duration.inp", "SIM-11.rpt", "SIM-11.out");
    EN_gettimeparam(ph, EN_DURATION, &dur);

    /* 1. Times at which a hydraulic solution is computed */
    printf("Hydraulic solutions at:");
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        print_time(t);
        last_h = t;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    EN_closeH(ph);
    printf("   (the last should be");
    print_time(dur);
    printf(")\n");
    if (last_h != dur) wrong++;

    /* 2. How far the water quality solver gets (the time steps add up) */
    EN_solveH(ph);
    EN_openQ(ph);
    EN_initQ(ph, EN_SAVE);
    do {
        EN_runQ(ph, &t);
        EN_nextQ(ph, &tstep);
        qual_end += tstep;
    } while (tstep > 0);
    EN_closeQ(ph);
    printf("Water quality simulated to:");
    print_time(qual_end);
    printf("              (should be");
    print_time(dur);
    printf(")\n");
    if (qual_end != dur) wrong++;
    EN_close(ph);
    EN_deleteproject(ph);

    /* 3. Pump usage factor from the energy section of the output file */
    ENR_init(&out);
    ENR_open(out, "SIM-11.out");
    ENR_getEnergyUsage(out, 1, &link, &energy, &n);
    usage = energy[0];
    ENR_free((void **)&energy);
    ENR_close(&out);
    printf("Pump 9 usage factor: %.2f %%               (should be at most 100 %%)\n",
           usage);
    if (usage > 100.01f) wrong++;

    if (wrong) {
        printf("FAIL: the run goes past its 1:30 duration "
               "(%d of 3 checks failed)\n", wrong);
        return 1;
    }
    printf("PASS: the run ends at 1:30 and the pump's usage factor is %.2f %%\n", usage);
    return 0;
}
