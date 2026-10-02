# SIM-14: PDA with kPa units uses two different demand curves when the pressure range is narrow

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | With kPa pressure units and a required-minus-minimum pressure range between 0.1 and 0.3 kPa, a pressure-driven analysis does not converge ("System may be unstable") and reports demands that are off the demand curve. |
| **Reached from** | Input files or `EN_setdemandmodel`, with pressure-driven demand and `PRESSURE KPA` |
| **Where** | `newdemandflows()` in [`src/hydsolver.c:582`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydsolver.c#L582), against `demandcoeffs()` in [`src/hydcoeffs.c:569`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L569) and `pdaconverged()` in [`src/hydsolver.c:709`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydsolver.c#L709) |
| **Since** | v2.2 (commit [a89f339](https://github.com/OpenWaterAnalytics/EPANET/commit/a89f339525bd0774b35fe43b9ff4e873723c17bc), "PDA fixes", July 2019) |
| **GitHub** | No related issue |
| **Fix** | One line: [`SIM-14_fix.patch`](SIM-14_fix.patch) |

## The problem

In a pressure-driven analysis, a junction's demand falls from its full value at the required pressure to zero at the minimum pressure. EPANET insists that the required pressure is at least 0.1 pressure units above the minimum (error 208 otherwise), so with kPa units a range of 0.1 kPa is accepted.

The solver evaluates the demand curve in three places. Two of them use the range the user gave. The third, which updates the demand flows between trials, uses at least 0.1 **ft**, which is 0.30 kPa. With a range between 0.1 and 0.3 kPa, the matrix and the convergence test follow one curve and the demand update follows another, so the iterations never settle.

In the test network (minimum 20 kPa, required 20.2 kPa):

- `EN_runH` returns warning 2 ("System may be unstable") after 211 trials. With a 5 kPa range the same network converges in 12.
- J1, at 20.04 kPa, gets 18.26 L/s. The PDA formula with the 0.2 kPa range gives 22.32 L/s. 18.26 L/s is what the 0.30 kPa curve gives at that pressure.

Other pressure units are not affected, because 0.1 psi, m, bar or ft is at least 0.1 ft.

## Why it happens

`Preq` and `Pmin` are stored in feet. `MINPDIFF` is 0.1 and is meant in the user's pressure units, as the input checks use it. `newdemandflows()` applies it as a floor in feet:

```c
// src/types.h
#define   MINPDIFF  0.1        // PDA min. pressure difference (psi or m)

// src/hydsolver.c, newdemandflows()
    dp = MAX((hyd->Preq - hyd->Pmin), MINPDIFF);

// src/hydcoeffs.c, demandcoeffs()
    dp = hyd->Preq - hyd->Pmin;

// src/hydsolver.c, pdaconverged()
    double dp = hyd->Preq - hyd->Pmin;
```

A 0.2 kPa range is 0.067 ft, so `newdemandflows()` uses 0.1 ft while the other two use 0.067 ft.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-14_pda-kpa.inp`](SIM-14_pda-kpa.inp) | LPS and kPa. Three 50 L/s junctions in a line, fed by a 12 m reservoir that cannot supply them all. PDA with minimum 20 kPa and required 20.2 kPa. |
| [`SIM-14_test.c`](SIM-14_test.c) | Solves the network with a 5 kPa range (control) and with the file's 0.2 kPa range, and compares each demand with the PDA formula at the node's pressure |
| [`SIM-14_fix.patch`](SIM-14_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-14            # unpatched EPANET: FAIL
tools/run-test.sh SIM-14 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-14_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-14_test
./SIM-14_test
```

**Without the fix**, the control case converges and the narrow range does not:

```
Required pressure 25.0 kPa: EN_runH returned 0 after 12 trials
  Node   Pressure   Demand   PDA formula
           (kPa)    (L/s)       (L/s)
  J1      20.978    22.11       22.11   
  J2      20.001     0.64        0.64   
  J3      20.000     0.02        0.01   

Required pressure 20.2 kPa: EN_runH returned 2 after 211 trials
  Node   Pressure   Demand   PDA formula
           (kPa)    (L/s)       (L/s)
  J1      20.040    18.26       22.32   <-- off the demand curve
  J2      20.000     0.09        0.11   
  J3      20.000     0.00        0.00   

FAIL: with a 0.2 kPa pressure range the solver does not converge and demands are off the PDA curve
```

The report file adds `WARNING: Maximum trials exceeded at 0:00:00 hrs. System may be unstable.`

**With the fix:**

```
Required pressure 20.2 kPa: EN_runH returned 0 after 14 trials
  Node   Pressure   Demand   PDA formula
           (kPa)    (L/s)       (L/s)
  J1      20.041    22.76       22.76   
  J2      20.000     0.12        0.12   
  J3      20.000     0.00        0.00   

PASS: the solver converges and demands follow the PDA curve for both pressure ranges
```

## The fix

Use the same range as `demandcoeffs()` and `pdaconverged()`:

```diff
-    dp = MAX((hyd->Preq - hyd->Pmin), MINPDIFF);
+    dp = hyd->Preq - hyd->Pmin;
```

The clamp is not needed to avoid a zero range: the parser (`optionvalue()`) and `EN_setdemandmodel` both reject a range below `MINPDIFF` user units, which is at least 0.033 ft (0.1 kPa).

**Alternative.** If a floor in feet is wanted as a safeguard, apply the same one in all three functions, for example in a small helper that returns the range, so that the curves cannot drift apart again.

EPANET's own unit tests (6 suites) pass with the patch, including the PDA tests in `tests/test_pda.cpp`.
