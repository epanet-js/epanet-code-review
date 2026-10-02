# SIM-09: REPORT START that is not a multiple of REPORT STEP mislabels every row

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Every reported row, in the report file and the binary output file, holds the results of a later time than its label. No solution is ever computed at the reporting times. |
| **Reached from** | Input file (`Report Start` in `[TIMES]`) or toolkit (`EN_REPORTSTART`), when the report start is not a multiple of the report step |
| **Where** | `inithyd()` in [`src/hydraul.c:185`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L185), against `initqual()` in [`src/quality.c:132`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L132) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 3 lines: [`SIM-09_fix.patch`](SIM-09_fix.patch) |

## The problem

With `Report Start 0:30` and `Report Timestep 1:00`, results are reported at 0:30, 1:30, 2:30 and so on. But the hydraulic solver never stops at those times. It solves at 0:00, 1:00, 2:00, ..., and the report labels each solution half an hour early. For Net1:

```
  Node Results at 1:30:00 hrs:
  ...
  11                 180.00    989.71    121.20
```

The demand of 180 gpm gives it away: junction 11 has a base demand of 150 gpm and pattern factor 1.0 until 2:00, so 180 gpm (factor 1.2) is the 2:00 demand. The pressure at 1:30 is really 121.03 psi; the 121.20 shown is the 2:00 value. The binary output file has the same values under the same labels, so every program that reads it gets them too.

## Why it happens

Reporting times are tracked in `time->Rtime`. The hydraulic driver uses it to shorten a time step so that it lands on the next reporting time (`timestep()`), and moves it on by `Rstep` after each step (`nexthyd()`). But `inithyd()` starts it at `Rstep`, ignoring `Rstart`:

```c
// src/hydraul.c, inithyd()
    // Initialize current time
    hyd->Haltflag = 0;
    time->Htime = 0;
    time->Hydstep = 0;
    time->Rtime = time->Rstep;
```

So the steps land on multiples of `Rstep` from 0 (1:00, 2:00, ...). The water quality pass, which writes the output file, starts its own reporting clock at `Rstart` and saves the first hydraulic solution at or after each reporting time:

```c
// src/quality.c, initqual()
    time->Rtime = time->Rstart;

// src/quality.c, runqual()
        // Save current results to output file
        if (time->Htime >= time->Rtime)
        {
            if (pr->outfile.Saveflag)
            {
                errcode = saveoutput(pr);
                pr->report.Nperiods++;
            }
            time->Rtime += time->Rstep;
        }
```

With no solution at 0:30, the 1:00 solution is saved as the 0:30 row, the 2:00 solution as the 1:30 row, and so on to the end of the run. When `Rstart` is a multiple of `Rstep` (the usual case, including 0), both clocks agree and nothing goes wrong.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-09_report-start.inp`](SIM-09_report-start.inp) | Net1 with `Report Start 0:30` and `Report Timestep 1:00`, reporting junction 11 only |
| [`SIM-09_test.c`](SIM-09_test.c) | Steps through the hydraulics and records the time and junction 11 pressure of every solution. Then runs the full analysis and checks each row of the output file against the solution at its labelled time |
| [`SIM-09_fix.patch`](SIM-09_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-09            # unpatched EPANET: FAIL
tools/run-test.sh SIM-09 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-09_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -lm -o SIM-09_test
./SIM-09_test
```

The report excerpt above comes from `runepanet SIM-09_report-start.inp SIM-09.rpt`.

**Without the fix:**

```
Report    Reported   Solution at    First solution at or after it
time      pressure   that time      (time, pressure)
 0:30     120.45    none            1:00   120.45
 1:30     121.20    none            2:00   121.20
 2:30     122.01    none            3:00   122.01
 3:30     122.37    none            4:00   122.37
...
FAIL: 24 of 24 reported rows hold a solution from a later time than their label
```

**With the fix:**

```
Report    Reported   Solution at    First solution at or after it
time      pressure   that time      (time, pressure)
 0:30     119.85    yes             0:30   119.85
 1:30     121.03    yes             1:30   121.03
 2:30     121.60    yes             2:30   121.60
 3:30     122.39    yes             3:30   122.39
...
PASS: all 24 reported rows hold the solution at the time they are labelled
```

## The fix

Start the hydraulic reporting clock at the first time after 0 on the grid `Rstart + k*Rstep`:

```diff
     time->Hydstep = 0;
-    time->Rtime = time->Rstep;
+
+    // First reporting time after 0 on the grid Rstart + k*Rstep
+    time->Rtime = time->Rstart % time->Rstep;
+    if (time->Rtime == 0) time->Rtime = time->Rstep;
```

When `Rstart` is a multiple of `Rstep`, this gives `Rstep`, as before, so those runs are unchanged (the output files for Net1, and for Net3 with `Report Start 2:00`, are byte-identical with and without the patch). `Rstep` is never 0: `adjustdata()` and `EN_settimeparam` both prevent it.

Like the current code, the hydraulics also stop on grid times before `Rstart` (for example at 1:00 when the report starts at 2:00). The simpler `Rtime = Rstart` (or `Rstep` when `Rstart` is 0) would skip those. That would change the time steps before the report start, and so the results slightly, for existing runs with a report start other than 0 whose hydraulic step does not divide the report step.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Setting the report start through the toolkit runs into [API-13](../../3-api/API-13-reportstart-validates-old-value/): `EN_settimeparam(EN_REPORTSTART)` checks the old value instead of the new one.
- [WQ-04](../../5-water-quality/WQ-04-reaction-averages-low-with-report-start/) is another report-start bug: the average reaction rates only count time after `Rstart` but are divided by the whole duration.
