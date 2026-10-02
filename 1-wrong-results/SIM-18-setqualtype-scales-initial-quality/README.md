# SIM-18: EN_setqualtype scales initial quality by 1/28.3 when switching to CHEM

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. After a switch to a chemical analysis, every node's initial quality is 28.3 times too small, and the run starts from those values. `EN_CONCENLIMIT` reads 28.3 times too high after a switch away from a chemical. |
| **Reached from** | Toolkit API: `EN_setqualtype(EN_CHEM, ...)` on a project whose analysis type is AGE, TRACE or NONE, including a project created with `EN_init`; and `EN_setqualtype` away from CHEM for the limiting concentration |
| **Where** | `EN_setqualtype()` in [`src/epanet.c:1862-1902`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1862-L1902) |
| **Since** | EPANET 2.1 and earlier changed the units without converting anything. Commit [`3c78856`](https://github.com/OpenWaterAnalytics/EPANET/commit/3c788567a43eb6f84c6fedab7586da5402b27d9c) (2017, in v2.2) added the conversion for one direction only. |
| **GitHub** | No related issue |
| **Fix** | About ten lines in one function: [`SIM-18_fix.patch`](SIM-18_fix.patch) |

## The problem

EPANET stores concentrations internally per cubic foot: a chemical's values are kept as the user's mg/L value times 28.317 (litres per cubic foot). Water age and source trace values are kept as they are. When `EN_setqualtype` changes the analysis type, it changes the conversion factor, and every stored concentration has to be rescaled so that the user's values stay the same.

`EN_setqualtype` rescales node initial quality only when it switches from CHEM to AGE or TRACE. It never rescales the limiting concentration. In the test:

- Net1 starts with chlorine at 0.5 mg/L at node 10. After switching to water age and back to chlorine, `EN_INITQUAL` reads 0.0177 (0.5 / 28.317), and a chlorine run starts node 10 at 0.0177 mg/L.
- A project created with `EN_init` has no quality analysis. A junction's initial quality set to 1 reads 0.0353 after `EN_setqualtype(EN_CHEM)`. A program that builds a network through the API and then turns on chlorine therefore runs from initial values 28.3 times too small.
- A limiting concentration of 2 mg/L reads 56.634 after switching to water age.

## Why it happens

`Ucf[QUALITY]` is `1/LperFT3` for CHEM and 1 otherwise. Values are stored as `user value / Ucf[QUALITY]`. The function rescales the tolerance `Ctol` around the change of factor, but node `C0` only in one direction, and `Climit` not at all:

```c
// src/epanet.c, EN_setqualtype()
    qual->Ctol *= Ucf[QUALITY];
    if (qual->Qualflag == CHEM) // Chemical analysis
    {
        ...
        ccf = 1.0 / LperFT3;
    }
    ...
    // When changing from CHEM to AGE or TRACE, nodes initial quality
    // values must be returned to their original ones
    if ((qual->Qualflag == AGE || qual->Qualflag == TRACE) && oldQualFlag == CHEM)
    {
        for (i = 1; i <= p->network.Nnodes; i++)
        {
            p->network.Node[i].C0 *= Ucf[QUALITY];
        }
    }

    Ucf[QUALITY] = ccf;
    Ucf[LINKQUAL] = ccf;
    Ucf[REACTRATE] = ccf;
    qual->Ctol /= Ucf[QUALITY];
```

`Climit` is converted with `Ucf[QUALITY]` when an input file is read ([`src/input1.c:577`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L577)) and in `EN_getoption` and `EN_setoption`, so it needs the same treatment as `Ctol`. Source strengths are stored in user units and converted during the run, so they are not affected.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-18_Net1.inp`](SIM-18_Net1.inp) | EPANET's Net1 example, unchanged (chlorine, node 10 starts at 0.5 mg/L) |
| [`SIM-18_test.c`](SIM-18_test.c) | Switches Net1 from chlorine to age and back, reading `EN_INITQUAL` and `EN_CONCENLIMIT` after each step, starts a chlorine run, then repeats the check on a project created with `EN_init` |
| [`SIM-18_fix.patch`](SIM-18_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-18            # unpatched EPANET: FAIL
tools/run-test.sh SIM-18 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-18_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-18_test
./SIM-18_test
```

**Without the fix:**

```
Step                             Value            Actual  Expected
Net1 as loaded (chlorine)        INITQUAL(10)     0.5000    0.5000   
                                 CONCENLIMIT      2.0000    2.0000   
after EN_setqualtype(EN_AGE)     INITQUAL(10)     0.5000    0.5000   
                                 CONCENLIMIT     56.6340    2.0000   <-- wrong
after EN_setqualtype(EN_CHEM)    INITQUAL(10)     0.0177    0.5000   <-- wrong
                                 CONCENLIMIT      2.0000    2.0000   
chlorine run, time 0             QUALITY(10)      0.0177    0.5000   <-- wrong
EN_init, then INITQUAL = 1       INITQUAL(J1)     1.0000    1.0000   
after EN_setqualtype(EN_CHEM)    INITQUAL(J1)     0.0353    1.0000   <-- wrong
FAIL: 4 values changed when the quality type was switched
```

**With the fix**, every value is the expected one:

```
after EN_setqualtype(EN_AGE)     INITQUAL(10)     0.5000    0.5000   
                                 CONCENLIMIT      2.0000    2.0000   
after EN_setqualtype(EN_CHEM)    INITQUAL(10)     0.5000    0.5000   
...
chlorine run, time 0             QUALITY(10)      0.5000    0.5000   
EN_init, then INITQUAL = 1       INITQUAL(J1)     1.0000    1.0000   
after EN_setqualtype(EN_CHEM)    INITQUAL(J1)     1.0000    1.0000   
PASS: switching the quality type keeps the values in user units
```

## The fix

Rescale node initial quality by the ratio of the old and new factors for every switch, and convert `Climit` the same way as `Ctol`:

```diff
     qual->Ctol *= Ucf[QUALITY];
+    qual->Climit *= Ucf[QUALITY];
 ...
-    // When changing from CHEM to AGE or TRACE, nodes initial quality
-    // values must be returned to their original ones
-    if ((qual->Qualflag == AGE || qual->Qualflag == TRACE) && oldQualFlag == CHEM)
+    // Convert nodes initial quality values to the new concentration
+    // units, so that their values in user units do not change
+    for (i = 1; i <= p->network.Nnodes; i++)
     {
-        for (i = 1; i <= p->network.Nnodes; i++)
-        {
-            p->network.Node[i].C0 *= Ucf[QUALITY];
-        }
+        p->network.Node[i].C0 *= Ucf[QUALITY] / ccf;
     }
 ...
     qual->Ctol /= Ucf[QUALITY];
+    qual->Climit /= Ucf[QUALITY];
```

The patch also removes `oldQualFlag`, which the new loop no longer needs. A switch to the same type multiplies by 1 and changes nothing.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The review logged this twice: once from the API (reading `EN_INITQUAL`) and once from a chlorine run that started from 0.0177 mg/L. Both are covered here.
