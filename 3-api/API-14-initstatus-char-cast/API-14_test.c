/*
 * API-14: EN_INITSTATUS is cast to char before the range check.
 *
 * Pipe P1 is open. A link status must be EN_CLOSED (0) or EN_OPEN (1), and
 * EN_setlinkvalue checks that, but only after converting the value to a
 * char. We set EN_INITSTATUS (before a run) and EN_STATUS (during a run)
 * to 256 and 257, which become 0 and 1 as a char, and read the status back.
 *
 * Correct behaviour: every call is refused with error 211 (illegal link
 * property value) and P1 stays open.
 */
#include <stdio.h>
#include "epanet2_2.h"

static int try_status(EN_Project ph, int property, const char *name, double value)
{
    int p1, err;
    double status;

    EN_getlinkindex(ph, "P1", &p1);
    err = EN_setlinkvalue(ph, p1, property, value);
    EN_getlinkvalue(ph, p1, property, &status);
    printf("%-13s = %3.0f: returned %3d, P1 is now %s   (expected 211, open)\n",
           name, value, err, status == EN_OPEN ? "open" : "CLOSED");

    /* Put the pipe back to open for the next case */
    EN_setlinkvalue(ph, p1, property, EN_OPEN);
    return err == 211 && status == EN_OPEN;
}

int main(void)
{
    EN_Project ph;
    int ok = 0;

    EN_createproject(&ph);
    EN_open(ph, "API-14_network.inp", "API-14.rpt", "");

    ok += try_status(ph, EN_INITSTATUS, "EN_INITSTATUS", 256);
    ok += try_status(ph, EN_INITSTATUS, "EN_INITSTATUS", 257);

    /* EN_STATUS is the current status, so set it during a run */
    EN_openH(ph);
    EN_initH(ph, EN_NOSAVE);
    ok += try_status(ph, EN_STATUS, "EN_STATUS", 256);
    ok += try_status(ph, EN_STATUS, "EN_STATUS", 257);
    EN_closeH(ph);

    EN_close(ph);
    EN_deleteproject(ph);

    if (ok < 4) {
        printf("FAIL: %d of 4 out-of-range statuses were accepted\n", 4 - ok);
        return 1;
    }
    printf("PASS: out-of-range statuses are refused with error 211\n");
    return 0;
}
