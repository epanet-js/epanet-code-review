# SIM-10: PATTERN START that is not a multiple of PATTERN STEP misses pattern changes

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Every change of a time pattern (demands, reservoir heads, pump speeds, energy prices) takes effect up to one hydraulic step late. Tank levels drift: 1.26 ft in Net1 within 6 hours. |
| **Reached from** | Input file (`Pattern Start` in `[TIMES]`) or toolkit (`EN_PATTERNSTART`), when the pattern start is not a multiple of the pattern step and the hydraulic step does not land on the pattern boundaries by itself |
| **Where** | `timestep()` in [`src/hydraul.c:667-668`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L667-L668) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | One line: [`SIM-10_fix.patch`](SIM-10_fix.patch) |

## The problem

`Pattern Start` shifts all time patterns: with `Pattern Start 0:30` and `Pattern Timestep 2:00`, the second pattern period starts 1:30 into the run, the third at 3:30, and so on. `demands()` handles this correctly: it picks the pattern period from `Htime + Pstart`.

But the hydraulic driver does not take a time step that ends at 1:30. With Net1's 1-hour hydraulic step it solves at 1:00 and then at 2:00, so the new demand only applies from 2:00. Every pattern change in the run is half an hour late, and the tanks fill and drain with the wrong demands in between. In Net1 the tank level at 6:00 is 132.38 ft without the fix and 131.12 ft with it.

## Why it happens

`timestep()` finds the next pattern period `n` from `Htime + Pstart`, but then measures the time to it from `Htime` alone:

```c
// src/hydraul.c, timestep()
    // Revise time step based on time until next demand period
    // (n = next pattern period, t = time till next period)
    n = ((time->Htime + time->Pstart) / time->Pstep) + 1;
    t = n * time->Pstep - time->Htime;
    if (t > 0 && t < tstep) tstep = t;
```

`n * Pstep` is in pattern time, so `t` is too long by `Pstart`. The step ends on a multiple of `Pstep` in simulation time (2:00, 4:00, ...), not on a pattern boundary. The boundaries are only hit when the hydraulic step happens to land on them, for example with a 30-minute step in this network.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-10_pattern-start.inp`](SIM-10_pattern-start.inp) | Net1 with `Pattern Start 0:30` (pattern step 2:00, hydraulic step 1:00) |
| [`SIM-10_test.c`](SIM-10_test.c) | Steps through the hydraulics, records the time and the demand at junction 11 of every solution, and checks that a solution starts at each pattern boundary |
| [`SIM-10_fix.patch`](SIM-10_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-10            # unpatched EPANET: FAIL
tools/run-test.sh SIM-10 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-10_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-10_test
./SIM-10_test
```

**Without the fix:**

```
Pattern period   First solution     Junction 11 demand
starts at        at or after it     there (gpm)
    1:30              2:00            180.0   <-- late
    3:30              4:00            210.0   <-- late
    5:30              6:00            240.0   <-- late
    7:30              8:00            210.0   <-- late
...
FAIL: 12 of 12 demand pattern changes are applied late, at the next whole step
```

**With the fix:**

```
Pattern period   First solution     Junction 11 demand
starts at        at or after it     there (gpm)
    1:30              1:30            180.0   
    3:30              3:30            210.0   
    5:30              5:30            240.0   
    7:30              7:30            210.0   
...
PASS: all 12 demand pattern changes are applied when the pattern period starts
```

## The fix

Measure the time to the next period in pattern time, as `demands()` does:

```diff
     n = ((time->Htime + time->Pstart) / time->Pstep) + 1;
-    t = n * time->Pstep - time->Htime;
+    t = n * time->Pstep - (time->Htime + time->Pstart);
```

When `Pstart` is 0 (the default), the two expressions are the same, so those runs are unchanged. EPANET's own unit tests (6 suites) pass with the patch.

A pattern start that is a whole number of pattern steps, such as `Pattern Start 2:00` with a 2-hour pattern step, is affected too, though less often. The old expression then points one or more periods too far ahead, so it never shortens a step. The boundaries are still hit while the steps stay on multiples of the hydraulic step, but not after a tank or control event moves them off it. In Net1 with `Pattern Start 2:00` and a 4-hour report step, the unpatched run solves at 12:13, 13:13, 14:13 and 15:13 and so misses the 14:00 boundary; with the fix it solves at 12:13, 13:13 and 14:00.

## Notes

- [SIM-11](../SIM-11-duration-overshoot/) also changes `timestep()`. The two patches touch different lines and apply together in either order.
- [SIM-09](../SIM-09-report-start-mislabels-rows/) is the same kind of bug for `Report Start`.
