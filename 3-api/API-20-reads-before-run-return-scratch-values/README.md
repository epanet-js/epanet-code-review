# API-20: Reads before a run return solver scratch values

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | Before any hydraulics are run, `EN_EMITTERFLOW` reads 448.83 gpm (1 cfs) for an emitter that has just been added, and a tank's `EN_TANKVOLUME` is negative (-196,350 ft³ for a tank that holds 19,635 ft³). |
| **Reached from** | Toolkit API: `EN_getnodevalue(EN_EMITTERFLOW)` after `EN_setnodevalue(EN_EMITTER)`, and `EN_getnodevalue(EN_TANKVOLUME, EN_HEAD, EN_PRESSURE)` on a tank, before `EN_initH` |
| **Where** | `EN_setnodevalue()` in [`src/epanet.c:2560`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2560); `EN_getnodevalue()`, case `EN_TANKVOLUME`, in [`src/epanet.c:2419-2421`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2419-L2421) |
| **Since** | `EN_EMITTERFLOW`: v2.3, a side effect of [PR #797](https://github.com/OpenWaterAnalytics/EPANET/pull/797) (commit [a74171f](https://github.com/OpenWaterAnalytics/EPANET/commit/a74171f), 2024). `EN_TANKVOLUME`: since it was added in 2.1 (2016). |
| **GitHub** | [PR #797](https://github.com/OpenWaterAnalytics/EPANET/pull/797), the fix for [issue #795](https://github.com/OpenWaterAnalytics/EPANET/issues/795), added the 1 cfs value. [PR #633](https://github.com/OpenWaterAnalytics/EPANET/pull/633) notes that `NodeHead` is "either 0 or undefined" before a simulation. |
| **Fix** | A few lines in two files: [`API-20_fix.patch`](API-20_fix.patch) |

## The problem

A program that builds or edits a model often reads values back before running it. Two reads return values that only make sense to the solver:

- **`EN_EMITTERFLOW` after `EN_setnodevalue(EN_EMITTER)`.** The setter gives the node a starting emitter flow of 1 cfs, so the getter reports 448.83 gpm (28.3 L/s) for a model that has not been run. The value was added by PR [#797](https://github.com/OpenWaterAnalytics/EPANET/issues/797) to fix issue [#795](https://github.com/OpenWaterAnalytics/EPANET/issues/795): an emitter added while the solver is running, with an emitter exponent above 1, gave NaN, because the solver evaluated `pow(0, negative)`. That fix only needs the starting value while the solver is open.
- **`EN_TANKVOLUME` before `EN_initH`.** The getter computes the volume from the tank's head in `NodeHead`, which is 0 until `inithyd()` sets it to the initial head. A tank at elevation 100 ft with a 10 ft initial level reports -196,350 ft³ instead of 19,635 ft³. `EN_HEAD` reads 0 ft and `EN_PRESSURE` a negative pressure for the same reason.

`EN_TANKLEVEL`'s setter already keeps `NodeHead` equal to the initial head, and `EN_ELEVATION`'s setter shifts it, so the intent is clear; the load path and `EN_settankdata` just never set it.

## Why it happens

```c
// src/epanet.c, EN_setnodevalue(), case EN_EMITTER
Node[index].Ke = value;
if (hyd->EmitterFlow[index] == 0.0) hyd->EmitterFlow[index] = 1.0;

// src/epanet.c, EN_getnodevalue()
case EN_EMITTERFLOW:
    v = hyd->EmitterFlow[index] * Ucf[FLOW];
    break;
...
case EN_TANKVOLUME:
    if (index <= nJuncs) return 0;
    v = tankvolume(p, index - nJuncs, NodeHead[index]) * Ucf[VOLUME];
    break;
```

`inithyd()` resets every emitter flow and gives each emitter a starting flow of 1 cfs at the start of a run, and sets each tank's `NodeHead` to `H0`. Before that, the arrays hold zeros (from `calloc`) or whatever a setter left there.

## How to reproduce

| File | What it is |
|---|---|
| [`API-20_network.inp`](API-20_network.inp) | Tank T1 (initial level 10 ft, 19,635 ft³) feeds junctions J1 and J2; emitter exponent 1.1 |
| [`API-20_test.c`](API-20_test.c) | Before any run: reads T1's volume and head, adds an emitter at J1 and reads its flow. Then the issue [#795](https://github.com/OpenWaterAnalytics/EPANET/issues/795) case: adds an emitter at J2 during a run. |
| [`API-20_fix.patch`](API-20_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-20            # unpatched EPANET: FAIL
tools/run-test.sh API-20 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-20_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-20_test
./API-20_test
```

**Without the fix:**

```
A. Before a run: T1 EN_TANKVOLUME -196349.5 ft3 (expected 19635.0), EN_HEAD 0.00 ft (expected 110.00)
B. Before a run: J1 EN_EMITTERFLOW 448.83 gpm after setting EN_EMITTER (expected 0.00)
   During a run (t = 3600 s): J2 EN_EMITTERFLOW 605.91 gpm after adding an emitter (expected finite)
FAIL: 2 of 3 reads returned a scratch value instead of a result
```

**With the fix:**

```
A. Before a run: T1 EN_TANKVOLUME 19635.0 ft3 (expected 19635.0), EN_HEAD 110.00 ft (expected 110.00)
B. Before a run: J1 EN_EMITTERFLOW 0.00 gpm after setting EN_EMITTER (expected 0.00)
   During a run (t = 3600 s): J2 EN_EMITTERFLOW 605.91 gpm after adding an emitter (expected finite)
PASS: reads before a run return the initial state, and 0 for flows
```

The last line before the verdict is the issue [#795](https://github.com/OpenWaterAnalytics/EPANET/issues/795) case. In a separate build with the starting value removed altogether, it reads `nan gpm`; with the patch it is unchanged.

## The fix

Give a new emitter its starting flow only while the solver is open; otherwise `inithyd()` does it:

```diff
         Node[index].Ke = value;
-        if (hyd->EmitterFlow[index] == 0.0) hyd->EmitterFlow[index] = 1.0;
+        // A new emitter needs a starting flow if the solver is running
+        // (inithyd() gives every emitter one at the start of a run)
+        if (hyd->OpenHflag && hyd->EmitterFlow[index] == 0.0)
+            hyd->EmitterFlow[index] = 1.0;
```

Set a tank's head to its initial head when the network is loaded (`convertunits()`, used by `EN_open`) and in `EN_settankdata`, as the `EN_TANKLEVEL` setter does:

```diff
         tank->V = tank->V0;
         tank->C = node->C0;
+        hyd->NodeHead[i] = tank->H0;    // head before a run
```

```diff
     Tank[j].H0 = (elev + initlvl) / Ucf[ELEV];
+    p->hydraul.NodeHead[index] = Tank[j].H0;   // head before a run
```

`inithyd()` sets the same heads at the start of every run, so results do not change. An alternative is a flag for "no results yet" that the result getters check, returning 0 or an error before the first `EN_runH`; that is a larger change to the API's behaviour.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

These are the same kind of read and are not changed by the patch:

- Between `EN_initH` and the first `EN_runH`, every emitter reads 1 cfs, because `inithyd()` seeds them. [API-26](../API-26-link-status-before-run/) documents this as a starting value, like the link flows in that window.
- `EN_getlinkvalue(EN_STATUS)` before any run reads 0 (closed) for an open pipe, and `EN_DEMAND` reads the parser's scratch values: see [API-26](../API-26-link-status-before-run/).
- `EN_addnode` grows the result arrays (`NodeHead`, `EmitterFlow` and others) with `realloc` but neither initialises the new slot nor shifts the tank slots when a junction is inserted, so a node added through the API reads leftover memory until a run (a new tank's `EN_HEAD` read -1.83e-06 ft in a separate run). See [API-27](../API-27-results-on-wrong-objects-after-edit/).
