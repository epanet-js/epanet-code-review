/*
 * FILE-17: [OPTIONS] values that load but cannot be saved and reloaded.
 *
 * For each case the test appends one [OPTIONS] line to FILE-17_network.inp
 * (a reservoir feeding one junction), opens the result, saves it with
 * EN_saveinpfile and opens the saved copy.
 *
 *   - CHECKFREQ and MAXCHECK below 1 are accepted and stored as (int)0.5 = 0.
 *     The saved "CHECKFREQ 0" is then refused by the same parser.
 *     Correct: refused when first read (error 213), as TRIALS below 1 is.
 *   - The other values are written with a fixed number of decimals, so small
 *     ones are saved as 0: the copy fails to open, or opens with 0, which
 *     for TOLERANCE, DAMPLIMIT, HEADERROR and FLOWCHANGE means "off".
 *     Correct: the saved copy opens with the same value.
 */
#include <stdio.h>
#include <math.h>
#include "epanet2_2.h"

#define PEXP -1    /* PRESSURE EXPONENT, read with EN_getdemandmodel */

typedef struct {
    const char *line;   /* line(s) appended to [OPTIONS] */
    int option;         /* EN_Option code to read back, or PEXP */
    int must_refuse;    /* 1 if the first open should fail */
} Case;

static const Case cases[] = {
    {"CHECKFREQ          0.5",        EN_CHECKFREQ,  1},
    {"MAXCHECK           0.5",        EN_MAXCHECK,   1},
    {"SPECIFIC GRAVITY   0.0000001",  EN_SP_GRAVITY, 0},
    {"DEMAND MULTIPLIER  0.00001",    EN_DEMANDMULT, 0},
    {"EMITTER EXPONENT   0.00001",    EN_EMITEXPON,  0},
    {"TOLERANCE          0.000000001", EN_TOLERANCE, 0},
    {"DAMPLIMIT          0.000000001", EN_DAMPLIMIT, 0},
    {"HEADERROR          0.000000001", EN_HEADERROR, 0},
    {"FLOWCHANGE         0.000000001", EN_FLOWCHANGE, 0},
    {"DEMAND MODEL PDA\n PRESSURE EXPONENT  0.00001", PEXP, 0},
};

/* Write FILE-17_case.inp: the network followed by one [OPTIONS] entry */
static int write_case(const char *line)
{
    FILE *in = fopen("FILE-17_network.inp", "r");
    FILE *out = fopen("FILE-17_case.inp", "w");
    int ch;

    if (in == NULL || out == NULL) return 0;
    while ((ch = fgetc(in)) != EOF) fputc(ch, out);
    fprintf(out, "\n[OPTIONS]\n %s\n", line);
    fclose(in);
    fclose(out);
    return 1;
}

/* Open a file, read the option's value and, if asked, save a copy */
static int open_file(const char *inpfile, int option, double *value, int save)
{
    EN_Project ph;
    int err, type;
    double pmin, preq;

    EN_createproject(&ph);
    err = EN_open(ph, inpfile, "FILE-17.rpt", "");
    if (err == 0) {
        if (option == PEXP) EN_getdemandmodel(ph, &type, &pmin, &preq, value);
        else EN_getoption(ph, option, value);
        if (save) EN_saveinpfile(ph, "FILE-17_saved.inp");
    }
    EN_close(ph);
    EN_deleteproject(ph);
    return err;
}

int main(void)
{
    int i, n = sizeof cases / sizeof cases[0], bad = 0;

    printf("%-32s %-22s %s\n", "[OPTIONS] value", "first open", "saved copy");
    for (i = 0; i < n; i++) {
        const Case *c = &cases[i];
        const char *name = c->line;
        double v1 = 0.0, v2 = 0.0;
        int err1, err2, ok;

        if (!write_case(c->line)) {
            printf("FAIL: cannot write FILE-17_case.inp\n");
            return 1;
        }
        if (c->option == PEXP) name = "PRESSURE EXPONENT  0.00001";
        err1 = open_file("FILE-17_case.inp", c->option, &v1, 1);

        if (err1 != 0) {
            /* Refused on first reading */
            printf("%-32s error %-16d -\n", name, err1);
            ok = c->must_refuse;
        } else {
            err2 = open_file("FILE-17_saved.inp", c->option, &v2, 0);
            if (err2 != 0) printf("%-32s %-22g error %d", name, v1, err2);
            else           printf("%-32s %-22g %g", name, v1, v2);
            ok = !c->must_refuse && err2 == 0 && fabs(v2 - v1) <= 1e-6 * fabs(v1);
            printf("%s\n", ok ? "" : "   <-- not reloaded");
        }
        if (!ok) bad++;
    }

    if (bad > 0) {
        printf("FAIL: %d of %d option values load but do not survive a save and reload\n",
               bad, n);
        return 1;
    }
    printf("PASS: every option value is either refused or survives a save and reload\n");
    return 0;
}
