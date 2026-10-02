# SIM-17: EN_MINLEVEL and EN_setcurve leave tank volumes stale

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. After the edit, the tank starts the run with a volume that does not match its initial level, so its level jumps on the first time step. A tank draining 200 gpm rises 4.2 ft in the test. |
| **Reached from** | Toolkit API: `EN_setnodevalue(EN_MINLEVEL)` on a cylindrical tank, or `EN_setcurve` / `EN_setcurvevalue` on a tank's volume curve, before a run |
| **Where** | `EN_setnodevalue()`, case `EN_MINLEVEL`, in [`src/epanet.c:2717-2721`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2717-L2721); `EN_setcurve()` in [`src/epanet.c:5369-5376`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5369-L5376) and `EN_setcurvevalue()` in [`src/epanet.c:5301-5304`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5301-L5304) |
| **Since** | `EN_setcurve` and `EN_setcurvevalue`: since they were added in 2.1 (2016). `EN_MINLEVEL`: since 2.2 (2019); in 2.0 it kept the volumes consistent. |
| **GitHub** | No related issue. Commit [7fd8e6a](https://github.com/OpenWaterAnalytics/EPANET/commit/7fd8e6a) (August 2026) fixed the same kind of problem in `EN_settankdata`. |
| **Fix** | Two lines for `EN_MINLEVEL` and a short loop in each curve setter: [`SIM-17_fix.patch`](SIM-17_fix.patch) |

## The problem

A tank stores its minimum, initial and maximum volumes (`Vmin`, `V0`, `Vmax`) alongside its levels. At the start of a run, `inithyd()` sets the tank's volume to `V0`, and each time step moves the level by the change in volume. If `V0` does not match the initial level, the level jumps on the first step by the difference divided by the area.

Three setters change a tank's level-volume relation without updating the stored volumes:

- **`EN_setnodevalue(EN_MINLEVEL)`** on a tank without a volume curve changes the minimum level but keeps `V0` and `Vmax`. In the test, tank T1 (50 ft diameter, initial level 10 ft) drains 200 gpm. With its minimum level raised from 0 to 5 ft, it rises 4.18 ft in the first hour instead of falling 0.82 ft.
- **`EN_setcurve`** replaces the points of a volume curve but keeps `Vmin`, `V0` and `Vmax` of the tanks that use it. Doubling every volume on T2's curve should halve the level change to 0.40 ft; instead the tank falls 5.40 ft in the first hour, because it starts with half the water.
- **`EN_setcurvevalue`** does the same for one point (tank T3).

`Vmax` is stale too, so a tank that fills is not treated as full at its maximum level. In a variant of the test where T1 fills at 2,000 gpm after the same `EN_MINLEVEL` change, its level reaches 23.17 ft against a maximum of 20 ft.

`EN_INITVOLUME` and `EN_MAXVOLUME` return the stale values, so a caller cannot see the problem by reading the tank back.

## Why it happens

For a tank without a volume curve, `tankvolume()` computes a volume from a head as

```c
// src/hydraul.c, tankvolume()
if (j == 0) return(tank->Vmin + (h - tank->Hmin) * tank->A);
```

`EN_MINLEVEL` keeps `Vmin` (by design, see the comment) and moves `Hmin`, so every volume above the minimum changes. It does not recompute `V0` and `Vmax`:

```c
// src/epanet.c, EN_setnodevalue(), case EN_MINLEVEL
Tank[j].Hmin = hTmp;
// NOTE: We assume that for non-volume curve tanks Vmin doesn't change
//       with Hmin. If not the case then a subsequent call setting
//       EN_MINVOLUME must be made.
break;
```

The neighbouring cases `EN_TANKDIAM`, `EN_MINVOLUME`, `EN_MAXLEVEL` and `EN_VOLCURVE` all recompute the volumes they affect. `EN_setcurve` and `EN_setcurvevalue` only load the new points into the curve; they never look at the tanks that use it.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-17_tanks.inp`](SIM-17_tanks.inp) | Three tanks, each draining 200 gpm into its own junction. T1 is a cylinder; T2 and T3 have volume curves with 2,000 ft² of area. |
| [`SIM-17_test.c`](SIM-17_test.c) | Runs one hour, edits the tanks (T1: minimum level 0 to 5 ft; T2: `EN_setcurve` doubles its curve; T3: `EN_setcurvevalue` doubles the top point), runs the hour again, and compares the level changes |
| [`SIM-17_fix.patch`](SIM-17_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-17            # unpatched EPANET: FAIL
tools/run-test.sh SIM-17 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-17_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-17_test
./SIM-17_test
```

**Without the fix**, `EN_INITVOLUME` disagrees with the volume at the initial level, and the levels jump:

```
       EN_INITVOLUME  volume at      level change in the hour (ft)
Tank       (ft3)      t = 0 (ft3)   before edit  after edit  expected
T1        19635.0        9817.5       -0.817       4.183    -0.817   <-- wrong
T2        20000.0       40000.0       -0.802      -5.401    -0.401   <-- wrong
T3        20000.0       40000.0       -0.802      -5.401    -0.401   <-- wrong
FAIL: 3 of 3 tanks keep a stale initial volume after the edit
```

**With the fix:**

```
T1         9817.5        9817.5       -0.817      -0.817    -0.817
T2        40000.0       40000.0       -0.802      -0.401    -0.401
T3        40000.0       40000.0       -0.802      -0.401    -0.401
PASS: every tank's volumes follow the edit
```

## The fix

`EN_MINLEVEL` recomputes `V0` and `Vmax` after moving `Hmin`, as `EN_MINVOLUME` does:

```diff
         Tank[j].Hmin = hTmp;
         // NOTE: We assume that for non-volume curve tanks Vmin doesn't change
         //       with Hmin. If not the case then a subsequent call setting
         //       EN_MINVOLUME must be made.
+        Tank[j].V0 = tankvolume(p, j, Tank[j].H0);     // new init. volume
+        Tank[j].Vmax = tankvolume(p, j, Tank[j].Hmax); // new max. volume
         break;
```

`EN_setcurve` and `EN_setcurvevalue` recompute the three volumes of every tank that uses the curve:

```diff
+    // Update the volumes of any tank that uses the curve
+    for (j = 1; j <= net->Ntanks; j++)
+    {
+        if (net->Tank[j].Vcurve != index) continue;
+        net->Tank[j].Vmin = tankvolume(p, j, net->Tank[j].Hmin);
+        net->Tank[j].V0 = tankvolume(p, j, net->Tank[j].H0);
+        net->Tank[j].Vmax = tankvolume(p, j, net->Tank[j].Hmax);
+    }
```

`EN_setcurvevalue` has no spare integer, so it reuses its point index `n` as the loop variable. A small shared helper next to `tankvolume()` would avoid the duplicate loop; the patch keeps the change inside `epanet.c`.

Points to note:

- The patch keeps the 2.2 rule that `EN_MINLEVEL` leaves `Vmin` unchanged. EPANET 2.0 instead kept the tank's shape (it raised `Vmin` and kept `V0`). Both give the same level changes; they differ only in the volumes reported.
- The curve setters do not update the tank's nominal area `A`, which for a tank with a volume curve only affects `EN_TANKDIAM` and the saved diameter. `EN_VOLCURVE` computes it (see [SIM-20](../SIM-20-volcurve-area-user-units/)).
- A new curve that no longer covers the tank's levels is still caught by `validatetanks()` (error 225) when the solver opens.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Changing a curve while the hydraulic solver is open is a separate question; see [SIM-22](../SIM-22-curve-change-while-solver-open/) for pumps and GPVs. Its patch also adds code at the end of `EN_setcurve` and is written on top of this one (`Requires: SIM-17`).
