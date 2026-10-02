# FILE-15: The illegal-valve check skips the link read just before the valve

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | An input file with an illegal valve pair (for example two PRVs feeding the same node) loads without error when the two valves are on consecutive lines. The same pair is rejected through the toolkit and when the lines are apart. |
| **Reached from** | Input file: two valves on consecutive [VALVES] lines whose combination `valvecheck()` forbids |
| **Where** | `valvedata()` in [`src/input3.c:602`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L602) |
| **Since** | v2.2 (2019): commit [ce2a27f](https://github.com/OpenWaterAnalytics/EPANET/commit/ce2a27f38e55d7a201e7ee1d28edac8deb4ed46c) in pull request [#562](https://github.com/OpenWaterAnalytics/EPANET/pull/562) |
| **GitHub** | A regression from the fix for [#561](https://github.com/OpenWaterAnalytics/EPANET/issues/561) (reversing a PRV with `EN_setlinknodes` gave error 220) |
| **Fix** | One line: [`FILE-15_fix.patch`](FILE-15_fix.patch) |

## The problem

EPANET rejects some valve combinations with error 220, "illegal valve connection to another valve": for example two PRVs that share a downstream node, two PSVs that share an upstream node, or a PSV directly downstream of a PRV. The solver cannot handle these.

When an input file is read, the check misses a pair whose two valves are on consecutive lines. In the test, PRVs V1 (J1 to J2) and V2 (J3 to J2) both end at J2. On consecutive lines the file loads with no error. With a pipe line between them the same file is rejected with error 220, and `EN_addlink` rejects the same valve with 220 too.

Files written by EPANET put all valves in one [VALVES] section, one after another, so the case the check misses is the common one.

## Why it happens

`valvecheck()` compares the new valve with every existing valve, except the one whose link index it is given. The skip exists for `EN_setlinknodes` and `EN_setlinktype`, which check a valve that is already in the network and must not compare it with itself. The parser calls it before the new valve is added and passes `net->Nlinks`, which at that point is the index of the previous link:

```c
// src/input3.c, valvedata()
// Check for illegal connections
if (valvecheck(pr, net->Nlinks, type, j1, j2))     // skips the previous link
{
    ...
}

// Add new valve to data base
errcode = addlinkID(net, net->Nlinks+1, parser->Tok[0]);

// src/project.c, valvecheck()
for (k = 1; k <= net->Nvalves; k++)
{
    valve = &net->Valve[k];
    if (valve->Link == index) continue;
    ...
```

If the previous link is a pipe or pump the skip does nothing, because only valves are compared. If it is a valve, that valve is never checked against the new one. `EN_addlink` passes 0 ([`epanet.c:3431`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3431)).

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-15_consecutive-prvs.inp`](FILE-15_consecutive-prvs.inp) | R1 feeds four junctions. PRVs V1 (J1 to J2) and V2 (J3 to J2) are on consecutive lines. |
| [`FILE-15_separated-prvs.inp`](FILE-15_separated-prvs.inp) | The same network, with pipe P4 read between the two valves |
| [`FILE-15_test.c`](FILE-15_test.c) | Opens both files and checks that each is rejected |
| [`FILE-15_fix.patch`](FILE-15_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-15            # unpatched EPANET: FAIL
tools/run-test.sh FILE-15 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-15_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-15_test
./FILE-15_test
```

The command-line runner shows it too: `runepanet FILE-15_consecutive-prvs.inp FILE-15.rpt` runs to completion.

**Without the fix**, only the file with a pipe between the valves is rejected:

```
FILE-15_consecutive-prvs.inp   EN_open returned   0 (expected 200)
FILE-15_separated-prvs.inp     EN_open returned 200 (expected 200)
FAIL: the illegal PRV pair is accepted when the valves are on consecutive lines
```

**With the fix**, both are rejected, and the report has `Error 220: illegal valve connection to another valve  in [VALVES] section:`:

```
FILE-15_consecutive-prvs.inp   EN_open returned 200 (expected 200)
FILE-15_separated-prvs.inp     EN_open returned 200 (expected 200)
PASS: the illegal PRV pair is rejected in both files
```

## The fix

The valve being read has no link index yet, so there is nothing to skip:

```diff
-    if (valvecheck(pr, net->Nlinks, type, j1, j2))
+    if (valvecheck(pr, 0, type, j1, j2))
```

EPANET's own unit tests (6 suites) pass with the patch.

Input files that loaded before may now be rejected with error 220. Those networks contain a valve pair that EPANET itself forbids, so the error is correct, but it is worth a line in the release notes.
