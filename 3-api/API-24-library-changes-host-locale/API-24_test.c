/*
 * API-24: the library changes the host program's numeric locale.
 *
 * A C program starts in the "C" locale, where printf writes 1.25. This test
 * puts LC_ALL=de_DE.UTF-8 (or fr_FR.UTF-8) in its environment, as a German
 * or French user's session would, then uses EPANET and checks after each
 * call that its own locale and number formatting are unchanged.
 * EN_createproject calls setlocale(LC_NUMERIC, ""), which switches the whole
 * process to the environment's locale, so the host's printf writes 1,25.
 *
 * Correct behaviour: the host keeps "C" and 1.25 throughout. EPANET still
 * reads the dot decimals in API-24_network.inp, writes a dot in the saved
 * input file, and (the intent of pull request #919) uses the system's
 * decimal comma in its report.
 *
 * Uses setenv, so it needs a POSIX system with a de_DE or fr_FR locale.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "epanet2_2.h"

static int host_ok = 1;

/* Print the host's numeric locale and how its printf formats 1.25 */
static void check_host(const char *when)
{
    char buf[32];
    const char *name = setlocale(LC_NUMERIC, NULL);
    int ok;

    snprintf(buf, sizeof buf, "%.2f", 1.25);
    ok = strcmp(name, "C") == 0 && strcmp(buf, "1.25") == 0;
    printf("  %-28s %-14s %s%s\n", when, name, buf, ok ? "" : "   <-- changed");
    if (!ok) host_ok = 0;
}

/* Return 1 if the file has a line containing both strings */
static int file_has(const char *file, const char *s1, const char *s2)
{
    char line[512];
    int found = 0;
    FILE *f = fopen(file, "r");

    while (f && !found && fgets(line, sizeof line, f))
        found = strstr(line, s1) && strstr(line, s2);
    if (f) fclose(f);
    return found;
}

int main(void)
{
    const char *candidates[] = {"de_DE.UTF-8", "fr_FR.UTF-8"};
    const char *env = NULL;
    EN_Project ph;
    int i, j1, err;
    double elev = 0.0;
    int parsed_ok, report_comma, saved_dot;

    /* Find an installed locale with a decimal comma, then go back to "C" */
    for (i = 0; i < 2 && env == NULL; i++)
        if (setlocale(LC_NUMERIC, candidates[i])) env = candidates[i];
    setlocale(LC_NUMERIC, "C");
    if (env == NULL) {
        printf("FAIL: cannot run: neither de_DE.UTF-8 nor fr_FR.UTF-8 is installed\n");
        return 1;
    }

    /* The user's environment, as EPANET will see it */
    setenv("LC_ALL", env, 1);
    printf("Environment: LC_ALL=%s\n", env);
    printf("  %-28s %-14s %s\n", "Host program", "LC_NUMERIC", "printf(\"%.2f\", 1.25)");
    check_host("before EN_createproject");

    EN_createproject(&ph);
    check_host("after EN_createproject");

    err = EN_open(ph, "API-24_network.inp", "API-24.rpt", "");
    EN_getnodeindex(ph, "J1", &j1);
    EN_getnodevalue(ph, j1, EN_ELEVATION, &elev);
    check_host("after EN_open");

    EN_solveH(ph);
    EN_solveQ(ph);
    EN_report(ph);
    check_host("after EN_solveH, EN_report");

    EN_saveinpfile(ph, "API-24_saved.inp");
    check_host("after EN_saveinpfile");

    EN_close(ph);
    EN_deleteproject(ph);

    /* What EPANET itself read and wrote */
    parsed_ok = err == 0 && elev == 12.5;
    report_comma = file_has("API-24.rpt", "Hydraulic Accuracy", "0,001000");
    saved_dot = file_has("API-24_saved.inp", "J1", "12.5");
    printf("EPANET read J1's elevation 12.5 as %g (EN_open returned %d)\n", elev, err);
    printf("Report uses the decimal comma (Hydraulic Accuracy 0,001000): %s\n",
           report_comma ? "yes" : "no");
    printf("Saved input file uses the decimal point (J1 elevation 12.5): %s\n",
           saved_dot ? "yes" : "no");

    if (!host_ok) {
        printf("FAIL: EPANET changed the host program's numeric locale\n");
        return 1;
    }
    if (!parsed_ok || !report_comma || !saved_dot) {
        printf("FAIL: the host's locale is unchanged, but EPANET's numbers are wrong\n");
        return 1;
    }
    printf("PASS: the host's locale is unchanged and EPANET's files are correct\n");
    return 0;
}
