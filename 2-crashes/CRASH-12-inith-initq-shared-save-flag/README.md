# CRASH-12: EN_initH and EN_initQ share one save flag

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Stepping the hydraulic and water quality solvers together with `EN_initH(EN_NOSAVE)` and `EN_initQ(EN_SAVE)` crashes at the first `EN_nextH`. The opposite combination silently saves no hydraulics. |
| **Reached from** | Toolkit: concurrent stepping (`EN_runH`/`EN_runQ`/`EN_nextH`/`EN_nextQ`) with different save flags for the two solvers |
| **Where** | `EN_initH()` in [`src/epanet.c:532-536`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L532-L536) and `EN_initQ()` in [`src/epanet.c:760-764`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L760-L764); the crash in `savehyd()`, [`src/output.c:190`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L190), called from `nexthyd()` at [`src/hydraul.c:262`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L262) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | A second flag, 9 lines in 3 files: [`CRASH-12_fix.patch`](CRASH-12_fix.patch) |

## The problem

A toolkit program can run the hydraulic and water quality solvers together, one time step at a time. Each solver has its own save flag: `EN_initH(ph, EN_SAVE)` asks for the hydraulics to be saved to the hydraulics file (needed for `EN_savehydfile`), and `EN_initQ(ph, EN_SAVE)` asks for results to be saved to the binary output file (needed for `EN_report`). The two choices are independent, but EPANET keeps only one of them:

- **`EN_initH(EN_NOSAVE)`, then `EN_initQ(EN_SAVE)`:** the program crashes at the first `EN_nextH`, which writes the hydraulics to a file that was never opened. This is a NULL `FILE *` passed to `fwrite`, so it crashes with or without sanitizers.
- **`EN_initH(EN_SAVE)`, then `EN_initQ(EN_NOSAVE)`:** the run completes, but no hydraulics are saved, and `EN_savehydfile` returns error 104 ("no hydraulics for water quality analysis").

## Why it happens

Both functions set the same field, `outfile.Saveflag`:

```c
// src/epanet.c, EN_initH()
    // Open hydraulics file if requested
    p->outfile.Saveflag = FALSE;
    if (sflag > 0)
    {
        errcode = openhydfile(p);
        if (!errcode) p->outfile.Saveflag = TRUE;
        ...

// src/epanet.c, EN_initQ()
    p->outfile.SaveQflag = FALSE;
    p->outfile.Saveflag = FALSE;
    if (saveFlag)
    {
        errcode = openoutfile(p);
        if (!errcode) p->outfile.Saveflag = TRUE;
    }
```

The hydraulic solver reads it to decide whether to write to `HydFile`, and the quality solver reads it to decide whether to write to `OutFile`:

```c
// src/hydraul.c, nexthyd()
    if (pr->outfile.Saveflag) errcode = savehyd(pr, &time->Htime);

// src/quality.c, runqual()
            if (pr->outfile.Saveflag)
            {
                errcode = saveoutput(pr);
```

In the usual sequence, `EN_solveH` followed by `EN_solveQ`, the two solvers never run at the same time, so sharing the flag does no harm.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-12_Net1.inp`](CRASH-12_Net1.inp) | EPANET's example network Net1, unchanged |
| [`CRASH-12_test.c`](CRASH-12_test.c) | Steps both solvers together through Net1 twice, each time on a freshly opened project. Case 1 saves hydraulics only and calls `EN_savehydfile`; case 2 saves results only and calls `EN_report` |
| [`CRASH-12_fix.patch`](CRASH-12_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-12            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-12 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-12_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-12_test
./CRASH-12_test
```

**Without the fix**, case 1 loses the hydraulics and case 2 crashes:

```
Case 1: EN_initH(EN_SAVE), EN_initQ(EN_NOSAVE)
  EN_savehydfile returns 104 (expected 0; 104 means no hydraulics were saved)
Case 2: EN_initH(EN_NOSAVE), EN_initQ(EN_SAVE)
AddressSanitizer:DEADLYSIGNAL
=================================================================
==...==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000068 (pc ... bp ... sp ... T0)
...
    #3 ... in savehyd output.c:190
    #4 ... in nexthyd hydraul.c:262
    #5 ... in EN_nextH epanet.c:585
    #6 ... in run_together CRASH-12_test.c:31
```

The test opens a new project for each case. If case 2 reuses the project from case 1, it does not crash: the hydraulics file opened in case 1 is still open, and the hydraulics are written into it although case 2 asked for them not to be saved.

**With the fix:**

```
Case 1: EN_initH(EN_SAVE), EN_initQ(EN_NOSAVE)
  EN_savehydfile returns 0 (expected 0; 104 means no hydraulics were saved)
Case 2: EN_initH(EN_NOSAVE), EN_initQ(EN_SAVE)
  EN_report returns 0 (expected 0)
PASS: the hydraulics and the results are saved as requested in both cases
```

## The fix

Give the output file its own flag. `Saveflag` keeps its meaning for the hydraulic solver, so `hydraul.c` does not change. `EN_initQ`, `EN_nextQ`, `EN_stepQ` and the three uses in `quality.c` switch to a new `OutSaveflag`:

```diff
     SaveQflag,             // Quality results saved flag
-    Saveflag;              // General purpose save flag
+    Saveflag,              // Hydraulics file save flag
+    OutSaveflag;           // Output file save flag
```

```diff
     p->outfile.SaveQflag = FALSE;
-    p->outfile.Saveflag = FALSE;
+    p->outfile.OutSaveflag = FALSE;
     if (saveFlag)
     {
         errcode = openoutfile(p);
-        if (!errcode) p->outfile.Saveflag = TRUE;
+        if (!errcode) p->outfile.OutSaveflag = TRUE;
     }
```

The project struct is allocated with `calloc`, so the new flag starts at 0. `EN_solveH` followed by `EN_solveQ` gives byte-identical output files for Net1 and Net3. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The performance work's prototype for writing results during the hydraulic run (X14, `p1_fused.diff`) also separates the two flags, the other way round: it keeps `Saveflag` for the output file and adds `HydSaveflag` for the hydraulics. This patch keeps `Saveflag` for the hydraulics so that it does not touch the lines in `nexthyd()` that [API-23](../../3-api/API-23-nexthyd-drops-save-error/) changes. Either naming fixes the bug.
- [CRASH-10](../CRASH-10-adjacency-lists-freed-by-both-solvers/) and [CRASH-11](../CRASH-11-closeq-not-idempotent/) are other problems with state shared between the two solvers.
