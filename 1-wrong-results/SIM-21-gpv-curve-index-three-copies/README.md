# SIM-21: A GPV's curve index is stored in three places and only one is kept current

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. After `EN_deletecurve`, or after a new curve is assigned to a GPV that has a control, the solver uses a different curve from the one `EN_GPV_CURVE` reports: 80 ft of head loss instead of 20 ft, or 20 ft instead of 80 ft, in the test. |
| **Reached from** | Toolkit: `EN_deletecurve` on any curve with a lower index than a GPV's curve (or on the GPV's own curve), and `EN_setlinkvalue(EN_GPV_CURVE)` on a GPV with a simple control |
| **Where** | `gpvcoeff()` in [`src/hydcoeffs.c:977`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L977), `adjustcurves()` in [`src/project.c:1252-1257`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1252-L1257), `controls()` in [`src/hydraul.c:628`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L628), `setcontrol()` in [`src/project.c:1362-1366`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1362-L1366) and `EN_setlinkvalue()` in [`src/epanet.c:4357-4358`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4357-L4358) |
| **Since** | v2.2 for `EN_deletecurve`; v2.3 for `EN_GPV_CURVE`, when that property was added |
| **GitHub** | No related issue |
| **Fix** | Eight lines in four files: [`SIM-21_fix.patch`](SIM-21_fix.patch) (requires [CRASH-16](../../2-crashes/CRASH-16-gpv-without-curve/) and [SIM-19](../SIM-19-gpv-curve-type-not-set/)) |

## The problem

A GPV has no setting, only a head loss curve, and EPANET keeps the curve's index in a setting field. It ends up in three places:

| Copy | Written by | Read by |
|---|---|---|
| `Link.Kc` | parser, `EN_GPV_CURVE`, `EN_deletecurve` | `EN_getlinkvalue(EN_GPV_CURVE)` |
| `Link.InitSetting` | parser, `EN_GPV_CURVE` (only while the solver is closed) | `inithyd()`, which copies it into `LinkSetting`; `EN_saveinpfile` |
| `Control.Setting` | `EN_addcontrol` and `[CONTROLS]`, when the control is created | `controls()`, which copies it into `LinkSetting` |

The solver reads `LinkSetting`. So the getter reports one curve and the solver uses another whenever the copies drift apart. In the test, GPV V1 carries 100 gpm on curve C2 (20 ft); C1 gives 5 ft and C3 gives 80 ft:

1. **Deleting C1.** The curves after it move down one index, so C2 is now curve 1. `EN_deletecurve` updates `Kc` to 1, but `InitSetting` stays 2, which is now C3. V1 loses 80 ft, and the saved file names C3.
2. **A control on the GPV.** V1 has `LINK V1 OPEN AT 0` (added with `EN_addcontrol`), and is then given C3 with `EN_GPV_CURVE`. When the control fires it copies its stored index, C2, into `LinkSetting`. V1 loses 20 ft instead of 80 ft.
3. **Deleting C2, V1's own curve.** `Kc` becomes 0 and `EN_GPV_CURVE` reports no curve, but `InitSetting` still says 2, now C3. The run succeeds with 80 ft of head loss.

## Why it happens

The solver takes the curve index from the run-time setting:

```c
// src/hydcoeffs.c, gpvcoeff()
        // Index of valve's head loss curve
        i = (int)ROUND(hyd->LinkSetting[k]);
```

`EN_deletecurve` fixes only `Kc`:

```c
// src/project.c, adjustcurves()
        if (network->Link[k].Type == GPV)
        {
            curve = INT(network->Link[k].Kc);
            adjustcurve(&curve, index);
            network->Link[k].Kc = curve;
        }
```

A GPV control stores the curve index that `Kc` had when the control was created (`s = net->Link[linkIndex].Kc` in `setcontrol()`, and the same in `controldata()`), and `controls()` writes it back whenever the control fires:

```c
// src/hydraul.c, controls()
            k1 = hyd->LinkSetting[k];
            k2 = k1;
            if (link->Type > PIPE) k2 = control->Setting;
```

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-21_gpv.inp`](SIM-21_gpv.inp) | GPM units. R1 feeds J3 (100 gpm) through GPV V1. Curves C1, C2 and C3 give 5, 20 and 80 ft at 100 gpm. V1 uses C2. |
| [`SIM-21_test.c`](SIM-21_test.c) | Runs the three cases above, each on a fresh copy of the network, and checks V1's head loss (and, for case 1, again after saving and reopening) |
| [`SIM-21_fix.patch`](SIM-21_fix.patch) | The fix (apply CRASH-16 and SIM-19 first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-21            # unpatched EPANET: FAIL
tools/run-test.sh SIM-21 --patched  # with the fix (and CRASH-16, SIM-19): PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-21_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-21_test
./SIM-21_test
```

**Without the fix:**

```
V1 head loss (ft)                              Expected   Actual
1. after deleting C1                              20.00    80.00   <-- wrong
1. after deleting C1, saved and reopened          20.00    80.00   <-- wrong
2. with a control, after assigning C3             80.00    20.00   <-- wrong
3. after deleting C2: EN_GPV_CURVE = 0, EN_solveH returned 0, head loss 80.00
   <-- wrong: the solver ran V1 with another curve
FAIL: 4 of 4 cases used the wrong curve for the GPV
```

**With the fix:**

```
V1 head loss (ft)                              Expected   Actual
1. after deleting C1                              20.00    20.00   
1. after deleting C1, saved and reopened          20.00    20.00   
2. with a control, after assigning C3             80.00    80.00   
3. after deleting C2: EN_GPV_CURVE = 0, EN_solveH returned 110, head loss 0.00
PASS: the GPV always uses the curve that EN_GPV_CURVE reports
```

## The fix

Make `Link.Kc`, the value the toolkit reads and writes, the one the solver uses, and keep the copies from contradicting it:

```diff
// gpvcoeff(): read the curve index from Kc
-        i = (int)ROUND(hyd->LinkSetting[k]);
+        i = (int)ROUND(pr->network.Link[k].Kc);

// adjustcurves(): keep InitSetting (what the INP writer uses) equal to Kc
             network->Link[k].Kc = curve;
+            network->Link[k].InitSetting = curve;

// controls(): a GPV control changes only the valve's status
-            if (link->Type > PIPE) k2 = control->Setting;
+            // (a GPV control only changes status; its curve index is in Kc)
+            if (link->Type > PIPE && link->Type != GPV) k2 = control->Setting;

// EN_setlinkvalue(EN_GPV_CURVE): update every copy, also while the solver is open
+            // The solver uses Kc, so it must stay a valid curve while open
+            if (curveIndex == 0 && hyd->OpenHflag) return 206;
             Link[index].Kc = curveIndex;
-            if (hyd->OpenHflag == FALSE) Link[index].InitSetting = curveIndex;
+            Link[index].InitSetting = curveIndex;
+            if (hyd->OpenHflag) hyd->LinkSetting[index] = curveIndex;
```

`Control.Setting` still holds a curve index for GPV controls, but nothing in the solver reads it any more. It is visible only through `EN_getcontrol`.

**Why the patch needs two others.** After this change, deleting a GPV's own curve leaves `Kc` at 0, and the solver would then read `Curve[0]`. [CRASH-16](../../2-crashes/CRASH-16-gpv-without-curve/) adds the check at `EN_openH` that turns this into error 110 (case 3 above). The same reasoning is why `EN_GPV_CURVE` now refuses curve 0 while the solver is open. [SIM-19](../SIM-19-gpv-curve-type-not-set/) changes the neighbouring line in `EN_setlinkvalue`, so this patch is made on top of it.

**Alternative.** Keep the solver on `LinkSetting` and update all three copies everywhere: `adjustcurves()` would also have to rewrite `InitSetting` and every control on the GPV, and `EN_GPV_CURVE` would have to do the same. That leaves more places to forget the next time.

EPANET's own unit tests (6 suites) pass with the three patches.

## Notes

- Updating `InitSetting` and `LinkSetting` in `EN_GPV_CURVE` while the solver is open is also half of the fix for [SIM-22](../SIM-22-curve-change-while-solver-open/): a GPV curve changed during a run is used from the next `EN_runH`, survives `EN_initH`, and is saved. SIM-22's own patch handles pump curves and requires this one.
- `gpvcoeff()` also has the TEMPCLOSED problem described in [SIM-02](../SIM-02-gpv-pbv-flow-through-empty-tank/). The two patches change different lines and apply together.
- Because a GPV curve assigned during a run now takes effect, assigning a one-point curve while the solver is open reaches the out-of-bounds read of [CRASH-06](../../2-crashes/CRASH-06-gpv-one-point-curve/). [CRASH-28](../../2-crashes/CRASH-28-setcurve-shrinks-gpv-curve-while-open/) refuses that assignment (and the same change made through `EN_setcurve`).
