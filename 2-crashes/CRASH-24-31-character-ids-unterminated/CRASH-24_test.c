/*
 * CRASH-24: EN_addnode and EN_addlink leave 31-character IDs unterminated.
 *
 * IDs may have up to EN_MAXID = 31 characters. The test builds a network with
 * EN_init and gives four objects an ID of exactly 31 characters: a tank and a
 * pipe added with EN_addnode and EN_addlink, and a tank and a pipe renamed
 * with EN_setnodeid and EN_setlinkid. For each one it looks the object up by
 * its ID, then reads the ID back into a buffer of EN_MAXID + 1 characters,
 * the size the documentation asks for.
 *
 * Correct behaviour: every look-up finds the object and every ID reads back
 * unchanged. Without the fix the IDs are stored without a terminating zero,
 * so the look-up fails, and reading the ID back runs past it and overflows
 * the caller's buffer (AddressSanitizer: stack-buffer-overflow).
 */
#include <stdio.h>
#include <string.h>
#include "epanet2_2.h"

/* Look up an object by ID, then read its ID back. Returns 1 if both work. */
static int check_id(EN_Project ph, int is_node, int index, const char *id)
{
    char buf[EN_MAXID + 1];
    int found;

    if (is_node) EN_getnodeindex(ph, id, &found);
    else EN_getlinkindex(ph, id, &found);
    printf("%s: found at index %d (expected %d)\n", id, found, index);

    if (is_node) EN_getnodeid(ph, index, buf);
    else EN_getlinkid(ph, index, buf);
    printf("  ID read back: %zu characters\n", strlen(buf));

    return found == index && strcmp(buf, id) == 0;
}

int main(void)
{
    EN_Project ph;
    int j1, t1, t2, p1, p2, ok = 1;

    /* Each of these IDs has exactly 31 characters */
    const char *tank_added   = "Tank_added_with_31_characters_X";
    const char *pipe_added   = "Pipe_added_with_31_characters_X";
    const char *tank_renamed = "Tank_renamed_to_31_characters_X";
    const char *pipe_renamed = "Pipe_renamed_to_31_characters_X";

    /* Unbuffered, so the output is not lost if the library aborts */
    setvbuf(stdout, NULL, _IONBF, 0);

    EN_createproject(&ph);
    EN_init(ph, "CRASH-24.rpt", "", EN_GPM, EN_HW);

    /* Objects with short IDs, two of which are renamed below */
    EN_addnode(ph, "J1", EN_JUNCTION, &j1);
    EN_addnode(ph, "T1", EN_TANK, &t1);
    EN_addlink(ph, "P1", EN_PIPE, "J1", "T1", &p1);

    EN_addnode(ph, tank_added, EN_TANK, &t2);
    EN_addlink(ph, pipe_added, EN_PIPE, "J1", "T1", &p2);
    EN_setnodeid(ph, t1, tank_renamed);
    EN_setlinkid(ph, p1, pipe_renamed);

    ok &= check_id(ph, 1, t2, tank_added);
    ok &= check_id(ph, 0, p2, pipe_added);
    ok &= check_id(ph, 1, t1, tank_renamed);
    ok &= check_id(ph, 0, p1, pipe_renamed);

    EN_close(ph);
    EN_deleteproject(ph);

    if (!ok) {
        printf("FAIL: 31-character IDs are stored without a terminating zero\n");
        return 1;
    }
    printf("PASS: all four 31-character IDs are found and read back unchanged\n");
    return 0;
}
