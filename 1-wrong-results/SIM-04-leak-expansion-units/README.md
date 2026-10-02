# SIM-04: Leak expansion is per metre of head in both unit systems, but the docs say per ft or m

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | A US-unit user who follows the documentation enters the leak expansion rate in sq mm per ft of head and gets 0.3048 times the variable-area leakage they intended. SI users are not affected. |
| **Reached from** | `[LEAKAGE]` in input files and `EN_setlinkvalue(EN_LEAK_EXPAN)`, in US units |
| **Where** | Code: `convert_pipe_to_node_leakage()` and `findlinkleakage()` in [`src/leakage.c:177`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/leakage.c#L177) and [`:291`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/leakage.c#L291). Docs: [`doc/toolkit-input.dox:252`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/doc/toolkit-input.dox#L252) and [`include/epanet2_enums.h:105`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/include/epanet2_enums.h#L105) |
| **Since** | The code reads per metre since [PR #901](https://github.com/OpenWaterAnalytics/EPANET/pull/901) (v2.3.4). The "ft or m" wording is in the v2.3 docs and was restated by [PR #906](https://github.com/OpenWaterAnalytics/EPANET/pull/906) (unreleased). |
| **GitHub** | [#899](https://github.com/OpenWaterAnalytics/EPANET/issues/899) (the design), [PR #901](https://github.com/OpenWaterAnalytics/EPANET/pull/901) (the code), [PR #906](https://github.com/OpenWaterAnalytics/EPANET/pull/906) (the new docs) |
| **Fix** | A decision. The patch corrects two documentation strings: [`SIM-04_fix.patch`](SIM-04_fix.patch) |

## The problem

Each `[LEAKAGE]` line gives a pipe a fixed leak area and a leak expansion rate: how much the leak area grows per unit of pressure head. The documentation says the rate is in "sq. mm per unit of modeled pressure head (in ft or m)", and the `EN_LEAK_EXPAN` comment says "sq mm per unit of pressure head". In US units that reads as sq mm per ft.

The code reads it as sq mm per **metre** of head in both unit systems. So in a US model with an expansion rate of 1 at 100 ft (30.48 m) of pressure head, the extra leak area is 30.48 sq mm, not 100 sq mm. The test pipe leaks 7.09 gpm; read per ft, as documented, it would leak 23.26 gpm.

The code is what the maintainers intended. In [#899](https://github.com/OpenWaterAnalytics/EPANET/issues/899) (January 2026) Lew Rossman wrote:

> The units of the leakage parameters supplied to the program are fixed at sq mm for leak area and sq mm per m for leak expansion, regardless of which unit system is being used by the rest of the input data, since this is how they are typically stated in the literature and will have "reasonable" values (imagine trying to express them in units of feet).

PR [#901](https://github.com/OpenWaterAnalytics/EPANET/issues/901) implemented that. PR [#906](https://github.com/OpenWaterAnalytics/EPANET/issues/906) (March 2026) then rewrote the `[LEAKAGE]` page and kept the "ft or m" wording.

## Why it happens

`leakage.c` converts both parameters to feet with fixed metre factors. The area is divided by `SQR(MperFT)` (sq mm to sq ft, apart from the 1e-6 in `c_orif`), and the expansion rate by `MperFT` once, which turns "per metre of head" into "per foot of head":

```c
// src/leakage.c, convert_pipe_to_node_leakage()
        c_area = c_orif * link->LeakArea / SQR(MperFT);
        c_expan = c_orif * link->LeakExpan / MperFT;
...
// src/leakage.c, findlinkleakage()
    a = link->LeakArea / SQR(MperFT);
    m = link->LeakExpan / MperFT;
```

Neither line depends on the project's unit system. The documentation says otherwise:

```
// doc/toolkit-input.dox, [LEAKAGE] (one line, shortened)
- the expansion in the average combined area of leakage-generating cracks as a function of the pressure head (in ft or m). ... expressed in units of sq. mm per unit of modeled pressure head (in ft or m)

// include/epanet2_enums.h
  EN_LEAK_EXPAN   = 27, //!< Leak expansion rate (sq mm per unit of pressure head)
```

The third remark on the same page already uses "(sq mm)/m" in its examples.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-04_us-leak.inp`](SIM-04_us-leak.inp) | GPM units. A 100 ft pipe from reservoir R1 to junction J1 at 100 ft of pressure head, with leak area 0 and expansion rate 1 |
| [`SIM-04_si-leak.inp`](SIM-04_si-leak.inp) | LPS units. The same with a 100 m pipe and 30 m of head |
| [`SIM-04_test.c`](SIM-04_test.c) | Compares EPANET's pipe leakage with the FAVAD equation, Q = 0.6 A √(2gh), for the rate read per metre and per unit of head |
| [`SIM-04_fix.patch`](SIM-04_fix.patch) | The documentation fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-04            # unpatched EPANET: PASS (shows the behaviour)
tools/run-test.sh SIM-04 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-04_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-04_test
./SIM-04_test
```

The patch changes only documentation, so the test cannot go from FAIL to PASS. It shows which reading the code uses, and passes when that is "per metre" in both unit systems, as the corrected documentation says. The output is the same with and without the patch:

```
                                      Expected leakage if the rate is per
Units  Head at J1   EPANET leakage    metre of head  unit of head (ft or m)
US      100.00 ft      7.0903 gpm         7.0886               23.2566
SI       30.00 m       0.4368 L/s         0.4367                0.4367
PASS: the expansion rate is read as sq mm per metre of head in both unit systems
```

The small difference from the hand calculation comes from EPANET's g = 32.2 ft/s² against 9.81 m/s² in the test.

## The fix

This is a decision between the code and the documentation. The patch takes the maintainers' stated design and corrects the documentation:

```diff
-- the expansion in the average combined area of leakage-generating cracks as a function of the pressure head (in ft or m). This is the derivative of the area with respect to the pressure head, and is expressed in units of sq. mm per unit of modeled pressure head (in ft or m)
+- the expansion in the average combined area of leakage-generating cracks as a function of the pressure head, per 100 units of pipe length (in ft or m). This is the derivative of the area with respect to the pressure head, and is expressed in units of sq. mm per m of pressure head regardless of the choice of model flow units.
...
-  EN_LEAK_EXPAN   = 27, //!< Leak expansion rate (sq mm per unit of pressure head)
+  EN_LEAK_EXPAN   = 27, //!< Leak expansion rate (sq mm per m of pressure head per 100 length units)
```

The new text also says that the rate is per 100 units of pipe length, like the leak area: the parser and `EN_setlinkvalue` scale both by `Ucf[LENGTH]`, and `leakage.c` multiplies both by the number of 100 ft sections.

**Alternative.** If "per unit of head" is preferred, change the code instead: in both places in `leakage.c`, use `link->LeakExpan * pr->Ucf[HEAD] / SQR(MperFT)`. SI models would not change. US models would get 1/0.3048 = 3.28 times more variable-area leakage than today, so existing US models with leakage would need their expansion rates divided by 3.28 to keep their results. That reverses the design decision in [#899](https://github.com/OpenWaterAnalytics/EPANET/issues/899), so it is not recommended.

EPANET's own unit tests (6 suites) pass with the patch.
