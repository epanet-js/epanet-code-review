# WQ-03: FIFO and LIFO tanks treat filling as overflow

| | |
|---|---|
| **Category** | [Water quality](../README.md) |
| **Impact** | Wrong results with no warning. A FIFO or LIFO tank discards water that it should store, including a tank that is not allowed to overflow, and the results depend on whether hydraulics and quality are run one after the other or together. The mass balance ratio stays at 1.000 because the lost mass is booked as outflow. |
| **Reached from** | Any water quality run with a FIFO or LIFO tank that is full at some point: `runepanet`, `EN_runproject`, `EN_solveH` then `EN_solveQ`, or hydraulics and quality stepped together |
| **Where** | `tankmix3()` (FIFO) in [`src/qualreact.c:640`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L640) and [`:674`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L674), and `tankmix4()` (LIFO) in [`src/qualreact.c:722`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L722) |
| **Since** | v2.3: PR [#771](https://github.com/OpenWaterAnalytics/EPANET/pull/771) (commit [`482f658`](https://github.com/OpenWaterAnalytics/EPANET/commit/482f658df4170e327135403f6901d89aa853acfb), 2024) |
| **GitHub** | Regression from the fix for [#769](https://github.com/OpenWaterAnalytics/EPANET/issues/769) |
| **Fix** | About 40 lines in three files: [`WQ-03_fix.patch`](WQ-03_fix.patch) |

## The problem

Issue [#769](https://github.com/OpenWaterAnalytics/EPANET/issues/769) pointed out that the water quality segments of an overflowing tank kept growing while the tank's volume stayed at its maximum. PR [#771](https://github.com/OpenWaterAnalytics/EPANET/issues/771) fixed this for all four tank mixing models. The completely mixed (MIXED) and two-compartment models test the volume the quality solver holds in its segments. The FIFO and LIFO models test `tank->V` instead, which belongs to the hydraulic solver and is not the tank's volume at the time of the quality step:

- **In the normal path** (`runepanet`, `EN_runproject`, `EN_solveH` then `EN_solveQ`) the quality solver runs after all the hydraulics, and `tank->V` still holds the volume at the end of the run. If the tank ends the run full, every step in which it fills is treated as overflow: the inflow passes straight through the tank and the water it should have stored is discarded.
- **When hydraulics and quality are stepped together**, `tank->V` holds the volume at the end of the current hydraulic step. The step in which the tank becomes full is treated as overflow.

Neither branch checks whether the tank is allowed to overflow.

In the test network, R1 fills tank T1, which is full after about 40 minutes and stays full. There are no demands, so water can only leave by overflowing the tank. With overflow switched off, 360,300 mg of chlorine enters the network and should all still be there at the end. The FIFO and LIFO tanks book 275,800 mg of it as overflow in the normal path (77 %), and 11,580 mg when stepped together. With overflow switched on, every model should end with the tank full of R1's water, as the MIXED tank does (444,800 mg). The FIFO and LIFO tanks end with 84,510 mg in the normal path and 366,000 mg when stepped together.

## Why it happens

```c
// src/qualreact.c, tankmix3() (FIFO)
    // Find volume leaving tank, adjusted so its volume doesn't exceed Vmax
    vout = vin - vnet;
    if (tank->V >= tank->Vmax && vnet > 0.0) vout = vin;
    ...
    // Account for mass lost in overflow from 1st segment
    if (tank->V >= tank->Vmax && vnet > 0.0)
        qual->MassBalance.outflow += vnet * tank->C;

// src/qualreact.c, tankmix4() (LIFO)
        // If tank full then remove vnet from leading segments
        if (tank->V >= tank->Vmax)
        {
```

`tank->V` is set by `inithyd()` and updated by `tanklevels()` at the end of each hydraulic step. It is not stored in the hydraulics file, so the quality solver cannot read the value that belongs to the period it is routing. Once the test is true, the whole net inflow `vnet` of the quality step leaves the tank, even on the step in which the tank only reaches its maximum level.

## How to reproduce

| File | What it is |
|---|---|
| [`WQ-03_filling-tank.inp`](WQ-03_filling-tank.inp) | R1 (1.0 mg/L) fills T1 (0 mg/L, 30 ft diameter, 2 to 20 ft) through J1. No demands. T1 is FIFO and cannot overflow; the test changes both settings. |
| [`WQ-03_test.c`](WQ-03_test.c) | Runs the network as MIXED, FIFO and LIFO, with overflow off and on, in the normal path and stepped together (12 runs), and reads the mass balance from each run's report file |
| [`WQ-03_fix.patch`](WQ-03_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh WQ-03            # unpatched EPANET: FAIL
tools/run-test.sh WQ-03 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc WQ-03_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o WQ-03_test
./WQ-03_test
```

**Without the fix**, only the MIXED tank, the control, gives the same results in both paths:

```
Model  Overflow  Path      Mass inflow  Mass outflow   Final mass   (mg)
MIXED  no        normal      3.603e+05         18.41    3.603e+05   
MIXED  no        stepped     3.603e+05         18.41    3.603e+05   
FIFO   no        normal      3.603e+05     2.758e+05    8.451e+04   <-- lost mass from a tank that cannot overflow
FIFO   no        stepped     3.603e+05     1.158e+04    3.487e+05   <-- lost mass from a tank that cannot overflow
LIFO   no        normal      3.603e+05     2.758e+05    8.451e+04   <-- lost mass from a tank that cannot overflow
LIFO   no        stepped     3.603e+05     1.158e+04    3.487e+05   <-- lost mass from a tank that cannot overflow
MIXED  yes       normal      1.101e+07     1.057e+07    4.448e+05   
MIXED  yes       stepped     1.101e+07     1.057e+07    4.448e+05   
FIFO   yes       normal      1.101e+07     1.093e+07    8.451e+04   <-- final mass differs from the MIXED tank
FIFO   yes       stepped     1.101e+07     1.064e+07     3.66e+05   <-- final mass differs from the MIXED tank
LIFO   yes       normal      1.101e+07     1.093e+07    8.451e+04   <-- final mass differs from the MIXED tank
LIFO   yes       stepped     1.101e+07     1.064e+07     3.66e+05   <-- final mass differs from the MIXED tank
FAIL: 8 of 12 runs book filling as overflow
```

The 18 mg booked for the MIXED tank without overflow is rounding: the last filling step overshoots the maximum volume by a fraction of a second's flow (under 1 ft³), and the mixed model trims it.

Net1, whose tank is completely mixed, also gives the same results in both paths, with or without the fix (mass outflow 3.227e+06 mg, final mass 4.712e+06 mg).

**With the fix**, every model and path agrees:

```
FIFO   no        normal      3.603e+05             0    3.603e+05   
FIFO   no        stepped     3.603e+05             0    3.603e+05   
LIFO   no        normal      3.603e+05             0    3.603e+05   
LIFO   no        stepped     3.603e+05             0    3.603e+05   
MIXED  yes       normal      1.101e+07     1.057e+07    4.448e+05   
MIXED  yes       stepped     1.101e+07     1.057e+07    4.448e+05   
FIFO   yes       normal      1.101e+07     1.057e+07    4.448e+05   
FIFO   yes       stepped     1.101e+07     1.057e+07    4.448e+05   
LIFO   yes       normal      1.101e+07     1.057e+07    4.448e+05   
LIFO   yes       stepped     1.101e+07     1.057e+07    4.448e+05   
PASS: every model and path books overflow only when the tank overflows
```

## The fix

The quality solver keeps its own record of each tank's volume, `Tank.Vq`. `initqual()` sets it to `V0`, and a new function `tankoverflow()` updates it on every FIFO or LIFO mixing step and returns the part of the net inflow that would take the tank above `Vmax`, for tanks that can overflow:

```c
    double vover = 0.0;

    if (tank->CanOverflow && vnet > 0.0)
    {
        vover = MAX(0.0, tank->Vq + vnet - tank->Vmax);
    }
    tank->Vq += vnet - vover;
    return vover;
```

`tankmix3()` withdraws the overflow `vover` from its first segments together with the outflow, and books `vover * tank->C` as overflow. `tankmix4()` removes `vover` from its leading segments, where [#771](https://github.com/OpenWaterAnalytics/EPANET/issues/771) removed `vnet`:

```diff
-    vout = vin - vnet;
-    if (tank->V >= tank->Vmax && vnet > 0.0) vout = vin;
+    vover = tankoverflow(tank, vnet);
+    vout = vin - vnet + vover;
 ...
-    if (tank->V >= tank->Vmax && vnet > 0.0)
-        qual->MassBalance.outflow += vnet * tank->C;
+    qual->MassBalance.outflow += vover * tank->C;
```

Because only the excess above `Vmax` is overflow, the step in which a tank becomes full stores what fits and discards the rest, as the MIXED model does. A tank that cannot overflow never discards anything.

This replaces `fix_tank_volume_filemode.diff` from the review's performance work. That patch set `tank->V` from the tank head each time the quality solver read a hydraulic period. It fixed the normal path, but not the stepped path, and it still used the volume at the start of a whole hydraulic period for every quality step within it.

EPANET's own unit tests (6 suites) pass with the patch. Results change for models with FIFO or LIFO tanks that become full; the 33k-junction model used in this review has LIFO tanks.

## Notes

- The MIXED and two-compartment models also trim overflow without checking `CanOverflow`. For a tank that cannot overflow, the hydraulic solver stops the tank filling at its maximum level, so this only ever trims rounding (the 18 mg above).
