# SIM-03: Constant-power pumps in SI input files are converted from kW twice

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. A constant-power pump in an SI input file delivers 1.341 times its rated power (34 % more head at a given flow). Each save and reload multiplies the power by 1.341 again. |
| **Reached from** | Input files in SI units with a `POWER` pump; also `EN_getlinkvalue(EN_PUMP_POWER)`, `EN_saveinpfile` and `EN_setflowunits` |
| **Where** | `convertunits()` in [`src/input1.c:609`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L609) and `constpowerpump()` in [`src/validate.c:279`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/validate.c#L279); also [`src/epanet.c:4044`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4044), [`src/epanet.c:4303`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4303) and [`src/inpfile.c:247`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L247) |
| **Since** | v2.3. A regression from [PR #758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (`EN_openX`, commits [10d5079](https://github.com/OpenWaterAnalytics/EPANET/commit/10d5079d750ab6a8b2070bb20778346afacc37ff) and [c0700c8](https://github.com/OpenWaterAnalytics/EPANET/commit/c0700c83ee383b2c716f9612840a767e4a5aa9d3), October 2023) |
| **GitHub** | No related issue |
| **Fix** | Four small changes in three files: [`SIM-03_fix.patch`](SIM-03_fix.patch) |

## The problem

A constant-power pump is given as `POWER <value>` in `[PUMPS]`, in kW for SI units and hp for US units. In an SI input file the value is converted from kW to hp twice, so the pump works as if it had 1/0.7457 = 1.341 times its power.

The test pump is rated 10 kW and lifts 10 L/s from a reservoir at head 0. It should add P/(ρgQ) = 101.9 m of head. EPANET gives 136.8 m. The other ways of handling the power disagree with each other:

- `EN_getlinkvalue(EN_PUMP_POWER)` reads 13.41 (hp) for the file's 10 kW pump.
- `EN_setlinkvalue(EN_PUMP_POWER, 10)` on the same pump gives 102.0 m, the correct head. So a file and a toolkit program describing the same pump get different results.
- `EN_saveinpfile` writes `POWER 13.4102`. After reloading, `EN_PUMP_POWER` reads 17.98 and the head gain is 183.5 m (1.341² times the correct value). Every further round trip multiplies the power by 1.341 again.
- `EN_setflowunits(EN_LPS)` on a US project leaves the stored 10 (hp) unchanged and then reads it as 10 kW, so the head gain rises from 26.87 m (88.14 ft) to 36.03 m.

US input files are not affected by the double conversion, because the kW-to-hp factor is 1 there.

## Why it happens

The power is held in `Link.Km`. PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) reorganised where pump coefficients are derived, and the kW-to-hp conversion ended up in two places: `convertunits()`, which runs when an input file is loaded, and `constpowerpump()`, which runs at every `EN_openH`:

```c
// src/input1.c, convertunits()
        else if (link->Type == PUMP)
        {
            link->Km /= pr->Ucf[POWER];               // kW -> hp
        }

// src/validate.c, constpowerpump()
    pump->R = -8.814 * pr->network.Link[pump->Link].Km / pr->Ucf[POWER];   // again
```

`Ucf[POWER]` is 0.7457 (kW per hp) for SI units and 1 for US units. The setter, the getter and the writer treat `Km` as the user's value, so they match `constpowerpump()` and not the parser:

```c
// src/epanet.c, EN_getlinkvalue(), EN_PUMP_POWER
            if (Pump[pmp].Ptype == CONST_HP) v = Link[index].Km; // Power in HP or KW
// src/epanet.c, EN_setlinkvalue(), EN_PUMP_POWER
            net->Link[index].Km = value;
// src/inpfile.c, saveinpfile()
        if (pump->Ptype == CONST_HP) sprintf(s1, "\tPOWER %.4f", link->Km);
```

`EN_setflowunits` converts curves and rule values to the new units but never touches `Km`, so the same stored number is read as hp before the call and as kW after it.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-03_si-pump.inp`](SIM-03_si-pump.inp) | LPS units. A 10 kW constant-power pump PU1 lifts 10 L/s from reservoir R1 (head 0) to junction J1. |
| [`SIM-03_us-pump.inp`](SIM-03_us-pump.inp) | GPM units. A 10 hp pump lifts 1 cfs (448.831 gpm) from R1 to J1. |
| [`SIM-03_test.c`](SIM-03_test.c) | Checks the head at J1 and `EN_PUMP_POWER` for the SI file as loaded, after setting the power through the API, after a save and reload, and for the US file before and after `EN_setflowunits(EN_LPS)` |
| [`SIM-03_fix.patch`](SIM-03_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-03            # unpatched EPANET: FAIL
tools/run-test.sh SIM-03 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-03_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-03_test
./SIM-03_test
```

The expected heads come from P = ρgQH. EPANET's constant 8.814 gives 102.0 m for the SI pump, within 0.1 % of 101.9 m.

**Without the fix:**

```
                                                     Expected     Actual
SI file: EN_PUMP_POWER (kW)                            10.000     13.410   <-- wrong
SI file: head gain (m)                                101.937    136.807   <-- wrong
SI file, 10 kW set through the API: head gain (m)     101.937    102.017   
SI file saved and reopened: EN_PUMP_POWER (kW)         10.000     17.983   <-- wrong
SI file saved and reopened: head gain (m)             101.937    183.460   <-- wrong
US file: head gain (ft)                                88.140     88.140   
US file switched to LPS: head gain (m)                 26.865     36.027   <-- wrong
US file switched to LPS: EN_PUMP_POWER (kW)             7.457     10.000   <-- wrong
FAIL: 6 of 8 checks wrong; SI pump power is converted from kW twice
```

**With the fix:**

```
                                                     Expected     Actual
SI file: EN_PUMP_POWER (kW)                            10.000     10.000   
SI file: head gain (m)                                101.937    102.017   
SI file, 10 kW set through the API: head gain (m)     101.937    102.017   
SI file saved and reopened: EN_PUMP_POWER (kW)         10.000     10.000   
SI file saved and reopened: head gain (m)             101.937    102.017   
US file: head gain (ft)                                88.140     88.140   
US file switched to LPS: head gain (m)                 26.865     26.865   
US file switched to LPS: EN_PUMP_POWER (kW)             7.457      7.457   
PASS: SI and US pumps give the correct head, and the power survives the API, a save and a change of units
```

## The fix

Keep `Link.Km` in hp for a constant-power pump, as every other quantity in the network structure is held in internal US units, and convert it once at each boundary:

| Where | Change |
|---|---|
| Parser, `convertunits()` | none: it already divides by `Ucf[POWER]` |
| `constpowerpump()` | drop `/ pr->Ucf[POWER]` |
| `EN_getlinkvalue(EN_PUMP_POWER)` | return `Km * Ucf[POWER]` |
| `EN_setlinkvalue(EN_PUMP_POWER)` | store `value / Ucf[POWER]` |
| `saveinpfile()` | write `Km * Ucf[POWER]` |
| `EN_setflowunits` | none: an internal value in hp does not depend on the user's units |

```diff
-    pump->R = -8.814 * pr->network.Link[pump->Link].Km / pr->Ucf[POWER];
+    pump->R = -8.814 * pr->network.Link[pump->Link].Km;
...
-            if (Pump[pmp].Ptype == CONST_HP) v = Link[index].Km; // Power in HP or KW
+            if (Pump[pmp].Ptype == CONST_HP) v = Link[index].Km * Ucf[POWER];
...
-            net->Link[index].Km = value;
+            net->Link[index].Km = value / Ucf[POWER];
...
-        if (pump->Ptype == CONST_HP) sprintf(s1, "\tPOWER %.4f", link->Km);
+        if (pump->Ptype == CONST_HP)
+        {
+            sprintf(s1, "\tPOWER %.4f", link->Km * pr->Ucf[POWER]);
+        }
```

For toolkit users nothing changes in SI projects built through the API: `EN_PUMP_POWER` is still set and read in kW. What changes is that SI input files give the right head, saved files reload unchanged, and `EN_setflowunits` keeps the pump's physical power.

**Alternative.** Keep `Km` in the user's units, as v2.2 did: delete the conversion in `convertunits()`, leave `constpowerpump()`, the getter, the setter and the writer as they are, and make `EN_setflowunits` rescale `Km` of every constant-power pump by the ratio of the new and old `Ucf[POWER]`. That is also four or five lines, but it leaves one more value that `EN_setflowunits` has to remember to convert, which is the kind of omission behind [SIM-19](../SIM-19-gpv-curve-type-not-set/).

EPANET's own unit tests (6 suites) pass with the patch. None of them uses a constant-power pump.

## Notes

- Before PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758), v2.2 kept `Km` in user units and divided the pump's `R` coefficient by `Ucf[POWER]` once, both in the parser and in the setter. Commit 10d5079 added a division of `Km` to `convertunits()` and to the setter (`Km = value / Ucf[POWER]`) while `validate.c` also divided `R`, so both paths were converted twice. Commit c0700c8 reverted the setter to the raw value and moved the `validate.c` division into `constpowerpump()`. That fixed the API path and left the parser's division in place.
- Existing SI input files with `POWER` pumps will give lower heads after the fix, because they currently run at 1.341 times their stated power. Files saved by 2.3.x from an SI project already carry an inflated `POWER` value.
- Setting `EN_PUMP_POWER` while the solver is open has no effect: that is [SIM-25](../SIM-25-pump-power-change-while-solver-open/), whose patch changes the setter after this one.
