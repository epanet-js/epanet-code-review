/*
 * CRASH-09: EN_saveinpfile overflows a 20-byte buffer for rule values of
 * 1e15 or more.
 *
 * writepremise() and writeaction() format a rule's value with "%.4f" into
 * char s_value[20]. A value of 1e15 or more needs 21 characters or more.
 *
 * The test adds two rules to a small network: one tests a tank level
 * against 1234567890123456, the other sets valve V1 to that value. It then
 * saves the project with EN_saveinpfile, opens the saved file and reads the
 * two values back.
 *
 * Correct behaviour: the file is saved and both values read back unchanged.
 * Without the fix, AddressSanitizer reports a stack-buffer-overflow in
 * EN_saveinpfile.
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

#define BIG 1234567890123456.0

int main(void)
{
    EN_Project ph;
    int err, err1, err2, logop, object, index, variable, relop, status, link;
    double level, setting;
    char rule1[120], rule2[120];

    /* Unbuffered, so the output is complete if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_open(ph, "CRASH-09_valve-tank.inp", "CRASH-09.rpt", "");

    /* EN_addrule edits its argument, so pass writable copies (see CRASH-19) */
    strcpy(rule1, "RULE BIGLEVEL\nIF TANK T1 LEVEL ABOVE 1234567890123456\n"
                  "THEN VALVE V1 STATUS IS OPEN");
    strcpy(rule2, "RULE BIGSETTING\nIF SYSTEM CLOCKTIME >= 1:00\n"
                  "THEN VALVE V1 SETTING = 1234567890123456");
    err1 = EN_addrule(ph, rule1);
    err2 = EN_addrule(ph, rule2);
    printf("EN_addrule returned %d and %d\n", err1, err2);

    err = EN_saveinpfile(ph, "CRASH-09_saved.inp");
    printf("EN_saveinpfile returned %d\n", err);
    EN_close(ph);

    /* Read the two values back from the saved file */
    err = EN_open(ph, "CRASH-09_saved.inp", "CRASH-09.rpt", "");
    printf("EN_open(saved file) returned %d\n", err);
    level = setting = 0.0;
    if (err == 0) {
        EN_getpremise(ph, 1, 1, &logop, &object, &index, &variable, &relop,
                      &status, &level);
        EN_getthenaction(ph, 2, 1, &link, &status, &setting);
    }
    printf("Premise value:  expected %.1f, read back %.1f\n", BIG, level);
    printf("Action setting: expected %.1f, read back %.1f\n", BIG, setting);
    EN_close(ph);
    EN_deleteproject(ph);

    if (err != 0 || level != BIG || setting != BIG) {
        printf("FAIL: the rule values did not survive the save\n");
        return 1;
    }
    printf("PASS: rule values of 1e15 or more are saved and read back unchanged\n");
    return 0;
}
