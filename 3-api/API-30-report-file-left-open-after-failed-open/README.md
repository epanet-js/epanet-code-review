# API-30: EN_deleteproject leaves the report file open after a failed EN_open

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After a failed `EN_open`, `EN_deleteproject` leaves the report file open and the network data allocated. The report stays empty until the program exits, so a program that shows it to the user shows nothing. A long-running program leaks one file handle per failed file, and after enough of them `EN_open` fails with error 303. |
| **Reached from** | Toolkit API: `EN_open` (or `EN_openX`) on a file with errors, then `EN_deleteproject` without `EN_close` |
| **Where** | `EN_deleteproject()` in [`src/epanet.c:68-71`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L68-L71); `openproject()` sets `Openflag` only on success, [`src/project.c:79-99`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L79-L99) |
| **Since** | v2.2, when `EN_deleteproject` was added |
| **GitHub** | No related issue |
| **Fix** | One line, needs CRASH-14: [`API-30_fix.patch`](API-30_fix.patch) |

## The problem

When an input file has errors, `EN_open` writes the error messages to the report file and returns 200. The project is not opened, and the documentation says it "will not accept toolkit function calls", so the natural next step is `EN_deleteproject`. That call does not close the project, because it calls `EN_close` only for an open project.

So the report file is never closed. Its messages are still in the C library's buffer: in the test, `API-30.rpt` is 0 bytes after `EN_deleteproject`, and the 655 bytes with "Error 203: undefined node J9" only reach the disk when the program exits. A GUI, script or service that opens the report to show the user why the file failed finds it empty.

Each failed open also keeps one file handle and all the network arrays. With the limit on open files set to 64, the 60th failed open in a row returns 303 (cannot open report file) instead of 200.

## Why it happens

```c
// src/epanet.c, EN_deleteproject()
    if (p == NULL) return -1;
    if (p->Openflag) {
      EN_close(p);
    }
    remove(p->TmpHydFname);
    ...
    free(p);
```

`openproject()` opens the report file and allocates the network before it reads the input, and sets `Openflag` only if the input had no errors (or only errors that `EN_openX` accepts). `EN_close` is the only function that frees the data and closes the files, and it works on a project that is not open. `EN_runproject` and the legacy `ENclose` call it unconditionally; `EN_deleteproject` does not.

## How to reproduce

| File | What it is |
|---|---|
| [`API-30_undefined-node.inp`](API-30_undefined-node.inp) | A reservoir and a junction; pipe P2 ends at node J9, which does not exist (error 203) |
| [`API-30_test.c`](API-30_test.c) | Opens the file, deletes the project and reads the report back. Then lowers the open-file limit to 64 and repeats the open and delete 200 times. |
| [`API-30_fix.patch`](API-30_fix.patch) | The fix (apply [`CRASH-14_fix.patch`](../../2-crashes/CRASH-14-calls-after-close/CRASH-14_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-30            # unpatched EPANET: FAIL
tools/run-test.sh API-30 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-30_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-30_test
./API-30_test
```

**Without the fix**, the report is empty and the file handles run out:

```
EN_open returned 200
API-30.rpt after EN_deleteproject: 0 bytes
Open 60 returned 303
59 of 200 further opens returned 200
FAIL: the report file was left open after the failed EN_open
```

**With the fix:**

```
EN_open returned 200
API-30.rpt after EN_deleteproject: 655 bytes
    Error 203: undefined node J9 in [PIPES] section:
    Error 200: one or more errors in input file
200 of 200 further opens returned 200
PASS: the report file is closed when the project is deleted
```

## The fix

`EN_deleteproject` always calls `EN_close`:

```diff
     if (p == NULL) return -1;
-    if (p->Openflag) {
-      EN_close(p);
-    }
+
+    // Close the project even if it is not open: a failed EN_open leaves
+    // the report file open and the network data allocated
+    EN_close(p);
     remove(p->TmpHydFname);
```

Most programs call `EN_close` before `EN_deleteproject`, so `EN_close` now runs twice on the same project. At 83e25bb that frees every network array a second time. [CRASH-14](../../2-crashes/CRASH-14-calls-after-close/) makes `EN_close` reset the pointers it frees, so a second call does nothing; this patch therefore requires CRASH-14. Built without it, three of EPANET's six unit-test suites stop with a double free, because their fixtures close and then delete each project.

An alternative is for `openproject()` to free the data and close the report file itself when the open fails, so that a failed `EN_open` leaves nothing behind. That needs more code in `openproject()`, and still has to leave the project safe for the `EN_close` that many programs call after a failed open.

EPANET's own unit tests (6 suites) pass with the patch and CRASH-14.

## Notes

- Found by reading the code in another part of this review, and confirmed by the test.
- The legacy API is not affected: `ENclose` calls `EN_close` whether or not `ENopen` succeeded.
