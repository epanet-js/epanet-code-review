/*
 * CRASH-13: EN_report before EN_closeQ closes the live output file.
 *
 * The test runs hydraulics, then steps through water quality with
 * EN_initQ(EN_SAVE), EN_runQ and EN_nextQ, and calls EN_report twice before
 * EN_closeQ. After each EN_report it copies the report and reads J1's head
 * from the node table. Between the two calls it opens a file of its own, as
 * any host program might. writeresults() reads the results through the
 * project's open output file and then fcloses it, but the project keeps the
 * pointer, which the C library may hand out again on the next fopen.
 *
 * Correct behaviour: both reports show the same head at J1, and the test's
 * own file stays open and writable.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Returns J1's head from the last node table in a report, or -1 */
static double head_at_J1(const char *path)
{
    char line[1024], id[32];
    double demand, head = -1.0, h;
    FILE *f = fopen(path, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "%31s %lf %lf", id, &demand, &h) == 3 &&
            strcmp(id, "J1") == 0) {
            head = h;
        }
    }
    if (f != NULL) fclose(f);
    return head;
}

int main(void)
{
    EN_Project ph;
    long t, tstep;
    int err, err1, err2, errClose, written;
    double head1, head2;
    FILE *own;

    EN_createproject(&ph);
    err = EN_open(ph, "CRASH-13_network.inp", "CRASH-13.rpt", "CRASH-13.out");
    if (!err) err = EN_solveH(ph);
    if (err) {
        printf("FAIL: could not open or solve the network (error %d)\n", err);
        return 1;
    }

    /* Step through water quality, saving results to the output file */
    EN_openQ(ph);
    EN_initQ(ph, EN_SAVE);
    do {
        EN_runQ(ph, &t);
        EN_nextQ(ph, &tstep);
    } while (tstep > 0);

    /* Two reports while the quality solver (and the output file) is open,
       with a file of the test's own opened in between */
    err1 = EN_report(ph);
    EN_copyreport(ph, "CRASH-13_first.rpt");
    own = fopen("CRASH-13_own.txt", "w");
    err2 = EN_report(ph);
    EN_copyreport(ph, "CRASH-13_second.rpt");
    written = fprintf(own, "still open\n");
    errClose = EN_closeQ(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    if (written >= 0) fclose(own);   /* not if EN_report already closed it */

    head1 = head_at_J1("CRASH-13_first.rpt");
    head2 = head_at_J1("CRASH-13_second.rpt");
    printf("1st EN_report returned %d, head at J1 %.2f ft\n", err1, head1);
    printf("2nd EN_report returned %d, head at J1 %.2f ft\n", err2, head2);
    printf("EN_closeQ returned %d\n", errClose);
    printf("fprintf to the test's own file after the 2nd report returned %d\n", written);

    if (err1 || err2 || errClose || head1 <= 0.0 || head2 != head1 || written < 0) {
        printf("FAIL: the second report shows a head of %.2f ft instead of %.2f ft%s\n",
               head2, head1, written < 0 ? ", and the test's own file was closed" : "");
        return 1;
    }
    printf("PASS: both reports show the same results\n");
    return 0;
}
