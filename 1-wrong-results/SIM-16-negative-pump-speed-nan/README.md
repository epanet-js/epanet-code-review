# SIM-16: A negative pump-speed pattern factor gives NaN for the rest of the run

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | One negative value in a pump's speed pattern turns the pump's flow and the heads around it into NaN, and they stay NaN after the factor is positive again. No error or warning is raised. |
| **Reached from** | Input files (`[PUMPS] ... PATTERN`) and `EN_setlinkvalue(EN_LINKPATTERN)` with a pattern that has a negative factor; also `EN_setpatternvalue` |
| **Where** | `demands()` in [`src/hydraul.c:545-555`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L545-L555), `setlinksetting()` in [`src/hydraul.c:454-465`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L454-L465) and `pumpcoeff()` in [`src/hydcoeffs.c:852`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L852) |
| **Since** | The first commit of this repository (2014), so every release |
| **GitHub** | No related issue |
| **Fix** | One line: [`SIM-16_fix.patch`](SIM-16_fix.patch) |

## The problem

A pump's speed can follow a time pattern. A speed of 0 closes the pump. A negative speed has no physical meaning, and EPANET rejects it everywhere else: in `[CONTROLS]`, in rule actions read from a file, in `EN_addcontrol` and in `EN_setlinkvalue(EN_SETTING)`. Patterns are not checked, because a pattern can also scale demands, where negative factors are valid.

In the test, pump PU1 has the speed pattern 1, −1, 1. In the second hour its flow and the head at J1 are NaN. In the third hour the speed is 1 again, and they are still NaN. The run continues, and nothing in the return codes says that anything is wrong.

## Why it happens

`demands()` passes the raw pattern factor to `setlinksetting()`. For a pump, that function stores the speed and changes the status only for exactly 0 or a positive speed on a closed pump, so a negative speed leaves the pump OPEN:

```c
// src/hydraul.c, setlinksetting()
    // For a pump, status is OPEN if speed > 0, CLOSED otherwise
    if (t == PUMP)
    {
        *k = value;
        if (value > 0 && *s <= CLOSED)
        {
            // Check if a re-opened pump needs its flow reset
            resetpumpflow(pr, index);
            *s = OPEN;
        }
        if (value == 0 && *s > CLOSED) *s = CLOSED;
    }
```

`pumpcoeff()` then scales the pump curve by the speed. For a 3-point curve, fitted as `h = H0 − R q^n` with a non-integer `n`, that takes a fractional power of a negative number:

```c
// src/hydcoeffs.c, pumpcoeff()
        r = pump->R * pow(setting, 2.0 - n);
```

The NaN reaches the pump's flow. When the factor goes back to 1, `setlinksetting()` sees a pump that is already OPEN, so it does not call `resetpumpflow()`. Nothing else re-initialises `LinkFlow`, so the NaN is carried into every later period.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-16_negative-speed.inp`](SIM-16_negative-speed.inp) | Pump PU1 (3-point curve) lifts 100 gpm from R1 to J1 with the speed pattern 1, −1, 1. Reservoir R2 (50 ft) supplies J1 through a check valve when the pump is off. |
| [`SIM-16_test.c`](SIM-16_test.c) | Runs the three hours and checks for NaN, for a closed pump in hour 1, and that hour 2 repeats hour 0 |
| [`SIM-16_fix.patch`](SIM-16_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-16            # unpatched EPANET: FAIL
tools/run-test.sh SIM-16 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-16_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-16_test
./SIM-16_test
```

**Without the fix:**

```
Hour   Speed   Pump flow (gpm)   Head at J1 (ft)
   0     1.0            100.00            150.00
   1    -1.0               nan               nan
   2     1.0               nan               nan
FAIL: 2 of 3 periods have NaN results, including the period after the speed is back to 1
```

**With the fix**, the pump is off in hour 1, R2 supplies J1, and the pump restarts in hour 2:

```
Hour   Speed   Pump flow (gpm)   Head at J1 (ft)
   0     1.0            100.00            150.00
   1     0.0              0.00             49.94
   2     1.0            100.00            150.00
PASS: no NaN; the pump is off at the negative speed and recovers afterwards
```

## The fix

Treat a negative speed as 0 where it is applied:

```diff
     // For a pump, status is OPEN if speed > 0, CLOSED otherwise
+    // (a negative speed, e.g. from a pattern, counts as 0)
     if (t == PUMP)
     {
+        if (value < 0.0) value = 0.0;
         *k = value;
```

The pump closes, its setting reads 0, and when the factor turns positive again the existing code reopens it and resets its flow with `resetpumpflow()`.

**Why not reject the pattern instead?** A load-time check is possible: `validatepumps()` could return an error at `EN_openH` when a pump's speed pattern has a negative factor. It would tell the user about the bad data, but it cannot cover every path. A pattern can be shared with demands, where negative factors are valid, and it can be changed with `EN_setpatternvalue` at any time, including while the solver is open. A guard where the speed is applied is needed anyway, and "0 or less means off" matches how EPANET already treats a speed of 0. The load-time check could be added on top as a warning.

The clamp also covers a negative pump setting given through `EN_setthenaction` or `EN_setelseaction`, which by reading the code are not range-checked either.

EPANET's own unit tests (6 suites) pass with the patch.
