/*
 * API-25: EN_report after a structural edit prints misaligned results.
 *
 * API-25_loop.inp: reservoir R1 supplies junction J3 (300 gpm) through pipe
 * P1, two parallel pipes P2 and P3 of different size, and pipe P4. The
 * report lists every link.
 *
 * For each of the six functions that change the network structure, we open
 * the network, run EN_solveH and EN_solveQ (which saves the results to the
 * binary output file), make one edit and call EN_report. The output file
 * holds results for the old network, so they no longer line up with the
 * nodes and links that EN_report lists.
 *
 * Correct behaviour: after any of these edits EN_report returns error 106
 * ("no results saved to report on") until the network is solved again.
 */
#include <stdio.h>
#include <string.h>
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

/* Print the first link results table that EN_report wrote, if any */
static void print_link_table(const char *filename)
{
    char line[256];
    int in_table = 0;
    FILE *f = fopen(filename, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "Link Results") != NULL) in_table = 1;
        else if (in_table && strspn(line, " \r\n") == strlen(line)) break;
        if (in_table) printf("    %s", line);
    }
    if (f != NULL) fclose(f);
}

/* Open the network, solve it, make edit number 'edit', then return what
   EN_report returns. For the first edit, also show the report it wrote. */
static int edit_then_report(int edit)
{
    EN_Project ph;
    int j1, j3, p2, p3, idx, err;

    EN_createproject(&ph);
    EN_open(ph, "API-25_loop.inp", "API-25.rpt", "");
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

    err = EN_report(ph);
    EN_close(ph);
    EN_deleteproject(ph);

    if (edit == 0 && err == 0) {
        /* P1 and P4 carry 300 gpm at 0.85 fps; P3 carries the smaller share */
        printf("  report written after EN_deletelink(P2):\n");
        print_link_table("API-25.rpt");
    }
    return err;
}

int main(void)
{
    int edit, err, stale = 0;

    for (edit = 0; edit < NEDITS; edit++) {
        err = edit_then_report(edit);
        printf("%-30s then EN_report returned %3d (expected 106)\n",
               edit_name[edit], err);
        if (err != 106) stale++;
    }

    if (stale > 0) {
        printf("FAIL: after %d of %d structural edits, EN_report printed results "
               "saved for the old network\n", stale, NEDITS);
        return 1;
    }
    printf("PASS: EN_report refuses stale results after every structural edit\n");
    return 0;
}
