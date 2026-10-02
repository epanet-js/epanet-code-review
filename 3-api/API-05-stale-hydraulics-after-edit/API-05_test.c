/*
 * API-05: structural edits do not invalidate the saved hydraulics.
 *
 * API-05_loop.inp: reservoir R1 supplies junction J3 (300 gpm) through
 * pipe P1, two parallel pipes P2 and P3 of different size, and pipe P4.
 *
 * For each of the six functions that change the network structure, we open
 * the network, run EN_solveH and EN_solveQ, make one edit and call EN_solveQ
 * again without re-running the hydraulics. The hydraulics file was written
 * for the old network, so its records no longer line up with the nodes and
 * links.
 *
 * Correct behaviour: after any of these edits EN_solveQ returns error 104
 * ("no hydraulics for water quality analysis") until EN_solveH is run again.
 */
#include <stdio.h>
#include "epanet2_2.h"

#define NEDITS 6

static const char *edit_name[NEDITS] = {
    "EN_deletelink(P2)",
    "EN_addlink(P5, J1 to J3)",
    "EN_addnode(J4)",
    "EN_deletenode(J3)",
    "EN_setlinknodes(P3, J1, J3)",
    "EN_setlinktype(P3, EN_CVPIPE)"
};

/* Open the network, solve it, make edit number 'edit', then return what
   EN_solveQ returns. For the first edit, also print the flow read back for P4. */
static int edit_then_solveq(int edit)
{
    EN_Project ph;
    int j1, j3, p2, p3, p4, idx, err;
    double flow;

    EN_createproject(&ph);
    EN_open(ph, "API-05_loop.inp", "API-05.rpt", "");
    EN_solveH(ph);
    EN_solveQ(ph);

    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodeindex(ph, "J3", &j3);
    EN_getlinkindex(ph, "P2", &p2);
    EN_getlinkindex(ph, "P3", &p3);

    switch (edit) {
    case 0: EN_deletelink(ph, p2, EN_UNCONDITIONAL); break;
    case 1: EN_addlink(ph, "P5", EN_PIPE, "J1", "J3", &idx); break;
    case 2: EN_addnode(ph, "J4", EN_JUNCTION, &idx); break;
    case 3: EN_deletenode(ph, j3, EN_UNCONDITIONAL); break;
    case 4: EN_setlinknodes(ph, p3, j1, j3); break;
    case 5: EN_setlinktype(ph, &p3, EN_CVPIPE, EN_UNCONDITIONAL); break;
    }

    err = EN_solveQ(ph);
    if (edit == 0 && err == 0) {
        /* P4 carries all of J3's 300 gpm; P3 is now link 2 and P4 link 3 */
        EN_getlinkindex(ph, "P4", &p4);
        EN_getlinkvalue(ph, p4, EN_FLOW, &flow);
        printf("  after EN_solveQ, P4 (all of J3's 300 gpm) reads %.1f gpm\n", flow);
    }

    EN_close(ph);
    EN_deleteproject(ph);
    return err;
}

int main(void)
{
    int edit, err, stale = 0;

    for (edit = 0; edit < NEDITS; edit++) {
        err = edit_then_solveq(edit);
        printf("%-30s then EN_solveQ returned %3d (expected 104)\n",
               edit_name[edit], err);
        if (err != 104) stale++;
    }

    if (stale > 0) {
        printf("FAIL: after %d of %d structural edits, EN_solveQ ran on hydraulics "
               "saved for the old network\n", stale, NEDITS);
        return 1;
    }
    printf("PASS: EN_solveQ refuses stale hydraulics after every structural edit\n");
    return 0;
}
