/*
 * CRASH-21: EN_deletedemand with index 0 leaves a dangling demand list.
 *
 * Junction J1 in CRASH-21_two-demands.inp has two demand categories,
 * Domestic (100 gpm) and Industrial (50 gpm). Demand indices start at 1.
 * We call EN_deletedemand with index 0, then read J1's demands back.
 *
 * Correct behaviour: index 0 is rejected with error 253 ("nonexistent demand
 * category") and both demands are kept. Without the fix the call returns 0
 * after freeing the first demand while J1 still points to it, so the next
 * read is a heap use-after-free (reported by AddressSanitizer).
 */
#include <stdio.h>
#include "epanet2_2.h"

int main(void)
{
    EN_Project ph;
    int j1, err, ndemands, i;
    double base;
    char name[EN_MAXID + 1];

    /* Unbuffered, so the output is not lost if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-21_two-demands.inp", "CRASH-21.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);

    err = EN_deletedemand(ph, j1, 0);
    printf("EN_deletedemand(J1, 0) returned %d (expected 253)\n", err);

    /* Read the demand list back; without the fix its head has been freed */
    EN_getnumdemands(ph, j1, &ndemands);
    printf("J1 now has %d demand categories:\n", ndemands);
    for (i = 1; i <= ndemands; i++) {
        EN_getbasedemand(ph, j1, i, &base);
        EN_getdemandname(ph, j1, i, name);
        printf("  %d: %-10s %6.1f gpm\n", i, name, base);
    }

    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 253 || ndemands != 2) {
        printf("FAIL: EN_deletedemand accepted index 0 and corrupted J1's demand list\n");
        return 1;
    }
    printf("PASS: index 0 is rejected with 253 and both demands are kept\n");
    return 0;
}
