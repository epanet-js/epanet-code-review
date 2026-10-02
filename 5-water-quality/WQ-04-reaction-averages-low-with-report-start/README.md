# WQ-04: Average reaction rates are low when REPORT START is not 0

| | |
|---|---|
| **Category** | [Water quality](../README.md) |
| **Impact** | Wrong results with no warning. The network-wide average reaction rates and source inflow rate in the binary output file are scaled by roughly the fraction of the run after REPORT START: 54 % of the true value in the test, with REPORT START at half the duration. |
| **Reached from** | Any chemical water quality run with a binary output file and `REPORT START` (or `EN_REPORTSTART`) greater than 0. The values are read by `ENR_getNetReacts` and by programs that show a reaction summary from the output file. |
| **Where** | `pipereact()` and `tankreact()` in [`src/qualreact.c:304`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L304) and [`:349`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L349), `findsourcequal()` in [`src/quality.c:533`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L533), and `savenetreacts()` in [`src/output.c:822`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L822) |
| **Since** | EPANET 2.0: the same tests are in the first commit of this repository (2014) |
| **GitHub** | No related issue |
| **Fix** | Five lines in three files: [`WQ-04_fix.patch`](WQ-04_fix.patch) |

## The problem

The epilog of the binary output file holds four network-wide averages, in mass per hour: the bulk, wall and tank reaction rates and the rate at which sources add mass. The quality solver adds to the four totals only from REPORT START on, which makes them averages over the reporting period. `savenetreacts()` then divides the totals by the whole duration instead of the length of the reporting period.

The window is also one hydraulic step too long. During a quality step, `Htime` already holds the end of the current hydraulic period, so the period that ends at REPORT START is counted too.

The test network has steady flow, a zero-order decay and a constant mass source, so its rates do not change during the run. With REPORT START 0 the averages match the rates worked out by hand. With REPORT START 12:00 in a 24 h run both come out at 13/24 of those rates (12 h of reporting plus the extra hour, divided by 24 h).

## Why it happens

```c
// src/qualreact.c, pipereact()
    // Update cumulative mass reacted
    if (pr->times.Htime >= pr->times.Rstart)
    {
        qual->Wbulk += fabs(dcbulk) * v;
        qual->Wwall += fabs(dcwall) * v;
    }

// src/output.c, savenetreacts()
    if (time->Dur > 0) t = (double)time->Dur / 3600.;
    else t = 1.;
    w[0] = (REAL4)(wbulk / t);
```

`tankreact()` (for `Wtank`) and `findsourcequal()` (for `Wsource`) use the same `Htime >= Rstart` test.

In the normal path, `runqual()` sets `Htime` to the end of the period it has read (`time->Htime = hydtime + hydstep`) before `nextqual()` routes the water through that period. When hydraulics and quality are stepped together, `EN_nextH` has already advanced `Htime` before `EN_nextQ` runs. Either way, the period from 11:00 to 12:00 sees `Htime` = 12:00 and is counted. `Qtime` holds the start of the quality step in both paths.

## How to reproduce

| File | What it is |
|---|---|
| [`WQ-04_steady-decay.inp`](WQ-04_steady-decay.inp) | R1 feeds J2 (200 gpm) through two 1000 ft, 12 in pipes. Everything starts at 1.0 mg/L, the bulk reaction is zero order (-0.1 mg/L/day) and J1 has a 60 mg/min mass source. |
| [`WQ-04_test.c`](WQ-04_test.c) | Runs the network with REPORT START 0 and 12:00, reads the averages back with the output-file reader and compares them with the rates worked out by hand |
| [`WQ-04_fix.patch`](WQ-04_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh WQ-04            # unpatched EPANET: FAIL
tools/run-test.sh WQ-04 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc WQ-04_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -lm -o WQ-04_test
./WQ-04_test
```

The expected bulk rate is the decay rate times the pipe volume: 0.1 mg/L/day / 24 × 1,571 ft³ × 28.32 L/ft³ = 185.3 mg/hr. The source adds 60 mg/min = 3,600 mg/hr.

**Without the fix:**

```
Average rate (mg/hr)        Bulk reaction   Source inflow
Expected                            185.3          3600.0
REPORT START 0:00                   185.3          3600.0
REPORT START 12:00                  100.4          1950.0
FAIL: with REPORT START 12:00 the averages are 54 % of the true rates
```

**With the fix:**

```
Average rate (mg/hr)        Bulk reaction   Source inflow
Expected                            185.3          3600.0
REPORT START 0:00                   185.3          3600.0
REPORT START 12:00                  185.3          3600.0
PASS: the averages do not depend on REPORT START
```

## The fix

Count a quality step when it starts at or after REPORT START, and divide by the length of the reporting period:

```diff
 // pipereact(), and the same in tankreact() and findsourcequal()
-    if (pr->times.Htime >= pr->times.Rstart)
+    if (pr->times.Qtime >= pr->times.Rstart)

 // savenetreacts()
-    if (time->Dur > 0) t = (double)time->Dur / 3600.;
+    // Rates are only accumulated over the reporting period
+    if (time->Dur > time->Rstart) t = (double)(time->Dur - time->Rstart) / 3600.;
     else t = 1.;
```

Both parts are needed. With only the new divisor, the test's REPORT START 12:00 run would report 13/12 of the true rates (8 % high).

The other reading is that the averages should cover the whole run. That would mean dropping the REPORT START tests and keeping the divisor. The code has counted only the reporting period since EPANET 2.0, presumably so that a spin-up period can be left out of the averages, so the patch keeps that and fixes the divisor.

With REPORT START 0 the results do not change. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- This was found by reading the code, and the test confirms it.
- When REPORT START is not a multiple of REPORT TIMESTEP, the hydraulic steps do not stop at REPORT START (see [SIM-09](../../1-wrong-results/SIM-09-report-start-mislabels-rows/)), so the first counted step may start a little after it.
- [API-12](../../3-api/API-12-sourcemass-cumulative/) changes the line above the `Wsource` test in `findsourcequal()` and is written to apply on top of this patch.
