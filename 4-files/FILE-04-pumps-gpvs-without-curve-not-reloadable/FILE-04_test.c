/*
 * FILE-04: pumps and GPVs without a curve are saved in a form the parser rejects.
 *
 * FILE-04_pump-no-curve.inp has a pump PU1 with a speed setting but no head
 * curve or power, which the parser accepts. The test adds a GPV, V1, through
 * the API without assigning a head-loss curve (the state EN_addlink leaves
 * it in), saves the project, prints the two lines written for them, and
 * reopens the saved file.
 *
 * Correct behaviour: the saved file reopens, PU1 still has no curve and a
 * speed of 0.9, and V1 is still a GPV with no curve (curve index 0).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Prints a line with each run of blanks and tabs shortened to one space */
static void print_compact(const char *line)
{
    int blank = 0;

    printf(" ");
    for (; *line != '\0'; line++) {
        if (*line == ' ' || *line == '\t') blank = 1;
        else {
            if (blank) putchar(' ');
            putchar(*line);
            blank = 0;
        }
    }
}

/* Prints the lines of a file that start with " <id>" followed by a blank */
static void print_lines(const char *path, const char *id)
{
    char line[1024];
    size_t n = strlen(id);
    FILE *f = fopen(path, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == ' ' && strncmp(line + 1, id, n) == 0 &&
            (line[n + 1] == ' ' || line[n + 1] == '\t')) {
            print_compact(line);
        }
    }
    if (f != NULL) fclose(f);
}

/* Prints the error messages in a report file */
static void print_errors(const char *path)
{
    char line[1024];
    FILE *f = fopen(path, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "Error") != NULL) {
            printf("%s%s", line, strchr(line, '\n') != NULL ? "" : "\n");
        }
    }
    if (f != NULL) fclose(f);
}

int main(void)
{
    EN_Project ph;
    int err, pu1, v1, type;
    double curve, speed, gpvcurve;

    /* Load the network and add a GPV with no curve */
    EN_createproject(&ph);
    err = EN_open(ph, "FILE-04_pump-no-curve.inp", "FILE-04.rpt", "");
    if (!err) err = EN_addlink(ph, "V1", EN_GPV, "J1", "J2", &v1);
    if (!err) err = EN_saveinpfile(ph, "FILE-04_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);
    if (err) {
        printf("FAIL: could not build and save the network (error %d)\n", err);
        return 1;
    }

    printf("Lines written for PU1 and V1:\n");
    print_lines("FILE-04_saved.inp", "PU1");
    print_lines("FILE-04_saved.inp", "V1");

    /* Reopen the saved file */
    EN_createproject(&ph);
    err = EN_open(ph, "FILE-04_saved.inp", "FILE-04_reload.rpt", "");
    printf("\nEN_open(saved file) returned %d\n", err);
    if (err) {
        EN_close(ph);
        EN_deleteproject(ph);
        print_errors("FILE-04_reload.rpt");
        printf("FAIL: the saved file does not load (error %d)\n", err);
        return 1;
    }

    EN_getlinkindex(ph, "PU1", &pu1);
    EN_getlinkindex(ph, "V1", &v1);
    EN_getlinkvalue(ph, pu1, EN_PUMP_HCURVE, &curve);
    EN_getlinkvalue(ph, pu1, EN_INITSETTING, &speed);
    EN_getlinktype(ph, v1, &type);
    EN_getlinkvalue(ph, v1, EN_GPV_CURVE, &gpvcurve);
    EN_close(ph);
    EN_deleteproject(ph);

    printf("PU1: head curve %.0f, speed %.2f\n", curve, speed);
    printf("V1:  type %s, curve %.0f\n", type == EN_GPV ? "GPV" : "not GPV", gpvcurve);
    if (curve != 0 || speed != 0.9 || type != EN_GPV || gpvcurve != 0) {
        printf("FAIL: the reloaded pump or valve is not the one that was saved\n");
        return 1;
    }
    printf("PASS: the saved file reloads with the same curveless pump and GPV\n");
    return 0;
}
