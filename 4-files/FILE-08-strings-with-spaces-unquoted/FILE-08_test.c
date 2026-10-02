/*
 * FILE-08: strings with spaces are written unquoted.
 *
 * The test opens FILE-08_network.inp, sets the chemical name to
 * "Free Chlorine" and the tag of junction J1 to "Pressure zone 2", both
 * accepted by the API, and saves the project. It prints the lines written
 * for them, reopens the saved file and reads the strings back.
 *
 * Correct behaviour: the reloaded project has the chemical name
 * "Free Chlorine" with units "mg/L", and J1's tag is "Pressure zone 2".
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

/* Prints the lines of a file that contain text */
static void print_lines(const char *path, const char *text)
{
    char line[1024];
    FILE *f = fopen(path, "r");

    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, text) != NULL) print_compact(line);
    }
    if (f != NULL) fclose(f);
}

int main(void)
{
    EN_Project ph;
    int err, j1, qualType, traceNode;
    char chemName[EN_MAXID + 1], chemUnits[EN_MAXID + 1], tag[EN_MAXMSG + 1];

    EN_createproject(&ph);
    err = EN_open(ph, "FILE-08_network.inp", "FILE-08.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);
    if (!err) err = EN_setqualtype(ph, EN_CHEM, "Free Chlorine", "mg/L", "");
    if (!err) err = EN_settag(ph, EN_NODE, j1, "Pressure zone 2");
    if (!err) err = EN_saveinpfile(ph, "FILE-08_saved.inp");
    EN_close(ph);
    EN_deleteproject(ph);
    if (err) {
        printf("FAIL: could not set the strings or save the file (error %d)\n", err);
        return 1;
    }

    printf("Lines written:\n");
    print_lines("FILE-08_saved.inp", "QUALITY  ");
    print_lines("FILE-08_saved.inp", "zone");

    /* Reopen the saved file and read the strings back */
    EN_createproject(&ph);
    err = EN_open(ph, "FILE-08_saved.inp", "FILE-08_saved.rpt", "");
    if (err) {
        printf("FAIL: the saved file does not load (error %d)\n", err);
        return 1;
    }
    EN_getqualinfo(ph, &qualType, chemName, chemUnits, &traceNode);
    EN_getnodeindex(ph, "J1", &j1);
    EN_gettag(ph, EN_NODE, j1, tag);
    EN_close(ph);
    EN_deleteproject(ph);

    printf("\nAfter reloading:\n");
    printf("  chemical name  '%s'  (expected 'Free Chlorine')\n", chemName);
    printf("  chemical units '%s'  (expected 'mg/L')\n", chemUnits);
    printf("  tag of J1      '%s'  (expected 'Pressure zone 2')\n", tag);

    if (strcmp(chemName, "Free Chlorine") != 0 || strcmp(chemUnits, "mg/L") != 0 ||
        strcmp(tag, "Pressure zone 2") != 0) {
        printf("FAIL: strings with spaces were split into separate tokens on reload\n");
        return 1;
    }
    printf("PASS: strings with spaces survive saving and reloading\n");
    return 0;
}
