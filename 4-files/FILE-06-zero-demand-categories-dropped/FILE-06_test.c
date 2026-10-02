/*
 * FILE-06: demand categories with zero base demand are dropped on save.
 *
 * FILE-06_categories.inp gives J1 a 10 gpm "Domestic" category and a zero
 * "Fire flow" placeholder with its own pattern, J2 a zero category with no
 * name or pattern, and J3 a zero category with only a pattern. The test
 * saves the project, reopens it and lists each junction's categories.
 *
 * Proposed behaviour (eladsal's suggestion in issue #174): a zero category
 * that has a name or a pattern is kept, so J1 keeps 2 categories and J3
 * keeps its pattern. A zero category with neither, as at J2, carries no
 * information and may be dropped.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

typedef struct {
    int count;          /* number of demand categories */
    double base[2];     /* base demand of the first two categories */
    char pattern[2][32];
    char name[2][32];
} Demands;

static Demands get_demands(EN_Project ph, const char *junction)
{
    Demands d;
    int node, i, pat;

    memset(&d, 0, sizeof(d));
    EN_getnodeindex(ph, junction, &node);
    EN_getnumdemands(ph, node, &d.count);
    for (i = 0; i < d.count && i < 2; i++) {
        EN_getbasedemand(ph, node, i + 1, &d.base[i]);
        EN_getdemandpattern(ph, node, i + 1, &pat);
        if (pat > 0) EN_getpatternid(ph, pat, d.pattern[i]);
        EN_getdemandname(ph, node, i + 1, d.name[i]);
    }
    return d;
}

static void print_demands(const char *junction, Demands d)
{
    int i;

    printf("  %s: %d categor%s", junction, d.count, d.count == 1 ? "y" : "ies");
    for (i = 0; i < d.count && i < 2; i++) {
        printf("  [%g gpm, pattern '%s', name '%s']", d.base[i], d.pattern[i], d.name[i]);
    }
    printf("\n");
}

int main(void)
{
    const char *ids[3] = {"J1", "J2", "J3"};
    Demands before[3], after[3];
    EN_Project ph;
    int i, err;

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-06_categories.inp", "FILE-06.rpt", "");
    for (i = 0; i < 3; i++) before[i] = get_demands(ph, ids[i]);
    if (!err) err = EN_saveinpfile(ph, "FILE-06_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);

    EN_createproject(&ph);
    if (!err) err = EN_open(ph, "FILE-06_saved.inp", "FILE-06_saved.rpt", "");
    for (i = 0; i < 3; i++) after[i] = get_demands(ph, ids[i]);
    EN_close(ph);
    EN_deleteproject(ph);
    if (err) {
        printf("FAIL: could not open, save or reopen the network (error %d)\n", err);
        return 1;
    }

    printf("Input file:\n");
    for (i = 0; i < 3; i++) print_demands(ids[i], before[i]);
    printf("Saved and reloaded:\n");
    for (i = 0; i < 3; i++) print_demands(ids[i], after[i]);

    /* J1's "Fire flow" placeholder and J3's pattern must survive */
    if (after[0].count != 2 || strcmp(after[0].name[1], "Fire flow") != 0 ||
        strcmp(after[0].pattern[1], "FIRE") != 0 ||
        strcmp(after[2].pattern[0], "FIRE") != 0) {
        printf("FAIL: zero-demand categories lost their name or pattern "
               "(J1 keeps %d of 2 categories, J3's pattern is '%s')\n",
               after[0].count, after[2].pattern[0]);
        return 1;
    }
    printf("PASS: zero-demand categories with a name or pattern are kept\n");
    return 0;
}
