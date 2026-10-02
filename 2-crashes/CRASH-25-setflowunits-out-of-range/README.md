# CRASH-25: EN_setflowunits does not check its argument

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | An out-of-range flow units code is accepted with return code 0. EPANET then reads past the end of its table of unit names, and converts flows with the CFS factor. |
| **Reached from** | Toolkit API: `EN_setflowunits` with a code below `EN_CFS` (0) or above `EN_CMS` (10) |
| **Where** | `EN_setflowunits()` in [`src/epanet.c:1494`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1494); the out-of-bounds read is in `initunits()` at [`src/input1.c:430`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L430) |
| **Since** | EPANET 2.2, when `EN_setflowunits` was added (2018) |
| **GitHub** | No related issue |
| **Fix** | One line: [`CRASH-25_fix.patch`](CRASH-25_fix.patch) |

## The problem

`EN_init` rejects a flow units code outside `EN_CFS` to `EN_CMS` with error 251. `EN_setflowunits` takes the same codes but does not check them. With 11, one past the end, it returns 0 and stores 11 as the project's flow units. `initunits()` then copies the units label from `RptFlowUnitsTxt[11]`, one entry past the end of an 11-entry table. AddressSanitizer reports a global buffer overflow; without it, the label is whatever pointer follows the table in memory, and a code far out of range can crash.

If the call survives, the project is in a state no input file can produce: the code is not US or SI, so the US branch of `initunits()` applies, with the CFS factor 1.0 for flows. `EN_saveinpfile` would index `FlowUnitsTxt[11]` the same way to write the `UNITS` option.

## Why it happens

```c
// src/epanet.c, EN_setflowunits()
if (!p->Openflag) return 102;
...
p->parser.Flowflag = units;
switch (units)
{
case LPS:
...
    p->parser.Unitsflag = SI;
    break;
default:
    p->parser.Unitsflag = US;
    break;
}
initunits(p);

// src/input1.c, initunits(), US branch
strcpy(rpt->Field[DEMAND].Units, RptFlowUnitsTxt[parser->Flowflag]);
```

`EN_init` has the check that is missing here:

```c
// src/epanet.c, EN_init()
if (unitsType < 0 || unitsType > CMS) return 251;
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-25_network.inp`](CRASH-25_network.inp) | Reservoir R1 feeds junction J1 through pipe P1, in GPM |
| [`CRASH-25_test.c`](CRASH-25_test.c) | Calls `EN_setflowunits(ph, 11)` and checks the return code and the units afterwards |
| [`CRASH-25_fix.patch`](CRASH-25_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-25            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-25 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-25_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-25_test
./CRASH-25_test
```

**Without the fix**, under AddressSanitizer:

```
EN_setflowunits(ph, 11) ...
==...==ERROR: AddressSanitizer: global-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in initunits input1.c:430
    #1 ... in EN_setflowunits epanet.c:1509
    #2 ... in main CRASH-25_test.c:25
...
... is located 0 bytes after global variable 'RptFlowUnitsTxt' defined in '.../src/epanet.c' (...) of size 88
```

Without sanitizers the test prints `FAIL`, because the call returns 0.

**With the fix:**

```
EN_setflowunits(ph, 11) ...
  returned 251 (expected 251); flow units code is now 1 (expected 1, GPM)
PASS: the out-of-range code is rejected and the units are unchanged
```

## The fix

Add the check from `EN_init`, before anything is changed:

```diff
     if (!p->Openflag) return 102;
+    if (units < 0 || units > CMS) return 251;
```

EPANET's own unit tests (6 suites) pass with the patch.
