# SIM-11: A duration that is not a multiple of the hydraulic step is overshot

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | The last hydraulic step runs past the end of the simulation. The energy report counts the extra time (usage factor 133 %, cost a third too high in the test), and the water quality run stops one step early, at 1:00 instead of 1:30. |
| **Reached from** | Input file or toolkit: a duration that is not on the reporting grid and that the hydraulic steps do not land on, for example one that is not a multiple of the hydraulic step |
| **Where** | `timestep()` in [`src/hydraul.c:663-684`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L663-L684), called from `nexthyd()` in [`src/hydraul.c:268`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L268) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 4 lines: [`SIM-11_fix.patch`](SIM-11_fix.patch) |

## The problem

Net1 run for 1:30 with a 1-hour hydraulic step should be solved at 0:00, 1:00 and 1:30, with a last step of 30 minutes. Instead the solver takes a full hour from 1:00 and solves at 2:00, half an hour past the end. That has three effects:

- **The energy report is wrong.** The pump runs for the whole of the simulation, so its usage factor should be 100 %. It is 133.33 %, because 2 hours of pumping are divided by the 1.5-hour duration. The cost per day is 307.06 instead of 230.20.
- **The water quality run is cut short.** The quality solver stops when the next hydraulic time is past the duration, so it ends at 1:00 and the last 30 minutes are never simulated.
- **A solution past the end is saved** to the hydraulics file. By reading the code, the flow balance also adds up the whole last step.

The step is already cut at every reporting time, so the duration has to be off the reporting grid. Then the overshoot happens whenever the steps do not land on the duration by themselves, for example with a 2-hour report and hydraulic step and a 25-hour duration. Whole-hour durations with hourly reporting are not affected.

## Why it happens

`timestep()` shortens the hydraulic step to end at the next pattern period, reporting time, tank fill or drain and control action, but not at the end of the simulation:

```c
// src/hydraul.c, timestep()
    // Normal time step is hydraulic time step
    tstep = time->Hstep;

    // Revise time step based on time until next demand period
    ...
    // Revise time step based on time until next reporting period
    t = time->Rtime - time->Htime;
    if (t > 0 && t < tstep) tstep = t;

    // Revise time step based on smallest time to fill or drain a tank
    tanktimestep(pr, &tstep);
    ...
```

The end of the run is only hit when it falls on one of those times, which in practice means on a reporting time. Otherwise `nexthyd()` moves `Htime` past `Dur`, and the code downstream assumes it never does:

```c
// src/hydraul.c, addenergy(): the step from 1:00 counts in full
    else if (time->Htime < time->Dur)
    {
        dt = (double) hstep / 3600.0;
    }

// src/quality.c, nextqual(): no quality step once Htime > Dur
    if (time->Htime <= time->Dur) hydstep = time->Htime - time->Qtime;
```

`saveenergy()` then divides the hours online by the duration, 1.5 hours.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-11_duration.inp`](SIM-11_duration.inp) | Net1 with `Duration 1:30`, a 1-hour hydraulic step, an energy price of 0.10 per kWh and the energy report turned on |
| [`SIM-11_test.c`](SIM-11_test.c) | Lists the hydraulic solution times, adds up the time steps of the water quality solver, and reads the pump's usage factor from the output file |
| [`SIM-11_fix.patch`](SIM-11_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-11            # unpatched EPANET: FAIL
tools/run-test.sh SIM-11 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-11_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -o SIM-11_test
./SIM-11_test
```

The energy table also shows it with the command-line runner, `runepanet SIM-11_duration.inp SIM-11.rpt`:

```
  9         133.33  75.00    861.05     95.96     96.07    307.06
```

and with the fix:

```
  9         100.00  75.00    859.36     95.92     96.07    230.20
```

**Without the fix:**

```
Hydraulic solutions at: 0:00 1:00 2:00   (the last should be 1:30)
Water quality simulated to: 1:00              (should be 1:30)
Pump 9 usage factor: 133.33 %               (should be at most 100 %)
FAIL: the run goes past its 1:30 duration (3 of 3 checks failed)
```

**With the fix:**

```
Hydraulic solutions at: 0:00 1:00 1:30   (the last should be 1:30)
Water quality simulated to: 1:30              (should be 1:30)
Pump 9 usage factor: 100.00 %               (should be at most 100 %)
PASS: the run ends at 1:30 and the pump's usage factor is 100.00 %
```

## The fix

Shorten the step to the time left, alongside the other limits in `timestep()`:

```diff
     t = time->Rtime - time->Htime;
     if (t > 0 && t < tstep) tstep = t;
 
+    // Revise time step so that it does not go past the end of the simulation
+    t = time->Dur - time->Htime;
+    if (t > 0 && t < tstep) tstep = t;
+
     // Revise time step based on smallest time to fill or drain a tank
```

`nexthyd()` only calls `timestep()` while `Htime < Dur`, so `t` is always positive there. The limit comes before the tank, control and rule checks, so they see the shortened step. Runs whose steps already land on the duration are unchanged. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-10](../SIM-10-pattern-start-boundary-missed/) also changes `timestep()`. The two patches touch different lines and apply together in either order.
- [SIM-07](../SIM-07-energy-averages-computed-in-place/) is another way to get a usage factor above 100 %.
