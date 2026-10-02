# CRASH-30: Time and option setters convert out-of-range values to integers with undefined behaviour

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Undefined behaviour inside the setter. In a plain build, NaN or a huge value can be stored instead of refused: `EN_MIXMODEL` NaN becomes mixing model 0, and `EN_TRIALS` 1e10 becomes 2,147,483,647 trials. That value plus `UNBALANCED CONTINUE` overflows in the solver, which then runs no trials and reports pressures of -307.64 psi instead of 127.54 psi. |
| **Reached from** | Toolkit: `EN_setoption`, `EN_setnodevalue` and `EN_setlinkvalue` for integer-valued codes, given NaN or a value outside the `int` range; `EN_settimeparam` with a very large time. Input file: `TRIALS` near 2,147,483,647 with `UNBALANCED CONTINUE`. |
| **Where** | `EN_setoption()` in [`src/epanet.c:1283`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1283), [`1297`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1297) and [`1379-1383`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1379-L1383); `ROUND()` in [`src/types.h:108`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/types.h#L108); `EN_settimeparam()` in [`src/epanet.c:1656`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1656); `hydsolve()` in [`src/hydsolver.c:112`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydsolver.c#L112) |
| **Since** | `EN_TRIALS`, the `ROUND()` conversions and the `maxtrials` sum: EPANET 2.00.12 (2008) or earlier. `EN_UNBALANCED`, `EN_CHECKFREQ` and `EN_MAXCHECK` were added in v2.2 for issue [#425](https://github.com/OpenWaterAnalytics/EPANET/issues/425) (2019). |
| **GitHub** | No related issue. Open pull request [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) would run the tests under UBSan, which flags these conversions. |
| **Fix** | A saturating `ROUND()`, two range checks and a saturating sum: [`CRASH-30_fix.patch`](CRASH-30_fix.patch) |

## The problem

[CRASH-04](../CRASH-04-inf-nan-time-values/) covers out-of-range values in an input file. The toolkit has the same problem, in more places. The review log listed it as the "F5 addendum", and each part below was confirmed here under UndefinedBehaviorSanitizer.

- **Integer-valued options and properties.** Converting NaN, or a double outside the `int` range, to `int` is undefined behaviour in C. The setters convert first and range-check afterwards. UBSan reports this for `EN_setoption` with `EN_TRIALS`, `EN_UNBALANCED`, `EN_CHECKFREQ`, `EN_MAXCHECK`, `EN_HEADLOSSFORM`, `EN_GLOBALPATTERN`, `EN_DEMANDPATTERN`, `EN_PRESS_UNITS` and `EN_STATUS_REPORT`. It also reports it for `EN_setnodevalue` with `EN_PATTERN`, `EN_SOURCEPAT`, `EN_SOURCETYPE`, `EN_VOLCURVE` and `EN_MIXMODEL`, and for `EN_setlinkvalue` with `EN_INITSTATUS`, `EN_LINKPATTERN`, `EN_PUMP_HCURVE`, `EN_PUMP_ECURVE`, `EN_PUMP_EPAT`, `EN_GPV_CURVE`, `EN_PCV_CURVE` and `EN_VALVE_TYPE`.
- **The result depends on the processor.** A plain build on Apple arm64 saturates, and turns NaN into 0. Most calls are then still refused by the range check that follows, but four are accepted: `EN_TRIALS` 1e10 (2,147,483,647 trials), `EN_CHECKFREQ` NaN (0, so periodic status checks stop, see [FILE-17](../../4-files/FILE-17-options-load-but-not-reload/)), `EN_UNBALANCED` 1e300, and `EN_MIXMODEL` NaN (model 0, complete mixing). On x86-64 an out-of-range conversion gives `INT_MIN`, so `EN_TRIALS` 1e10 would leave the solver with no trials (not run here).
- **TRIALS near the `int` limit.** `TRIALS` 2,147,483,647 is a valid `int`. With `UNBALANCED CONTINUE 10`, from the API or an input file, `hydsolve()` adds the two and overflows a signed `int`. In a plain build the sum wraps to a negative number, the solver runs no trials, and the results are its initial guess: -307.64 psi instead of 127.54 psi at Net1's junction 10.
- **Times.** `EN_settimeparam` takes a `long` and checks only that it is not negative. `EN_PATTERNSTART` = `LONG_MAX` is accepted, and the first `EN_runH` overflows `n * time->Pstep` in `timestep()` ([`hydraul.c:668`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L668)). Of the other time parameters set to `LONG_MAX`, none failed in a 50-step run.

## Why it happens

```c
// src/types.h
#define ROUND(x) (((x)>=0) ? (int)((x)+.5) : (int)((x)-.5))

// src/epanet.c, EN_setoption()
if (option == EN_UNBALANCED)
{
    hyd->ExtraIter = (int)value;
...
case EN_TRIALS:
    if (value < 1.0) return 213;
    hyd->MaxIter = (int)value;                  // no upper limit
...
case EN_DEMANDPATTERN:
    pat = ROUND(value);                         // converted first...
    if (pat < 0 || pat > net->Npats) return 205;    // ...checked afterwards

// src/hydsolver.c, hydsolve()
maxtrials = hyd->MaxIter;
if (hyd->ExtraIter > 0) maxtrials += hyd->ExtraIter;
```

The checks after the conversion are fine for in-range values. They cannot help once the conversion itself is undefined.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-30_Net1.inp`](CRASH-30_Net1.inp) | EPANET's Net1 example, unchanged |
| [`CRASH-30_trials-int-max.inp`](CRASH-30_trials-int-max.inp) | Reservoir R1 feeds junction J1. [OPTIONS] has `Trials 2147483647` and `Unbalanced Continue 10`. |
| [`CRASH-30_test.c`](CRASH-30_test.c) | Part 1 calls nine setters with out-of-range values and checks that each is refused. Part 2 solves with TRIALS at `INT_MAX`, through the API and from the file, and compares the pressure with a solve at 200 trials. |
| [`CRASH-30_fix.patch`](CRASH-30_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-30            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-30 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-30_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o CRASH-30_test
./CRASH-30_test
```

**Without the fix**, UndefinedBehaviorSanitizer stops at the first call:

```
Part 1: out-of-range values, each should be refused
.../src/epanet.c:1297:24: runtime error: 1e+10 is outside the range of representable values of type 'int'
    #0 ... in EN_setoption epanet.c:1297
    #1 ... in main CRASH-30_test.c:73
```

A plain Release build (Apple arm64, no sanitizers) runs to the end:

```
Part 1: out-of-range values, each should be refused
  EN_setoption(EN_TRIALS, 1e10)                      returned   0   <-- accepted
  EN_setoption(EN_CHECKFREQ, NAN)                    returned   0   <-- accepted
  EN_setoption(EN_UNBALANCED, 1e300)                 returned   0   <-- accepted
  EN_setoption(EN_DEMANDPATTERN, 1e300)              returned 205
  EN_setnodevalue(junction 10, EN_PATTERN, 1e300)    returned 205
  EN_setnodevalue(tank 2, EN_MIXMODEL, NAN)          returned   0   <-- accepted
  EN_setlinkvalue(pump 9, EN_PUMP_HCURVE, 1e300)     returned 206
  EN_setlinkvalue(pipe 10, EN_INITSTATUS, 1e300)     returned 211
  EN_settimeparam(EN_PATTERNSTART, LONG_MAX)         returned   0   <-- accepted
Part 2: TRIALS = INT_MAX with UNBALANCED CONTINUE 10
  set through EN_setoption (Net1):                   10 pressure -307.64 psi (127.54 with 200 trials)   <-- wrong
  read from CRASH-30_trials-int-max.inp:             J1 pressure -43.33 psi (25.51 with 200 trials)   <-- wrong
FAIL: 7 out-of-range values were accepted or gave a wrong result
```

**With the fix**:

```
Part 1: out-of-range values, each should be refused
  EN_setoption(EN_TRIALS, 1e10)                      returned 213
  EN_setoption(EN_CHECKFREQ, NAN)                    returned 213
  EN_setoption(EN_UNBALANCED, 1e300)                 returned 213
  EN_setoption(EN_DEMANDPATTERN, 1e300)              returned 205
  EN_setnodevalue(junction 10, EN_PATTERN, 1e300)    returned 205
  EN_setnodevalue(tank 2, EN_MIXMODEL, NAN)          returned 251
  EN_setlinkvalue(pump 9, EN_PUMP_HCURVE, 1e300)     returned 206
  EN_setlinkvalue(pipe 10, EN_INITSTATUS, 1e300)     returned 211
  EN_settimeparam(EN_PATTERNSTART, LONG_MAX)         returned 213
Part 2: TRIALS = INT_MAX with UNBALANCED CONTINUE 10
  set through EN_setoption (Net1):                   10 pressure 127.54 psi (127.54 with 200 trials)
  read from CRASH-30_trials-int-max.inp:             J1 pressure 25.51 psi (25.51 with 200 trials)
PASS: out-of-range values are refused; TRIALS = INT_MAX solves normally
```

## The fix

**`ROUND()` saturates.** There are 18 `ROUND(value)` conversions in the setters (two of them in lines that [API-14](../../3-api/API-14-initstatus-char-cast/) and [SIM-21](../../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) change). Rather than add a check at each, the macro clamps its result to the `int` range and maps NaN to `INT_MIN`. Every caller already range-checks the result, and an index or code of `INT_MAX` or `INT_MIN` is always out of range, so these calls now return their usual error. In-range values give the same result as before. `types.h` gets `#include <limits.h>`.

```diff
-#define ROUND(x) (((x)>=0) ? (int)((x)+.5) : (int)((x)-.5))
-                                              // round-off of x
+#define ROUND(x) (((x)>=INT_MAX) ? INT_MAX : ((x)>=0) ? (int)((x)+.5) : \
+                  ((x)>INT_MIN) ? (int)((x)-.5) : INT_MIN)
+                                              // round-off of x (limited to
+                                              // INT_MIN..INT_MAX; NaN gives
+                                              // INT_MIN)
```

The macro is also used by the solver (`tanktimestep()`, `controltimestep()`, the GPV curve index in `hydcoeffs.c`) and by the input-file reader for `PRECISION`. For those it only changes the result where the old conversion was undefined.

**The four options stored with `(int)`** are checked once, at the top of `EN_setoption`, as [CRASH-04](../CRASH-04-inf-nan-time-values/) does in the parser:

```diff
+    // Options stored as integers must fit in an int (NaN fails this test)
+    if ((option == EN_TRIALS || option == EN_UNBALANCED ||
+         option == EN_CHECKFREQ || option == EN_MAXCHECK) &&
+        !(fabs(value) <= INT_MAX)) return 213;
```

**Times** set through `EN_settimeparam` are limited to half the range of a `long`, the same limit CRASH-04 sets for input files:

```diff
-    if (value < 0) return 213;
+
+    // Times must fit in half the range of a long, so that two of them
+    // can be added without overflow
+    if (value < 0 || value > LONG_MAX / 2) return 213;
```

**The trial count** saturates instead of overflowing:

```diff
-    if (hyd->ExtraIter > 0) maxtrials += hyd->ExtraIter;
+    if (hyd->ExtraIter > 0) maxtrials += MIN(hyd->ExtraIter, INT_MAX - maxtrials);
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- **Not in this patch.** `EN_addcontrol` and `EN_setcontrol` with a `TIMER` or `TIMEOFDAY` level of 1e300 also stop UBSan, at `t = (long)level` in `setcontrol()` ([`project.c:1380`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1380)). [API-22](../../3-api/API-22-timeofday-controls-not-wrapped/) and API-28 change that line, so the range check belongs with them. A rule time premise of 1e300 hours (`IF SYSTEM TIME > 1e300`) stops UBSan in `checktime()` ([`rules.c:1012`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1012)), next to the rule time conversion of [CRASH-29](../CRASH-29-huge-rule-time-to-text/).
- **NaN for other properties** is still accepted without an error, for example `EN_ELEVATION` NaN, because those properties are stored as doubles and their range checks are ordered comparisons. A NaN check at the top of the setters would cover them all.
- With [API-14](../../3-api/API-14-initstatus-char-cast/) applied, `EN_INITSTATUS` is range-checked as a double before the conversion. That check, `value < 0.0 || value > 1.0`, lets NaN through, which then becomes status 0 (closed).
