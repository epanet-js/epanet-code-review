# CRASH-28: EN_setcurve can shrink a GPV's curve to one point while the solver is open

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap read out of bounds on the next `EN_runH`: a crash, or a head loss computed from the memory next to the curve. |
| **Reached from** | Toolkit, between `EN_openH` and `EN_closeH`: `EN_setcurve` with one point on a curve that a GPV uses. Once [SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) is applied, also `EN_setlinkvalue(EN_GPV_CURVE)` with a one-point curve. |
| **Where** | `EN_setcurve()` in [`src/epanet.c:5356-5363`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5356-L5363); the bad read is in `curvecoeff()`, [`src/hydcoeffs.c:936-944`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L936-L944) |
| **Since** | v2.1, when `ENsetcurve` was added (2015). It has always accepted a single point. |
| **GitHub** | No related issue |
| **Fix** | Thirteen lines: [`CRASH-28_fix.patch`](CRASH-28_fix.patch) (requires [CRASH-06](../CRASH-06-gpv-one-point-curve/) and [SIM-22](../../1-wrong-results/SIM-22-curve-change-while-solver-open/)) |

## The problem

A GPV's head loss is interpolated between two points of its curve, so the curve needs at least two points. [CRASH-06](../CRASH-06-gpv-one-point-curve/) adds that check to `EN_openH`. But a program can change a curve while the solver is open, and `EN_setcurve` accepts a single point. The next `EN_runH` then makes `curvecoeff()` read before the start of the curve's arrays, exactly as in CRASH-06.

In the test, GPV V1 carries 1000 gpm on a two-point curve and loses 10 ft. With the solver open, `EN_setcurve` replaces the curve with the single point (500 gpm, 10 ft). The call returns 0, and the sanitizer build stops in `curvecoeff()` on the next `EN_runH`. The crash also happens with all the other patches in this review applied, since none of them checks `EN_setcurve`.

There is a second way in once [SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) is applied. SIM-21 makes the solver read a GPV's curve index from `Link.Kc`, so that `EN_setlinkvalue(EN_GPV_CURVE)` takes effect during a run (the fix for [SIM-22](../../1-wrong-results/SIM-22-curve-change-while-solver-open/)). Assigning a one-point curve that way then reaches the same read. Without SIM-21 the assignment has no effect during a run, so it cannot crash.

## Why it happens

`EN_setcurve` checks its arguments, but not whether the curve's users can work with the new points:

```c
// src/epanet.c, EN_setcurve()
    // Check for valid arguments
    if (!p->Openflag) return 102;
    if (index <= 0 || index > net->Ncurves) return 206;
    if (xValues == NULL || yValues == NULL) return 206;
    if (nPoints <= 0) return 202;

    // Check that x values are increasing
    for (j = 1; j < nPoints; j++) if (xValues[j-1] >= xValues[j]) return 230;
```

One point is a valid pump curve, and when the solver is closed `EN_openH` catches a one-point GPV curve (with CRASH-06). While the solver is open nothing does. `curvecoeff()` then picks the segment `k1 = k2 - 1` with `k2` clamped to `npts - 1`, which for one point is `x[-1]` to `x[0]` when the flow is above the point (see CRASH-06 for the details).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-28_gpv.inp`](CRASH-28_gpv.inp) | Reservoir R1 supplies 1000 gpm to J1 through GPV V1. V1's curve C1 has two points (10 ft at 1000 gpm); C2 has one point. |
| [`CRASH-28_test.c`](CRASH-28_test.c) | With the solver open: (1) `EN_setcurve` gives C1 one point, then re-solves; (2) `EN_GPV_CURVE` assigns C2 to V1, then re-solves. Each call should be refused and the head loss stay at 10 ft. |
| [`CRASH-28_fix.patch`](CRASH-28_fix.patch) | The fix (apply CRASH-06, SIM-22 and their prerequisites first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-28            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-28 --patched  # with the fix and its prerequisites: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-28_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o CRASH-28_test
./CRASH-28_test
```

**Without the fix**, the sanitizer build stops in step 1:

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in curvecoeff hydcoeffs.c:943
    #1 ... in headlosscoeffs hydcoeffs.c:275
    #2 ... in hydsolve hydsolver.c:121
    #3 ... in runhyd hydraul.c:220
...
... is located 8 bytes before 80-byte region [...)
...
As loaded                                                V1 head loss  10.00 ft
```

**With the fix:**

```
As loaded                                                V1 head loss  10.00 ft
1. EN_setcurve(C1, one point)        call returned 202   V1 head loss  10.00 ft
2. EN_GPV_CURVE = C2 (one point)     call returned 211   V1 head loss  10.00 ft
PASS: a GPV cannot be given a one-point curve while the solver is open
```

## The fix

While the solver is open, refuse the two calls that could give a GPV a one-point curve:

```diff
 // EN_setcurve(): fewer than 2 points for a curve that a GPV uses
     if (nPoints <= 0) return 202;
 
+    // A GPV's head loss curve needs at least 2 points. EN_openH checks
+    // this, so refuse to break it while the solver is open.
+    if (nPoints < 2 && p->hydraul.OpenHflag)
+    {
+        for (j = 1; j <= net->Nlinks; j++)
+        {
+            if (net->Link[j].Type == GPV && ROUND(net->Link[j].Kc) == index)
+                return 202;
+        }
+    }
...
 // EN_setlinkvalue(EN_GPV_CURVE): a one-point curve
             if (curveIndex == 0 && hyd->OpenHflag) return 206;
+            // ... and have at least 2 points, as EN_openH requires
+            if (hyd->OpenHflag && net->Curve[curveIndex].Npts < 2) return 211;
```

The errors reuse the codes each function already returns for bad arguments: 202 ("illegal numeric value") for `EN_setcurve`, which already returns it for `nPoints <= 0`, and 211 ("illegal link property value") for `EN_setlinkvalue`. The checks apply only while the solver is open. With the solver closed, a program may still build a curve one point at a time (with `EN_setcurve` and then `EN_setcurvevalue`), and `EN_openH` checks the result.

**Why the patch needs others.** The `EN_GPV_CURVE` check sits on the lines that [SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) adds, and the `EN_setcurve` check goes next to code that [SIM-22](../../1-wrong-results/SIM-22-curve-change-while-solver-open/) changes. SIM-22 requires SIM-21, so `Requires: CRASH-06 SIM-22` brings in everything, including the `EN_openH` check from CRASH-16 and CRASH-06.

EPANET's own unit tests (6 suites) pass with the patch and its prerequisites.

## Notes

- `EN_setcurvevalue` cannot remove points, and `EN_deletecurve` is refused while the solver is open, so these two checks close the remaining ways to give a running GPV a curve with fewer than two points.
- The check scans all links only when a curve is set to a single point while the solver is open, so it costs nothing in normal use.
