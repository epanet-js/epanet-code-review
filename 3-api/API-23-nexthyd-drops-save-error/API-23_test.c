/*
 * API-23: nexthyd overwrites a savehyd write error.
 *
 * Builds a chain of 1,200 pipes fed by a reservoir and runs its hydraulics
 * for 24 hours, saving them to EPANET's scratch hydraulics file. Run 1 has
 * no limits. In run 2 the process's file size limit (RLIMIT_FSIZE) makes
 * one write to that file fail with EFBIG, part way through the 4th hour;
 * the SIGXFSZ handler then lifts the limit, as when a full disk gets space
 * back. We compare the saved hydraulics files and the error codes.
 *
 * Correct behaviour: the failed write is reported, as error 308 ("cannot
 * save results to file") from EN_nextH, and a damaged hydraulics file is
 * not used. This test needs POSIX (setrlimit and SIGXFSZ).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#ifndef _WIN32
#include <signal.h>
#include <sys/resource.h>

#define NPIPES     1200     /* each saved step is about 24 kB */
#define SIZE_LIMIT 90000    /* bytes; reached during the 4th saved step */

static struct rlimit normal_limit;
static volatile sig_atomic_t write_failed = 0;

/* Called when a write goes past the limit: note it and lift the limit */
static void on_file_too_big(int sig)
{
    (void)sig;
    write_failed = 1;
    setrlimit(RLIMIT_FSIZE, &normal_limit);
}

/* Reservoir R0 feeding junctions J1 ... J1200 in a chain */
static void build_chain(EN_Project ph, const char *rptfile)
{
    char from[16], to[16], pipe[16];
    int i, index;

    EN_init(ph, rptfile, "", EN_GPM, EN_HW);
    EN_addnode(ph, "R0", EN_RESERVOIR, &index);
    EN_setnodevalue(ph, index, EN_ELEVATION, 200.0);
    strcpy(from, "R0");
    for (i = 1; i <= NPIPES; i++) {
        snprintf(to, sizeof to, "J%d", i);
        snprintf(pipe, sizeof pipe, "P%d", i);
        EN_addnode(ph, to, EN_JUNCTION, &index);
        EN_addlink(ph, pipe, EN_PIPE, from, to, &index);
        strcpy(from, to);
    }
    EN_getnodeindex(ph, to, &index);
    EN_setnodevalue(ph, index, EN_BASEDEMAND, 100.0);   /* gpm at the far end */
    EN_settimeparam(ph, EN_DURATION, 24 * 3600);
}

static long file_size(const char *name)
{
    long size = -1;
    FILE *f = fopen(name, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    return size;
}

/* Runs the hydraulics step by step, then copies the hydraulics file.
   Returns the first error from EN_nextH (0 if none). */
static int run(int n, const char *what)
{
    EN_Project ph;
    char rptfile[32], hydfile[32];
    long t, tstep, failed_at = 0;
    int err, first_err = 0;

    snprintf(rptfile, sizeof rptfile, "API-23_run%d.rpt", n);
    snprintf(hydfile, sizeof hydfile, "API-23_run%d.hyd", n);
    EN_createproject(&ph);
    build_chain(ph, rptfile);

    if (n == 2) {
        struct rlimit limit = normal_limit;
        limit.rlim_cur = SIZE_LIMIT;
        setrlimit(RLIMIT_FSIZE, &limit);
    }
    EN_openH(ph);
    EN_initH(ph, EN_SAVE);
    do {
        EN_runH(ph, &t);
        err = EN_nextH(ph, &tstep);
        if (err && !first_err) {
            first_err = err;
            failed_at = t;
        }
    } while (tstep > 0);
    EN_closeH(ph);
    setrlimit(RLIMIT_FSIZE, &normal_limit);

    printf("Run %d, %s:\n", n, what);
    printf("  a write to the hydraulics file failed: %s\n", write_failed ? "yes" : "no");
    if (first_err)
        printf("  EN_nextH returned %d at t = %ld s\n", first_err, failed_at);
    else
        printf("  EN_nextH returned 0 at every step\n");
    err = EN_savehydfile(ph, hydfile);
    printf("  EN_savehydfile returns %d", err);
    if (err == 0) printf("; the hydraulics file has %ld bytes", file_size(hydfile));
    printf("\n  EN_solveQ returns %d\n", EN_solveQ(ph));

    EN_close(ph);
    EN_deleteproject(ph);
    return first_err;
}

int main(void)
{
    int err;

    getrlimit(RLIMIT_FSIZE, &normal_limit);
    signal(SIGXFSZ, on_file_too_big);

    run(1, "no file size limit");
    err = run(2, "one write fails");

    if (!write_failed) {
        printf("FAIL: the test could not make a write fail\n");
        return 1;
    }
    if (err != 308) {
        printf("FAIL: a write to the hydraulics file failed, but EN_nextH "
               "reported no error\n");
        return 1;
    }
    printf("PASS: EN_nextH reports error 308 for the failed write\n");
    return 0;
}

#else

int main(void)
{
    printf("PASS: not run; this test needs setrlimit() and SIGXFSZ (POSIX)\n");
    return 0;
}

#endif
