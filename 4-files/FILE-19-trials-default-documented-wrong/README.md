# FILE-19: The documented default for TRIALS is 40; the code uses 200

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Documentation only. A user who relies on the reference believes that a network without a `TRIALS` line gets 40 trials per time step; it gets 200. |
| **Reached from** | Input file: an [OPTIONS] section without `TRIALS`, and the same default for projects built through the toolkit |
| **Where** | The [OPTIONS] page of the input file reference, [`doc/toolkit-input.dox:357`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/doc/toolkit-input.dox#L357); the code's value is `MAXITER` in [`src/input1.c:25`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L25) |
| **Since** | The sentence was added in commit [2bbbf73](https://github.com/OpenWaterAnalytics/EPANET/commit/2bbbf736b0bf49b633dd73d261f0147a4265e65c) (May 2019) and has been in the documentation of every release since v2.2 |
| **GitHub** | No related issue |
| **Fix** | One word in the documentation: [`FILE-19_fix.patch`](FILE-19_fix.patch) |

## The problem

The input file reference says:

> **TRIALS** are the maximum number of trials used to solve network hydraulics at each hydraulic time step of a simulation. The default is 40.

The code uses 200. The report's input summary for such a network says `Maximum Trials .................... 200`, and `EN_getoption(EN_TRIALS)` returns 200.

The other option defaults on the same page (`ACCURACY` 0.001, `CHECKFREQ` 2, `MAXCHECK` 10, `DAMPLIMIT` 0, `EMITTER EXPONENT` 0.5, `TOLERANCE` 0.01 and so on) match the code. `TRIALS` is the only one that does not.

## Which value is intended

The evidence points to 200:

- The source marks 200 as a deliberate change: `#define MAXITER 200 ... //(2.00.12 - LR)` in EPANET 2.00.12 (2008). The value has not changed since.
- The input file appendix of the EPANET 2.2 Users Manual (USEPA/EPANET2.2, `User_Manual/docs/input_keywords.rst`) says "The default is 200."
- The same manual's chapter on the GUI analysis options says "Suggested value is 40". The GUI keeps its own defaults, which is probably where 40 in the toolkit reference came from.

So the documentation, not the code, should change.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-19_no-trials-option.inp`](FILE-19_no-trials-option.inp) | Reservoir R1 feeds junction J1. [OPTIONS] does not set `TRIALS`. |
| [`FILE-19_test.c`](FILE-19_test.c) | Opens the file and prints the `TRIALS` value EPANET uses |
| [`FILE-19_fix.patch`](FILE-19_fix.patch) | The fix (documentation only) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-19            # unpatched EPANET: PASS (the code is right)
tools/run-test.sh FILE-19 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-19_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-19_test
./FILE-19_test
```

The patch changes only the documentation, so the test cannot fail without it. It records the value the documentation should state, and passes on both builds:

```
EN_open returned 0
Default TRIALS used by EPANET:   200
Default stated in toolkit-input.dox at 83e25bb: 40
PASS: the default TRIALS is 200, as the corrected documentation states
```

## The fix

```diff
-\b TRIALS are the maximum number of trials used to solve network hydraulics at each hydraulic time step of a simulation. The default is 40.
+\b TRIALS are the maximum number of trials used to solve network hydraulics at each hydraulic time step of a simulation. The default is 200.
```

No other file in the repository states the default. `EN_TRIALS` in `epanet2_enums.h` and the bindings (`epanet2.bas`, `.cs`, `.pas`, `.vb`) describe the option without a value.

EPANET's own unit tests (6 suites) pass with the patch.
