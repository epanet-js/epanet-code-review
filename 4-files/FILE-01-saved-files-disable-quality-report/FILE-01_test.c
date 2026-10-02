/*
 * FILE-01: every saved file turns off the node Quality column when reloaded.
 *
 * FILE-01_chlorine.inp models chlorine and asks for the default report
 * columns plus the friction factor ("F-Factor Yes"). The test writes a report
 * for the original file, saves the project with EN_saveinpfile, writes a
 * report for the saved file, and checks which columns each report has.
 *
 * Correct behaviour: saving and reloading does not change the report. Both
 * reports have the "Chlorine" column in the node table and the "F-Factor"
 * column in the link table.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Returns 1 if any line of the file contains text */
static int file_contains(const char *path, const char *text)
{
    char line[1024];
    int found = 0;
    FILE *f = fopen(path, "r");

    if (f == NULL) return 0;
    while (!found && fgets(line, sizeof(line), f) != NULL) {
        found = strstr(line, text) != NULL;
    }
    fclose(f);
    return found;
}

/* Runs hydraulics and quality for inpFile and writes the report to rptFile */
static int write_report(const char *inpFile, const char *rptFile)
{
    EN_Project ph;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, inpFile, rptFile, "FILE-01.out");
    if (!err) err = EN_solveH(ph);
    if (!err) err = EN_solveQ(ph);
    if (!err) err = EN_report(ph);
    EN_close(ph);
    EN_deleteproject(ph);
    return err;
}

int main(void)
{
    EN_Project ph;
    char line[1024];
    FILE *f;
    int chlorineA, chlorineB, ffactorA, ffactorB;

    /* Report for the original file */
    if (write_report("FILE-01_chlorine.inp", "FILE-01_original.rpt") != 0) {
        printf("FAIL: could not run the original file\n");
        return 1;
    }
    chlorineA = file_contains("FILE-01_original.rpt", "Chlorine");
    ffactorA = file_contains("FILE-01_original.rpt", "F-Factor");

    /* Save the project unchanged */
    EN_createproject(&ph);
    EN_open(ph, "FILE-01_chlorine.inp", "FILE-01.rpt", "");
    EN_saveinpfile(ph, "FILE-01_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);

    /* The field options the writer put in [REPORT] for "Quality" and "F-Factor" */
    printf("[REPORT] lines in the saved file:\n");
    f = fopen("FILE-01_saved.inp", "r");
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, " Quality ", 9) == 0 || strncmp(line, " F-Factor ", 10) == 0) {
            printf("  %s", line);
        }
    }
    if (f != NULL) fclose(f);

    /* Report for the saved file */
    if (write_report("FILE-01_saved.inp", "FILE-01_saved.rpt") != 0) {
        printf("FAIL: could not run the saved file\n");
        return 1;
    }
    chlorineB = file_contains("FILE-01_saved.rpt", "Chlorine");
    ffactorB = file_contains("FILE-01_saved.rpt", "F-Factor");

    printf("\nReport column        original file   saved and reloaded\n");
    printf("Chlorine (nodes)     %-15s %s\n", chlorineA ? "yes" : "no",
           chlorineB ? "yes" : "no");
    printf("F-Factor (links)     %-15s %s\n", ffactorA ? "yes" : "no",
           ffactorB ? "yes" : "no");

    if (chlorineA != chlorineB || ffactorA != ffactorB) {
        printf("FAIL: after saving and reloading, the report loses the%s%s%s\n",
               chlorineA != chlorineB ? " Chlorine column" : "",
               chlorineA != chlorineB && ffactorA != ffactorB ? " and the" : "",
               ffactorA != ffactorB ? " F-Factor column" : "");
        return 1;
    }
    printf("PASS: the saved file reports the same columns as the original\n");
    return 0;
}
