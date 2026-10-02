# API-13: EN_settimeparam(EN_REPORTSTART) validates the old value

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A report start after the end of the run is accepted, and from then on every valid report start is refused with 213. A run that reports no period writes NaN averages to the output file when `STATISTIC AVERAGE` is set. |
| **Reached from** | Toolkit API: `EN_settimeparam(EN_REPORTSTART)`. The NaN is also reached from an input file, by a run that halts before `REPORT START` with a `STATISTIC` other than `NONE`. |
| **Where** | `EN_settimeparam()` in [`src/epanet.c:1695`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1695); `savetimestat()` in [`src/output.c:759`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L759) |
| **Since** | EPANET 2.0: `ENsettimeparam` in v2.0.12 has the same test |
| **GitHub** | No related issue |
| **Fix** | One line in each file: [`API-13_fix.patch`](API-13_fix.patch) |

## The problem

`EN_settimeparam(EN_REPORTSTART, value)` is meant to refuse a report start after the end of the simulation. It tests the report start the project already has instead of the new one:

- With a 3-hour duration, a report start of 4 hours is accepted (return code 0).
- After that, the stored report start is beyond the duration, so every later call fails with 213, including a valid one such as 0. The only way out is to change the duration.

A report start after the end means no time period is reported. With `STATISTIC AVERAGE` (or `MINIMUM`, `MAXIMUM`, `RANGE`), `savetimestat()` then divides the sums by zero periods and writes NaN to the output file as the single "average" period. The original review's repro saw this on Net1 with a 48-hour report start: all node pressures in the output file were NaN.

Validating the new value closes that route, but not every route to zero periods. A run whose hydraulics halt before the report start (`UNBALANCED STOP`) reports no period either, and the test network does exactly that. Adding `REPORT START 2:00` to its `[TIMES]` section and running it with `runepanet` also gives an output file whose single period is NaN.

## Why it happens

```c
// src/epanet.c, EN_settimeparam()
case EN_REPORTSTART:
    if (time->Rstart > time->Dur) return 213;   // tests the old value
    time->Rstart = value;
    break;
```

```c
// src/output.c, savetimestat(), called by savefinaloutput()
case AVG:
    for (i = 1; i <= n; i++) x[i] = stat1[i] / (float)rpt->Nperiods;
    break;
```

`savefinaloutput()` computes the statistic whenever `Tstatflag != SERIES`, even when `rpt->Nperiods` is 0, and then sets `Nperiods` to 1. With `MINIMUM` or `MAXIMUM` the same case writes the initial sentinels (±10^10) instead of NaN (by reading the code).

## How to reproduce

| File | What it is |
|---|---|
| [`API-13_halts.inp`](API-13_halts.inp) | A 3-hour run with `STATISTIC AVERAGE`, `TRIALS 3` and `UNBALANCED STOP`. Hour 0 converges; at hour 1 the demand jumps 100,000-fold and the run halts. |
| [`API-13_test.c`](API-13_test.c) | A: sets the report start to 4 h and then to 0. B: sets it to 2 h, runs hydraulics and quality, and reads the averages from the output file. |
| [`API-13_fix.patch`](API-13_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-13            # unpatched EPANET: FAIL
tools/run-test.sh API-13 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-13_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -lm -o API-13_test
./API-13_test
```

**Without the fix:**

```
A. REPORTSTART 4 h (duration 3 h): returned 0 (expected 213)
   REPORTSTART 0:                  returned 213 (expected 0); REPORTSTART is now 14400 s
B. Output file: 1 period; average pressure at J1 = nan psi   (expected no NaN)
FAIL: 2 of 2 checks failed
```

**With the fix**, the output file of the halted run has no results, as it would with `STATISTIC NONE`, and the output reader says so:

```
A. REPORTSTART 4 h (duration 3 h): returned 213 (expected 213)
   REPORTSTART 0:                  returned 0 (expected 0); REPORTSTART is now 0 s
B. Output file: ENR_open returned 436, no results   (expected no NaN)
PASS: the new report start is validated, and no NaN reaches the output file
```

## The fix

Test the new value:

```diff
     case EN_REPORTSTART:
-        if (time->Rstart > time->Dur) return 213;
+        if (value > time->Dur) return 213;
```

And compute the statistic only when at least one period was reported:

```diff
     // Save time series statistic if computed
-    if (rpt->Tstatflag != SERIES && out->TmpOutFile != NULL)
+    // (there is none if no time period was reported)
+    if (rpt->Tstatflag != SERIES && out->TmpOutFile != NULL &&
+        rpt->Nperiods > 0)
```

The guard sits in `savefinaloutput()` rather than at the division, so `Nperiods` stays 0 and the output file says "no results" instead of carrying a period of zeros. The temporary statistics file is still closed by `closeoutfile()`. A smaller alternative is to divide by `MAX(rpt->Nperiods, 1)` at line 759, which writes 0 for the averages but leaves the ±10^10 sentinels for the other statistics.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-09](../../1-wrong-results/SIM-09-report-start-mislabels-rows/) and [WQ-04](../../5-water-quality/WQ-04-reaction-averages-low-with-report-start/) are other problems with `REPORT START`.
