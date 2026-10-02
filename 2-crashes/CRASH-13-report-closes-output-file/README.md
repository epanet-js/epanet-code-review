# CRASH-13: EN_report before EN_closeQ closes the live output file

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | The project keeps a pointer to a closed `FILE`. A second `EN_report` prints all-zero tables, or works on another file that has taken over the freed `FILE`, and closes it; `EN_closeQ` then calls `fclose` on the same pointer again, which is undefined behaviour (a double free with glibc). |
| **Reached from** | Toolkit API: `EN_report` after a stepped quality run (`EN_initQ(EN_SAVE)`, `EN_runQ`, `EN_nextQ`) and before `EN_closeQ` |
| **Where** | `writeresults()` in [`src/report.c:578`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L578), [`:599`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L599) and [`:642-647`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L642-L647); the second close is in `closeoutfile()`, [`src/project.c:290-298`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L290-L298), called by `EN_closeQ` ([`src/epanet.c:846`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L846)) |
| **Since** | v2.2 (2019), from the change that lets other programs read the output file while a project is open ([#407](https://github.com/OpenWaterAnalytics/EPANET/issues/407), commit [324487b](https://github.com/OpenWaterAnalytics/EPANET/commit/324487ba86725e0e6443b8928b6f6ed44830f00b)) |
| **GitHub** | No related issue |
| **Fix** | One condition: [`CRASH-13_fix.patch`](CRASH-13_fix.patch) |

## The problem

A toolkit program that steps through water quality can call `EN_report` as soon as the last `EN_nextQ` returns, before `EN_closeQ`. The binary output file is still open then, and `writeresults()` reads the results through the project's own handle, `out->OutFile`. When it has finished, it closes that handle, but `out->OutFile` still points at it.

From then on:

- A second `EN_report` reads through the closed stream, gets nothing, and prints tables of zeros. Its return code is 0.
- If the program has opened a file in the meantime, the C library may have given it the same `FILE` structure. The second `EN_report` then seeks and reads in the program's file and closes it. In the test, the program's next `fprintf` to its own file fails.
- `EN_closeQ` calls `fclose` on the pointer again. With glibc the `FILE` was freed by the first `fclose`, so this is a double free.

`EN_solveQ` closes the output file before returning, so the usual `EN_solveQ`, `EN_report` sequence does not meet this.

## Why it happens

```c
// src/report.c, writeresults()
FILE *outFile = out->OutFile;
...
// Return if no output file
if (outFile == NULL) outFile = fopen(pr->outfile.OutFname, "rb");
if (outFile == NULL) return 106;
...
// Free output file
if (outFile != NULL)
{
    fclose(outFile);          // also closes the project's out->OutFile
    outFile = NULL;           // clears only the local copy
}
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-13_network.inp`](CRASH-13_network.inp) | Reservoir R1 supplies J1 and J2; chlorine is modelled and all nodes are reported |
| [`CRASH-13_test.c`](CRASH-13_test.c) | Steps through quality with `EN_initQ(EN_SAVE)`, calls `EN_report` twice before `EN_closeQ`, opens a file of its own between the two calls, and compares J1's head in the two reports |
| [`CRASH-13_fix.patch`](CRASH-13_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-13            # unpatched EPANET: FAIL
tools/run-test.sh CRASH-13 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-13_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-13_test
./CRASH-13_test
```

**Without the fix**, the second report is all zeros and the test's own file has been closed. On macOS the C library keeps closed `FILE` structures for reuse instead of freeing them, so AddressSanitizer has nothing to report and the test detects the bug by its effects. With glibc, by reading the code, the second `EN_report` would read a freed `FILE` and `EN_closeQ` would free it again.

```
1st EN_report returned 0, head at J1 149.88 ft
2nd EN_report returned 0, head at J1 0.00 ft
EN_closeQ returned 0
fprintf to the test's own file after the 2nd report returned -1
FAIL: the second report shows a head of 0.00 ft instead of 149.88 ft, and the test's own file was closed
```

**With the fix**:

```
1st EN_report returned 0, head at J1 149.88 ft
2nd EN_report returned 0, head at J1 149.88 ft
EN_closeQ returned 0
fprintf to the test's own file after the 2nd report returned 11
PASS: both reports show the same results
```

## The fix

Close the file only if `writeresults()` opened it:

```diff
-    // Free output file
-    if (outFile != NULL)
+    // Close output file if it was opened here
+    // (the project's own OutFile is closed by closeoutfile)
+    if (outFile != NULL && outFile != out->OutFile)
     {
         fclose(outFile);
         outFile = NULL;
```

The project's handle stays open until `EN_closeQ` or `EN_close` closes it, as `closeoutfile()` expects. Reading through it is safe at this point: `EN_report` only runs once `SaveQflag` is set, after the last `EN_nextQ` has written the final results, and `writeresults()` seeks before it reads.

EPANET's own unit tests (6 suites) pass with the patch.
