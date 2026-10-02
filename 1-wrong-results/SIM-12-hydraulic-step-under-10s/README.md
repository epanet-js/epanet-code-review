# SIM-12: A hydraulic step under 10 s hangs water quality and divides by zero in rule timing

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | With a hydraulic step of 1 to 9 seconds, a water quality run never finishes. With rules, the program divides by zero; on arm64 the run then stops at time 0 and reports success. |
| **Reached from** | Input file: `Hydraulic Timestep` under 0:00:10 with no `Quality Timestep` or `Rule Timestep` given |
| **Where** | `adjustdata()` in [`src/input1.c:240`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L240) and [`:244`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L244); the effects in `nextqual()` in [`src/quality.c:283-288`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L283-L288) and `ruletimestep()` in [`src/hydraul.c:857`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L857) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 2 lines plus a guard: [`SIM-12_fix.patch`](SIM-12_fix.patch) |

## The problem

A hydraulic step of 5 seconds is accepted without complaint, but:

- **A water quality run hangs.** `EN_solveQ` (and so `runepanet`) never returns. Nothing is written and no error is reported.
- **A run with rules divides by zero.** With UndefinedBehaviorSanitizer this is reported as `division by zero` in `ruletimestep()`. Without it, the result depends on the processor: on x86 the program dies with SIGFPE (from the earlier write-up), and on arm64 the division gives 0, so the run stops after the first solution at time 0 and `runepanet` prints `EPANET ran successfully` with an empty report.

## Why it happens

If the input file gives no quality or rule time step, `adjustdata()` sets them to a tenth of the hydraulic step, with integer division:

```c
// src/input1.c, adjustdata()
    // If no quality timestep, then make it 1/10 of hydraulic timestep
    if (time->Qstep == 0) time->Qstep = time->Hstep / 10;

    // If no rule time step then make it 1/10 of hydraulic time step
    // but not greater than hydraulic time step
    if (time->Rulestep == 0) time->Rulestep = time->Hstep / 10;
```

For a step under 10 s both are 0. `EN_settimeparam` rejects 0 for both, but here EPANET sets them itself. The quality solver then never advances, and the rule timing takes a remainder by 0:

```c
// src/quality.c, nextqual()
        qtime = 0;
        while (!qual->OutOfMemory && qtime < hydstep)
        {
            dt = MIN(time->Qstep, hydstep - qtime);   // always 0
            qtime += dt;
            transport(pr, dt);
        }

// src/hydraul.c, ruletimestep()
        dt = time->Rulestep;
        dt1 = time->Rulestep - (tnow % time->Rulestep);
```

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-12_quality.inp`](SIM-12_quality.inp) | Reservoir, junction and tank with chlorine from the reservoir; 5-second hydraulic step, 1-minute duration, no quality step |
| [`SIM-12_rules.inp`](SIM-12_rules.inp) | The same network without quality and with one rule (close P2 after 20 s); no rule step |
| [`SIM-12_test.c`](SIM-12_test.c) | Part 1 runs `EN_solveQ` on the first file, with a 10-second alarm. Part 2 steps through the hydraulics of the second file and checks that the run reaches its 60-second duration |
| [`SIM-12_fix.patch`](SIM-12_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-12            # unpatched EPANET: HANG
tools/run-test.sh SIM-12 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-12_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-12_test
./SIM-12_test
```

The test uses `alarm()` and `signal()` to stop the hang after 10 seconds. On Windows it compiles without them, and part 1 hangs until you stop it.

**Without the fix**, part 1 never finishes, so part 2 is not reached:

```
SIM-12_quality.inp: hydraulic step 5 s, quality step 0 s, rule step 0 s

HANG: EN_solveQ is still running after 10 s (a quality step of 0 s never advances)
```

The rules file on its own, with the sanitizer build of `runepanet` (`runepanet SIM-12_rules.inp SIM-12.rpt`):

```
.../src/hydraul.c:857:38: runtime error: division by zero
    #0 ... in ruletimestep hydraul.c:857
    #1 ... in timestep hydraul.c:682
    #2 ... in nexthyd hydraul.c:268
    #3 ... in EN_nextH epanet.c:585
```

With a plain (non-sanitizer) build on arm64, the same command prints `EPANET ran successfully`, and stepping through it with `EN_runH`/`EN_nextH` gives one solution at t = 0 and then a time step of 0.

**With the fix:**

```
SIM-12_quality.inp: hydraulic step 5 s, quality step 1 s, rule step 1 s
  EN_solveQ returned 0
SIM-12_rules.inp: hydraulic step 5 s, quality step 1 s, rule step 1 s
  last hydraulic solution at 60 s (duration 60 s)
PASS: the quality run finishes and the run with a rule reaches 60 s
```

## The fix

Make both defaults at least 1 second:

```diff
-    if (time->Qstep == 0) time->Qstep = time->Hstep / 10;
+    if (time->Qstep == 0) time->Qstep = MAX(1, time->Hstep / 10);
 ...
-    if (time->Rulestep == 0) time->Rulestep = time->Hstep / 10;
+    if (time->Rulestep == 0) time->Rulestep = MAX(1, time->Hstep / 10);
```

`adjustdata()` already makes the hydraulic step at least 1 second, so the existing `MIN(..., Hstep)` limits keep both values between 1 s and the hydraulic step. As a guard, `ruletimestep()` also treats a rule step that is not positive like having no rules, evaluating the rules once per time step instead of dividing by it:

```diff
-    if (net->Nrules == 0)
+    if (net->Nrules == 0 || time->Rulestep <= 0)
```

Hydraulic steps of 10 s or more get the same defaults as before. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Upstream pull request [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) adds a build option to run EPANET's tests under UndefinedBehaviorSanitizer. It would have caught the division by zero.
- `EN_settimeparam(EN_HYDSTEP)` does not recompute the default quality and rule steps, so a toolkit program that sets a short step after `EN_open` does not hit this bug; the input file does.
