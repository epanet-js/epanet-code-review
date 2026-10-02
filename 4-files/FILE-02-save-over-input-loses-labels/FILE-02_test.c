/*
 * FILE-02: saving over the input file loses [LABELS] and [BACKDROP].
 *
 * EPANET does not keep map labels and the backdrop in memory. EN_saveinpfile
 * copies those sections from the input file into the file it writes.
 * The test opens a copy of FILE-02_labels.inp (2 labels, 4 backdrop lines),
 * saves it once to a new file and once over itself, and counts the lines
 * in both sections of each saved file.
 *
 * Correct behaviour: both saved files keep the 2 label lines and the
 * 4 backdrop lines, whatever the file name.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Counts the data lines (not blank, not a comment) in one section of a file */
static int count_lines(const char *path, const char *section)
{
    char line[1024], word[64];
    int inside = 0, n = 0;
    FILE *f = fopen(path, "r");

    if (f == NULL) return -1;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "%63s", word) != 1 || word[0] == ';') continue;
        if (word[0] == '[') inside = (strcmp(word, section) == 0);
        else if (inside) n++;
    }
    fclose(f);
    return n;
}

/* Copies a text file */
static void copy_file(const char *from, const char *to)
{
    char line[1024];
    FILE *in = fopen(from, "r");
    FILE *out = fopen(to, "w");

    while (fgets(line, sizeof(line), in) != NULL) fputs(line, out);
    fclose(in);
    fclose(out);
}

static void report(const char *what, const char *path)
{
    printf("%-34s %6d %10d\n", what, count_lines(path, "[LABELS]"),
           count_lines(path, "[BACKDROP]"));
}

int main(void)
{
    EN_Project ph;
    int err1, err2, labels, backdrop;

    /* Work on a copy, since the test overwrites it */
    copy_file("FILE-02_labels.inp", "FILE-02_work.inp");

    printf("File                               LABELS   BACKDROP\n");
    report("input file", "FILE-02_work.inp");

    EN_createproject(&ph);
    EN_open(ph, "FILE-02_work.inp", "FILE-02.rpt", "");
    err1 = EN_saveinpfile(ph, "FILE-02_other.inp");   /* a different file */
    err2 = EN_saveinpfile(ph, "FILE-02_work.inp");    /* the input file itself */
    EN_close(ph);
    EN_deleteproject(ph);

    report("saved to a new file", "FILE-02_other.inp");
    report("saved over the input file", "FILE-02_work.inp");
    printf("EN_saveinpfile returned %d and %d\n", err1, err2);

    labels = count_lines("FILE-02_work.inp", "[LABELS]");
    backdrop = count_lines("FILE-02_work.inp", "[BACKDROP]");
    if (labels != 2 || backdrop != 4) {
        printf("FAIL: saving over the input file kept %d of 2 label lines "
               "and %d of 4 backdrop lines\n", labels, backdrop);
        return 1;
    }
    printf("PASS: saving over the input file keeps the labels and the backdrop\n");
    return 0;
}
