/*
 * API-27: adding or deleting nodes and links leaves results attached to the
 * wrong objects.
 *
 * API-27_tank-network.inp: R1 -> P1 -> J1 -> P2 -> J2 -> P3 -> tank T1.
 * Node and link results (heads, flows, ...) live in arrays indexed like the
 * nodes and links. For each of three edits we solve the network, note every
 * node's head and every link's flow, make the edit and read them again
 * without solving:
 *   1. EN_addnode adds junction J3, which goes in before R1 and T1;
 *   2. EN_deletenode deletes J1 (and with it P1 and P2);
 *   3. EN_deletelink deletes P1.
 *
 * Correct behaviour: every node and link that is left keeps its own results,
 * and the new junction J3, which has no results yet, reads 0.
 */
#include <stdio.h>
#include "epanet2_2.h"

#define NNODES 5
#define NLINKS 3
static const char *node_ids[NNODES] = {"J1", "J2", "R1", "T1", "J3"};
static const char *link_ids[NLINKS] = {"P1", "P2", "P3"};

/* Read each named object's head (nodes) or flow (links); 0 if it is absent */
static void read_values(EN_Project ph, int nodes, double *v)
{
    int i, index, n = nodes ? NNODES : NLINKS;

    for (i = 0; i < n; i++) {
        v[i] = 0.0;
        if (nodes && EN_getnodeindex(ph, node_ids[i], &index) == 0)
            EN_getnodevalue(ph, index, EN_HEAD, &v[i]);
        if (!nodes && EN_getlinkindex(ph, link_ids[i], &index) == 0)
            EN_getlinkvalue(ph, index, EN_FLOW, &v[i]);
    }
}

/* Print the objects that still exist; return how many read a wrong value */
static int compare(EN_Project ph, int nodes, const double *before)
{
    double after[NNODES];
    int i, index, err, wrong = 0, n = nodes ? NNODES : NLINKS;
    const char *id;

    read_values(ph, nodes, after);
    for (i = 0; i < n; i++) {
        id = nodes ? node_ids[i] : link_ids[i];
        err = nodes ? EN_getnodeindex(ph, id, &index) : EN_getlinkindex(ph, id, &index);
        if (err) continue;    /* deleted */
        printf("    %-3s %-4s %12.6g %-3s (expected %.6g)%s\n", id,
               nodes ? "head" : "flow", after[i], nodes ? "ft" : "gpm", before[i],
               after[i] == before[i] ? "" : "  <-- wrong");
        if (after[i] != before[i]) wrong++;
    }
    return wrong;
}

/* Solve, make edit number 'edit', and check the results that are left */
static int run_edit(int edit)
{
    EN_Project ph;
    int index, wrong;
    double heads[NNODES], flows[NLINKS];
    const char *what[] = {"EN_addnode(J3)", "EN_deletenode(J1)", "EN_deletelink(P1)"};

    EN_createproject(&ph);
    EN_open(ph, "API-27_tank-network.inp", "API-27.rpt", "");
    EN_solveH(ph);
    read_values(ph, 1, heads);
    read_values(ph, 0, flows);

    if (edit == 0) EN_addnode(ph, "J3", EN_JUNCTION, &index);
    if (edit == 1) {
        EN_getnodeindex(ph, "J1", &index);
        EN_deletenode(ph, index, EN_UNCONDITIONAL);
    }
    if (edit == 2) {
        EN_getlinkindex(ph, "P1", &index);
        EN_deletelink(ph, index, EN_UNCONDITIONAL);
    }

    printf("After %s, without solving again:\n", what[edit]);
    wrong = compare(ph, 1, heads) + compare(ph, 0, flows);

    EN_close(ph);
    EN_deleteproject(ph);
    return wrong;
}

int main(void)
{
    int edit, wrong = 0;

    for (edit = 0; edit < 3; edit++) wrong += run_edit(edit);

    if (wrong > 0) {
        printf("FAIL: after the edits, %d results belong to another node or link, "
               "or are unset\n", wrong);
        return 1;
    }
    printf("PASS: every node and link keeps its own results after the edits\n");
    return 0;
}
