# FILE-02: Saving over the input file loses [LABELS] and [BACKDROP]

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | `EN_saveinpfile` to the file the project was opened from deletes the map labels and the backdrop settings, with return code 0. |
| **Reached from** | Toolkit API: `EN_saveinpfile` with the input file's own name |
| **Where** | `saveinpfile()` and `saveauxdata()` in [`src/inpfile.c:135`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L135), [`:54-59`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L54-L59) and [`:883`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L883) |
| **Since** | v2.0.12 (2014). The code in the repository's first commit copied these sections to a `tmpfile()` before opening the new file; 2.00.12 moved the copy to the end. |
| **GitHub** | Reported as [#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522) (2019). Closed in May 2025 as resolved by [#549](https://github.com/OpenWaterAnalytics/EPANET/pull/549), which fixed only the vertices. |
| **Fix** | About 25 lines: [`FILE-02_fix.patch`](FILE-02_fix.patch) |

## The problem

EPANET does not keep map labels or the backdrop in memory. When a project is saved, `saveinpfile()` writes everything it holds and then copies the `[LABELS]` and `[BACKDROP]` sections from the original input file.

If the new file has the same name as the input file, the input file has already been emptied by the time those sections are copied, so they are lost. `EN_saveinpfile` returns 0. Saving a model over itself is the normal "Save" in a script or a toolkit-based GUI, so this deletes the labels every time.

Issue [#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522) reported this in 2019, when vertices were lost the same way. PR [#549](https://github.com/OpenWaterAnalytics/EPANET/pull/549) added vertices to the data model, so they are now written from memory, and [#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522) was closed in 2025 as resolved by it. Labels and the backdrop are still copied from the file.

## Why it happens

```c
// src/inpfile.c, saveinpfile()
// Open the new text file
if ((f = fopen(fname, "wt")) == NULL) return 302;      // truncates fname
...
// Save auxiliary data to new input file
fprintf(f, "\n");
saveauxdata(pr, f);

// src/inpfile.c, saveauxdata()
// Re-open the input file
if (InFile == NULL)
{
    InFile = fopen(pr->parser.InpFname, "rt");          // the same, now empty, file
    if (InFile == NULL) return;
}
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-02_labels.inp`](FILE-02_labels.inp) | A two-node network with 2 map labels and a 4-line `[BACKDROP]` section |
| [`FILE-02_test.c`](FILE-02_test.c) | Copies the file to `FILE-02_work.inp`, opens the copy, saves it once to a new file and once over itself, and counts the label and backdrop lines in each |
| [`FILE-02_fix.patch`](FILE-02_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-02            # unpatched EPANET: FAIL
tools/run-test.sh FILE-02 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-02_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-02_test
./FILE-02_test
```

**Without the fix**:

```
File                               LABELS   BACKDROP
input file                              2          4
saved to a new file                     2          4
saved over the input file               0          0
EN_saveinpfile returned 0 and 0
FAIL: saving over the input file kept 0 of 2 label lines and 0 of 4 backdrop lines
```

**With the fix**:

```
File                               LABELS   BACKDROP
input file                              2          4
saved to a new file                     2          4
saved over the input file               2          4
EN_saveinpfile returned 0 and 0
PASS: saving over the input file keeps the labels and the backdrop
```

## The fix

Copy the two sections to a scratch file before the output file is opened, and append that copy at the end. `saveauxdata()` itself is unchanged; it now writes into the scratch file:

```diff
+    // Copy auxiliary data from the input file to a scratch file
+    // first, since fname may be the name of the input file
+    getTmpName(auxFname);
+    aux = fopen(auxFname, "w+t");
+    if (aux != NULL) saveauxdata(pr, aux);
+
     // Open the new text file
-    if ((f = fopen(fname, "wt")) == NULL) return 302;
+    if ((f = fopen(fname, "wt")) == NULL)
+    {
...
     // Save auxiliary data to new input file
     fprintf(f, "\n");
-    saveauxdata(pr, f);
+    if (aux != NULL)
+    {
+        rewind(aux);
+        while (fgets(s, MAXLINE, aux) != NULL) fputs(s, f);
+        fclose(aux);
+        remove(auxFname);
+    }
+    else saveauxdata(pr, f);
```

The copy is made on every save, so it does not matter whether the two names are spelled the same way, are relative or absolute, or reach the same file through a link. `getTmpName()` is what EPANET already uses for its scratch hydraulics and output files. If no scratch file can be created, the patch falls back to the old behaviour, so a save to a different file never loses the sections.

This restores the order of the EPANET 2.0 code in the repository's first commit (`savemapinfo()` into a `tmpfile()`, then `fopen(fname, "wt")`). In the discussion of [#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522), the maintainers described the same approach and judged it "a lot of work"; copying only the two auxiliary sections keeps it small. The fuller fix is to read labels and the backdrop into memory, as [#549](https://github.com/OpenWaterAnalytics/EPANET/issues/549) did for vertices, which would also let the API edit them.

EPANET's own unit tests (6 suites) pass with the patch.
