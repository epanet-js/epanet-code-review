/*
 * CRASH-04: inf, nan or huge values in [TIMES] and [OPTIONS] overflow integer
 * conversions.
 *
 * For each case the test copies CRASH-04_network.inp (a reservoir feeding one
 * junction) to CRASH-04_case.inp, appends one [TIMES] or [OPTIONS] line with
 * an out-of-range value, and opens the result. The parser converts times to
 * a long number of seconds with (long)(3600*y + 0.5), and TRIALS, CHECKFREQ
 * and MAXCHECK with (int)y, without checking that the value fits. That
 * conversion is undefined behaviour in C; UndefinedBehaviorSanitizer stops at
 * the first case.
 *
 * Correct behaviour: every case is rejected with error 200 (an invalid
 * option value in the report).
 */
#include <stdio.h>
#include "epanet2_2.h"

typedef struct {
    const char *section;
    const char *line;
    int is_time;        /* 1: read back with EN_gettimeparam, 0: EN_getoption */
    int code;           /* the EN_TimeParameter or EN_Option code */
} Case;

static const Case cases[] = {
    {"[TIMES]",   "Duration            1e300", 1, EN_DURATION},
    {"[TIMES]",   "Hydraulic Timestep  inf",   1, EN_HYDSTEP},
    {"[TIMES]",   "Pattern Start       nan",   1, EN_PATTERNSTART},
    {"[OPTIONS]", "Trials              1e11",  0, EN_TRIALS},
    {"[OPTIONS]", "CheckFreq           1e11",  0, EN_CHECKFREQ},
    {"[OPTIONS]", "MaxCheck            nan",   0, EN_MAXCHECK},
};

/* Write CRASH-04_case.inp: the network followed by one extra line */
static int write_case(const Case *c)
{
    FILE *in = fopen("CRASH-04_network.inp", "r");
    FILE *out = fopen("CRASH-04_case.inp", "w");
    int ch;

    if (in == NULL || out == NULL) return 0;
    while ((ch = fgetc(in)) != EOF) fputc(ch, out);
    fprintf(out, "\n%s\n %s\n", c->section, c->line);
    fclose(in);
    fclose(out);
    return 1;
}

int main(void)
{
    int i, n = sizeof cases / sizeof cases[0], accepted = 0;

    /* Unbuffered, so the output so far is not lost if EPANET crashes */
    setvbuf(stdout, NULL, _IONBF, 0);

    for (i = 0; i < n; i++) {
        const Case *c = &cases[i];
        EN_Project ph;
        long t;
        double v;
        int err;

        if (!write_case(c)) {
            printf("FAIL: cannot write CRASH-04_case.inp\n");
            return 1;
        }
        printf("%s %s\n", c->section, c->line);
        EN_createproject(&ph);
        err = EN_open(ph, "CRASH-04_case.inp", "CRASH-04.rpt", "");

        if (err == 200) {
            printf("    EN_open returned 200\n");
        } else {
            /* Accepted: show what the value was converted to */
            accepted++;
            if (c->is_time) {
                EN_gettimeparam(ph, c->code, &t);
                printf("    EN_open returned %d, value stored as %ld s   <-- accepted\n",
                       err, t);
            } else {
                EN_getoption(ph, c->code, &v);
                printf("    EN_open returned %d, value stored as %.0f   <-- accepted\n",
                       err, v);
            }
        }
        EN_close(ph);
        EN_deleteproject(ph);
    }

    if (accepted > 0) {
        printf("FAIL: %d of %d out-of-range values were accepted\n", accepted, n);
        return 1;
    }
    printf("PASS: every out-of-range value is rejected with error 200\n");
    return 0;
}
