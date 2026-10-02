# FILE-04: Pumps and GPVs without a curve are written in a form the parser rejects

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A project with a pump that has no head curve or power, or a GPV with no head-loss curve, saves without error but the saved file does not open (error 200). |
| **Reached from** | Toolkit API: `EN_saveinpfile` after `EN_addlink(EN_PUMP)` or `EN_addlink(EN_GPV)` without assigning a curve, or after opening a file with a bare pump line such as `PU1 R1 J1` |
| **Where** | `saveinpfile()` in [`src/inpfile.c:255-263`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L255-L263) (pumps) and [`:317-327`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L317-L327) (GPVs); the readers are `pumpdata()` and `valvedata()` in [`src/input3.c:517`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L517) and [`:641-646`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L641-L646) |
| **Since** | Pumps: v2.3, when PR [#758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (`EN_openX`, commit [10d5079](https://github.com/OpenWaterAnalytics/EPANET/commit/10d5079d750ab6a8b2070bb20778346afacc37ff)) removed the reader for the old pump format. GPVs: since `EN_addlink` (v2.2). |
| **GitHub** | No related issue |
| **Fix** | 4 lines added, 20 removed: [`FILE-04_fix.patch`](FILE-04_fix.patch) |

## The problem

A model that is being built through the toolkit often has a pump or a GPV whose curve is not assigned yet. `EN_addlink` creates both in that state, and the parser accepts a pump line with no `HEAD` or `POWER` keyword. Saving such a project returns 0, but the saved file cannot be opened:

- The pump is written in the numeric pump-curve format of EPANET 1.x (`PU1 R1 J1 -0.0000 -0.0000 0.0000 0.0 0.0000`). `pumpdata()` no longer reads that format and reports error 201. Its speed goes to `[STATUS]` instead of the pump line.
- The GPV is written with `0.0000` in place of a curve ID, and `valvedata()` reports error 206, undefined curve.

## Why it happens

```c
// src/inpfile.c, saveinpfile(), [PUMPS]
// Old format used for pump curve
else
{
    fprintf(f, "\n%s %12.4f %12.4f %12.4f          0.0 %12.4f", s,
           -pump->H0 * pr->Ucf[HEAD],
           (-pump->H0 - pump->R * pow(pump->Q0, pump->N)) * pr->Ucf[HEAD],
           pump->Q0 * pr->Ucf[FLOW], pump->Qmax * pr->Ucf[FLOW]);
    continue;
}
...
// [VALVES]
// For GPV, setting = head curve index
if (link->Type == GPV && (j = ROUND(kc)) > 0)
{
    sprintf(s1, "%-31s\t%-12.4f", net->Curve[j].ID, km);
}
...
else sprintf(s1, "%-12.4f\t%-12.4f", kc, km);    // a GPV with curve 0 ends up here
```

The parser already has a form for each: `pumpdata()` returns early for a line with only an ID and two nodes (`if (n < 4) return 0;`), leaving a pump with no curve. `valvedata()` reads a setting only when a sixth token is present (`if (n > 5)`), so `V1 J1 J2 12 GPV` loads as a GPV with curve index 0, the same state `EN_addlink` creates.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-04_pump-no-curve.inp`](FILE-04_pump-no-curve.inp) | Reservoir R1, pump PU1 with `SPEED 0.9` but no curve or power, and junctions J1 and J2 |
| [`FILE-04_test.c`](FILE-04_test.c) | Adds GPV V1 with `EN_addlink` and no curve, saves the project, prints the lines written for PU1 and V1, reopens the file and checks both links |
| [`FILE-04_fix.patch`](FILE-04_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-04            # unpatched EPANET: FAIL
tools/run-test.sh FILE-04 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-04_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-04_test
./FILE-04_test
```

**Without the fix** (the test shortens runs of blanks and tabs in the saved lines to one space):

```
Lines written for PU1 and V1:
  PU1 R1 J1 -0.0000 -0.0000 0.0000 0.0 0.0000
  PU1 0.9000
  V1 J1 J2 10.0000 GPV 0.0000 0.0000 

EN_open(saved file) returned 200
  Error 201: syntax error -0.0000 in [PUMPS] section:
  Error 206: undefined curve 0.0000 in [VALVES] section:
  Error 200: one or more errors in input file
FAIL: the saved file does not load (error 200)
```

**With the fix**:

```
Lines written for PU1 and V1:
  PU1 R1 J1 SPEED 0.9000 ;no HEAD or POWER keyword: accepted by the parser
  V1 J1 J2 10.0000 GPV 

EN_open(saved file) returned 0
PU1: head curve 0, speed 0.90
V1:  type GPV, curve 0
PASS: the saved file reloads with the same curveless pump and GPV
```

## The fix

Write each link in the form the parser already accepts:

```diff
-        // Old format used for pump curve
-        else
-        {
-            fprintf(f, "\n%s %12.4f %12.4f %12.4f          0.0 %12.4f", s,
-            ...
-            continue;
-        }
+        // Pump has no curve or power assigned yet
+        else strcpy(s1, "");
         strcat(s, s1);
...
+        // GPV with no head curve assigned yet: leave out the setting
+        else if (link->Type == GPV) strcpy(s1, "");
```

A curveless pump now goes through the normal code, so its `PATTERN` and `SPEED` keywords are written on the pump line (a pump with neither is written as `PU1 R1 J1`). The patch also removes the `[STATUS]` entry that carried the speed for the old format, since the pump line now has it.

**What to write for a GPV with no curve** was a choice. The options:

1. **Write the line without a setting** (the patch). The file loads and the project is the same as before the save, as it is for a curveless pump. The minor-loss coefficient cannot be written without a setting in front of it, so a non-zero one set before the curve is lost.
2. **Refuse to save** (return an error). This would stop a GUI from saving work in progress, although the toolkit lets the user create that state.
3. **Leave the valve out.** The saved network would lose a link and change its connectivity.

With option 1, the missing curve is reported when a run starts, as error 226 already is for a pump with no curve. At 83e25bb a GPV with no curve crashes the solver instead, whether it comes from the API or from a GPV line without a curve. [CRASH-16](../../2-crashes/CRASH-16-gpv-without-curve/) adds that check for both paths (error 206 in the report, and `EN_openH` returns 110).

EPANET's own unit tests (6 suites) pass with the patch.
