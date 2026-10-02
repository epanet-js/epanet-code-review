/*
 * API-30: EN_deleteproject leaves the report file open after a failed EN_open.
 *
 * EN_open writes its error messages to the report file. When the file has
 * an input error, the project is not opened, and EN_deleteproject calls
 * EN_close only for an open project. So the report file is never closed:
 * its messages stay in the stdio buffer until the program exits, and the
 * file handle (and all the network data) is leaked.
 *
 * API-30_undefined-node.inp has a pipe that ends at an undefined node.
 * The test opens it, deletes the project and reads the report file back.
 * It then repeats the open and delete 200 times with the limit on open
 * files lowered to 64, as a long-running program might.
 *
 * Correct behaviour: the report holds the error messages as soon as the
 * project is deleted, and every EN_open returns 200.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"
#ifndef _WIN32
#include <sys/resource.h>
#endif

#define INPFILE "API-30_undefined-node.inp"

/* Opens the file, which fails, and deletes the project; returns the error */
static int open_and_delete(void)
{
    EN_Project ph;
    int err;

    EN_createproject(&ph);
    err = EN_open(ph, INPFILE, "API-30.rpt", "");
    EN_deleteproject(ph);
    return err;
}

int main(void)
{
    int i, err, n_ok = 0, first_other = 0;
    long size;
    char line[256];
    FILE *f;

    /* 1. The report of one failed open */
    err = open_and_delete();
    printf("EN_open returned %d\n", err);
    f = fopen("API-30.rpt", "r");
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    printf("API-30.rpt after EN_deleteproject: %ld bytes\n", size);
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strstr(line, "Error")) printf("  %s\n", line);
    }
    fclose(f);

    /* 2. Many failed opens, with a low limit on open files */
#ifndef _WIN32
    {
        struct rlimit lim;
        getrlimit(RLIMIT_NOFILE, &lim);
        lim.rlim_cur = 64;
        setrlimit(RLIMIT_NOFILE, &lim);
    }
#endif
    for (i = 1; i <= 200; i++) {
        err = open_and_delete();
        if (err == 200) n_ok++;
        else if (!first_other) {
            first_other = i;
            printf("Open %d returned %d\n", i, err);
        }
    }
    printf("%d of 200 further opens returned 200\n", n_ok);

    if (size == 0 || n_ok != 200) {
        printf("FAIL: the report file was left open after the failed EN_open\n");
        return 1;
    }
    printf("PASS: the report file is closed when the project is deleted\n");
    return 0;
}
