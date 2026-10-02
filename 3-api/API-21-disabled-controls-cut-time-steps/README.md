# API-21: Controls disabled with link index 0 still cut time steps

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A control de-activated through `EN_setcontrol` with link index 0 never acts, but it still shortens the hydraulic time step to its trigger time, adding solutions and changing results slightly. A time-of-day control does this every day. |
| **Reached from** | Toolkit: `EN_setcontrol(ph, index, type, 0, ...)` |
| **Where** | `controltimestep()` in [`src/hydraul.c:758-817`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L758-L817), against the skip in `controls()` at [`src/hydraul.c:591`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L591) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 3 lines: [`API-21_fix.patch`](API-21_fix.patch) |

## The problem

`EN_setcontrol` accepts a link index of 0 to de-activate a control; the comment in the code says so:

```c
// src/epanet.c, EN_setcontrol()
    // Check that controlled link exists (0 index de-activates the control)
    if (linkIndex == 0)
    {
        net->Control[index].Link = 0;
        return 0;
    }
```

The control then no longer changes any link, but the solver still stops at its trigger time. In the test, a TIMER control at 0:30 is added to Net1 and de-activated. The hydraulics are solved at 0:00, 0:30, 1:00, ... instead of every hour. Every later result is computed from a different sequence of tank levels, and the extra solution is written to the hydraulics file. By reading the code, a de-activated `TIMEOFDAY` control adds a step every day, and a de-activated tank-level control adds one whenever the tank is about to reach its level.

## Why it happens

`controls()`, which applies the controls, skips any control whose link index is 0:

```c
// src/hydraul.c, controls()
        if ( (k = control->Link) <= 0) continue;
```

`controltimestep()`, which shortens the time step so that the solver stops when a control will fire, has no such check. It computes the time to the trigger, and then checks whether the control would change the status of link `k = 0`. That is an unused slot whose status stays 0 (`XHEAD`), which never matches a control's `OPEN` or `CLOSED`:

```c
// src/hydraul.c, controltimestep()
        // Revise the current estimated next time step
        if (t > 0 && t < *tstep)
        {
            // Check if rule actually changes link status or setting
            k = control->Link;
            link = &net->Link[k];
            if ( (link->Type > PIPE && hyd->LinkSetting[k] != control->Setting)
              || (hyd->LinkStatus[k] != control->Status) )
            {
              *tstep = t;
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-21_Net1.inp`](API-21_Net1.inp) | EPANET's example network Net1, unchanged |
| [`API-21_test.c`](API-21_test.c) | Adds a TIMER control at 0:30 on pump 9, de-activates it with link index 0, and lists the hydraulic solution times in the first 4 hours |
| [`API-21_fix.patch`](API-21_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-21            # unpatched EPANET: FAIL
tools/run-test.sh API-21 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-21_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-21_test
./API-21_test
```

**Without the fix:**

```
Control 3 after EN_setcontrol: link index 0
Hydraulic solutions in the first 4 hours: 0:00 0:30 1:00 2:00 3:00 4:00
FAIL: the de-activated control still cut 1 time step(s) short
```

**With the fix:**

```
Control 3 after EN_setcontrol: link index 0
Hydraulic solutions in the first 4 hours: 0:00 1:00 2:00 3:00 4:00
PASS: the de-activated control does not change the time steps
```

## The fix

Skip the control in `controltimestep()` as `controls()` does:

```diff
         if (!control->isEnabled)
         {
             continue;
         }
+
+        // Skip a control de-activated by setting its link to 0
+        if (control->Link <= 0) continue;
+
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `EN_setcontrolenabled`, added in v2.3, is the newer way to switch a control off. Both `controls()` and `controltimestep()` check its `isEnabled` flag, so it does not have this problem. The link-index-0 route predates it and is still accepted.
- `EN_saveinpfile` and the unit conversion in `input1.c` already skip controls with link index 0.
