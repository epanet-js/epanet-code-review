# FILE-16: [PIPES], [PUMPS] and [VALVES] lines with too few fields are silently dropped

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A link whose input line is too short to define it is left out of the network without any error or warning, and the model runs without it. |
| **Reached from** | Input files (`EN_open`, `EN_openX`, `runepanet`): a `[PIPES]` or `[PUMPS]` line with fewer than 3 fields, or a `[VALVES]` line with fewer than 5 |
| **Where** | `pipedata()`, `pumpdata()` and `valvedata()` in [`src/input3.c:355`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L355), [`:479`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L479) and [`:586`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L586) |
| **Since** | v2.3. A regression from [PR #758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (`EN_openX`, commit [10d5079](https://github.com/OpenWaterAnalytics/EPANET/commit/10d5079d750ab6a8b2070bb20778346afacc37ff), October 2023). EPANET 2.2 reported these lines as error 201. |
| **GitHub** | [#746](https://github.com/OpenWaterAnalytics/EPANET/issues/746), the request behind PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758). Its design says every skipped or defaulted item "would be flagged with a message added to the Status Report". No issue reports this bug. |
| **Fix** | Three tokens: [`FILE-16_fix.patch`](FILE-16_fix.patch) |

## The problem

PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) made most fields of a link line optional, so that a partly built model can still be opened: a pipe needs only its ID and two end nodes, and the other fields take default values. A line too short even for that cannot define a link. EPANET 2.2 rejected it with error 201 ("syntax error"). Since PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) it is skipped with no message at all.

In the test file, three of seven link lines are short: pipe P2 and pump PU2 have lost their end node, and valve V2 has lost its type. `EN_open` returns 0, the three links are not in the project, and `EN_solveH` returns 0. `runepanet` reports `EPANET ran successfully` with 2 pipes, 1 pump and 1 valve instead of 3, 2 and 2. A model with a damaged line therefore runs without a pipe, pump or valve, and nothing in the report says so. If the missing link isolates part of the network, the user gets an unconnected-node error that points at the wrong cause.

## Why it happens

PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) rewrote the link parsers so that each one declares `int errcode = 0;` and uses it later for `addlinkID()`. The early length check passes that variable to `setError()` before anything has set it:

```c
// src/input3.c, pipedata()
    int errcode = 0;
    ...
    n = parser->Ntokens;
    if (n < 3) return setError(parser, -1, errcode);

// src/input3.c, pumpdata()
    if (n < 3) return setError(parser, -1, errcode);

// src/input3.c, valvedata()
    if (n < 5) return setError(parser, -1, errcode);
```

`setError()` returns its third argument, so the functions return 0 and `readdata()` treats the line as read. `tankdata()`, rewritten in the same PR, sets `errcode = 201` for its short lines.

The other section parsers were checked for the same pattern:

- `[COORDINATES]` and `[VERTICES]` lines that are too short are also ignored without a message (they returned 201 in 2.2). That is deliberate: both functions are documented with "since node coords. are not used in any computations, invalid data are simply ignored".
- Short `[QUALITY]`, `[REACTIONS]` and `[MIXING]` lines, and `[OPTIONS]` keywords without a value, are also ignored, but they were in 2.2 as well. They are not part of this issue.
- `[TANKS]`, `[PATTERNS]`, `[CURVES]`, `[DEMANDS]`, `[CONTROLS]`, `[SOURCES]`, `[EMITTERS]`, `[LEAKAGE]` and `[STATUS]` return 201 for short lines. A `[JUNCTIONS]` line needs only its ID.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-16_short-lines.inp`](FILE-16_short-lines.inp) | Seven links. `P2 R1` and `PU2 R2` have no end node, and `V2 J3 J4 12` has no valve type. Each has a complete twin in parallel (P1, PU1, V1), so the network still solves without them. |
| [`FILE-16_test.c`](FILE-16_test.c) | Opens the file. If it loads, lists which links are missing and solves; otherwise counts the `Error 201` lines in the report |
| [`FILE-16_fix.patch`](FILE-16_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-16            # unpatched EPANET: FAIL
tools/run-test.sh FILE-16 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-16_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-16_test
./FILE-16_test
```

or with the command-line runner:

```sh
runepanet FILE-16_short-lines.inp FILE-16.rpt
```

**Without the fix:**

```
EN_open returned 0
  P1   loaded
  P2   <-- not in the project
  P3   loaded
  PU1  loaded
  PU2  <-- not in the project
  V1   loaded
  V2   <-- not in the project
EN_solveH returned 0
FAIL: 3 of 7 link lines were dropped without any error
```

`runepanet` prints `... EPANET ran successfully.`, and the report's summary has:

```
      Number of Pipes ................... 2
      Number of Pumps ................... 1
      Number of Valves .................. 1
```

**With the fix:**

```
EN_open returned 200
Report lines with "Error 201": 3
PASS: each short link line is reported as error 201
```

and the report shows each line:

```
  Error 201: syntax error  in [PIPES] section:
   P2  R1                                         ; end node missing
...
  Error 201: syntax error  in [PUMPS] section:
   PU2  R2                                        ; end node missing
...
  Error 201: syntax error  in [VALVES] section:
   V2  J3  J4  12                                 ; valve type missing
...
  Error 200: one or more errors in input file
```

## The fix

Pass the error code itself:

```diff
 // pipedata() and pumpdata()
-    if (n < 3) return setError(parser, -1, errcode);
+    if (n < 3) return setError(parser, -1, 201);
 // valvedata()
-    if (n < 5) return setError(parser, -1, errcode);
+    if (n < 5) return setError(parser, -1, 201);
```

`EN_open` then fails with 200, as it does for any other input error. `EN_openX` still opens the file, as it is meant to, but now writes the three messages to the report. The minimum field counts are unchanged, so lines that PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) made valid (for example `P2 R1 J1` with default length, diameter and roughness) are still accepted.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- This was found while checking [CRASH-16](../../2-crashes/CRASH-16-gpv-without-curve/): a `[VALVES]` GPV line with no curve field has 5 fields, so it passes this check and loads with no curve.
- [FILE-15](../FILE-15-valve-check-skips-previous-link/) changes another line of `valvedata()`. The two patches apply together.
