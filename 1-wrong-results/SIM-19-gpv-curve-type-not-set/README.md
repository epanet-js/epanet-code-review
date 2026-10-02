# SIM-19: GPV curves set through the API are never typed as head loss curves

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | After `EN_setflowunits` changes the unit system, a GPV curve assigned through the toolkit keeps its old numbers, so the valve's head loss is wrong (almost 5 times too small in the test). The wrong curve type is also written to saved files. |
| **Reached from** | Toolkit: `EN_setlinkvalue(EN_GPV_CURVE)` followed by `EN_setflowunits` to the other unit system |
| **Where** | `assigncurvetypes()` in [`src/project.c:1148-1152`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1148-L1152) and `EN_setlinkvalue()` in [`src/epanet.c:4352-4360`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4352-L4360) |
| **Since** | v2.3.5. [PR #903](https://github.com/OpenWaterAnalytics/EPANET/pull/903) (commit [3d70ced](https://github.com/OpenWaterAnalytics/EPANET/commit/3d70ced3fdc078fa0a9aa5717980347df6ae6fb1), February 2026) added `assigncurvetypes()` to fix curve types before conversion; its GPV branch never matches. |
| **GitHub** | [PR #903](https://github.com/OpenWaterAnalytics/EPANET/pull/903) |
| **Fix** | Two lines: [`SIM-19_fix.patch`](SIM-19_fix.patch) |

## The problem

`EN_setflowunits` converts every curve whose type says what its x and y values are: a head loss curve is converted from gpm and ft to L/s and m, for example. A curve's type is set when something uses it. The `[VALVES]` parser makes a GPV's curve a head loss curve, but `EN_setlinkvalue(EN_GPV_CURVE)` does not, so a curve assigned through the toolkit stays a generic curve.

PR [#903](https://github.com/OpenWaterAnalytics/EPANET/issues/903) added `assigncurvetypes()`, which `EN_setflowunits` calls first, to repair such types. For GPVs it looks in the wrong field and does nothing.

In the test, GPV V1 is given curve C2, (0, 0) to (1000 gpm, 20 ft), with `EN_GPV_CURVE`. It loses 2 ft at 100 gpm. After `EN_setflowunits(EN_LPS)`:

- C2 still reads (1000, 20), now taken as 1000 L/s and 20 m. It should read (63.09 L/s, 6.096 m).
- V1 loses 0.126 m instead of 0.610 m (2 ft).
- `EN_getcurvetype` reports `EN_GENERIC_CURVE`, and `EN_saveinpfile` writes `GENERIC` into `[CURVES]`, so the type is still wrong after the file is reopened.

## Why it happens

A GPV keeps its curve index in `Link.Kc`. `Valve[].Curve` is only used by PCVs, and is 0 for a GPV:

```c
// src/project.c, assigncurvetypes()
        else if (link->Type == GPV){
            if((j = valve->Curve) > 0) {          // always 0 for a GPV
                network->Curve[j].Type = HLOSS_CURVE;
            }
        }

// src/epanet.c, EN_setlinkvalue()
    case EN_GPV_CURVE:
        if (Link[index].Type == GPV)
        {
            curveIndex = ROUND(value);
            if (curveIndex < 0 || curveIndex > net->Ncurves) return 206;
            Link[index].Kc = curveIndex;          // no curve type set
            if (hyd->OpenHflag == FALSE) Link[index].InitSetting = curveIndex;
        }
        break;
```

The parser, by contrast, sets `net->Curve[c].Type = HLOSS_CURVE` in `valvedata()`.

When a saved file is reopened, the `GENERIC` keyword on the curve's first line in `[CURVES]` is read after `[VALVES]`, so it overwrites the head loss type that `valvedata()` assigned.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-19_gpv.inp`](SIM-19_gpv.inp) | GPM units. Reservoir R1 feeds J2 (100 gpm) through pipe P1 and GPV V1. V1 uses C1 (10 ft at 1000 gpm); C2 (20 ft at 1000 gpm) is unused. |
| [`SIM-19_test.c`](SIM-19_test.c) | Assigns C2 to V1 with `EN_GPV_CURVE`, checks the curve type and head loss, switches to LPS, checks the converted curve and head loss, then saves, reopens and checks the type again |
| [`SIM-19_fix.patch`](SIM-19_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-19            # unpatched EPANET: FAIL
tools/run-test.sh SIM-19 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-19_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-19_test
./SIM-19_test
```

**Without the fix:**

```
                                               Expected     Actual
C2 curve type (EN_HLOSS_CURVE = 3)               3.0000     4.0000   <-- wrong
GPM: V1 head loss (ft)                           2.0000     2.0000   
LPS: C2 second point, flow (L/s)                63.0902  1000.0000   <-- wrong
LPS: C2 second point, head loss (m)              6.0960    20.0000   <-- wrong
LPS: V1 head loss (m)                            0.6096     0.1262   <-- wrong
Saved and reopened: C2 curve type                3.0000     4.0000   <-- wrong
FAIL: 5 of 6 checks wrong; the API-assigned GPV curve is not a head loss curve and is not converted
```

The saved file has ` C2  0.0000  0.0000  GENERIC`.

**With the fix:**

```
                                               Expected     Actual
C2 curve type (EN_HLOSS_CURVE = 3)               3.0000     3.0000   
GPM: V1 head loss (ft)                           2.0000     2.0000   
LPS: C2 second point, flow (L/s)                63.0902    63.0906   
LPS: C2 second point, head loss (m)              6.0960     6.0960   
LPS: V1 head loss (m)                            0.6096     0.6096   
Saved and reopened: C2 curve type                3.0000     3.0000   
PASS: the API-assigned GPV curve is typed, converted and saved as a head loss curve
```

## The fix

Read the GPV's curve index from `Kc` in `assigncurvetypes()`, and type the curve when it is assigned, as the parser does:

```diff
         else if (link->Type == GPV){
-            if((j = valve->Curve) > 0) {
+            // A GPV's curve index is stored in its Kc property
+            if((j = ROUND(link->Kc)) > 0) {
                 network->Curve[j].Type = HLOSS_CURVE;
...
             Link[index].Kc = curveIndex;
             if (hyd->OpenHflag == FALSE) Link[index].InitSetting = curveIndex;
+            if (curveIndex > 0) net->Curve[curveIndex].Type = HLOSS_CURVE;
```

The first change is enough for `EN_setflowunits`, including for files saved before the fix that carry `GENERIC`. The second makes `EN_getcurvetype` and saved files right straight away.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-21](../SIM-21-gpv-curve-index-three-copies/) and [SIM-22](../SIM-22-curve-change-while-solver-open/) also change the `EN_GPV_CURVE` case of `EN_setlinkvalue`. SIM-21's patch is made on top of this one (`Requires: SIM-19`).
- `EN_PCV_CURVE` does not type its curve either, but `assigncurvetypes()` reads `Valve[].Curve` correctly for PCVs, so `EN_setflowunits` is not affected (PCV curves are percentages and are not converted).
