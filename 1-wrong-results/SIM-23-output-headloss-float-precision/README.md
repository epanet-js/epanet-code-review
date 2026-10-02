# SIM-23: Output-file head loss is computed from heads rounded to 4-byte floats

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Head loss and friction factor in the binary output file, and in the text report, are wrong for lightly loaded pipes, some by a factor of 40 and some stored as 0 although the pipe carries flow. In Net2, 25 % of pipe values are off by more than 1 %. |
| **Reached from** | Any run where `EN_solveQ` writes the output file from saved hydraulics: `runepanet`, `EN_runproject`, or `EN_solveH` followed by `EN_solveQ` |
| **Where** | `linkoutput()` in [`src/output.c:520-536`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L520-L536) and [`:589-605`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L589-L605), using the heads read by `readhyd()` at [`:363-364`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L363-L364) and saved by `savehyd()` at [`:196-198`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L196-L198) |
| **Since** | The first commit of this repository (2014), so every release |
| **GitHub** | No issue. In PR [#200](https://github.com/OpenWaterAnalytics/EPANET/pull/200) (2018) LRossman wondered whether casting results to single precision affected the differences being compared. Possibly related: [#836](https://github.com/OpenWaterAnalytics/EPANET/issues/836), regression tests that pass on Windows but fail on Linux. |
| **Fix** | About 40 lines in 7 files, no change to any file format: [`SIM-23_fix.patch`](SIM-23_fix.patch) |

## The problem

The output file stores each pipe's head loss per 1,000 length units and its friction factor. Both are computed from the difference between the heads at the pipe's two ends. In the usual path, `EN_solveH` saves the hydraulics to a scratch file and `EN_solveQ` reads them back and writes the output file. The scratch file stores heads as 4-byte floats, so when `EN_solveQ` takes their difference, most of the digits of a small head loss are gone.

A head of 1,000 ft is stored to the nearest 0.00006 ft. A pipe whose head loss is smaller than a few of those steps gets a value that is far off, or exactly 0. `EN_getlinkvalue(EN_HEADLOSS)` is not affected, because it uses the solver's double-precision heads. The text report reads its tables from the output file, so its head-loss and friction-factor columns have the same errors.

| Network | Values off by more than 1 % | More than 10 % | Stored as 0 | Worst |
|---|---|---|---|---|
| Net1 | 0.7 % | 0.3 % | 0 % | 10 % |
| Net2 | 25.4 % | 9.1 % | 1.6 % | 3,898 % |
| Net3 | 7.0 % | 3.9 % | 2.5 % | 4,193 % |
| A 33k-junction real model, 24 h (from the review) | 58 % | 32 % | 17.5 % | |

The error is about one float step of head divided by the pipe length, so it matters for lightly loaded pipes. In Net2 every value above 0.01 ft per 1,000 ft is within 1 %, but 1,016 of the 2,217 values are below that. In the real model, more than half of all values were off by more than 1 %.

## Why it happens

```c
// src/output.c, savehyd()
// Save current nodal heads
for (i = 1; i <= net->Nnodes; i++) x[i] = (REAL4)hyd->NodeHead[i];
fwrite(x + 1, sizeof(REAL4), net->Nnodes, HydFile);

// src/output.c, readhyd(), called by EN_solveQ for each hydraulic period
if (f_read(x, net->Nnodes, HydFile) < (unsigned)net->Nnodes) result = 0;
else for (i = 1; i <= net->Nnodes; i++) hyd->NodeHead[i] = x[i];

// src/output.c, linkoutput()
case HEADLOSS:
    ...
        h = hyd->NodeHead[net->Link[i].N1] -
            hyd->NodeHead[net->Link[i].N2];
        ...
            x[i] = (REAL4)(1000.0 * h / net->Link[i].Len);
```

When hydraulics and quality are stepped together (`EN_runH` and `EN_runQ` in one loop), `readhyd()` is not used and the output is correct.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-23_Net2.inp`](SIM-23_Net2.inp) | EPANET's example network Net2, unchanged (55 hours, hourly reports) |
| [`SIM-23_test.c`](SIM-23_test.c) | Runs the hydraulics step by step, recording `1000 * fabs(H1 - H2) / L` from the solver's heads at each reporting time, then runs `EN_solveQ` and compares with the head loss in the output file |
| [`SIM-23_fix.patch`](SIM-23_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-23            # unpatched EPANET: FAIL
tools/run-test.sh SIM-23 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-23_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -lm -o SIM-23_test
./SIM-23_test
```

**Without the fix**:

```
Pipe head-loss values compared:  2217 (56 reporting periods)
Off by more than 1 %:            563 (25.4 %)
Off by more than 10 %:           202 (9.1 %)
Stored as exactly 0:             35 (1.6 %)
Worst: pipe 20 at 12:00, solver 0.000002, output file 0.000087 ft/1000 ft (3898 % off)
FAIL: 25.4 % of the head losses in the output file are off by more than 1 %
```

**With the fix**:

```
Pipe head-loss values compared:  2217 (56 reporting periods)
Off by more than 1 %:            0 (0.0 %)
Off by more than 10 %:           0 (0.0 %)
Stored as exactly 0:             0 (0.0 %)
Worst: pipe 35 at 24:00, solver 0.000245, output file 0.000245 ft/1000 ft (0 % off)
PASS: the output file holds the solver's head loss for every pipe
```

Net1 and Net3 also give 0 values off by more than 1 % with the fix.

## The fix

The head loss has to come from double-precision heads, and the obvious way is to store the heads in the scratch file as doubles. That changes the hydraulics file, which is not only a scratch file:

- `EN_savehydfile` copies it to a user file, and `EN_usehydfile` or `HYDRAULICS USE` reads it back in a later run.
- **EPANET-MSX reads it directly.** `MSXsolveH` calls `ENsolveH` and `ENsavehydfile`, and MSX's `getHydVars()` then reads each period as 4-byte demands, heads and flows, skipping status and settings ([`src/msxqual.c`](https://github.com/OpenWaterAnalytics/epanet-msx/blob/main/src/msxqual.c) on the main branch; the same in `Legacy DLL` on dev). `MSXusehydfile` reads the version field but does not check it.

So the patch leaves the hydraulics file alone. While the hydraulics run, `savehyd()` also writes the heads as doubles to a second scratch file, and `readhyd()` reads them back over the 4-byte heads:

```diff
     // Save current nodal heads
     for (i = 1; i <= net->Nnodes; i++) x[i] = (REAL4)hyd->NodeHead[i];
     fwrite(x + 1, sizeof(REAL4), net->Nnodes, HydFile);
     //f_save(x, net->Nnodes, HydFile);
+    if (out->HeadFile != NULL)
+    {
+        fwrite(hyd->NodeHead + 1, sizeof(double), net->Nnodes, out->HeadFile);
+    }
```

```diff
     if (f_read(x, net->Nnodes, HydFile) < (unsigned)net->Nnodes) result = 0;
     else for (i = 1; i <= net->Nnodes; i++) hyd->NodeHead[i] = x[i];
+
+    // Replace the heads with their full-precision copy, if one was kept
+    if (out->HeadFile != NULL &&
+        fread(hyd->NodeHead + 1, sizeof(double), net->Nnodes, out->HeadFile) <
+        (unsigned)net->Nnodes) result = 0;
```

The rest of the patch manages the second file next to `HydFile`: `openhydfile()` creates it (with `getTmpName()`) whenever the hydraulics are computed rather than read from a saved file, `inithyd()` and `initqual()` rewind it where they rewind `HydFile`, and a new `closeheadfile()` closes and deletes it when `HydFile` is reopened and in `EN_close`. `linkoutput()` is unchanged; it now gets double-precision heads.

What this does and does not change:

- The hydraulics file is byte-for-byte the same. A file saved by the patched library and one saved by 83e25bb are identical, and each can be used by the other with `EN_usehydfile`.
- With `EN_usehydfile` or `HYDRAULICS USE` there is no second file, and the output keeps the old precision. Nothing in the saved file can restore it.
- If the second file cannot be created, the run carries on with the 4-byte heads as before.
- The scratch data grows by 8 bytes per node per hydraulic period, which is about 40 % more than the hydraulics file when nodes and links are similar in number.

**Alternatives.**

- **Store doubles in the hydraulics file** and raise `ENGINE_VERSION` from 201 to 202. This is the smallest change, three short hunks in `output.c` and `types.h`. `EN_usehydfile` would then reject older files cleanly with error 306, but EPANET-MSX would read the new layout as 4-byte values and compute wrong results with no error, until MSX is changed to check the version and read the new layout. A format change is not needed to fix this bug, so the patch avoids it; if the maintainers prefer it, it should be coordinated with an MSX release.
- **Write the output file during the hydraulic run** when there is no water-quality analysis, so the scratch file is not needed at all. The review's performance work prototyped this (item X14); it is the fuller fix, and a larger change.
- **Compute the pipe head loss from the flow** with the head-loss formula. This needs no file at all, but it duplicates the solver's formulas, differs from the head difference by the solver's residual, and does not cover pumps and valves.

EPANET's own unit tests (6 suites) pass with the patch.
