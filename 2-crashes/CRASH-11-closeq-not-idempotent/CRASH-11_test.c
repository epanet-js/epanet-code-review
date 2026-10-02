/*
 * CRASH-11: EN_closeQ after EN_solveQ frees the segment pool twice.
 *
 * EN_solveQ opens, runs and closes the water quality solver itself. A
 * program that then calls EN_closeQ again, for example in its clean-up
 * code, makes closequal() delete the segment memory pool a second time.
 * EN_closeH has the same role for hydraulics and is safe to call twice.
 *
 * The test runs Net1 with EN_solveH and EN_solveQ, then calls EN_closeQ.
 *
 * Correct behaviour: the extra EN_closeQ does nothing and returns 0.
 * Without the fix, AddressSanitizer reports a heap-use-after-free in
 * mempool_delete().
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int err;

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-11_Net1.inp", "CRASH-11.rpt", "");

    err = EN_solveH(ph);
    printf("EN_solveH returned %d\n", err);

    /* EN_solveQ ends with its own call to EN_closeQ */
    err = EN_solveQ(ph);
    printf("EN_solveQ returned %d\n", err);

    /* Hydraulics for comparison: a second close is harmless */
    err = EN_closeH(ph);
    printf("EN_closeH returned %d\n", err);

    err = EN_closeQ(ph);
    printf("EN_closeQ returned %d\n", err);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 0) {
        printf("FAIL: the extra EN_closeQ returned an error\n");
        return 1;
    }
    printf("PASS: EN_closeQ after EN_solveQ is harmless\n");
    return 0;
}
