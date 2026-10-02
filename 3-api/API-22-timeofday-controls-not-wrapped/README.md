# API-22: API TIMEOFDAY controls are not reduced modulo 24 h

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A time-of-day control added or changed through the toolkit with a clock time of 24 h or more never fires, with no error. The same control read from an input file works. |
| **Reached from** | Toolkit: `EN_addcontrol` or `EN_setcontrol` with `EN_TIMEOFDAY` and a time of 86400 s or more (for example 24:00 for midnight) |
| **Where** | `setcontrol()` in [`src/project.c:1380`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1380), against the parser in [`src/input3.c:1043`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1043) |
| **Since** | v2.3. Commit [a9079b0](https://github.com/OpenWaterAnalytics/EPANET/commit/a9079b023b9f256055195363a8d9bbcb3bfff964) (pull request [#732](https://github.com/OpenWaterAnalytics/EPANET/pull/732), 2023) moved the API code into `setcontrol()` and dropped the modulo that `ENsetcontrol` had had since 2.00.12. |
| **GitHub** | No related issue |
| **Fix** | One line: [`API-22_fix.patch`](API-22_fix.patch) |

## The problem

A toolkit program adds a control to close pipe 111 at midnight, giving the clock time as 24:00 (86400 s), with the run starting at 8:00 AM. The pipe should close 16 hours into the run. It never closes, and `EN_addcontrol` returns 0.

If the project is saved with `EN_saveinpfile`, the file says `AT CLOCKTIME 24:00:00`. Read back by the parser, that control fires at midnight as intended, so the saved file and the project it came from behave differently.

## Why it happens

The parser reduces a clock time modulo one day:

```c
// src/input3.c, controldata()
    control->Time = (long)(3600.0 * time);
    if (ctltype == TIMEOFDAY) control->Time %= SECperDAY;
```

`setcontrol()`, which `EN_addcontrol` and `EN_setcontrol` use, stores the time as given:

```c
// src/project.c, setcontrol()
    else if (type == TIMER || type == TIMEOFDAY)
    {
        t = (long)level;
        if (t < 0) return 202;
    }
```

`controls()` fires the control when the time of day, `(Htime + Tstart) % SECperDAY`, equals the control time. The time of day is always below 86400, so the control never fires.

## How to reproduce

| File | What it is |
|---|---|
| [`API-22_Net1.inp`](API-22_Net1.inp) | EPANET's example network Net1, unchanged |
| [`API-22_test.c`](API-22_test.c) | Starts Net1 at 8:00 AM, adds a TIMEOFDAY control that closes pipe 111 at 24:00, and finds when the pipe closes. Then saves the project, opens the saved file and does the same |
| [`API-22_fix.patch`](API-22_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-22            # unpatched EPANET: FAIL
tools/run-test.sh API-22 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-22_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-22_test
./API-22_test
```

**Without the fix:**

```
Control added with EN_addcontrol:  pipe 111 never closes
Same control from the saved file:  pipe 111 closes at 16:00 into the run
FAIL: the TIMEOFDAY control for 24:00 added through the API never fires
```

**With the fix:**

```
Control added with EN_addcontrol:  pipe 111 closes at 16:00 into the run
Same control from the saved file:  pipe 111 closes at 16:00 into the run
PASS: the TIMEOFDAY control for 24:00 fires at midnight
```

With the fix, the saved file says `AT CLOCKTIME 0:00:00`.

## The fix

Reduce the time as the parser does:

```diff
         t = (long)level;
         if (t < 0) return 202;
+        if (type == TIMEOFDAY) t %= SECperDAY;
```

`EN_getcontrol` then returns 0 for this control, as it does for the same control read from a file. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The review found this by reading the code; the test confirms it.
- The same commit also changed the conversion from rounding (`(long)ROUND(lvl)`) to truncation (`(long)level`), so a time such as `8.2 * 3600` (29519.999999999996) is stored a second early. That is [API-28](../API-28-control-times-truncated/), whose patch goes on top of this one.
- [FILE-05](../../4-files/FILE-05-timer-controls-lose-a-second/) is another control-time problem, on the file side: timer controls lose a second on each save and reload.
