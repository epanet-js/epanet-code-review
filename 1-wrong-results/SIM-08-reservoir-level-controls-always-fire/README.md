# SIM-08: Level controls on a reservoir always fire

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | A simple control `IF NODE <reservoir> BELOW` or `ABOVE` fires at every time step, whatever the reservoir's head. The controlled link is forced to the control's status for the whole run, with no warning. |
| **Reached from** | Input file (`[CONTROLS]`) or toolkit (`EN_addcontrol`, `EN_setcontrol`): any level control whose node is a reservoir |
| **Where** | `controls()` in [`src/hydraul.c:594-603`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L594-L603), with `tankvolume()` in [`src/hydraul.c:1114`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L1114) |
| **Since** | EPANET 2.00.12 (2008), the oldest version in the repository history |
| **GitHub** | No related issue |
| **Fix** | 7 lines: [`SIM-08_fix.patch`](SIM-08_fix.patch) |

## The problem

The parser and `EN_addcontrol` accept a reservoir as the node of a level control, and treat it like a tank: the level is measured from the reservoir's `Head` value, so the control grade is `Head + level`. That is useful when the reservoir's head follows a time pattern, for example a river intake that should close when the river rises:

```
[CONTROLS]
 LINK P1 CLOSED IF NODE RIVER ABOVE 10
 LINK P2 CLOSED IF NODE LAKE BELOW 10
```

In the test network, RIVER rises from level 0 to 20 ft at 3:00 and LAKE falls from 20 ft to 0. Both pipes should close at 3:00. Instead, both close at 0:00 and stay closed, and the status report says the controls did it:

```
     0:00:00: Pipe P1 changed by Reservoir RIVER control
     0:00:00: Pipe P2 changed by Reservoir LAKE control
```

The same happens for any `BELOW` or `ABOVE` value. By reading the code, it also applies to pumps and valves, and if a link has an `OPEN` and a `CLOSED` control on the same reservoir, both fire at every step and the one listed last wins.

## Why it happens

`controls()` decides whether a tank-level control fires by comparing two volumes: the tank's volume at its current head and its volume at the control grade.

```c
// src/hydraul.c, controls()
        // Link is controlled by tank level
        if ((n = control->Node) > 0 && n > net->Njuncs)
        {
            h = hyd->NodeHead[n];
            vplus = ABS(hyd->NodeDemand[n]);
            v1 = tankvolume(pr,n - net->Njuncs,h);
            v2 = tankvolume(pr,n - net->Njuncs, control->Grade);
            if (control->Type == LOWLEVEL && v1 <= v2 + vplus) reset = 1;
            if (control->Type == HILEVEL && v1 >= v2 - vplus)  reset = 1;
        }

// src/hydraul.c, tankvolume()
    // Use level*area if no volume curve
    j = tank->Vcurve;
    if (j == 0) return(tank->Vmin + (h - tank->Hmin) * tank->A);
```

A reservoir has area `A = 0` and no volume curve, so `tankvolume()` returns `Vmin` for every head. `v1` equals `v2`, so `v1 <= v2 + vplus` and `v1 >= v2 - vplus` are both true at every step.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-08_reservoir-levels.inp`](SIM-08_reservoir-levels.inp) | Reservoirs RIVER and LAKE feed junction J1 through pipes P1 and P2, with head patterns that change their levels by 20 ft at 3:00. One level control on each. Reservoir MAIN keeps J1 supplied. |
| [`SIM-08_test.c`](SIM-08_test.c) | Runs the hydraulics hour by hour and compares the status of P1 and P2 with what the controls should give |
| [`SIM-08_fix.patch`](SIM-08_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-08            # unpatched EPANET: FAIL
tools/run-test.sh SIM-08 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-08_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-08_test
./SIM-08_test
```

The input file has the status report turned on, so `runepanet SIM-08_reservoir-levels.inp SIM-08.rpt` shows the control actions at 0:00 too.

**Without the fix:**

```
Time   RIVER level  P1      expected   LAKE level  P2      expected
 0:00       0.0 ft  CLOSED  OPEN          20.0 ft  CLOSED  OPEN
 1:00       0.0 ft  CLOSED  OPEN          20.0 ft  CLOSED  OPEN
 2:00       0.0 ft  CLOSED  OPEN          20.0 ft  CLOSED  OPEN
 3:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
 4:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
 5:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
FAIL: 6 pipe statuses are wrong; the reservoir level controls fired while their conditions were false
```

**With the fix:**

```
Time   RIVER level  P1      expected   LAKE level  P2      expected
 0:00       0.0 ft  OPEN    OPEN          20.0 ft  OPEN    OPEN
 1:00       0.0 ft  OPEN    OPEN          20.0 ft  OPEN    OPEN
 2:00       0.0 ft  OPEN    OPEN          20.0 ft  OPEN    OPEN
 3:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
 4:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
 5:00      20.0 ft  CLOSED  CLOSED         0.0 ft  CLOSED  CLOSED
PASS: both reservoir level controls fire at 3:00, when their conditions become true
```

## The fix

For a reservoir, compare its head with the control grade directly. The grade is already the reservoir's `Head` value plus the control level (`convertunits()` in `input1.c` and `setcontrol()` in `project.c` compute it the same way as for a tank), so no other change is needed:

```diff
             v1 = tankvolume(pr,n - net->Njuncs,h);
             v2 = tankvolume(pr,n - net->Njuncs, control->Grade);
+
+            // A reservoir has no volume, so compare its head instead
+            if (net->Tank[n - net->Njuncs].A == 0.0)
+            {
+                v1 = h;
+                v2 = control->Grade;
+                vplus = 0.0;
+            }
             if (control->Type == LOWLEVEL && v1 <= v2 + vplus) reset = 1;
```

`controltimestep()` needs no change: a reservoir's head only changes at pattern steps, which are already time-step boundaries. EPANET's own unit tests (6 suites) pass with the patch.

The alternative is to reject a reservoir as the node of a level control, in `controldata()` and `setcontrol()`. That would make existing input files fail to load, and level controls on patterned reservoirs have a real use, so the patch keeps them and makes them work.
