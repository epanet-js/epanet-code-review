# API-29: EN_addlink leaves a new pump's diameter and a new link's flow, status and setting unset

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A new link takes values from the link that last used its slot. In the test, a pump made with `EN_setlinktype` is reported with an 8 in diameter, and a new PRV reads 100 gpm, status open and a 43.33 psi setting before it has ever been run. |
| **Reached from** | Toolkit API: `EN_addlink`, or `EN_setlinktype` (which deletes the link and adds a new one). The diameter shows in a report with `DIAMETER YES`; the flow, status and setting show in `EN_getlinkvalue` until the next run. |
| **Where** | `EN_addlink()` in [`src/epanet.c:3436-3445`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3436-L3445) (the arrays grow) and [`:3489-3529`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3489-L3529) (the new link's values); the parser sets a pump's diameter to 0 at [`src/input3.c:496`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L496) |
| **Since** | v2.2, the first release with `EN_addlink` |
| **GitHub** | No related issue |
| **Fix** | Four lines: [`API-29_fix.patch`](API-29_fix.patch) |

## The problem

`EN_addlink` puts a new link at the end of the link array and of the solver's flow, status and setting arrays. The arrays grow with `realloc` and never shrink, and `EN_deletelink` moves later links down without clearing the last slot. So after any deletion, the slot a new link gets still holds another link's values. `EN_setlinktype` to a pump or valve always works this way: it deletes the link and adds a new one. Some values are not set by `EN_addlink`, and the new link keeps the old ones:

- **A pump's diameter.** A pump has no diameter: the input file parser sets 0, and the report prints 0.00. `EN_addlink` sets the diameter of a new pipe or valve but not of a pump. In the test, pipe P1 is turned into a pump and the report gives it the 8.00 in of pipe P2, whose copy sat in the last slot. The report also uses the value for its `DIAMETER BELOW` and `DIAMETER ABOVE` limits.
- **The flow, status and setting.** `EN_addlink` never writes the new link's entries in these solver arrays. In the test, after a run, pipe P2 (100 gpm) is deleted and a pressure-reducing valve V9 is added. Before any run V9 reads P2's 100 gpm, an open status instead of active, and a setting of 43.33 psi, which is P2's roughness of 100 read as a pressure. By reading `inithyd()`, the next run resets the status and setting but keeps any flow larger than `QZERO` as the starting flow (unless `EN_initH` is given `EN_INITFLOW`), so the stale flow also becomes V9's starting value.

Both checks give the same result in a build without sanitizers, because the values come from a reused slot. In a slot that was never used the values are uninitialised memory; in a separate run, a pump added with `EN_addlink` alone reported -0.00 in under AddressSanitizer, which fills new memory with a fixed byte pattern, and 0.00 without it.

By reading the code, the stale diameter shows only in the report: `EN_getlinkvalue(EN_DIAMETER)` returns 0 for any pump, the binary output file writes 0 for pumps, and water quality only gives pipes a volume. The review log also listed the binary output and the pipe volume; those exclude pumps.

## Why it happens

```c
// src/epanet.c, EN_addlink()
    size = (n + 1) * sizeof(double);
    hyd->LinkFlow = (double *)realloc(hyd->LinkFlow, size);
    hyd->LinkSetting = (double *)realloc(hyd->LinkSetting, size);
    size = (n + 1) * sizeof(StatusType);
    hyd->LinkStatus = (StatusType *)realloc(hyd->LinkStatus, size);
    ...
    if (linkType == PUMP)
    {
        link->Kc = 1.0; // Speed factor
        link->Km = 0.0; // Horsepower
        link->Len = 0.0;
    }
    else if (linkType <= PIPE)  // pipe or cvpipe
    {
        // 10" diameter new ductile iron pipe with
        // length of average city block
        link->Diam = 10 / p->Ucf[DIAM];
```

Nothing after the `realloc` calls writes `LinkFlow[n]`, `LinkStatus[n]` or `LinkSetting[n]`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-29_booster.inp`](API-29_booster.inp) | Pump PU1 (link 1) lifts water from R1 to J1; pipes P1 (link 2, 12 in, 150 gpm) and P2 (link 3, 8 in, 100 gpm) carry it on to J2 and J3. The report has a Diameter column. |
| [`API-29_test.c`](API-29_test.c) | 1: turns P1 into a 5 hp pump with `EN_setlinktype`, solves, and reads both pumps' diameters from the report. 2: in a new project, solves, deletes P2, adds PRV V9 from J1 to J3, and reads V9's flow, status and setting before running again. |
| [`API-29_fix.patch`](API-29_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-29            # unpatched EPANET: FAIL
tools/run-test.sh API-29 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-29_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-29_test
./API-29_test
```

**Without the fix**, the link table in `API-29.rpt` gives the new pump P2's diameter:

```
  Link                   in       gpm       fps   /1000ft
  --------------------------------------------------------
  PU1                  0.00    200.00      0.00   -197.80  Pump
  P2                   8.00    100.00      0.64      0.42
  P1                   8.00    150.00      0.00   -131.87  Pump
```

and the test prints:

```
1. Diameter in the report: PU1 0.00 in, P1 (now a pump) 8.00 in (expected 0.00 for both)
2. P2 carried 100.00 gpm. New PRV V9 (link 3) before a run:
   flow 100.00 gpm, status 1, setting 43.33 psi (expected 0.00 gpm, status 2 = active, 0.00 psi)
FAIL: the new links carry values left over from other links
```

**With the fix**:

```
1. Diameter in the report: PU1 0.00 in, P1 (now a pump) 0.00 in (expected 0.00 for both)
2. P2 carried 100.00 gpm. New PRV V9 (link 3) before a run:
   flow 0.00 gpm, status 2, setting 0.00 psi (expected 0.00 gpm, status 2 = active, 0.00 psi)
PASS: new links start with no diameter (pumps) and no results
```

## The fix

Set a new pump's diameter to 0, as the parser does, and start every new link with no flow at its initial status and setting:

```diff
         link->Kc = 1.0; // Speed factor
         link->Km = 0.0; // Horsepower
         link->Len = 0.0;
+        link->Diam = 0.0;
     }
 ...
     link->InitSetting = link->Kc;
+
+    // A new link has no results yet: no flow, and its initial status
+    // and setting
+    hyd->LinkFlow[n] = 0.0;
+    hyd->LinkStatus[n] = link->InitStatus;
+    hyd->LinkSetting[n] = link->InitSetting;
+
     link->R = 0;
```

With a flow of 0, `inithyd()` gives the link its usual starting flow at the next run. [API-26](../API-26-link-status-before-run/) gives the links of a loaded network their initial status and setting before the first run; this patch does the same for a link added later, and the two patches apply together. The second block sits in the middle of the new link's defaults rather than at the end of the function, so that it stays clear of the line that [API-05](../API-05-stale-hydraulics-after-edit/) adds there. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The pump diameter was entry A17 in the review log, but it was left out of the earlier summary. The flow, status and setting were pointed out by another group while this was written up, and are fixed in the same patch because they come from the same slot.
- [API-27](../API-27-results-on-wrong-objects-after-edit/) covers the other editing functions: `EN_addnode`, `EN_deletenode` and `EN_deletelink` renumber nodes and links without moving their results.
