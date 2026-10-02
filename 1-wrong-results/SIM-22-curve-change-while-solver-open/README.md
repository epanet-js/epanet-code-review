# SIM-22: Changing a GPV or pump curve while the solver is open has no effect

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | A toolkit program that changes a GPV or pump curve during a run gets results for the old curve, while the getters report the new one. The change is not picked up by `EN_initH` either, and a GPV change is lost when the project is saved. |
| **Reached from** | Toolkit, while the hydraulic solver is open: `EN_setlinkvalue(EN_GPV_CURVE)`, `EN_setheadcurveindex` (or `EN_setlinkvalue(EN_PUMP_HCURVE)`) and `EN_setcurve` on a pump's head curve |
| **Where** | `EN_setlinkvalue()` in [`src/epanet.c:4357-4358`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4357-L4358), `EN_setheadcurveindex()` in [`src/epanet.c:4683-4688`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4683-L4688) and `EN_setcurve()` in [`src/epanet.c:5369-5376`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5369-L5376) |
| **Since** | Pump curves: v2.3, a regression from [PR #758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (commit [10d5079](https://github.com/OpenWaterAnalytics/EPANET/commit/10d5079d750ab6a8b2070bb20778346afacc37ff)), which removed the fix for [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550). GPV curves: v2.3, when `EN_GPV_CURVE` was added. |
| **GitHub** | [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550) "EN_setcurve fails to update pump curve coefficients" (2019, closed) |
| **Fix** | About 40 lines for pumps: [`SIM-22_fix.patch`](SIM-22_fix.patch). It requires [SIM-21](../SIM-21-gpv-curve-index-three-copies/), which fixes GPVs, and [SIM-17](../SIM-17-tank-volumes-stale-after-edit/), which adds code at the same place in `EN_setcurve`. |

## The problem

A toolkit program can change a network between hydraulic time steps. For curves this does not work:

- **GPV curve.** `EN_setlinkvalue(EN_GPV_CURVE)` changes the curve that `EN_getlinkvalue(EN_GPV_CURVE)` reports, but the solver keeps the old one. `EN_initH` restarts with the old one, and `EN_saveinpfile` writes the old one.
- **Pump curve.** `EN_setheadcurveindex`, and `EN_setcurve` on a pump's curve, change the stored curve, but the pump keeps the coefficients it was given at `EN_openH`. The change takes effect only after `EN_closeH` and `EN_openH`.

In the test, pump PU1 (50 ft at 100 gpm) and GPV V1 (10 ft at 100 gpm) carry a fixed 100 gpm. With the solver open, V1 is given a 30 ft curve and PU1 an 80 ft curve, and the 80 ft curve is then edited to 120 ft. The solver still gives 50 ft and 10 ft after each change and after `EN_initH`. The saved file has the new pump curve but the old GPV curve.

## Why it happens

The GPV setter updates `InitSetting` only while the solver is closed, and never the run-time `LinkSetting` that the solver reads (see [SIM-21](../SIM-21-gpv-curve-index-three-copies/) for the three copies of the index):

```c
// src/epanet.c, EN_setlinkvalue(), EN_GPV_CURVE
            Link[index].Kc = curveIndex;
            if (hyd->OpenHflag == FALSE) Link[index].InitSetting = curveIndex;
```

A pump's curve coefficients (`H0`, `R`, `N`, `Qmax`, `Hmax`) are derived only in `validatepumps()`, which `EN_openH` runs. The setters only store the curve:

```c
// src/epanet.c, EN_setheadcurveindex()
    pump->Hcurve = curveIndex;
    net->Link[linkIndex].Km = 0.0;
    return 0;

// src/epanet.c, EN_setcurve()
    curve->Npts = nPoints;
    for (j = 0; j < nPoints; j++)
    {
        curve->X[j] = xValues[j];
        curve->Y[j] = yValues[j];
    }
    return 0;
```

Issue [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550) reported the `EN_setcurve` case in 2019. EPANET 2.2 fixed it by recomputing the pump's coefficients in these functions (`updatepumpparams()` and `adjustpumpparams()`). PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) moved the coefficient code into `validate.c` and removed those calls, so the 2.2 fix no longer exists.

By reading the code, multi-point pump curves are partly affected: `pumpcoeff()` reads their points on each trial, but `Qmax` and `Hmax`, which the status checks use, still come from `EN_openH`. The test uses one-point curves, whose coefficients are entirely precomputed.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-22_pump-gpv.inp`](SIM-22_pump-gpv.inp) | Pump PU1 (curve PC1: 50 ft at 100 gpm) and GPV V1 (curve GC1: 10 ft at 100 gpm) in series carry a fixed 100 gpm from R1 to J2. PC2 (80 ft) and GC2 (30 ft) are unused. |
| [`SIM-22_test.c`](SIM-22_test.c) | With the solver open, assigns GC2 and PC2 and re-solves, edits PC2 with `EN_setcurve` and re-solves, restarts with `EN_initH`, then saves, reopens and solves |
| [`SIM-22_fix.patch`](SIM-22_fix.patch) | The fix for pump curves (apply CRASH-16, SIM-19, SIM-21 and SIM-17 first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-22            # unpatched EPANET: FAIL
tools/run-test.sh SIM-22 --patched  # with the fix and its prerequisites: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-22_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-22_test
./SIM-22_test
```

**Without the fix:**

```
                               Pump gain (ft)    GPV loss (ft)
                               Expect  Actual   Expect  Actual
As loaded                        50.0    50.00     10.0    10.00   
1. GC2 and PC2 assigned          80.0    50.00     30.0    10.00   <-- wrong
2. PC2 changed to 120 ft        120.0    50.00     30.0    10.00   <-- wrong
3. after EN_initH               120.0    50.00     30.0    10.00   <-- wrong
4. saved and reopened           120.0   120.00     30.0    10.00   <-- wrong
FAIL: 4 of 4 steps ignore a curve change made while the solver was open
```

The saved file has `PU1 R1 J1 HEAD PC2` but `V1 J1 J2 12.0000 GPV GC1`.

**With the fix:**

```
                               Pump gain (ft)    GPV loss (ft)
                               Expect  Actual   Expect  Actual
As loaded                        50.0    50.00     10.0    10.00   
1. GC2 and PC2 assigned          80.0    80.00     30.0    30.00   
2. PC2 changed to 120 ft        120.0   120.00     30.0    30.00   
3. after EN_initH               120.0   120.00     30.0    30.00   
4. saved and reopened           120.0   120.00     30.0    30.00   
PASS: curve changes made while the solver is open are used, kept after EN_initH and saved
```

## The fix

The recommended fix applies the change, as EPANET 2.2 did, rather than refusing it with error 262 ("attempt to modify network structure while solver is active"). Changing a curve does not change the network's structure, and a program that adjusts curves between time steps is a reasonable use of the toolkit.

**GPV curves** are fixed by [SIM-21](../SIM-21-gpv-curve-index-three-copies/), which makes the solver read the curve index from `Link.Kc`, and makes `EN_GPV_CURVE` update `InitSetting` and `LinkSetting` too, also while the solver is open. This patch requires SIM-21.

**Pump curves.** This patch moves the per-pump code of `validatepumps()` into a new function, `updatepumpparams()` (the 2.2 name), and calls it when the solver is open:

```c
// src/validate.c
int updatepumpparams(Project *pr, int i)
{
    ...
    // Pump has a designated pump curve
    if (pump->Hcurve > 0)
        return pumpcurvepump(pr, pump, &net->Curve[pump->Hcurve]);

    // Pump has a constant power setting
    if (net->Link[pump->Link].Km > 0.0)
        return constpowerpump(pr, pump);

    // Pump has no pump curve info assigned
    pump->Ptype = NOCURVE;
    return 226;
}

// src/epanet.c, EN_setheadcurveindex()
    if (p->hydraul.OpenHflag)
    {
        errcode = updatepumpparams(p, pumpIndex);
        if (errcode)
        {
            pump->Hcurve = oldCurve;
            net->Link[linkIndex].Km = oldKm;
            updatepumpparams(p, pumpIndex);
        }
    }
    return errcode;

// src/epanet.c, EN_setcurve()
    if (p->hydraul.OpenHflag)
    {
        for (j = 1; j <= net->Npumps; j++)
        {
            if (net->Pump[j].Hcurve != index) continue;
            errcode = updatepumpparams(p, j);
            if (errcode) return errcode;
        }
    }
```

`validatepumps()` calls the same function, so `EN_openH` behaves as before. In `EN_setcurve` the new block goes after the one that [SIM-17](../SIM-17-tank-volumes-stale-after-edit/) adds there for tank volume curves, which is why this patch also requires SIM-17. While the solver is open, `EN_setheadcurveindex` refuses a curve that is not a valid pump curve (error 227, or 226 for curve 0) and keeps the old one. `EN_setcurve` returns the error but keeps the new points, as 2.2 did; the next `EN_openH` would report the same error. The declaration goes into `funcs.h`.

When the solver is closed nothing changes: `EN_openH` derives the coefficients as before.

EPANET's own unit tests (6 suites) pass with the patch and its prerequisites.

## Notes

- `EN_setlinkvalue(EN_PUMP_POWER)` has the same problem while the solver is open. That is [SIM-25](../SIM-25-pump-power-change-while-solver-open/), whose patch calls `updatepumpparams()` from this one. It is a separate patch because [SIM-03](../SIM-03-si-power-pumps-converted-twice/) changes the same lines.
- [FILE-04](../../4-files/FILE-04-pumps-gpvs-without-curve-not-reloadable/) covers how a GPV without a curve is written to a file.
