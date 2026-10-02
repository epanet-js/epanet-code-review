# FILE-18: Clock times in rule premises are truncated and lose a second

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Wrong results with no warning. A rule on `SYSTEM TIME` or `SYSTEM CLOCKTIME` can fire one rule time step early, and a saved file loses the second for good. 10,545 of the 86,400 clock times in a day are affected. |
| **Reached from** | Input file: `[RULES]` premises on `SYSTEM TIME` or `SYSTEM CLOCKTIME` written as `h:mm:ss`. Toolkit API: the same premise in `EN_addrule`. Then `EN_saveinpfile`. |
| **Where** | `newpremise()` in [`src/rules.c:824-825`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L824-L825); the truncations are in `checktime()`, [`src/rules.c:1012`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1012), and `gettimetxt()`, [`src/rules.c:1496-1497`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1496-L1497) |
| **Since** | The first commit of this repository (2014) for reading and checking rules; v2.2 for saving them |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`FILE-18_fix.patch`](FILE-18_fix.patch) |

## The problem

A rule premise such as `IF SYSTEM TIME >= 8:00:01` is stored as 28,800.999999999996 s, not 28,801 s, because `hour()` builds the time from hours, minutes and seconds in floating point. Every user of the value then truncates it:

- **The rule check** compares `(long)value`, 28,800 s, so the rule is true at 8:00:00. Rules are checked at the end of every rule time step (360 s here), so in the test the rule fires at 8:00 instead of 8:06.
- **The saved file** gets `8:00:00`, because `gettimetxt()` truncates the seconds too. The second is lost for good. For 3,295 of the affected times the shorter time is affected again, so the next open and save loses another second.

The same happens with `CLOCKTIME`. 10,545 of the 86,400 whole-second times in a day convert to slightly less than the whole second. The [CONTROLS] reader had the same flaw ([FILE-05](../FILE-05-timer-controls-lose-a-second/)); the [TIMES] reader already rounds.

## Why it happens

```c
// src/rules.c, newpremise()
    if (v == r_TIME || v == r_CLOCKTIME)
    {
        if (parser->Ntokens == 6) x = hour(Tok[4], Tok[5]) * 3600.;
        else                      x = hour(Tok[4], "") * 3600.;
        if (x < 0.0) return 202;
    }

// src/rules.c, checktime()
    // Test premise's time
    x = (long)(p->value);
```

`hour()` returns `y[0] + y[1] / 60.0 + y[2] / 3600.0` hours. For 8:00:01 that is 8.000277..., which times 3600 is 28,800.999999999996 in double precision.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-18_rule-times.inp`](FILE-18_rule-times.inp) | Reservoir R1 feeds junction J1 through pipes P1, P2 and P3. Rule TIMED closes P1 at `SYSTEM TIME >= 8:00:01`; rule CLOCK closes P2 at `SYSTEM CLOCKTIME >= 9:00:01 AM`. Hydraulic step 1 h, rule step 6 min. |
| [`FILE-18_test.c`](FILE-18_test.c) | Prints the stored premise times, runs the hydraulics step by step to find when each pipe closes, then saves the project and prints the saved premises |
| [`FILE-18_fix.patch`](FILE-18_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-18            # unpatched EPANET: FAIL
tools/run-test.sh FILE-18 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-18_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-18_test
./FILE-18_test
```

**Without the fix**, both rules fire six minutes early and the saved times lose their second:

```
TIMED premise stored as 28800.999999999996 s (expected 28801)
CLOCK premise stored as 32400.999999999996 s (expected 32401)
P1 closes at 8:00:00 (expected 8:06:00)
P2 closes at 9:00:00 (expected 9:06:00)
  saved: IF   SYSTEM  TIME >= 8:00:00
  saved: IF   SYSTEM  CLOCKTIME >= 9:00:00
FAIL: the rule times lost a second and the rules fired early
```

**With the fix:**

```
TIMED premise stored as 28801 s (expected 28801)
CLOCK premise stored as 32401 s (expected 32401)
P1 closes at 8:06:00 (expected 8:06:00)
P2 closes at 9:06:00 (expected 9:06:00)
  saved: IF   SYSTEM  TIME >= 8:00:01
  saved: IF   SYSTEM  CLOCKTIME >= 9:00:01
PASS: rule times are kept to the second
```

## The fix

Round the parsed time to the nearest second, as `timedata()` does for [TIMES] and the FILE-05 fix does for [CONTROLS]:

```diff
         else                      x = hour(Tok[4], "") * 3600.;
         if (x < 0.0) return 202;
+        x = floor(x + 0.5);  // whole seconds, e.g. 8:00:01 is 28800.999...
     }
```

and `#include <math.h>` at the top of `rules.c` for `floor()`. Once the stored value is a whole number, the truncations in `checktime()` and `gettimetxt()` are exact, so they need no change. `floor()` is used rather than a cast to `long`, because the time can be any size (see [CRASH-29](../../2-crashes/CRASH-29-huge-rule-time-to-text/)).

The fix is in the reader, so it also corrects files saved by earlier versions, unless the second was already lost in the save. Times set with `EN_setpremisevalue` are stored as given, in seconds, and are not rounded.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Found while writing up [FILE-05](../FILE-05-timer-controls-lose-a-second/), which fixes the same truncation for simple controls in `controldata()`.
- [CRASH-29](../../2-crashes/CRASH-29-huge-rule-time-to-text/) changes `checktime()` to compare the stored `double` instead of `(long)value`, to remove an undefined conversion. By reading, that alone makes the rules in this test fire at 8:06 and 9:06, but the stored values and the saved file still lose the second, so this fix is still needed.
- [CRASH-26](../../2-crashes/CRASH-26-rule-status-on-node-variable/) adds a check a few lines below in `newpremise()`, and [FILE-03](../FILE-03-filltime-rules-not-reloadable/) changes the rule writer. The patches apply together.
