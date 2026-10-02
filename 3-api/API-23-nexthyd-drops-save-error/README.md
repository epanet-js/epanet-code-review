# API-23: nexthyd overwrites a savehyd write error

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A failed write to the hydraulics file (disk full, quota, file size limit) is not reported. The run ends with return code 0, and the water quality run then reads a damaged file, either silently (return code 0) or with error 307 at the end. |
| **Reached from** | Toolkit or input file: any run that saves hydraulics (`EN_solveH`, `EN_initH(EN_SAVE)`, `runepanet`) when a write to the hydraulics file fails |
| **Where** | `nexthyd()` in [`src/hydraul.c:262-269`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L262-L269); error checks in `savehyd()` and `savehydstep()` in [`src/output.c:216-248`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L216-L248) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 3 lines in 2 files: [`API-23_fix.patch`](API-23_fix.patch) |

## The problem

Every hydraulic step is written to a scratch hydraulics file, which the water quality run reads back. If one of those writes fails, `EN_nextH` should return error 308 ("cannot save results to file"). It returns 0.

In the test, one write fails part way through the 4th hour of a 24-hour run (the file size limit is reached, then lifted again, as when a full disk gets space back). Every `EN_nextH` returns 0, and the hydraulics file ends up 3,532 bytes shorter than it should be (6,092 bytes on Linux). All later records are read from the wrong position, but `EN_savehydfile` and `EN_solveQ` both return 0.

If the disk stays full, the result depends on the C library. On Linux (glibc) every `EN_nextH` still returns 0, the file is cut off at the limit, and the error only appears later, as error 307 ("cannot read hydraulics file") from `EN_solveQ`. On macOS the error is reported one step late, because the following small write fails too.

## Why it happens

`nexthyd()` stores the result of `savehyd()` in `errcode`, then overwrites it with the result of `savehydstep()`, which writes only the 4-byte length of the next step:

```c
// src/hydraul.c, nexthyd()
    if (pr->outfile.Saveflag) errcode = savehyd(pr, &time->Htime);
    if (hyd->Haltflag) time->Htime = time->Dur;

    // Compute next time step & update tank levels
    *tstep = 0;
    hydstep = 0;
    if (time->Htime < time->Dur) hydstep = timestep(pr);
    if (pr->outfile.Saveflag) errcode = savehydstep(pr,&hydstep);
```

If the 4-byte write succeeds, which it does once the disk has space again, and on Linux even while it is full (the write goes into the stdio buffer, and the failed flush is not checked), the error from `savehyd()` is lost.

`savehyd()` also misses most failures in the first place. It makes six `fwrite` calls but checks only the count returned by the last one, and nothing checks `fflush`:

```c
// src/output.c, savehyd()
    // Save link settings & check for successful write-to-disk
    // (We assume that if any of the previous fwrites failed,
    // then this one will also fail.)
    ...
    if (fwrite(x + 1, sizeof(REAL4), net->Nlinks, HydFile) <
        (unsigned)net->Nlinks
       ) errcode = 308;
    ...
    free(x);
    fflush(HydFile);
    return errcode;
```

With buffered output that assumption does not hold. A write only reaches the disk when the buffer is flushed, and once the failed data has been dropped from the buffer, the next `fwrite` succeeds. By reading the code, for a network small enough that one step's results fit in the buffer (about 200 links with a 4 KB buffer), every failure happens in the unchecked `fflush`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-23_test.c`](API-23_test.c) | Builds a chain of 1,200 pipes with the toolkit and runs its hydraulics twice for 24 hours: once normally, and once with `RLIMIT_FSIZE` set so that one write to the hydraulics file fails. Compares the saved hydraulics files and the return codes |
| [`API-23_fix.patch`](API-23_fix.patch) | The fix |

The network has 1,200 pipes so that each step's results are larger than the stdio buffer. Then the failed write lands in the last `fwrite` of `savehyd()`, which does see it, and the test shows the error being overwritten, rather than not being detected at all.

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-23            # unpatched EPANET: FAIL
tools/run-test.sh API-23 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-23_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-23_test
./API-23_test
```

The test uses `setrlimit` and `SIGXFSZ`, so it needs a POSIX system. On Windows it compiles to a stub that prints `PASS: not run`. It was run on macOS (arm64) and on Linux (Ubuntu 22.04, glibc 2.35, in Docker), with the same verdicts.

**Without the fix** (macOS):

```
Run 1, no file size limit:
  a write to the hydraulics file failed: no
  EN_nextH returned 0 at every step
  EN_savehydfile returns 0; the hydraulics file has 600433 bytes
  EN_solveQ returns 0
Run 2, one write fails:
  a write to the hydraulics file failed: yes
  EN_nextH returned 0 at every step
  EN_savehydfile returns 0; the hydraulics file has 596901 bytes
  EN_solveQ returns 0
FAIL: a write to the hydraulics file failed, but EN_nextH reported no error
```

In a build with a print added to `nexthyd()`, the failing step shows `savehyd()` returning 308 and `savehydstep()` then returning 0, at t = 10800 s.

**With the fix:**

```
...
Run 2, one write fails:
  a write to the hydraulics file failed: yes
  EN_nextH returned 308 at t = 10800 s
  EN_savehydfile returns 104
  EN_solveQ returns 104
PASS: EN_nextH reports error 308 for the failed write
```

## The fix

Keep the first error in `nexthyd()`, with EPANET's `ERRCODE` macro, which also skips `savehydstep()` once an error is set:

```diff
-    if (pr->outfile.Saveflag) errcode = savehydstep(pr,&hydstep);
+    if (pr->outfile.Saveflag) ERRCODE(savehydstep(pr,&hydstep));
```

and check the stream's error flag after the flush in `savehyd()` and `savehydstep()`, so that a failure in any of the writes, or in the flush, is reported:

```diff
     fflush(HydFile);
+    if (ferror(HydFile)) errcode = 308;
     return errcode;
```

The error flag stays set, so every later step also returns 308. That matters when stepping: with the `nexthyd()` change alone, the test gets 308 at t = 10800 s, but the later steps return 0, the last `EN_nextH` marks the hydraulics as saved, and `EN_solveQ` reads the damaged file without an error (checked on macOS and Linux). With the whole patch, `EN_savehydfile` and `EN_solveQ` refuse it with error 104. `EN_solveH` stops at the first error and returns 308.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The review found this by reading the code. The test confirms it on macOS and Linux. The persistent-failure results above come from a variant of the test that keeps the limit in place and ignores `SIGXFSZ`.
- The binary output file is checked more carefully: `nodeoutput()`, `linkoutput()` and `savefinaloutput()` check each write. But `savenetdata()` checks only its last write, and nothing checks the final `fclose()` in `closeoutfile()`, which flushes the last part of the file (by reading). This patch does not change them.
