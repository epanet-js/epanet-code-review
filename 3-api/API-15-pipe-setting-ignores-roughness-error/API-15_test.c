/*
 * API-15: EN_SETTING on a pipe ignores the roughness error.
 *
 * For a pipe, EN_INITSETTING and EN_SETTING are its roughness, and
 * EN_setlinkvalue passes the value on to the EN_ROUGHNESS case. That case
 * refuses a roughness of 0 or less with error 211. We set EN_ROUGHNESS,
 * EN_INITSETTING and EN_SETTING of pipe P1 (roughness 100) to -5.
 *
 * Correct behaviour: all three calls return 211 and the roughness stays 100.
 */
#include <stdio.h>
#include "epanet2_2.h"

static int try_roughness(EN_Project ph, int property, const char *name)
{
    int p1, err;
    double rough;

    EN_getlinkindex(ph, "P1", &p1);
    err = EN_setlinkvalue(ph, p1, property, -5.0);
    EN_getlinkvalue(ph, p1, EN_ROUGHNESS, &rough);
    printf("%-14s = -5: returned %3d, roughness is %.0f   (expected 211, 100)\n",
           name, err, rough);
    return err == 211;
}

int main(void)
{
    EN_Project ph;
    int ok = 0;

    EN_createproject(&ph);
    EN_open(ph, "API-15_network.inp", "API-15.rpt", "");

    ok += try_roughness(ph, EN_ROUGHNESS, "EN_ROUGHNESS");
    ok += try_roughness(ph, EN_INITSETTING, "EN_INITSETTING");
    ok += try_roughness(ph, EN_SETTING, "EN_SETTING");

    EN_close(ph);
    EN_deleteproject(ph);

    if (ok < 3) {
        printf("FAIL: %d of 3 calls returned 0 for an invalid roughness\n", 3 - ok);
        return 1;
    }
    printf("PASS: every call reports the invalid roughness with error 211\n");
    return 0;
}
