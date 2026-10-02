/*
 * CRASH-10: both solvers free the shared node adjacency lists.
 *
 * The hydraulic and water quality solvers share one set of adjacency lists
 * (net->Adjlist). EN_openQ reuses the lists when the hydraulic solver has
 * already built them, but EN_closeH and EN_closeQ each free them. Closing one
 * solver therefore pulls the lists from under the other one.
 *
 * Part A closes the hydraulic solver and then runs water quality from the
 * saved hydraulics. Part B closes the quality solver and then runs the
 * hydraulics again, with TRIALS 1 so that every step raises a warning and the
 * connectivity check walks the lists. Both use Net1.
 *
 * Correct behaviour: every call returns 0 or a warning code. Without the fix,
 * part A dereferences a NULL list array in sortnodes() (a segmentation fault,
 * with or without sanitizers).
 */
#include <stdio.h>
#include "epanet2_2.h"

#define INP "CRASH-10_Net1.inp"

/* Part A: EN_closeH while the quality solver is open, then EN_runQ */
static int quality_after_closeH(void)
{
    EN_Project ph;
    long t, tstep;
    int err;

    EN_createproject(&ph);
    EN_open(ph, INP, "CRASH-10.rpt", "");

    /* Run all hydraulic periods, saving them for the quality solver */
    EN_openH(ph);
    EN_initH(ph, EN_SAVE);
    do {
        EN_runH(ph, &t);
        EN_nextH(ph, &tstep);
    } while (tstep > 0);

    /* EN_openQ finds the hydraulic solver's lists and uses them */
    EN_openQ(ph);
    EN_closeH(ph);
    printf("A: EN_closeH done; running quality from the saved hydraulics\n");

    EN_initQ(ph, EN_NOSAVE);
    do {
        err = EN_runQ(ph, &t);
        if (err == 0) err = EN_nextQ(ph, &tstep);
    } while (err == 0 && tstep > 0);
    printf("A: quality run ended at %ld h with code %d\n", t / 3600, err);

    EN_closeQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    return err;
}

/* Part B: EN_closeQ while the hydraulic solver is open, then EN_runH */
static int hydraulics_after_closeQ(void)
{
    EN_Project ph;
    long t, tstep;
    int err, worst = 0;

    EN_createproject(&ph);
    EN_open(ph, INP, "CRASH-10.rpt", "");

    /* One trial per step: every step ends with a warning, which runs disconnected() */
    EN_setoption(ph, EN_TRIALS, 1);

    /* Run hydraulics and quality together */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    EN_openQ(ph);
    EN_initQ(ph, EN_NOSAVE);
    do {
        EN_runH(ph, &t);
        EN_runQ(ph, &t);
        EN_nextH(ph, &tstep);
        EN_nextQ(ph, &tstep);
    } while (tstep > 0);

    EN_closeQ(ph);
    printf("B: EN_closeQ done; running the hydraulics again\n");

    EN_initH(ph, EN_NOSAVE);
    do {
        err = EN_runH(ph, &t);
        if (err > worst) worst = err;
        EN_nextH(ph, &tstep);
    } while (tstep > 0);
    printf("B: hydraulic run ended at %ld h, highest EN_runH code %d (a warning)\n",
           t / 3600, worst);

    EN_closeH(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    return worst;
}

int main(void)
{
    int errA, errB;

    /* Unbuffered, so the output is complete if the library crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    errA = quality_after_closeH();
    errB = hydraulics_after_closeQ();

    if (errA > 100 || errB > 100) {
        printf("FAIL: a run after closing the other solver returned an error\n");
        return 1;
    }
    printf("PASS: each solver keeps working after the other one is closed\n");
    return 0;
}
