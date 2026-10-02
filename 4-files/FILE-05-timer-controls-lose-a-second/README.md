# FILE-05: Timer controls lose one second per save

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A time-based control can fire one second early after a save and reload, or two seconds early after two. Some clock times are one second early even in the original file. |
| **Reached from** | Input file: `[CONTROLS]` lines with `AT TIME` or `AT CLOCKTIME`, and every file saved by `EN_saveinpfile` |
| **Where** | `controldata()` in [`src/input3.c:1042`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1042); the writer is `saveinpfile()`, [`src/inpfile.c:514`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L514) |
| **Since** | v2.0.12 (2014), the first release in this repository |
| **GitHub** | No related issue |
| **Fix** | One line: [`FILE-05_fix.patch`](FILE-05_fix.patch) |

## The problem

`EN_saveinpfile` writes a timer control's time in decimal hours with four decimals, for example `AT TIME 0.3333 HOURS` for 1,200 s. When the file is read, the time is converted back to seconds and truncated: 0.3333 h is 1,199.88 s, which becomes 1,199 s. The control now fires a second earlier, which can move it to a different hydraulic time step.

This happens to 38,745 of the 86,400 whole-second times in a day (45 %). A time can lose another second on the next save: 22,801 s becomes 22,800 s and then 22,799 s.

The truncation also affects times written as `h:mm:ss`, which is how the original input file usually gives them and how `AT CLOCKTIME` controls are always saved. `0:01:01` is 60.99999999999999 s in floating point and is read as 60 s, so the original file is already one second off. This affects 10,545 of the 86,400 clock times in a day.

## Why it happens

```c
// src/input3.c, controldata()
control->Time = (long)(3600.0 * time);     // truncates
if (ctltype == TIMEOFDAY) control->Time %= SECperDAY;
```

`hour()` returns the time in hours as `y[0] + y[1] / 60.0 + y[2] / 3600.0`, which is rarely an exact binary number. The `[TIMES]` reader, `timedata()`, rounds the same value:

```c
// src/input3.c, timedata()
t = (long)(3600.0 * y + 0.5);
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-05_timer-controls.inp`](FILE-05_timer-controls.inp) | Two pipes with three `AT TIME` controls (1,200, 22,801 and 61 s) and one `AT CLOCKTIME` control (28,801 s) |
| [`FILE-05_test.c`](FILE-05_test.c) | Reads each control's time with `EN_getcontrol`, then saves and reopens the project twice and reads the times again |
| [`FILE-05_fix.patch`](FILE-05_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-05            # unpatched EPANET: FAIL
tools/run-test.sh FILE-05 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-05_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-05_test
./FILE-05_test
```

**Without the fix**:

```
Control                   Expected  Loaded   1st save  2nd save  (s)
AT TIME 0:20                  1200      1200      1199      1199
AT TIME 6:20:01              22801     22801     22800     22799
AT TIME 0:01:01                 61        60        60        60
AT CLOCKTIME 8:00:01 AM      28801     28800     28800     28800
FAIL: 10 of the 12 control times are off by one or two seconds
```

The first save writes `0.3333 HOURS`, `6.3336 HOURS`, `0.0167 HOURS` and `CLOCKTIME 8:00:00`.

**With the fix**:

```
Control                   Expected  Loaded   1st save  2nd save  (s)
AT TIME 0:20                  1200      1200      1200      1200
AT TIME 6:20:01              22801     22801     22801     22801
AT TIME 0:01:01                 61        61        61        61
AT CLOCKTIME 8:00:01 AM      28801     28801     28801     28801
PASS: every control keeps its time through loading and saving
```

## The fix

Round, as `timedata()` does:

```diff
-    control->Time = (long)(3600.0 * time);
+    control->Time = (long)(3600.0 * time + 0.5);
```

Four decimals of an hour are 0.36 s, so `%.4f HOURS` is never more than 0.18 s from the true time, and a rounded time always comes back to the second that was saved. The fix is in the reader, so it also repairs files saved by earlier versions, as long as they have not already lost the second. Writing timer controls as `h:mm:ss` would make saved files easier to read, but without this rounding those times would still be truncated.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

Rule premises on `SYSTEM TIME` and `SYSTEM CLOCKTIME` are truncated the same way in `rules.c`, which this patch does not change. That is [FILE-18](../FILE-18-rule-times-lose-a-second/), with its own fix in `newpremise()`.
