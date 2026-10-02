# API-18: EN_TANKLEVEL returns the initial level but is documented as current

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | The documentation says `EN_TANKLEVEL` is the current computed tank level and read-only; it is the initial level, and it can be set. Code that follows the documentation reads a level that never changes during a run. For a reservoir, the setter writes the head but the getter returns 0. |
| **Reached from** | Toolkit API: `EN_getnodevalue(EN_TANKLEVEL)` during a run; `EN_setnodevalue` / `EN_getnodevalue(EN_TANKLEVEL)` on a reservoir |
| **Where** | The enum comment in [`include/epanet2_enums.h:45`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/include/epanet2_enums.h#L45), the results table in [`doc/toolkit-usage.dox:185`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/doc/toolkit-usage.dox#L185), and `EN_getnodevalue()` in [`src/epanet.c:2329-2332`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2329-L2332) |
| **Since** | The reservoir asymmetry is in EPANET 2.0 (v2.0.12). The enum comment dates from 2019 and is in every release since 2.2. |
| **GitHub** | [PR #633](https://github.com/OpenWaterAnalytics/EPANET/pull/633) (2021): the maintainer ruled that `EN_TANKLEVEL` is the initial level and that the documentation should be corrected. It never was. |
| **Fix** | Documentation (two files) and three lines of code: [`API-18_fix.patch`](API-18_fix.patch) |

## The problem

`epanet2_enums.h` documents `EN_TANKLEVEL` as

```c
EN_TANKLEVEL    = 8, //!< Current computed tank water level (read only)
```

and the toolkit guide lists it under "Retrieving Computed Results". Neither is true. `EN_getnodevalue(EN_TANKLEVEL)` returns the tank's initial level, `H0 - El`, at any time; `EN_setnodevalue(EN_TANKLEVEL)` sets it. In the test, a tank that starts at 10 ft and drains for an hour reports `EN_TANKLEVEL` 10.000 ft at the end, while its level is 9.183 ft. A program that follows the documentation to track tank levels gets a constant.

In PR [#633](https://github.com/OpenWaterAnalytics/EPANET/issues/633) (2021) a user proposed changing the getter to return the current level. The maintainer declined: "It should be the initial water level in the tank before the start of the simulation. This is an input parameter, just like the Minimum and Maximum levels are." He proposed correcting the documentation instead, and noted that renaming the property to `EN_INITLEVEL` would be clearer but would break backward compatibility. The documentation was never changed.

Reservoirs have a second inconsistency. For a reservoir, `EN_setnodevalue(EN_TANKLEVEL, x)` sets its head (and elevation) to `x`, but `EN_getnodevalue(EN_TANKLEVEL)` returns `H0 - El`, which is always 0 for a reservoir. Setting 60 and reading it back gives 0.

## Why it happens

```c
// src/epanet.c, EN_getnodevalue()
case EN_TANKLEVEL:
    if (index <= nJuncs) return 0;
    v = (Tank[index - nJuncs].H0 - Node[index].El) * Ucf[ELEV];
    break;

// src/epanet.c, EN_setnodevalue()
case EN_TANKLEVEL:
    ...
    if (Tank[j].A == 0.0) /* Tank is a reservoir */
    {
        Tank[j].H0 = value / Ucf[ELEV];
        Tank[j].Hmin = Tank[j].H0;
        Tank[j].Hmax = Tank[j].H0;
        Node[index].El = Tank[j].H0;
        ...
```

`H0` is only changed by the setters; the solver keeps the current level in `NodeHead`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-18_network.inp`](API-18_network.inp) | Tank T1 (initial level 10 ft) drains 200 gpm into J1; reservoir R1 (head 50 ft) is a backup behind a check valve |
| [`API-18_test.c`](API-18_test.c) | A: reads T1's `EN_TANKLEVEL` and its current level during a one-hour run. B: sets R1's `EN_TANKLEVEL` to 60 and reads it back. |
| [`API-18_fix.patch`](API-18_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-18            # unpatched EPANET: FAIL
tools/run-test.sh API-18 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-18_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-18_test
./API-18_test
```

Part A shows the documented behaviour after the fix, so it is the same with and without it. The test fails without the fix because of part B:

```
A. t = 0 s: T1 EN_TANKLEVEL 10.000 ft, current level 10.000 ft
A. t = 3600 s: T1 EN_TANKLEVEL 10.000 ft, current level 9.183 ft
B. R1: EN_setnodevalue(EN_TANKLEVEL, 60) returned 0; EN_TANKLEVEL reads 0 (expected 60), EN_ELEVATION reads 60
FAIL: a reservoir's EN_TANKLEVEL does not read back the value set
```

**With the fix:**

```
A. t = 0 s: T1 EN_TANKLEVEL 10.000 ft, current level 10.000 ft
A. t = 3600 s: T1 EN_TANKLEVEL 10.000 ft, current level 9.183 ft
B. R1: EN_setnodevalue(EN_TANKLEVEL, 60) returned 0; EN_TANKLEVEL reads 60 (expected 60), EN_ELEVATION reads 60
PASS: EN_TANKLEVEL is the initial level of a tank and the head of a reservoir, for both set and get
```

## The fix

The documentation follows the ruling in PR [#633](https://github.com/OpenWaterAnalytics/EPANET/issues/633):

```diff
-  EN_TANKLEVEL    = 8, //!< Current computed tank water level (read only)
+  EN_TANKLEVEL    = 8, //!< Tank initial water level (reservoir head)
```

In `doc/toolkit-usage.dox` the patch removes `EN_TANKLEVEL` from the table of computed results and adds a sentence after it: a tank's current level is its `EN_HEAD` minus its `EN_ELEVATION`, and `EN_TANKLEVEL` is the initial level, an input like the minimum and maximum levels.

For reservoirs the patch makes the getter return what the setter assigns, the head:

```diff
     case EN_TANKLEVEL:
+        // Initial level of a tank, or head of a reservoir (what
+        // EN_setnodevalue assigns)
         if (index <= nJuncs) return 0;
-        v = (Tank[index - nJuncs].H0 - Node[index].El) * Ucf[ELEV];
+        if (Tank[index - nJuncs].A == 0.0) v = Tank[index - nJuncs].H0 * Ucf[ELEV];
+        else v = (Tank[index - nJuncs].H0 - Node[index].El) * Ucf[ELEV];
```

The reservoir case is a choice. The options:

- **Getter returns the head (this patch).** Set and get agree, and nothing that sets a reservoir's head through `EN_TANKLEVEL` breaks. Code that relied on reading 0 for a reservoir would see its head instead.
- **Document the asymmetry only.** State that for a reservoir the setter sets the head (as `EN_ELEVATION` does) and the getter returns 0. No behaviour change.
- **Refuse reservoirs in the setter** with 263, as the other tank-only properties do, and point callers to `EN_ELEVATION`. Cleanest, but breaks existing callers.

The first option also fixes the rollback of `EN_setnodevalues(EN_TANKLEVEL)` for reservoirs (see [API-11](../API-11-bulk-setters-read-before-write/)), which restores the value the getter read.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The maintainer's comment in [#633](https://github.com/OpenWaterAnalytics/EPANET/issues/633) also points out that `NodeHead` is "either 0 or undefined" before a simulation, which is why the current level cannot simply be returned before a run. [API-20](../API-20-reads-before-run-return-scratch-values/) is about such reads.
