# CRASH-22: EN_setcurvevalue reads past the curve before range-checking the point index

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | A point index two or more past the end of a curve reads outside the curve's arrays, and the new x-value is checked against that stray value instead of the last point. A curve can end up with decreasing x-values. |
| **Reached from** | Toolkit API: `EN_setcurvevalue` with `pointIndex` of at least the number of points + 2 |
| **Where** | `EN_setcurvevalue()` in [`src/epanet.c:5274-5288`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5274-L5288) |
| **Since** | v2.2 (the function dates from April 2019) |
| **GitHub** | No related issue |
| **Fix** | Two lines moved: [`CRASH-22_fix.patch`](CRASH-22_fix.patch) |

## The problem

The comment on `EN_setcurvevalue` in `epanet.c` says: "if pointIndex exceeds the curve's length a new point is added". The function then clamps the index to `Npts + 1`, but only after it has checked that the new x-value keeps the curve's x-values increasing. That check uses the unclamped index, so for a 3-point curve and point index 5 it reads `X[3]`, one element past the end of the array.

Under AddressSanitizer this is a heap-buffer-overflow. Without sanitizers, the new point is compared with whatever lies past the array. In the test, built without sanitizers, the point (500, 60) is accepted after (2000, 50), so the curve's x-values are 0, 1000, 2000, 500. A pump or valve that uses such a curve then interpolates on a curve that is not monotonic. Point index `Npts + 1` works correctly, which is probably why this has not been noticed.

## Why it happens

```c
// src/epanet.c, EN_setcurvevalue()
    int n = pointIndex - 1;
    ...
    if (pointIndex <= 0) return 251;

    // Check that new point maintains increasing x values
    if (n - 1 >= 0) x1 = curve->X[n-1];
    if (n + 1 < curve->Npts) x2 = curve->X[n+1];
    if (x <= x1 || x >= x2) return 230;

    // Expand curve if need be
    if (pointIndex > curve->Npts) pointIndex = curve->Npts + 1;
```

`x1` should be the last point's x-value when a point is appended. With `pointIndex` = `Npts + 2` or more, `X[n-1]` is past the last point.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-22_test.c`](CRASH-22_test.c) | Builds a project with `EN_init` and a 3-point curve, (0, 100), (1000, 80), (2000, 50), then calls `EN_setcurvevalue` with point index 5 and x = 500 (must be refused with 230), then x = 3000 (must be added as point 4) |
| [`CRASH-22_fix.patch`](CRASH-22_fix.patch) | The fix |

No input file is needed. With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-22            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-22 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-22_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-22_test
./CRASH-22_test
```

**Without the fix**, the first call reads past the 3-point array:

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in EN_setcurvevalue epanet.c:5283
    #1 ... in main CRASH-22_test.c:35
...
... is located 0 bytes after 24-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in resizecurve project.c:1276
    #2 ... in EN_setcurve epanet.c:5367
```

Built without sanitizers, the test runs on and shows the wrong check:

```
EN_setcurvevalue(C1, point 5, x = 500) returned 0 (expected 230)
EN_setcurvevalue(C1, point 5, x = 3000) returned 0 (expected 0)
C1 now has 5 points; point 4 is (500, 60) (expected 4 points, point 4 = (3000, 20))
FAIL: EN_setcurvevalue checked a point index past the end of the curve against the wrong point
```

**With the fix**:

```
EN_setcurvevalue(C1, point 5, x = 500) returned 230 (expected 230)
EN_setcurvevalue(C1, point 5, x = 3000) returned 0 (expected 0)
C1 now has 4 points; point 4 is (3000, 20) (expected 4 points, point 4 = (3000, 20))
PASS: a point index past the end is checked against the last point and appended
```

## The fix

Clamp the index, and set `n` from it, before the x-value check:

```diff
-    int n = pointIndex - 1;
+    int n;
 ...
     if (pointIndex <= 0) return 251;
 
+    // A point index past the end of the curve adds a new last point
+    if (pointIndex > curve->Npts) pointIndex = curve->Npts + 1;
+    n = pointIndex - 1;
+
     // Check that new point maintains increasing x values
     if (n - 1 >= 0) x1 = curve->X[n-1];
     if (n + 1 < curve->Npts) x2 = curve->X[n+1];
     if (x <= x1 || x >= x2) return 230;
 
     // Expand curve if need be
-    if (pointIndex > curve->Npts) pointIndex = curve->Npts + 1;
     if (pointIndex >= curve->Capacity)
```

The rest of the function is unchanged: it already sets `n` to the new last point when it appends. EPANET's own unit tests (6 suites) pass with the patch.

The other choice would be to return 251 for a point index above `Npts + 1`, but that would break callers that rely on the comment and pass a large index to append.

## Notes

- The append behaviour is described only in the source comment, not in the `EN_setcurvevalue` documentation in `epanet2_2.h`. Adding a sentence there would help.
