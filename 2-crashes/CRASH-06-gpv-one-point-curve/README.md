# CRASH-06: A one-point GPV head loss curve reads out of bounds

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap read out of bounds on every solver trial: a crash, or a head loss computed from the memory next to the curve. |
| **Reached from** | Input files and the toolkit: any GPV whose head loss curve has a single point |
| **Where** | `curvecoeff()` in [`src/hydcoeffs.c:936-944`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L936-L944); `validatecurves()` in [`src/validate.c:109-151`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/validate.c#L109-L151) accepts one point |
| **Since** | The first commit of this repository (2014), so every release |
| **GitHub** | No related issue |
| **Fix** | Ten lines added to the check from CRASH-16: [`CRASH-06_fix.patch`](CRASH-06_fix.patch) (requires [CRASH-16](../CRASH-16-gpv-without-curve/)) |

## The problem

A GPV's head loss is read off its curve by linear interpolation between the two points that bracket the current flow. A curve with a single point has no such pair. EPANET accepts the curve anyway, because a single point is valid for a pump curve, and the interpolation then reads one element before or after the curve's one-element arrays.

In the test, a GPV with the one-point curve (500 gpm, 10 ft) carries 1000 gpm. The sanitizer build stops with a heap-buffer-overflow on the first trial. Without the sanitizers the run "succeeds" with a head loss built from neighbouring memory.

## Why it happens

`curvecoeff()` looks for the first point at or above the flow and uses the segment that ends there:

```c
// src/hydcoeffs.c, curvecoeff()
    // Find linear segment of curve that brackets flow q
    k2 = 0;
    while (k2 < npts && x[k2] < q) k2++;
    if (k2 == 0) k2++;
    else if (k2 == npts)  k2--;
    k1 = k2 - 1;

    // Compute slope and intercept of this segment
    *r = (y[k2] - y[k1]) / (x[k2] - x[k1]);
```

With `npts == 1`: if the flow is above the point, `k2` goes to 1, back to 0, and `k1` is −1, so the code reads before the start of the arrays. If the flow is at or below the point, `k2` becomes 1 and `k1` 0, so it reads past the curve's one point. That second read usually stays inside the arrays' allocated capacity (the parser grows a curve's arrays 10 points at a time), so AddressSanitizer may not flag it, but the values read were never set. The test uses a flow above the point.

`validatecurves()` only checks that a curve has at least one point and that its x values increase. Pumps are protected separately, because `pumpcurvepump()` treats a one-point curve as a power function and never calls `curvecoeff()` on it. GPVs have no such check.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-06_one-point-curve.inp`](CRASH-06_one-point-curve.inp) | Reservoir R1 supplies 1000 gpm to J1 through GPV V1, whose curve C1 is the single point (500 gpm, 10 ft) |
| [`CRASH-06_test.c`](CRASH-06_test.c) | Solves the network and expects error 110 |
| [`CRASH-06_fix.patch`](CRASH-06_fix.patch) | The fix (apply [`CRASH-16_fix.patch`](../CRASH-16-gpv-without-curve/CRASH-16_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-06            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-06 --patched  # with the fix (and CRASH-16): PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-06_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-06_test
./CRASH-06_test
```

`runepanet CRASH-06_one-point-curve.inp CRASH-06.rpt` reaches the same read.

**Without the fix:**

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in curvecoeff hydcoeffs.c:943
    #1 ... in headlosscoeffs hydcoeffs.c:275
    #2 ... in hydsolve hydsolver.c:121
    #3 ... in runhyd hydraul.c:220
...
... is located 8 bytes before 80-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in resizecurve project.c:1278
    #2 ... in curvedata input3.c:786
```

"8 bytes before" the curve's array is `x[-1]`: the flow (1000 gpm) is above the curve's only point.

**With the fix:**

```
EN_solveH returned 110
PASS: a one-point GPV curve is reported as an error (110) and the solver does not run
```

and the report says:

```
  Error 231: no data provided for curve C1 (a GPV curve needs 2 or more points)
  Error 110: cannot solve network hydraulic equations
```

## The fix

[CRASH-16](../CRASH-16-gpv-without-curve/) adds `validatevalves()`, which runs at `EN_openH` and checks that each GPV has a curve. This patch adds a second test to it:

```c
        // Head loss is interpolated between curve points, so a GPV's
        // curve needs at least 2 (validatecurves() reports 0 points)
        else if (net->Curve[c].Npts == 1)
        {
            sprintf(pr->Msg, "Error 231: %s %s (a GPV curve needs 2 or more points)",
                geterrmsg(231, errmsg), net->Curve[c].ID);
            writeline(pr, pr->Msg);
            result = 0;
        }
```

`EN_openH` then returns 110, as for the other validation errors. There is no error code for "too few points", so the patch reuses 231 ("no data provided for curve") and adds the reason to the report line. A new code would be clearer if the maintainers prefer one.

The patch has a `Requires: CRASH-16` line because it edits the function CRASH-16 adds.

EPANET's own unit tests (6 suites) pass with both patches.

## Notes

- This check runs only at `EN_openH`. `EN_setcurve` can still shrink a GPV's curve to one point while the solver is open; that is [CRASH-28](../CRASH-28-setcurve-shrinks-gpv-curve-while-open/), whose patch builds on this one.
