# SIM-24: EN_deletecurve leaves a tank's volumes computed from the deleted curve

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. A tank whose volume curve is deleted becomes a cylinder, but keeps the volumes of the curve, so its level jumps on the first time step. In the test, a tank draining 20 gpm falls 7.90 ft in an hour instead of 0.40 ft. |
| **Reached from** | Toolkit API: `EN_deletecurve` on a curve that a tank uses as its volume curve, followed by a run |
| **Where** | `EN_deletecurve()` in [`src/epanet.c:5115-5116`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5115-L5116), which calls `adjustcurves()` in [`src/project.c:1225-1229`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1225-L1229) |
| **Since** | EPANET 2.2, when `EN_deletecurve` was added |
| **GitHub** | No related issue |
| **Fix** | A short loop in `EN_deletecurve`: [`SIM-24_fix.patch`](SIM-24_fix.patch) |

## The problem

`EN_deletecurve` is allowed on a curve that a tank uses for its volumes. It sets the tank's curve index to 0, so from then on `tankvolume()` and `tankgrade()` treat the tank as a cylinder with its nominal area, the slope of the deleted curve from its first point to its last. The tank's stored minimum, initial and maximum volumes (`Vmin`, `V0`, `Vmax`) still come from the curve.

For a straight-line curve the two agree. For any other shape they do not, and the run starts from a volume that does not match the initial level. In the test, tank T1 is an elevated tank with a narrow riser below 10 ft and a wide bowl above; its curve gives 1,000 ft³ at the initial level of 10 ft, and its nominal area is 400 ft². After the curve is deleted:

- `EN_INITVOLUME` still reads 1,000 ft³, while a 400 ft² cylinder holds 4,000 ft³ at 10 ft;
- over the first hour, draining 20 gpm (160.4 ft³), the level falls 7.90 ft, to 2.1 ft, instead of 0.40 ft.

The run reports no warning.

## Why it happens

```c
// src/epanet.c, EN_deletecurve()
// Adjust references by other objects to curves
adjustcurves(net, index);

// src/project.c, adjustcurves()
// Adjust tank volume curves
for (j = 1; j <= network->Ntanks; j++)
{
    adjustcurve(&network->Tank[j].Vcurve, index);   // sets Vcurve to 0
}
```

`adjustcurves()` only changes indices. The other way to remove a tank's volume curve, `EN_setnodevalue(EN_TANKDIAM)`, does update the volumes:

```c
// src/epanet.c, EN_setnodevalue(), case EN_TANKDIAM
Tank[j].Vcurve = 0;                        // remove volume curve

// Since the volume curve no longer applies we assume that the tank's
// shape below Hmin is cylindrical and Vmin equals area times Hmin
Tank[j].Vmin = Tank[j].A * (Tank[j].Hmin - Node[index].El);
...
Tank[j].V0 = tankvolume(p, j, Tank[j].H0);     // new init. volume
Tank[j].Vmax = tankvolume(p, j, Tank[j].Hmax); // new max. volume
```

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-24_tank-curve.inp`](SIM-24_tank-curve.inp) | Elevated tank T1 with volume curve VC (100 ft³ per ft up to 10 ft, 700 ft³ per ft above), starting at 10 ft and draining 20 gpm into J1 |
| [`SIM-24_test.c`](SIM-24_test.c) | Deletes VC with `EN_deletecurve`, compares `EN_INITVOLUME` with the volume at the initial level, and runs one hour |
| [`SIM-24_fix.patch`](SIM-24_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-24            # unpatched EPANET: FAIL
tools/run-test.sh SIM-24 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-24_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-24_test
./SIM-24_test
```

**Without the fix:**

```
After EN_deletecurve: T1 area 400.0 ft2 (diameter 22.57 ft)
T1 EN_INITVOLUME 1000.0 ft3, volume at the initial level 4000.0 ft3
T1 level change in one hour: -7.901 ft (expected -0.401)
FAIL: T1 kept the volumes of the deleted curve, so its level is wrong by 7.50 ft after one hour
```

**With the fix:**

```
After EN_deletecurve: T1 area 400.0 ft2 (diameter 22.57 ft)
T1 EN_INITVOLUME 4000.0 ft3, volume at the initial level 4000.0 ft3
T1 level change in one hour: -0.401 ft (expected -0.401)
PASS: T1's volumes follow its new shape
```

## The fix

Before `adjustcurves()` drops the curve index, give each tank that uses the curve the same treatment as `EN_TANKDIAM`:

```diff
+    // A tank that uses the curve becomes a cylinder with the curve's
+    // nominal area, so its volumes must be updated (see EN_TANKDIAM)
+    for (i = 1; i <= net->Ntanks; i++)
+    {
+        if (net->Tank[i].Vcurve != index) continue;
+        net->Tank[i].Vcurve = 0;
+        net->Tank[i].Vmin = net->Tank[i].A *
+            (net->Tank[i].Hmin - net->Node[net->Tank[i].Node].El);
+        net->Tank[i].V0 = tankvolume(p, i, net->Tank[i].H0);
+        net->Tank[i].Vmax = tankvolume(p, i, net->Tank[i].Hmax);
+    }
+
     // Adjust references by other objects to curves
     adjustcurves(net, index);
```

The loop sits in `EN_deletecurve` rather than in `adjustcurves()`, because `tankvolume()` needs the project, and `adjustcurves()` only gets the network.

Alternatives:

- **Keep `Vmin` from the curve** and recompute only `V0` and `Vmax`, as `EN_MINLEVEL` assumes for cylindrical tanks. The levels change the same way; only the volumes reported below the minimum level differ.
- **Refuse to delete a curve that a tank uses**, with an error, so the caller sets a diameter first. That is stricter than the rest of `EN_deletecurve`, which accepts deleting a curve that a pump or valve uses (as `EN_deletepattern` accepts deleting a pattern in use).

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The nominal area comes from the curve. If the curve was assigned with `EN_setnodevalue(EN_VOLCURVE)` in an SI project, that area is 10.76 times too small unless [SIM-20](../SIM-20-volcurve-area-user-units/) is applied.
- [SIM-17](../SIM-17-tank-volumes-stale-after-edit/) fixes the same kind of stale volume after `EN_MINLEVEL`, `EN_setcurve` and `EN_setcurvevalue`. The three patches change different functions and apply together.
