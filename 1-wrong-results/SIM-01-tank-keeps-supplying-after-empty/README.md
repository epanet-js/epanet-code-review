# SIM-01: A draining tank keeps supplying water after it is empty

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. The tank delivers water it does not have, or ends below its minimum level. |
| **Reached from** | Any extended-period run with a tank that empties |
| **Where** | `tanklevels()` in [`src/hydraul.c:1086`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L1086) |
| **Since** | The first commit of this repository (2014), so every release |
| **GitHub** | Reported as [#936](https://github.com/OpenWaterAnalytics/EPANET/issues/936) on 27 September 2026, after this review. Fixed on `dev` by [#940](https://github.com/OpenWaterAnalytics/EPANET/pull/940) (merged 1 October 2026), with the same change as this patch. |
| **Fix** | One character: [`SIM-01_fix.patch`](SIM-01_fix.patch) |

## The problem

When a tank drains, EPANET picks the next time step so that the tank reaches its minimum level exactly at the end of that step. The time step is a whole number of seconds, so the tank lands up to half a second away from the minimum, on either side. A clamp in `tanklevels()` is meant to absorb that: if the tank will be empty within the next second, set it to exactly empty.

The clamp has the wrong sign, so it almost never fires. Two things can then happen:

- **The tank lands just above its minimum level.** It is not treated as empty, so its outlet stays open and it keeps supplying the full demand for the whole next hydraulic step. That water does not exist. In the test network, the tank delivers 526 ft³ (3,900 gal) more than it held at 200 gpm.
- **The tank lands just below its minimum level.** It stays there for the rest of the run.

The flow balance in the report still shows a ratio of 1.000, because the phantom water is booked as storage outflow.

## Why it happens

`NodeDemand[n]` is the tank's net inflow in ft³/s, so it is negative while the tank drains. The "full within one second" test adds it; the "empty within one second" test subtracts it:

```c
// src/hydraul.c, tanklevels()
dv = hyd->NodeDemand[n] * tstep;
tank->V += dv;

// Check if tank full/empty within next second
if (tank->V + hyd->NodeDemand[n] >= tank->Vmax)        // correct
{
    tank->V = tank->Vmax;
}
else if (tank->V - hyd->NodeDemand[n] <= tank->Vmin)   // wrong sign
{
    tank->V = tank->Vmin;
}
```

For a draining tank, `V - NodeDemand` is `V + |q|`, which is only at or below `Vmin` if the tank is already more than a second's flow below its minimum. So the test does not catch a tank that is just above `Vmin` (it should snap to empty) or just below it (it should snap back up).

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-01_tank-drains.inp`](SIM-01_tank-drains.inp) | Tank T1 (50 ft diameter, 3 ft above its minimum) drains into junction J1. Reservoir R1, behind a check valve, takes over once the tank is empty. |
| [`SIM-01_test.c`](SIM-01_test.c) | Runs the network step by step for J1 demands of 190 to 215 gpm, adds up the volume leaving the tank and compares it with the volume the tank lost |
| [`SIM-01_fix.patch`](SIM-01_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-01            # unpatched EPANET: FAIL
tools/run-test.sh SIM-01 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-01_test
./SIM-01_test
```

**Without the fix**, 10 of the 26 runs deliver water the tank did not have, and the other 16 end below the minimum volume:

```
Demand   Excess delivered   Final volume
 (gpm)        (ft3)             (ft3)
   190          -0.00           -0.0455   <-- ended below its minimum volume
   191         237.44            0.0000   <-- delivered water it did not have
   ...
   200         526.19            0.0000   <-- delivered water it did not have
   ...
FAIL: out of 26 runs, the tank delivered water it did not have in 10 and ended below its minimum volume in 16
```

At 200 gpm the tank reaches 0 ft at 13,219 s. The step from 13,219 s to 14,400 s (1,181 s) still shows 200 gpm leaving the tank, and the reservoir only takes over at 14,400 s.

**With the fix**, every run stays within one second's flow of the tank's volume:

```
   190           0.04            0.0000
   191          -0.02            0.0000
   ...
   200          -0.07            0.0000
   ...
PASS: the tank never delivers more than it held
```

## The fix

Add the net inflow, as the "full" test does:

```diff
-        else if (tank->V - hyd->NodeDemand[n] <= tank->Vmin)
+        else if (tank->V + hyd->NodeDemand[n] <= tank->Vmin)
```

EPANET's own unit tests (6 suites) pass with the patch.

## Upstream status

[#936](https://github.com/OpenWaterAnalytics/EPANET/issues/936) reported this independently on 27 September 2026, together with the matching problem at a full tank: `tankstatus()` compared heads, and a full tank's recomputed head can round to just below its maximum. [#940](https://github.com/OpenWaterAnalytics/EPANET/pull/940), merged into `dev` on 1 October 2026, makes this one-character change and also makes `tankstatus()` compare volumes. The test in this folder passes on `dev` at [8bca02d](https://github.com/OpenWaterAnalytics/EPANET/commit/8bca02d57af9225e4dd8bba4d9d51b8431a38588) without any patch, so this patch is only needed for a tree at 83e25bb or a backport to 2.3.x.
