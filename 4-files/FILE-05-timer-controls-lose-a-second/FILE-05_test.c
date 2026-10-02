/*
 * FILE-05: timer controls lose one second per save.
 *
 * FILE-05_timer-controls.inp has three AT TIME controls and one AT CLOCKTIME
 * control, at 1200, 22801, 61 and 28801 seconds. The test reads each
 * control's time with EN_getcontrol after opening the file, then saves and
 * reopens the project twice and reads the times again.
 *
 * Correct behaviour: every control keeps the time written in the input file,
 * to the second, after loading and after each save and reload.
 */
#include <stdio.h>
#include "epanet2_2.h"

#define NCONTROLS 4

/* Opens inpFile, stores each control's time (s) in t, and saves the
   project to outFile if outFile is not NULL */
static int read_times(const char *inpFile, const char *outFile, long t[])
{
    EN_Project ph;
    int i, err, type, link, node;
    double setting, level;

    EN_createproject(&ph);
    err = EN_open(ph, inpFile, "FILE-05.rpt", "");
    for (i = 0; !err && i < NCONTROLS; i++) {
        err = EN_getcontrol(ph, i + 1, &type, &link, &setting, &node, &level);
        t[i] = (long)level;
    }
    if (!err && outFile != NULL) err = EN_saveinpfile(ph, outFile);
    EN_close(ph);
    EN_deleteproject(ph);
    return err;
}

int main(void)
{
    /* Times written in the input file, in seconds */
    const long expected[NCONTROLS] = {1200, 22801, 61, 28801};
    const char *text[NCONTROLS] = {"AT TIME 0:20", "AT TIME 6:20:01",
                                   "AT TIME 0:01:01", "AT CLOCKTIME 8:00:01 AM"};
    long loaded[NCONTROLS], save1[NCONTROLS], save2[NCONTROLS];
    int i, wrong = 0;

    if (read_times("FILE-05_timer-controls.inp", "FILE-05_save1.inp", loaded) ||
        read_times("FILE-05_save1.inp", "FILE-05_save2.inp", save1) ||
        read_times("FILE-05_save2.inp", NULL, save2)) {
        printf("FAIL: a file did not load or save\n");
        return 1;
    }

    printf("Control                   Expected  Loaded   1st save  2nd save  (s)\n");
    for (i = 0; i < NCONTROLS; i++) {
        printf("%-24s  %8ld  %8ld  %8ld  %8ld\n", text[i], expected[i],
               loaded[i], save1[i], save2[i]);
        if (loaded[i] != expected[i]) wrong++;
        if (save1[i] != expected[i]) wrong++;
        if (save2[i] != expected[i]) wrong++;
    }

    if (wrong) {
        printf("FAIL: %d of the 12 control times are off by one or two seconds\n", wrong);
        return 1;
    }
    printf("PASS: every control keeps its time through loading and saving\n");
    return 0;
}
