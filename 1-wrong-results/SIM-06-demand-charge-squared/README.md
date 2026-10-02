# SIM-06: Demand charge is multiplied by the price twice and compounds across runs

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | The report shows peak kW × Dcost² as the demand charge, and the total cost includes it. Each further `EN_solveQ` or `EN_saveH` in the same project multiplies the charge by Dcost again, in the report and the output file. |
| **Reached from** | Input file or toolkit: any run with a `Demand Charge` other than 0 or 1 in `[ENERGY]` (or `EN_DEMANDCHARGE`) and an energy report |
| **Where** | `saveenergy()` in [`src/output.c:324-325`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L324-L325), `writeenergy()` in [`src/report.c:551-553`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L551-L553), `inithyd()` in [`src/hydraul.c:158-170`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L158-L170) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 3 lines in 2 files: [`SIM-06_fix.patch`](SIM-06_fix.patch) |

## The problem

The demand charge is the peak pumping power (kW) times the price per kW (`Demand Charge` in `[ENERGY]`). For Net1 with a demand charge of 2.0, the peak is 96.71 kW, so the charge is 193.42. A single `runepanet` run reports twice that:

```
  9          57.71  75.00    880.42     96.25     96.71      0.00
  ----------------------------------------------------------------
                                         Demand Charge:    386.83
                                         Total Cost:       386.83
```

The binary output file holds the right value (193.41) after the first run. In a toolkit program that runs the analysis more than once in the same project, both go up:

- A second `EN_solveQ` (or `EN_saveH`) on the same hydraulics multiplies the charge by 2.0 again: 773.66 in the report, 386.83 in the output file.
- A fresh `EN_solveH` does not reset it. With the demand multiplier halved, the peak is 96.73 kW, but the report shows 1,547.31.

With a demand charge of 1.0 the error is invisible, and with 0 there is no charge at all, which is probably why it has gone unnoticed.

## Why it happens

`addenergy()` keeps the peak total pump power in `hyd->Emax`. When the output file is opened, `saveenergy()` turns it into the charge and stores the result back in `Emax`. The report then multiplies by the price a second time:

```c
// src/output.c, saveenergy(), called from openoutfile() on every EN_initQ(EN_SAVE)
    // ... compute and save demand charge
    hyd->Emax = hyd->Emax * hyd->Dcost;
    x[0] = (REAL4)hyd->Emax;

// src/report.c, writeenergy()
    sprintf(s, FMT74, "", hyd->Emax * hyd->Dcost);
    ...
    sprintf(s, FMT75, "", csum + hyd->Emax * hyd->Dcost);
```

`Emax` is set to 0 only in `setdefaults()` when the project is opened. `inithyd()` resets every per-pump energy total but not `Emax`, and `addenergy()` keeps the larger of the old and new values:

```c
// src/hydraul.c, addenergy()
    hyd->Emax = MAX(hyd->Emax, psum);
```

So once `saveenergy()` has inflated `Emax`, a new hydraulic run cannot bring it back down.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-06_demand-charge.inp`](SIM-06_demand-charge.inp) | Net1 (one pump) with `Demand Charge 2.0` and the energy report turned on |
| [`SIM-06_test.c`](SIM-06_test.c) | Runs the analysis three times in one project: a full run, `EN_solveQ` again, then a new `EN_solveH` with the demand multiplier at 0.5. After each run it reads the peak kW and the demand charge from the report, and the demand charge from the binary output file |
| [`SIM-06_fix.patch`](SIM-06_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-06            # unpatched EPANET: FAIL
tools/run-test.sh SIM-06 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-06_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-06_test
./SIM-06_test
```

The first-run error also shows with the command-line runner:

```sh
runepanet SIM-06_demand-charge.inp SIM-06.rpt    # Demand Charge: 386.83
```

**Without the fix:**

```
Run  What               Peak kW   Expected   Report   Output file
  1  first run           96.71     193.42   386.83        193.41
  2  EN_solveQ again     96.71     193.42   773.66        386.83
  3  new EN_solveH       96.73     193.46  1547.31        773.66
FAIL: the demand charge is wrong in 3 of 3 runs (expected 2.0 x peak kW each time)
```

**With the fix:**

```
Run  What               Peak kW   Expected   Report   Output file
  1  first run           96.71     193.42   193.41        193.41
  2  EN_solveQ again     96.71     193.42   193.41        193.41
  3  new EN_solveH       96.73     193.46   193.46        193.46
PASS: every run charges 2.0 x peak kW, in the report and the output file
```

(The expected column multiplies the peak as printed, rounded to 0.01 kW.)

## The fix

Write the charge to the output file without changing `Emax`, so that `Emax` stays the peak kW that `writeenergy()` expects, and reset `Emax` with the other energy totals in `inithyd()`:

```diff
     // ... compute and save demand charge
-    hyd->Emax = hyd->Emax * hyd->Dcost;
-    x[0] = (REAL4)hyd->Emax;
+    x[0] = (REAL4)(hyd->Emax * hyd->Dcost);
```

```diff
         pump->Energy.CurrentEffic = 0.0;
     }
+    hyd->Emax = 0.0;
```

The report and output file are unchanged for runs without a demand charge (checked with Net1 and Net3). EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-07](../SIM-07-energy-averages-computed-in-place/) is the same pattern for the per-pump statistics: `saveenergy()` converts the pump energy totals in place, so a second `EN_solveQ` corrupts them too. The two patches change different lines of `saveenergy()` and apply in either order.
- [SIM-11](../SIM-11-duration-overshoot/) also affects the energy table: when the duration is not a multiple of the hydraulic step, energy is added up past the end of the run.
