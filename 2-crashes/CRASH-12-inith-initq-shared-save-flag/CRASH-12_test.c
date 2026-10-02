/*
 * CRASH-12: EN_initH and EN_initQ share one save flag.
 *
 * Runs Net1 with the hydraulic and water quality solvers stepped together,
 * twice, with different save flags:
 *   Case 1: EN_initH(EN_SAVE),   EN_initQ(EN_NOSAVE)
 *           The hydraulics should be saved, so EN_savehydfile should work.
 *   Case 2: EN_initH(EN_NOSAVE), EN_initQ(EN_SAVE)
 *           The results should go to the output file, so EN_report should
 *           work. In unpatched EPANET the first EN_nextH writes to the
 *           hydraulics file, which was never opened (a crash).
 *
 * Correct behaviour: each solver keeps the save flag it was given.
 */
#include <stdio.h>
#include "epanet2_2.h"

/* Steps both solvers to the end of the run; returns the last error code */
static int run_together(EN_Project ph, int hyd_flag, int qual_flag)
{
    long t, tstep, qstep;
    int err = 0;

    EN_openH(ph);
    EN_initH(ph, hyd_flag);
    EN_openQ(ph);
    EN_initQ(ph, qual_flag);
    do {
        EN_runH(ph, &t);
        EN_runQ(ph, &t);
        err = EN_nextH(ph, &tstep);
        if (err) break;
        err = EN_nextQ(ph, &qstep);
        if (err) break;
    } while (tstep > 0);
    EN_closeQ(ph);
    EN_closeH(ph);
    return err;
}

/* Opens a fresh copy of Net1, so the cases do not share any open files */
static EN_Project open_net1(void)
{
    EN_Project ph;

    EN_createproject(&ph);
    EN_open(ph, "CRASH-12_Net1.inp", "CRASH-12.rpt", "CRASH-12.out");
    return ph;
}

int main(void)
{
    EN_Project ph;
    int err, wrong = 0;

    printf("Case 1: EN_initH(EN_SAVE), EN_initQ(EN_NOSAVE)\n");
    ph = open_net1();
    run_together(ph, EN_SAVE, EN_NOSAVE);
    err = EN_savehydfile(ph, "CRASH-12.hyd");
    printf("  EN_savehydfile returns %d (expected 0; 104 means no hydraulics "
           "were saved)\n", err);
    if (err) wrong++;
    EN_close(ph);
    EN_deleteproject(ph);

    printf("Case 2: EN_initH(EN_NOSAVE), EN_initQ(EN_SAVE)\n");
    fflush(stdout);
    ph = open_net1();
    run_together(ph, EN_NOSAVE, EN_SAVE);
    err = EN_report(ph);
    printf("  EN_report returns %d (expected 0)\n", err);
    if (err) wrong++;
    EN_close(ph);
    EN_deleteproject(ph);

    if (wrong) {
        printf("FAIL: %d of 2 cases lost the save flag given to EN_initH or EN_initQ\n",
               wrong);
        return 1;
    }
    printf("PASS: the hydraulics and the results are saved as requested in both cases\n");
    return 0;
}
