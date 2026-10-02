# FILE-13: TRIALS below 1 gives zero solver iterations

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | The hydraulic solver runs no trials and reports its initial guess as the solution. In the test, J1's pressure is -43.33 psi instead of 25.51 psi. The convergence error is then read without having been set. |
| **Reached from** | Input file: `TRIALS` in [OPTIONS] with a value above 0 and below 1 |
| **Where** | `optionvalue()` in [`src/input3.c:2167`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2167); `relerr` in `runhyd()`, [`src/hydraul.c:205`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L205) |
| **Since** | EPANET 2.00.12 (2008) or earlier, so every release |
| **GitHub** | No related issue |
| **Fix** | Five lines: [`FILE-13_fix.patch`](FILE-13_fix.patch) |

## The problem

`TRIALS` is the maximum number of solver iterations per time step. The parser accepts any value above 0 and stores it as an integer, so `TRIALS 0.5` becomes 0. The solver then runs no trials at all. It keeps its initial guess (here a head of 0 ft at J1) and returns it as the result, with no error.

In the test network a reservoir at 200 ft feeds a junction at 100 ft. With `TRIALS 0.5`, J1's pressure is -43.33 psi, and the report's flow balance shows 0 gpm of inflow for 500 gpm of demand. With 200 trials it is 25.51 psi. In this run the status report said `Balanced after 1 trials`, followed by `WARNING: Maximum trials exceeded at 0:00:00 hrs. System may be unstable.` (which of these appears depends on an unset value, see below).

`EN_setoption(EN_TRIALS, 0.5)` returns error 213 ([`epanet.c:1296`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1296)), so only input files are affected.

## Why it happens

```c
// src/input3.c, optionvalue()
// All other options must be > 0
if (y <= 0.0) return setError(parser, nvalue, 213);
...
else if (match(tok0, w_TRIALS))   hyd->MaxIter = (int)y;      // 0.5 -> 0
```

With `MaxIter = 0` and the default `UNBALANCED STOP`, the trial loop in `hydsolve()` never runs. The loop is the only place that sets the convergence error `*relerr`, and `runhyd()` reads it afterwards:

```c
// src/hydsolver.c, hydsolve()
maxtrials = hyd->MaxIter;
if (hyd->ExtraIter > 0) maxtrials += hyd->ExtraIter;
*iter = 1;
while (*iter <= maxtrials)
{
    ...
    *relerr = newerr;
    ...
}
...
hyd->RelativeError = *relerr;

// src/hydraul.c, runhyd()
double relerr;       // Solution accuracy
...
errcode = hydsolve(pr,&iter,&relerr);
if (!errcode)
{
    if (rpt->Statflag) writehydstat(pr,iter,relerr);
    if (relerr > hyd->Hacc && hyd->ExtraIter == -1)
    {
        hyd->Haltflag = 1;
    }
```

So whether the run is reported as balanced, unbalanced or halted depends on whatever was on the stack. `hydsolve()` also copies the unset value into `hyd->RelativeError` (returned by `EN_getstatistic(EN_RELATIVEERROR)`) when the first trial fails with an ill-conditioned matrix ([`hydsolver.c:127-131`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydsolver.c#L127-L131)). Both uses of the unset value were found by reading the code; AddressSanitizer and UBSan do not detect uninitialised reads.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-13_trials-0.5.inp`](FILE-13_trials-0.5.inp) | Reservoir R1 (200 ft) feeds junction J1 (100 ft, 500 gpm) through one pipe. [OPTIONS] has `Trials 0.5`. |
| [`FILE-13_test.c`](FILE-13_test.c) | Opens the file. If it loads, solves it as it is and again with `EN_TRIALS` set to 200, and compares J1's pressure. |
| [`FILE-13_fix.patch`](FILE-13_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-13            # unpatched EPANET: FAIL
tools/run-test.sh FILE-13 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-13_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-13_test
./FILE-13_test
```

**Without the fix**:

```
EN_open returned 0 (expected 200)
TRIALS stored as 0
J1 pressure with TRIALS 0.5:   -43.33 psi
J1 pressure with TRIALS 200:    25.51 psi
FAIL: TRIALS 0.5 was accepted and the solver ran no trials
```

**With the fix**, the report has `Error 213: invalid option value 0.5 in [OPTIONS] section:`:

```
EN_open returned 200 (expected 200)
PASS: TRIALS below 1 is rejected
```

## The fix

Reject `TRIALS` below 1, as `EN_setoption` does, and give `relerr` a defined value:

```diff
-    else if (match(tok0, w_TRIALS))   hyd->MaxIter = (int)y;
+    else if (match(tok0, w_TRIALS))
+    {
+        if (y < 1.0) return setError(parser, nvalue, 213);
+        hyd->MaxIter = (int)y;
+    }
 ...
-    double relerr;       // Solution accuracy
+    double relerr = 0.0; // Solution accuracy
```

With `TRIALS` at least 1, the loop always runs at least once, so the initialisation only matters for the ill-conditioned first trial (where `hyd->RelativeError` now reads 0 instead of an unset value) and for any future path to zero trials. It could equally go at the top of `hydsolve()`, next to the other convergence statistics it sets to 0.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `TRIALS` values too large for an `int`, and `TRIALS nan`, are covered by [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/). Its check sits a few lines above this one; the two patches apply together in either order.
- `PRESSURE EXPONENT 0`, the other half of the earlier item P16, is [SIM-15](../../1-wrong-results/SIM-15-pressure-exponent-zero/).
