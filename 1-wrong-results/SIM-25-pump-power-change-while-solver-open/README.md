# SIM-25: Changing a pump's power while the solver is open has no effect

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | A toolkit program that sets `EN_PUMP_POWER` during a run gets results for the old pump. A constant-power pump keeps its old power. A curve pump switched to constant power adds no head at all. `EN_initH` does not pick up the change either. |
| **Reached from** | Toolkit: `EN_setlinkvalue(EN_PUMP_POWER)` between `EN_openH` and `EN_closeH` |
| **Where** | `EN_setlinkvalue()` in [`src/epanet.c:4296-4305`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4296-L4305); the constant-power branch of `pumpcoeff()` in [`src/hydcoeffs.c:848-874`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L848-L874) |
| **Since** | v2.3. A regression from [PR #758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (commit [10d5079](https://github.com/OpenWaterAnalytics/EPANET/commit/10d5079d750ab6a8b2070bb20778346afacc37ff)), which removed the `updatepumpparams()` call from this setter. |
| **GitHub** | No issue reports this. [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550) is the same problem for pump curves (see [SIM-22](../SIM-22-curve-change-while-solver-open/)). |
| **Fix** | Three lines: [`SIM-25_fix.patch`](SIM-25_fix.patch) (requires [SIM-03](../SIM-03-si-power-pumps-converted-twice/) and [SIM-22](../SIM-22-curve-change-while-solver-open/)) |

## The problem

`EN_setlinkvalue(EN_PUMP_POWER)` gives a pump a constant power. Before a run that works, because `EN_openH` derives the pump's head curve coefficients from the stored power. During a run it does not: the pump keeps the coefficients it had when the solver was opened.

In the test, pumps PU1 and PU2 each lift a fixed 1 cfs. PU1 has a constant power of 10 hp (88.14 ft); PU2 has a head curve (100 ft). With the solver open, PU1 is set to 20 hp and PU2 to 10 hp:

- PU1 still adds 88.14 ft instead of 176.28 ft.
- PU2 adds no head at all instead of 88.14 ft.

Both stay wrong after `EN_initH`. Only `EN_closeH` and `EN_openH` give the right values.

## Why it happens

The setter marks the pump as constant power and stores the power, but does not touch the coefficients `R` and `N` that `pumpcoeff()` uses:

```c
// src/epanet.c, EN_setlinkvalue()
    case EN_PUMP_POWER:
        if (Link[index].Type == PUMP)
        {
            if (value <= 0.0) return 211;
            pumpIndex = findpump(&p->network, index);
            net->Pump[pumpIndex].Ptype = CONST_HP;
            net->Pump[pumpIndex].Hcurve = 0;
            net->Link[index].Km = value;
        }
        break;
```

`constpowerpump()` turns the power into a negative coefficient `R` (proportional to `Km`) and sets `N = -1`, but only `validatepumps()` calls it, at `EN_openH`. So PU1 keeps the `R` for 10 hp.

For PU2 the result is worse. Its `R` and `N` still come from its head curve, where `R` is positive, but the constant-power branch of `pumpcoeff()` expects a negative `R`:

```c
// src/hydcoeffs.c, pumpcoeff()
        r = pump->R * pow(setting, 2.0 - n);

        // Constant HP pump
        if (pump->Ptype == CONST_HP)
        {
            // ... compute pump curve's gradient
            hgrad = -r / q / q;
            ...
            // ... treat as open valve if gradient too small
            else if (hgrad < CSMALL)
            {
                hyd->P[k] = 1.0 / CSMALL;
```

The gradient is negative, so the pump is treated as an open valve.

EPANET 2.2 called `updatepumpparams()` in this setter. PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) moved the coefficient code into `validate.c` and removed the call, together with the ones for pump curves that [SIM-22](../SIM-22-curve-change-while-solver-open/) restores.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-25_power-pumps.inp`](SIM-25_power-pumps.inp) | GPM units. PU1 (constant power, 10 hp) and PU2 (head curve, 100 ft at 1 cfs) each lift a fixed 1 cfs from a reservoir at head 0 |
| [`SIM-25_test.c`](SIM-25_test.c) | With the solver open, sets PU1 to 20 hp and PU2 to 10 hp, re-solves, restarts with `EN_initH`, and finally reopens the solver for comparison |
| [`SIM-25_fix.patch`](SIM-25_fix.patch) | The fix (apply SIM-03, SIM-22 and SIM-22's prerequisites first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-25            # unpatched EPANET: FAIL
tools/run-test.sh SIM-25 --patched  # with the fix and its prerequisites: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-25_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-25_test
./SIM-25_test
```

**Without the fix:**

```
                             PU1 gain (ft)       PU2 gain (ft)
                             Expect   Actual     Expect   Actual
As loaded                      88.14    88.14    100.00   100.00   
PU1 20 hp, PU2 10 hp          176.28    88.14     88.14    -0.00   <-- wrong
after EN_initH                176.28    88.14     88.14    -0.00   <-- wrong
after EN_closeH, EN_openH     176.28   176.28     88.14    88.14   
FAIL: 2 of 4 solves ignore a power change made while the solver was open
```

**With the fix:**

```
                             PU1 gain (ft)       PU2 gain (ft)
                             Expect   Actual     Expect   Actual
As loaded                      88.14    88.14    100.00   100.00   
PU1 20 hp, PU2 10 hp          176.28   176.28     88.14    88.14   
after EN_initH                176.28   176.28     88.14    88.14   
after EN_closeH, EN_openH     176.28   176.28     88.14    88.14   
PASS: a pump power set while the solver is open is used from the next EN_runH
```

## The fix

Recompute the pump's coefficients when the solver is open, with the `updatepumpparams()` function that [SIM-22](../SIM-22-curve-change-while-solver-open/) adds:

```diff
             net->Pump[pumpIndex].Hcurve = 0;
             net->Link[index].Km = value / Ucf[POWER];
+
+            // If the hydraulic solver is open, update the pump's
+            // coefficients now (otherwise EN_openH does this)
+            if (hyd->OpenHflag) updatepumpparams(p, pumpIndex);
         }
```

`updatepumpparams()` cannot fail here: the setter has set `Hcurve` to 0 and checked that the power is positive, so it calls `constpowerpump()`, which always succeeds.

The patch requires SIM-22 for `updatepumpparams()`, and [SIM-03](../SIM-03-si-power-pumps-converted-twice/) because SIM-03 changes the line above (`Km = value / Ucf[POWER]`).

EPANET's own unit tests (6 suites) pass with the patch and its prerequisites.
