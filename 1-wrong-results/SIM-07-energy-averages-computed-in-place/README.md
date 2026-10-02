# SIM-07: Energy averages are computed in place, so a second EN_solveQ corrupts them

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | A second `EN_solveQ` or `EN_saveH` on the same hydraulics reports a usage factor of 240 % and an efficiency of 130 % for a pump that ran 58 % of the time at 75 %, in the report and the output file. No error or warning. |
| **Reached from** | Toolkit: `EN_solveQ`, `EN_saveH` or `EN_initQ(EN_SAVE)` called more than once after one hydraulic run, for example to run several water quality scenarios on the same hydraulics |
| **Where** | `saveenergy()` in [`src/output.c:279-307`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L279-L307), read back by `writeenergy()` in [`src/report.c:538-545`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L538-L545) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | One new function that works on a copy, used by both writers: [`SIM-07_fix.patch`](SIM-07_fix.patch) |

## The problem

A toolkit program can solve the hydraulics once and then run `EN_solveQ` several times, for example for different water quality settings. The hydraulics, and so the pump energy use, are the same each time. But the energy table changes on every run after the first. For Net1 at $0.10/kWh:

| Statistic | Run 1 | Run 2 |
|---|---|---|
| Usage factor (%) | 57.71 | 240.47 |
| Avg. efficiency (%) | 75.00 | 129.95 |
| kWh/Mgal | 880.42 | 566.47 |
| Avg. kW | 96.25 | 1.67 |

The binary output file carries the same wrong values. With a 24-hour duration the cost per day happens to stay the same. With any other duration it is rescaled too: with Net1 shortened to 12 hours, the cost per day went from 231.26 to 462.51 to 925.02 over three runs.

`EN_saveH` runs `EN_solveQ` internally, so `EN_saveH` followed by `EN_solveQ` does the same.

## Why it happens

During the hydraulic run, `addenergy()` adds up totals in `pump->Energy`: hours online, efficiency × hours, kWh, kWh per unit flow × hours and cost. `saveenergy()` runs from `openoutfile()` on every `EN_initQ(EN_SAVE)`. It converts those totals into averages and percentages and writes them back into the same fields:

```c
// src/output.c, saveenergy()
        pump = &net->Pump[i];
        if (hdur == 0.0) pump->Energy.TotalCost *= 24.0;
        else
        {
            // ... convert total hrs. online to fraction of total time online
            t = pump->Energy.TimeOnLine;  //currently holds total hrs. online
            pump->Energy.TimeOnLine = t / hdur;

            // ... convert cumulative values to time-averaged ones
            if (t > 0.0)
            {
                pump->Energy.Efficiency /= t;
                pump->Energy.KwHrsPerFlow /= t;
                pump->Energy.KwHrs /= t;
            }
            ...
        }

        // ... express time online and avg. efficiency as percentages
        pump->Energy.TimeOnLine *= 100.0;
        pump->Energy.Efficiency *= 100.0;
```

Only `inithyd()` resets the totals. The second `saveenergy()` therefore treats the averages as totals. The usage factor, 57.71, is taken as hours online: it becomes 57.71 / 24 × 100 = 240.47 %, and the efficiency, 75.00, becomes 75.00 / 57.71 × 100 = 129.95 %. `writeenergy()` prints whatever is in `pump->Energy`, so the report shows the same values.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-07_Net1.inp`](SIM-07_Net1.inp) | EPANET's example network Net1, unchanged (one pump) |
| [`SIM-07_test.c`](SIM-07_test.c) | Sets an energy price, solves the hydraulics once, then runs `EN_solveQ` and `EN_report` twice. After each run it reads the pump's six energy statistics from the report and from the output file |
| [`SIM-07_fix.patch`](SIM-07_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-07            # unpatched EPANET: FAIL
tools/run-test.sh SIM-07 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-07_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -lm -o SIM-07_test
./SIM-07_test
```

**Without the fix:**

```
                        Run 1     Run 2     Run 2
                       report    report  out file
Usage factor (%)        57.71    240.47    240.47   <-- changed
Avg. efficiency (%)     75.00    129.95    129.95   <-- changed
kWh/Mgal               880.42    566.47    566.47   <-- changed
Avg. kW                 96.25      1.67      1.67   <-- changed
Peak kW                 96.71     96.71     96.71   
Cost/day               133.32    133.32    133.32   
FAIL: 4 of 6 energy statistics changed when EN_solveQ was run again on the same hydraulics
```

**With the fix:**

```
                        Run 1     Run 2     Run 2
                       report    report  out file
Usage factor (%)        57.71     57.71     57.71   
Avg. efficiency (%)     75.00     75.00     75.00   
kWh/Mgal               880.42    880.42    880.42   
Avg. kW                 96.25     96.25     96.25   
Peak kW                 96.71     96.71     96.71   
Cost/day               133.32    133.32    133.32   
PASS: the second EN_solveQ reports the same energy statistics as the first
```

## The fix

The conversion moves, unchanged, into a new function `pumpenergystats()` in `output.c`. It copies the pump's totals into a local `Senergy` and converts the copy. `saveenergy()` and `writeenergy()` both call it, so the totals in `pump->Energy` are never overwritten:

```diff
+void pumpenergystats(Project *pr, int i, Senergy *e)
+...
+    *e = pr->network.Pump[i].Energy;
+    hdur = time->Dur / 3600.0;
+    if (hdur == 0.0) e->TotalCost *= 24.0;
+    ...
```

```diff
         pump = &net->Pump[j];
-        csum += pump->Energy.TotalCost;
+        pumpenergystats(pr, j, &e);
+        csum += e.TotalCost;
```

The arithmetic is the same as before, so a single run gives the same report and a byte-identical output file (checked with Net1 and Net3). `EN_report` needs the output file to have been written, so `writeenergy()` always ran after `saveenergy()` and never printed the raw totals; it still prints the same values. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-06](../SIM-06-demand-charge-squared/) is the same pattern for the demand charge, which `saveenergy()` also writes back into `hyd->Emax`. The two patches change different lines of `saveenergy()` and apply together in either order.
- [SIM-11](../SIM-11-duration-overshoot/) gives a usage factor above 100 % for another reason: energy added up past the end of the run.
