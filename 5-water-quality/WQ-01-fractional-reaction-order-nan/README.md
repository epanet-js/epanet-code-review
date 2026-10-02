# WQ-01: A reaction order between 0 and 1 gives NaN across the network

| | |
|---|---|
| **Category** | [Water quality](../README.md) |
| **Impact** | Wrong results with no error. Concentrations become NaN wherever the affected water goes, and stay NaN in any tank it enters. The mass balance ratio is NaN. |
| **Reached from** | Input file (`[REACTIONS] Order Bulk` or `Order Tank` between 0 and 1) or toolkit (`EN_setoption` with `EN_BULKORDER` or `EN_TANKORDER`), in any chemical run where some water has a concentration of exactly 0 |
| **Where** | `bulkrate()` in [`src/qualreact.c:399`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L399) |
| **Since** | The same expression is in the first commit of this repository (2014) |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`WQ-01_fix.patch`](WQ-01_fix.patch) |

## The problem

EPANET accepts any bulk or tank reaction order, and orders between 0 and 1 are a reasonable fit for some decay data. With such an order, any water at exactly 0 mg/L gets a reaction rate of NaN. That is common: pipes and tanks often start at 0, and the explicit reaction step clamps a decaying concentration to exactly 0 once it would go negative.

The NaN then travels with the water. It does not wash out of a completely mixed tank, so the tank stays NaN for the rest of the run, and the network downstream of it turns NaN when the tank drains. The run finishes with no error or warning.

In the test network, reservoir R1 (1.0 mg/L) fills tank T1 through junction J1 for three hours and T1 then drains back. Everything except R1 starts at 0 mg/L. With a bulk order of 1, all values are finite. With a bulk order of 0.5, T1 is NaN from the first hour, J1 and P1 are NaN from 4:00, when T1 starts to drain, and the mass balance ratio is NaN.

## Why it happens

For an n-th order reaction the rate is `kb * c^n`. `bulkrate()` computes it as `c1 * c^(n-1)`, where `c1` is `c` when there is no limiting concentration:

```c
// src/qualreact.c, bulkrate()
        // Account for limiting potential
        if (qual->Climit == 0.0) c1 = c;
        else c1 = MAX(0.0, SGN(kb) * (qual->Climit - c));

        // Compute concentration potential
        if (order == 1.0) c = c1;
        else if (order == 2.0) c = c1 * c;
        else c = c1 * pow(MAX(0.0, c), order - 1.0);
    }

    // Reaction rate = bulk coeff. * potential
    if (c < 0) c = 0;
    return kb * c;
```

For `c = 0` and `order < 1` the exponent is negative, so `pow(0, order - 1)` is `inf` and `0 * inf` is NaN. The `c < 0` guard is false for NaN. `pipereact()` and `tankreact()` then apply `MAX(0.0, cnew)`, which is also false for NaN and passes it on. With a limiting concentration (`Climit > 0`) and growth, the same line gives `c1 * inf = inf` instead.

## How to reproduce

| File | What it is |
|---|---|
| [`WQ-01_half-order.inp`](WQ-01_half-order.inp) | R1 (1.0 mg/L, head 100 ft, dropping to 40 ft at 3:00) feeds J1 (500 gpm) and tank T1 through two 1-mile pipes. Bulk order 0.5, bulk coefficient -10/day. |
| [`WQ-01_test.c`](WQ-01_test.c) | Runs the network with bulk order 1 and then 0.5 and prints the hourly quality at J1, T1 and P1 and the mass balance ratio |
| [`WQ-01_fix.patch`](WQ-01_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh WQ-01            # unpatched EPANET: FAIL
tools/run-test.sh WQ-01 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc WQ-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o WQ-01_test
./WQ-01_test
```

**Without the fix**, order 1 is fine and order 0.5 gives NaN:

```
Bulk order 1.0
  Hour    J1 (mg/L)   T1 (mg/L)   P1 (mg/L)
     0      0.0000      0.0000      0.0000
     1      0.8452      0.0006      0.9362
...
     4      0.0511      0.0511      0.2056
...
  Mass balance ratio: 1.0000

Bulk order 0.5
  Hour    J1 (mg/L)   T1 (mg/L)   P1 (mg/L)
     0      0.0000      0.0000      0.0000
     1      0.8402         nan      0.9351
     2      0.8256         nan      0.9276
     3      0.8090         nan      0.9190
     4         nan         nan         nan
     5         nan         nan         nan
     6         nan         nan         nan
  Mass balance ratio: nan

FAIL: order 0.5 gives NaN concentrations
```

J1 and P1 recover during the first three hours because fresh water from R1 flushes the NaN out of the pipes. The NaN that reached T1 stays there, and comes back out when T1 drains.

**With the fix**, order 0.5 gives finite values. They fall faster than with order 1 once the concentration is low, as a half-order reaction should (the tank keeps the default tank order of 1):

```
Bulk order 0.5
  Hour    J1 (mg/L)   T1 (mg/L)   P1 (mg/L)
     0      0.0000      0.0000      0.0000
     1      0.8402      0.0004      0.9351
     2      0.8256      0.0541      0.9276
     3      0.8090      0.0723      0.9190
     4      0.0175      0.0473      0.1434
     5      0.0047      0.0309      0.0003
     6      0.0001      0.0203      0.0000
  Mass balance ratio: 1.0000

PASS: order 0.5 gives finite concentrations
```

## The fix

Return a zero reaction potential when the concentration is not positive:

```diff
         else if (order == 2.0) c = c1 * c;
-        else c = c1 * pow(MAX(0.0, c), order - 1.0);
+        else if (c <= 0.0) c = 0.0;   // avoids pow(0, <0) = inf for order < 1
+        else c = c1 * pow(c, order - 1.0);
```

Zero is the limit of `kb * c^n` as `c` goes to 0 for any positive order. For orders above 1, `pow(0, order - 1)` was already 0, so those results do not change, and orders 0, 1 and 2 and Michaelis-Menten kinetics (negative orders) take other branches. With a limiting concentration and growth, the formula has no finite value at `c = 0` for orders below 1; the fix gives 0 there, so the concentration does not grow from nothing, which matches orders above 1.

EPANET's own unit tests (6 suites) pass with the patch.
