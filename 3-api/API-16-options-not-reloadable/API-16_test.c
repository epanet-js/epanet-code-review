/*
 * API-16: EN_CHECKFREQ, EN_MAXCHECK below 1 and tiny viscosity are accepted
 * but cannot be reloaded.
 *
 * For each option value we call EN_setoption, save the project with
 * EN_saveinpfile, reopen the saved file and read the option back. The first
 * two values are valid and only show that a normal round trip works.
 *
 * Correct behaviour: every value that EN_setoption accepts survives the save
 * and reload unchanged. A value that the input file cannot represent is
 * refused by EN_setoption with error 213.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

typedef struct {
    int option;
    const char *name;
    double value;
} Case;

int main(void)
{
    Case cases[] = {
        {EN_CHECKFREQ, "CHECKFREQ",   3},       /* valid */
        {EN_SP_VISCOS, "VISCOSITY",   0.5},     /* valid */
        {EN_CHECKFREQ, "CHECKFREQ",   0.25},
        {EN_MAXCHECK,  "MAXCHECK",    0.5},
        {EN_SP_VISCOS, "VISCOSITY",   1e-10},
        {EN_SP_VISCOS, "VISCOSITY",   5e-4},
        {EN_SP_DIFFUS, "DIFFUSIVITY", 5e-5},
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);
    int k, wrong = 0;

    printf("Option       value     EN_setoption  reopen  value after reload\n");
    for (k = 0; k < ncases; k++) {
        EN_Project ph;
        int err_set, err_open;
        double reloaded = 0.0;
        int ok;

        EN_createproject(&ph);
        EN_open(ph, "API-16_network.inp", "API-16.rpt", "");
        err_set = EN_setoption(ph, cases[k].option, cases[k].value);
        EN_saveinpfile(ph, "API-16_saved.inp");
        EN_close(ph);

        /* Error 200 means the saved file has errors */
        err_open = EN_open(ph, "API-16_saved.inp", "API-16_saved.rpt", "");
        if (err_open < 100) EN_getoption(ph, cases[k].option, &reloaded);
        EN_close(ph);
        EN_deleteproject(ph);

        if (err_set == 213) {
            ok = 1;   /* refused: nothing was changed */
            printf("%-11s %8g  %8d     (not saved)\n", cases[k].name, cases[k].value,
                   err_set);
        }
        else {
            /* CHECKFREQ and MAXCHECK are integers */
            double expected = cases[k].option == EN_CHECKFREQ ||
                              cases[k].option == EN_MAXCHECK ?
                              floor(cases[k].value) : cases[k].value;
            ok = err_open < 100 && fabs(reloaded - expected) <= 1e-6 * fabs(expected);
            printf("%-11s %8g  %8d  %8d   %-10g %s\n", cases[k].name, cases[k].value,
                   err_set, err_open, reloaded,
                   ok ? "" : "<-- accepted, but lost in the saved file");
        }
        wrong += !ok;
    }

    if (wrong) {
        printf("FAIL: %d option values were accepted but do not survive a save and "
               "reload\n", wrong);
        return 1;
    }
    printf("PASS: every accepted value survives a save and reload; "
           "the others are refused\n");
    return 0;
}
