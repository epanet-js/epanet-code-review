/*
 * CRASH-15: EN_setnodevalues and EN_setlinkvalues write through a NULL
 * badIndex.
 *
 * The badIndex argument of the two bulk setters is optional: both functions
 * start with "if (badIndex) *badIndex = 0;". We pass NULL and give values
 * that the first node or link rejects (a negative emitter coefficient, a
 * negative pipe diameter), so the call takes its error path.
 *
 * Correct behaviour: each call returns its error code (209 for the emitter,
 * 211 for the diameter) and leaves the network unchanged. The unpatched
 * library stores the failing index through the NULL pointer and crashes.
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    double node_values[3] = {-1.0, -1.0, -1.0};   /* invalid emitter coefficients */
    double link_values[2] = {-1.0, -1.0};         /* invalid pipe diameters */
    double diam;
    int err_nodes, err_links;

    EN_createproject(&ph);
    EN_open(ph, "CRASH-15_network.inp", "CRASH-15.rpt", "");

    /* No badIndex wanted, so pass NULL */
    printf("EN_setnodevalues(EN_EMITTER, -1 for every node, NULL) ...\n");
    fflush(stdout);
    err_nodes = EN_setnodevalues(ph, EN_EMITTER, node_values, NULL);
    printf("  returned %d (expected 209)\n", err_nodes);

    printf("EN_setlinkvalues(EN_DIAMETER, -1 for every link, NULL) ...\n");
    fflush(stdout);
    err_links = EN_setlinkvalues(ph, EN_DIAMETER, link_values, NULL);
    EN_getlinkvalue(ph, 1, EN_DIAMETER, &diam);
    printf("  returned %d (expected 211); P1 diameter is still %.1f in\n",
           err_links, diam);

    EN_close(ph);
    EN_deleteproject(ph);

    if (err_nodes != 209 || err_links != 211) {
        printf("FAIL: the bulk setters did not return the expected error codes\n");
        return 1;
    }
    printf("PASS: both bulk setters return an error code when badIndex is NULL\n");
    return 0;
}
