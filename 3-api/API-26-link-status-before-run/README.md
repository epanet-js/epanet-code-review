# API-26: EN_STATUS, EN_SETTING and EN_DEMAND read leftover values before a run

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | Before any hydraulics are run, every link reads as closed, pumps and valves read a setting of 0, and junction demands read the parser's scratch values: 448.83 times the base demand in a GPM model, or -4.5e12 gpm for a junction listed in `[DEMANDS]`. |
| **Reached from** | Toolkit API: `EN_getlinkvalue(EN_STATUS, EN_SETTING)` and `EN_getnodevalue(EN_DEMAND)` after `EN_open` and before `EN_initH` |
| **Where** | `EN_getlinkvalue()`, cases `EN_STATUS` and `EN_SETTING`, in [`src/epanet.c:3975-3988`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3975-L3988); `EN_getnodevalue()`, case `EN_DEMAND`, in [`src/epanet.c:2352`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2352); the parser's use of `NodeDemand` in `juncdata()`, [`src/input3.c:137-140`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L137-L140) |
| **Since** | EPANET 2.0: the status arrays have always been filled by `inithyd()`, and the parser has always used the demand array as a marker |
| **GitHub** | No related issue. In [PR #633](https://github.com/OpenWaterAnalytics/EPANET/pull/633) the maintainer notes that solver arrays are "either 0 or undefined" before a simulation. |
| **Fix** | Three lines in `convertunits()` and a documentation paragraph: [`API-26_fix.patch`](API-26_fix.patch) |

## The problem

A program that opens a model and lists its links or nodes before running it reads values the solver has not set yet:

- **`EN_STATUS` is 0 (closed) for every link.** In the test, open pipe P1, pump PU1 and PRV V1 all read as closed. `EN_INITSTATUS` gives 1, 1 and 2 (active).
- **`EN_SETTING` is 0 for pumps and valves.** PU1 runs at speed 0.9 and V1 is set to 40 psi; both read 0. (A pipe's setting is its roughness, which is read from the pipe data and is correct.)
- **`EN_DEMAND` is the parser's marker.** While reading `[JUNCTIONS]`, the parser stores each junction's demand, in user units, in the solver's demand array, and overwrites it with `MISSING` (-10^10) when the junction appears in `[DEMANDS]`. The getter multiplies that by the flow factor: J2, with 100 gpm, reads 44,883.1 gpm, and J3 reads -4.49e12 gpm.

All three are correct as soon as `EN_initH` has run. Before that, they are not "no result" values like the zero flows, but values that look like results and are wrong.

A related case is documented rather than changed: between `EN_initH` and the first `EN_runH`, flows hold the solver's starting values. P1 reads 156.67 gpm (a velocity of 1 ft/s, the starting guess) and the emitter at J3 reads 448.83 gpm (1 cfs, see [API-20](../API-20-reads-before-run-return-scratch-values/)).

## Why it happens

The link status and setting arrays are allocated with `calloc` and only filled in `inithyd()`:

```c
// src/project.c, allocdata()
pr->hydraul.LinkSetting = (double *)calloc(n, sizeof(double));
pr->hydraul.LinkStatus  = (StatusType *)calloc(n, sizeof(StatusType));

// src/hydraul.c, inithyd()
// Initialize status and setting
hyd->LinkStatus[i] = link->InitStatus;
hyd->LinkSetting[i] = link->InitSetting;
```

Status 0 is `XHEAD`, which the getter reports as closed (`LinkStatus <= CLOSED`). The demand array doubles as the parser's scratch space:

```c
// src/input3.c, juncdata()
// Create a demand for the junction and use NodeDemand as an indicator
// to be used when processing demands from the [DEMANDS] section
if (!adddemand(node, d, p, NULL)) return 101;
hyd->NodeDemand[njuncs] = d;

// src/input3.c, demanddata()
hyd->NodeDemand[j] = MISSING; // marker - next iteration will append a new category.
```

Nothing clears it after parsing, and `inithyd()` does not reset junction demands either; the solver overwrites them on the first `EN_runH`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-26_network.inp`](API-26_network.inp) | Pump PU1 (speed 0.9) and PRV V1 (40 psi) supply J2 (100 gpm in `[JUNCTIONS]`) and J3 (50 gpm in `[DEMANDS]`, plus an emitter); pipe P2 is closed |
| [`API-26_test.c`](API-26_test.c) | Before any run, compares each link's `EN_STATUS` and `EN_SETTING` with its initial values and reads the two demands; then shows flows between `EN_initH` and `EN_runH` |
| [`API-26_fix.patch`](API-26_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-26            # unpatched EPANET: FAIL
tools/run-test.sh API-26 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-26_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-26_test
./API-26_test
```

**Without the fix:**

```
Before a run:
Link EN_STATUS EN_INITSTATUS EN_SETTING EN_INITSETTING
P1        0            1       100           100   <-- differs
P2        0            0       100           100
PU1       0            1         0           0.9   <-- differs
V1        0            2         0            40   <-- differs
J2 EN_DEMAND 44883.1 gpm, J3 EN_DEMAND -4.48831e+12 gpm (expected 0, 0)

After EN_initH:  P1 EN_FLOW   156.67 gpm, J3 EN_EMITTERFLOW   448.83 gpm  (starting values)
After EN_runH:   P1 EN_FLOW    81.57 gpm, J3 EN_EMITTERFLOW    31.57 gpm  (results)
FAIL: 5 of 6 values read before a run are leftovers
```

**With the fix:**

```
Before a run:
Link EN_STATUS EN_INITSTATUS EN_SETTING EN_INITSETTING
P1        1            1       100           100
P2        0            0       100           100
PU1       1            1       0.9           0.9
V1        2            2        40            40
J2 EN_DEMAND 0 gpm, J3 EN_DEMAND 0 gpm (expected 0, 0)

After EN_initH:  P1 EN_FLOW   156.67 gpm, J3 EN_EMITTERFLOW   448.83 gpm  (starting values)
After EN_runH:   P1 EN_FLOW    81.57 gpm, J3 EN_EMITTERFLOW    31.57 gpm  (results)
PASS: before a run, links read their initial status and setting, and demands read 0
```

The two lines after `EN_initH` and `EN_runH` are the same with and without the fix.

## The fix

When the network is loaded, `convertunits()` now gives each link its initial status and setting, and clears the parser's marker from each junction's demand:

```diff
         for (demand = node->D; demand != NULL; demand = demand->next)
         {
             demand->Base /= pr->Ucf[DEMAND];
         }
+
+        // The parser used NodeDemand as a marker; no demand is computed yet
+        hyd->NodeDemand[i] = 0.0;
     }
```

```diff
         link->InitSetting = link->Kc;
+
+        // Before a run a link's status and setting are its initial ones
+        hyd->LinkStatus[k] = link->InitStatus;
+        hyd->LinkSetting[k] = link->InitSetting;
     }
```

`inithyd()` sets the same values at the start of every run, so results do not change.

For the window between `EN_initH` and `EN_runH`, the patch adds a paragraph to the `EN_initH` documentation: until `EN_runH` computes the first solution, flows such as `EN_FLOW` and `EN_EMITTERFLOW` are the solver's starting values, not results. Link flows there are starting values by design (the first digit of `EN_initH`'s flag chooses whether to re-initialise them), and the emitter's 1 cfs is the same kind of value, so documenting both seemed better than special-casing emitters. Moving the emitter seed from `inithyd()` to the start of `hydsolve()` would make `EN_EMITTERFLOW` read 0 there, but would leave `EN_FLOW` as it is.

What the patch does not cover:

- Values set with `EN_INITSTATUS` or `EN_INITSETTING` after loading are not copied to the current status, because after a run that would overwrite the last computed status. Before a run, `EN_STATUS` therefore shows the status as loaded.
- Junction heads are still 0 before a run, so `EN_PRESSURE` reads minus the elevation. With [API-20](../API-20-reads-before-run-return-scratch-values/), which sets tank and reservoir heads on load, an open link next to a tank or reservoir now reads a head loss before a run as well: 50 ft for PU1 in this network, in a build with both patches. Neither is a result.
- Links added with `EN_addlink` get their status, setting and flow slots from `realloc` without initialising them (by reading the code). `inithyd()` sets the status and setting, but when the flag says not to re-initialise flows it keeps whatever is in the new link's flow slot as the starting guess.

A complete answer would be a "no hydraulic results yet" flag that the result getters check, returning 0 (or the initial value) until the first `EN_runH`.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The patch changes `convertunits()` in different places from [API-20](../API-20-reads-before-run-return-scratch-values/) (tank heads) and does not touch `EN_setlinkvalue` ([API-14](../API-14-initstatus-char-cast/), [API-15](../API-15-pipe-setting-ignores-roughness-error/)); they all apply together.
