# SIM-13: PCV curve extrapolation uses 1 instead of 100 %

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. A positional control valve set above the last point of its curve gets the wrong loss coefficient: 78 % too much head loss in the test, or a fully open valve when the curve ends at 1 % open. |
| **Reached from** | Any PCV with a curve whose last point is below 100 % open, set between that point and 100 % (input file, controls or `EN_setlinkvalue`) |
| **Where** | `pcvlosscoeff()` in [`src/hydcoeffs.c:212`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L212) |
| **Since** | v2.3. A regression from [PR #747](https://github.com/OpenWaterAnalytics/EPANET/pull/747) (commit [6aa2a21](https://github.com/OpenWaterAnalytics/EPANET/commit/6aa2a2132715a3d9d938c350988bb020250c1cac), August 2023), which changed PCV curves from fractions to percentages |
| **GitHub** | No related issue |
| **Fix** | One line: [`SIM-13_fix.patch`](SIM-13_fix.patch) |

## The problem

A PCV's curve gives the valve's flow coefficient as a percentage of its fully open value (Kv/Kvo) against the percentage the valve is open. When the setting is above the curve's last point, EPANET extrapolates in a straight line from that point to (100 %, 100 %).

The extrapolation still uses the end point (1, 1) from before PR [#747](https://github.com/OpenWaterAnalytics/EPANET/issues/747), when curves were in fractions. With percentages that line goes the wrong way:

- **V1**, set 90 % open with a curve ending at (80, 50), should have Kv/Kvo = 75 % and a head loss of 2.22 ft at 1000 gpm. EPANET gives 56.2 % and 3.96 ft. The same curve with an explicit (100, 100) point gives 2.22 ft, as it should.
- **V3**, set 50 % open with a curve ending at (1, 1), should have Kv/Kvo = 50 % and a head loss of 5.00 ft. The extrapolation divides 0 by 0, the NaN is clamped to 1, and the valve is treated as fully open: 1.25 ft.

## Why it happens

`x` and `y` are percentages; `kvr` is converted to a fraction only after the curve lookup:

```c
// src/hydcoeffs.c, pcvlosscoeff()
        x = curve->X;            // x = % open
        y = curve->Y;            // y = Kv / Kvo as a %
...
        // s lies above last point of curve
        else if (s > x[npts-1])
        {
            k2 = npts - 1;
            kvr = (s - x[k2]) / (1. -  x[k2]) * (1. - y[k2]) + y[k2];
        }
...
    // Convert kvr from % to fraction
    kvr /= 100.;
    kvr = MIN(kvr, 1.0);
```

For V1 the slope is (1 − 50)/(1 − 80) = 0.62 instead of (100 − 50)/(100 − 80) = 2.5, so `kvr` = 50 + 10 × 0.62 = 56.2. For V3, `x[k2]` and `y[k2]` are both 1, so the expression is 49/0 × 0 = NaN. `MIN(NaN, 1.0)` evaluates to 1.0, because the comparison `NaN < 1.0` is false. PR [#747](https://github.com/OpenWaterAnalytics/EPANET/issues/747) rescaled the "fully open" test from `s >= 1.0` to `s >= 100.0` and added the `/ 100`, but did not change this line.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-13_pcv-curves.inp`](SIM-13_pcv-curves.inp) | Three PCVs (12 in, fully open loss coefficient 10), each feeding a 1000 gpm junction from a reservoir. V1: 90 %, curve to (80, 50). V2: 90 %, the same curve plus (100, 100). V3: 50 %, curve (0, 0), (1, 1). |
| [`SIM-13_test.c`](SIM-13_test.c) | Solves the network and compares each valve's head loss with the fully open loss divided by (Kv/Kvo)² |
| [`SIM-13_fix.patch`](SIM-13_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-13            # unpatched EPANET: FAIL
tools/run-test.sh SIM-13 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-13_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-13_test
./SIM-13_test
```

**Without the fix:**

```
Fully open head loss: 1.2494 ft

Valve  Kv ratio   Expected head loss   Actual head loss
                         (ft)                (ft)
V1         75 %           2.2212               3.9555   <-- wrong
V2         75 %           2.2212               2.2212   
V3         50 %           4.9978               1.2494   <-- wrong
FAIL: 2 PCV(s) set above their curve's last point have the wrong head loss
```

**With the fix:**

```
V1         75 %           2.2212               2.2212   
V2         75 %           2.2212               2.2212   
V3         50 %           4.9978               4.9978   
PASS: extrapolating a PCV curve to (100, 100) gives the right head loss
```

## The fix

Use the percentage end point:

```diff
-            kvr = (s - x[k2]) / (1. -  x[k2]) * (1. - y[k2]) + y[k2];
+            kvr = (s - x[k2]) / (100. - x[k2]) * (100. - y[k2]) + y[k2];
```

The denominator cannot be zero: this branch runs only when `s > x[npts-1]`, and `s >= 100` has already returned the fully open coefficient.

EPANET's own unit tests (6 suites) pass with the patch. The PCV test in `tests/test_valve.cpp` uses a curve that ends at 100 % open, so it never reaches this branch.
