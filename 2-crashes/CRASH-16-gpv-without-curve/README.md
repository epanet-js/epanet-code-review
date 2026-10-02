# CRASH-16: A GPV with no head loss curve crashes the solver

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | `EN_solveH` (or `EN_runH`) reads the unused curve slot `Curve[0]`, whose data pointers were never set: a crash, or head losses computed from whatever that memory holds. |
| **Reached from** | Toolkit: `EN_addlink(EN_GPV)` or `EN_setlinktype(..., EN_GPV, ...)` without a following `EN_setlinkvalue(EN_GPV_CURVE)`, or `EN_deletecurve` on a GPV's own curve. Input file: a `[VALVES]` GPV line with no curve field. |
| **Where** | `validateproject()` in [`src/validate.c:330-347`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/validate.c#L330-L347) has no valve check; the bad read is in `curvecoeff()`, [`src/hydcoeffs.c:930-944`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L930-L944), called from `gpvcoeff()` |
| **Since** | Toolkit path: v2.2, when `EN_addlink` and `EN_setlinktype` arrived. File path: v2.3, when [PR #758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) relaxed the `[VALVES]` token count from 6 to 5. |
| **GitHub** | No related issue |
| **Fix** | A new 30-line check function: [`CRASH-16_fix.patch`](CRASH-16_fix.patch) |

## The problem

A general purpose valve (GPV) has no setting, only a head loss curve. A GPV created through the toolkit starts with curve index 0, meaning "no curve", until `EN_setlinkvalue(EN_GPV_CURVE)` assigns one. If the program solves the network first, EPANET does not stop it. The hydraulic solver looks up curve 0, which is not a curve: the curve array is 1-based and slot 0 is never initialised.

Under the sanitizers the run stops in `curvecoeff()` on the first trial. Without them the result depends on the memory in that slot, so it can crash or produce numbers.

The same happens with an input file whose GPV line has no curve field (`V1 J1 J2 12 GPV`), which the parser accepts. `EN_deletecurve` on the curve a GPV uses sets the GPV's curve index to 0 too. The solver then uses a stale copy of the index ([SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/)): if the deleted curve was the last one, that copy points at the vacated slot, whose arrays are now NULL, and the run stops with a null-pointer read in `curvecoeff()` (seen under UBSan); otherwise the GPV silently runs on another curve.

## Why it happens

`validateproject()`, run by `EN_openH`, checks tanks, pumps, patterns and curves, but not valves:

```c
// src/validate.c, validateproject()
    if (!validatetanks(pr)) errcode = 110;
    if (!validatepumps(pr)) errcode = 110;
    if (!validatepatterns(pr)) errcode = 110;
    if (!validatecurves(pr)) errcode = 110;
    return errcode;
```

`gpvcoeff()` passes the curve index straight to `curvecoeff()`, which dereferences the curve's point arrays:

```c
// src/hydcoeffs.c, curvecoeff()
    curve = &pr->network.Curve[i];     // i is 0 for a GPV without a curve
    x = curve->X;                      // x = flow
    y = curve->Y;                      // y = head
    npts = curve->Npts;
    ...
    *r = (y[k2] - y[k1]) / (x[k2] - x[k1]);
```

`addcurve()` in `input2.c` and `EN_addcurve` grow the curve array with `realloc` and initialise only the new entry, so the `X`, `Y` and `Npts` of `Curve[0]` hold whatever the allocator left there.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-16_network.inp`](CRASH-16_network.inp) | Reservoir R1 feeds J1; J2 (100 gpm) has no supply. Curve C1 exists but is not used. |
| [`CRASH-16_gpv-line-without-curve.inp`](CRASH-16_gpv-line-without-curve.inp) | The same network with `V1 J1 J2 12 GPV` and no curve field |
| [`CRASH-16_test.c`](CRASH-16_test.c) | Part 1 adds a GPV from J1 to J2 with `EN_addlink` and solves. Part 2 opens the second file and solves. |
| [`CRASH-16_fix.patch`](CRASH-16_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-16            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-16 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-16_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-16_test
./CRASH-16_test
```

The file path alone also shows it: `runepanet CRASH-16_gpv-line-without-curve.inp CRASH-16.rpt`.

**Without the fix**, the sanitizer build stops in Part 1. The address it reports, `0xbebe...`, is AddressSanitizer's fill pattern for uninitialised heap memory:

```
.../src/hydcoeffs.c:943:11: runtime error: load of misaligned address 0xbebebebebebebec6 for type 'double', which requires 8 byte alignment
    #0 ... in curvecoeff hydcoeffs.c:943
    #1 ... in headlosscoeffs hydcoeffs.c:275
    #2 ... in hydsolve hydsolver.c:121
    #3 ... in runhyd hydraul.c:220
...
Part 1: GPV added with EN_addlink
  EN_GPV_CURVE of the new GPV = 0 (no curve)
```

**With the fix**, both paths stop at `EN_openH` with error 110:

```
Part 1: GPV added with EN_addlink
  EN_GPV_CURVE of the new GPV = 0 (no curve)
  EN_solveH returned 110
Part 2: GPV line without a curve in the input file
  EN_GPV_CURVE of V1 = 0 (no curve)
  EN_solveH returned 110
PASS: a GPV without a curve is reported as an error (110) and the solver does not run
```

and the report says why:

```
  Error 206: undefined curve for GPV V1
  Error 110: cannot solve network hydraulic equations
```

## The fix

Add `validatevalves()` to `validate.c` and call it from `validateproject()`, next to the checks for pumps and curves. It reports any GPV whose curve index is not a valid curve:

```c
    for (i = 1; i <= net->Nvalves; i++)
    {
        link = &net->Link[net->Valve[i].Link];
        if (link->Type != GPV) continue;

        // Curve index is stored in the valve's Kc property
        c = ROUND(link->Kc);
        if (c < 1 || c > net->Ncurves)
        {
            sprintf(pr->Msg, "Error 206: %s for GPV %s",
                geterrmsg(206, errmsg), link->ID);
            writeline(pr, pr->Msg);
            result = 0;
        }
    }
```

This follows the pattern of `validatepumps()`, which reports a pump with no curve as error 226 and makes `EN_openH` return 110. Error 206 ("undefined curve") is reused; there is no GPV-specific code. Checking at `EN_openH` covers every path (the toolkit, the parser and `EN_deletecurve`) without forcing a program to assign the curve in the same call that creates the valve.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The other GPV curve issues build on this check. [CRASH-06](../CRASH-06-gpv-one-point-curve/) adds a test for one-point curves to `validatevalves()`, and [SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) makes the solver read the curve index from `Kc`, the value checked here. Both name this patch in a `Requires:` line.
- A PCV without a curve does not crash: `pcvlosscoeff()` treats curve 0 as a linear valve characteristic.
- [FILE-04](../../4-files/FILE-04-pumps-gpvs-without-curve-not-reloadable/) covers how `EN_saveinpfile` writes a GPV without a curve.
- While checking the file path, a related parser bug turned up: since PR [#758](https://github.com/OpenWaterAnalytics/EPANET/pull/758), `[PIPES]`, `[PUMPS]` and `[VALVES]` lines with too few fields are dropped without error 201. That is [FILE-16](../../4-files/FILE-16-short-link-lines-dropped/).
