# API-28: Control times added through the API are truncated, not rounded

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A timer or clock-time control whose time is computed in floating point, for example 8.2 hours × 3600, can be stored a second early and act a second early. `EN_getcontrol` returns the wrong second, and the error is written to saved files. A time too large for a `long`, such as 1e300, or NaN is converted anyway, which is undefined behaviour, and accepted. |
| **Reached from** | Toolkit: `EN_addcontrol` or `EN_setcontrol` with `EN_TIMER` or `EN_TIMEOFDAY` and a time slightly below a whole second, or a time that is not finite or does not fit in a `long` |
| **Where** | `setcontrol()` in [`src/project.c:1380`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1380) |
| **Since** | The truncation since v2.3: commit [a9079b0](https://github.com/OpenWaterAnalytics/EPANET/commit/a9079b023b9f256055195363a8d9bbcb3bfff964) (pull request [#732](https://github.com/OpenWaterAnalytics/EPANET/pull/732), 2023) replaced `(long)ROUND(lvl)` with `(long)level`, and v2.2 rounds correctly. The out-of-range conversion is older: `ROUND()` converted to `int` without a range check before that commit too. |
| **GitHub** | No issue about the rounding. The new `setcontrol()` came from the work on [#730](https://github.com/OpenWaterAnalytics/EPANET/issues/730) (fixed status for valves in API controls); [#731](https://github.com/OpenWaterAnalytics/EPANET/issues/731) reported two other mistakes in it, which were fixed before it was merged |
| **Fix** | 3 lines and a comment, on top of [API-22](../API-22-timeofday-controls-not-wrapped/): [`API-28_fix.patch`](API-28_fix.patch) |

## The problem

A host program that keeps its schedule in hours converts to seconds before calling `EN_addcontrol`. Many ordinary times do not come out as whole numbers in double precision:

| Schedule | Computed as | Value passed | Stored by EPANET |
|---|---|---|---|
| Close pipe 111 after 8.2 hours | `8.2 * 3600` | 29519.999999999996 | 29519 (8:11:59) |
| Close pipe 122 at 4:20 AM | `(4 + 20 / 60.0) * 3600` | 15599.999999999998 | 15599 (4:19:59) |

Both controls act a second early, the status report says `8:11:59: Pipe 111 changed by timer control`, and the solver stops at 8:11:59 instead of 8:12:00. A program that looks for the 8:12:00 solution does not find one, and `EN_getcontrol` returns a time one second short of what was set.

This is not rare. Of the 1,440 minutes in a day, 50 come out slightly below the whole second when computed as `(hours + minutes / 60.0) * 3600`, and so do 7 of the 240 tenths of an hour from 0.0 to 23.9 (4.1, 8.2, 8.7, 16.4, 16.9, 17.4 and 17.9) when computed as `hours * 3600`.

The error grows when the project is saved: `EN_saveinpfile` writes the timer control as `AT TIME 8.1997 HOURS`, and the saved file reads back as 29518 s, another second lost ([FILE-05](../../4-files/FILE-05-timer-controls-lose-a-second/)).

The same conversion has no range check. A time of 1e300 s, or NaN, does not fit in a `long`, so converting it is undefined behaviour; UndefinedBehaviorSanitizer stops the program there. In a plain build `EN_addcontrol` accepts both and returns 0.

## Why it happens

```c
// src/project.c, setcontrol()
    else if (type == TIMER || type == TIMEOFDAY)
    {
        t = (long)level;
        if (t < 0) return 202;
    }
```

The cast truncates toward zero, and it is undefined for a value outside the range of a `long` (1e300, infinity or NaN); the check `t < 0` comes too late to catch those. Before the API code was moved into `setcontrol()`, `EN_addcontrol` and `EN_setcontrol` rounded:

```c
// src/epanet.c, EN_addcontrol() before commit a9079b0
    if (type == TIMER) t = (long)ROUND(lvl);
    if (type == TIMEOFDAY) t = (long)ROUND(lvl) % SECperDAY;
```

The same commit also dropped the `% SECperDAY`, which is [API-22](../API-22-timeofday-controls-not-wrapped/).

The input-file parser truncates in the same way (`(long)(3600.0 * time)` in `controldata()`), so `LINK 111 CLOSED AT TIME 8.2` in a file also gives 29519 s. That is part of [FILE-05](../../4-files/FILE-05-timer-controls-lose-a-second/), whose patch rounds there.

## How to reproduce

| File | What it is |
|---|---|
| [`API-28_Net1.inp`](API-28_Net1.inp) | EPANET's example network Net1, unchanged |
| [`API-28_test.c`](API-28_test.c) | Adds the two controls in the table above with `EN_addcontrol`, reads their times back with `EN_getcontrol`, and runs the hydraulics to find when each pipe closes. Then tries to add timer controls at 1e300 s and at NaN |
| [`API-28_fix.patch`](API-28_fix.patch) | The fix (apply [`API-22_fix.patch`](../API-22-timeofday-controls-not-wrapped/API-22_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-28            # unpatched EPANET: CRASH
tools/run-test.sh API-28 --patched  # with API-22 and this fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-28_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-28_test
./API-28_test
```

**Without the fix**, the first two controls are stored a second early, and the 1e300 s control stops the sanitizer build:

```
Control    Time passed in (s)   Stored   Pipe closes  Expected
TIMER      29519.999999999996    29519    8:11:59      8:12:00
TIMEOFDAY  15599.999999999998    15599    4:19:59      4:20:00
.../src/project.c:1380:13: runtime error: 1e+300 is outside the range of representable values of type 'long'
    #0 ... in setcontrol project.c:1380
    #1 ... in EN_addcontrol epanet.c:5413
    #2 ... in main API-28_test.c:83
```

Against a plain build of EPANET (macOS arm64) the test runs to the end:

```
EN_addcontrol with a time of 1e300 s returns 0 (expected 202)
EN_addcontrol with a time of NaN returns 0 (expected 202)
FAIL: 2 of 2 controls act one second early, and 2 of 2 impossible times were accepted
```

**With the fix:**

```
Control    Time passed in (s)   Stored   Pipe closes  Expected
TIMER      29519.999999999996    29520    8:12:00      8:12:00
TIMEOFDAY  15599.999999999998    15600    4:20:00      4:20:00
EN_addcontrol with a time of 1e300 s returns 202 (expected 202)
EN_addcontrol with a time of NaN returns 202 (expected 202)
PASS: both control times are rounded to the second meant, and impossible times are refused
```

The test also passes against EPANET v2.2, which still rounded.

## The fix

Check the range first, then round to the nearest second:

```diff
     else if (type == TIMER || type == TIMEOFDAY)
     {
-        t = (long)level;
-        if (t < 0) return 202;
+        // Time in seconds must be a number from 0 up to half the range
+        // of a long, so that times can be added without overflow (NaN
+        // fails this test)
+        if (!(level >= 0.0 && level < LONG_MAX / 2.0)) return 202;
+        t = (long)(level + 0.5);
         if (type == TIMEOFDAY) t %= SECperDAY;
     }
```

and `#include <limits.h>` at the top of `project.c`. The limit, half the range of a `long`, is the one [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/) uses for times in input files and [CRASH-30](../../2-crashes/CRASH-30-api-out-of-range-conversions/) for `EN_settimeparam`. Checking `level` rather than `t` also means rounding cannot turn a time such as -0.7 s into an accepted 0. A negative time between -1 and 0 s used to be truncated to 0 and accepted; it now returns error 202, as it did before commit a9079b0.

The patch sits on top of API-22, which adds the `% SECperDAY` line, and has a `Requires: API-22` line in its header, because the two patches change neighbouring lines. It uses `(long)(level + 0.5)` instead of EPANET's `ROUND()` macro, which converts to `int`.

EPANET's own unit tests (6 suites) pass with the patch. With it, the saved file says `AT TIME 8.2000 HOURS`, but the parser still reads that back as 29519 s; with FILE-05's patch as well it reads back as 29520 s (checked with all current patches applied).

## Notes

- Found while writing up API-22, by comparing `setcontrol()` with the code it replaced; confirmed by the test. The out-of-range case was found alongside [CRASH-30](../../2-crashes/CRASH-30-api-out-of-range-conversions/), which fixes the other conversions of this kind in the API; it is fixed here because this patch changes the same line. It is older than the truncation: before commit a9079b0, `ROUND()` converted the time to an `int`, which is undefined for 1e300 too.
