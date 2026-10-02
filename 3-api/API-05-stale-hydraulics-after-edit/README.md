# API-05: Structural edits do not invalidate the saved hydraulics

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After a node or link is added, deleted, reconnected or retyped, `EN_solveQ` returns 0 and uses hydraulic results saved for the old network, read out of alignment. Flows, quality and the output file are wrong with no warning. |
| **Reached from** | Toolkit API: `EN_solveH` (or a saved `EN_initH`/`EN_runH`/`EN_nextH` run), then any of `EN_addnode`, `EN_deletenode`, `EN_addlink`, `EN_deletelink`, `EN_setlinktype`, `EN_setlinknodes`, then `EN_solveQ`, `EN_openQ` or `EN_saveH` without re-running the hydraulics |
| **Where** | `EN_openQ()` checks only `outfile.SaveHflag` at [`src/epanet.c:736`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L736); none of the six editing functions clears it. `readhyd()` reads each record with the current counts at [`src/output.c:360-373`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L360-L373). |
| **Since** | v2.2, when the network editing functions were added |
| **GitHub** | No related issue |
| **Fix** | One statement in each of six functions: [`API-05_fix.patch`](API-05_fix.patch). Apply with [CRASH-11](../../2-crashes/CRASH-11-closeq-not-idempotent/) (see below). |

## The problem

A toolkit program often solves a network, edits it and solves it again. If it re-runs only the water quality step after the edit, EPANET does not notice that the saved hydraulics belong to a different network. `EN_solveQ` returns 0 and routes water with flows that belong to other links.

In the test network, reservoir R1 supplies junction J3 (300 gpm) through pipe P1, two parallel pipes P2 and P3, and pipe P4. After `EN_deletelink(P2)` and a second `EN_solveQ`, P4 reads 41.7 gpm, which is P3's old flow. The same happens after each of the six editing functions. In the original review run on Net1 with its chlorine analysis, deleting link 1 this way changed node 5's final chlorine from 0.5938 to 0.5 mg/L, with a return code of 0.

## Why it happens

`EN_solveH` (or a run started with `EN_initH(EN_SAVE)`) writes one record per time step to a scratch hydraulics file and sets `outfile.SaveHflag`. Each record holds `Nnodes` demands and heads and `Nlinks` flows, statuses and settings. `EN_openQ` only checks the flag:

```c
// src/epanet.c, EN_openQ()
    if (!p->hydraul.OpenHflag && !p->outfile.SaveHflag) return 104;
```

and `readhyd()` reads each record with the counts the network has now:

```c
// src/output.c, readhyd()
    if (f_read(x, net->Nnodes, HydFile) < (unsigned)net->Nnodes) result = 0;
    else for (i = 1; i <= net->Nnodes; i++) hyd->NodeDemand[i] = x[i];
    ...
    if (f_read(x, net->Nlinks, HydFile) < (unsigned)net->Nlinks) result = 0;
    else for (i = 1; i <= net->Nlinks; i++) hyd->LinkFlow[i] = x[i];
```

After a link is deleted, every link after it gets the old flow of the link one place before it, and every later array in the record starts one value early. A hydraulics file supplied with `EN_usehydfile` has its node and link counts checked when it is opened; the scratch file is never checked again.

The six functions that change the network's structure all refuse to run while a solver is open (error 262), but none of them clears `SaveHflag`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-05_loop.inp`](API-05_loop.inp) | R1 supplies J3 (300 gpm) through P1, two parallel pipes P2 (12 in) and P3 (6 in), and P4. Quality `None`; see the note on CRASH-11 below. |
| [`API-05_test.c`](API-05_test.c) | For each of the six editing functions: open, `EN_solveH`, `EN_solveQ`, one edit, `EN_solveQ` again, and print what the second `EN_solveQ` returns |
| [`API-05_fix.patch`](API-05_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-05            # unpatched EPANET: FAIL
tools/run-test.sh API-05 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-05_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-05_test
./API-05_test
```

**Without the fix**, every edit is followed by a "successful" `EN_solveQ`, and P4 reads P3's old flow:

```
  after EN_solveQ, P4 (all of J3's 300 gpm) reads 41.7 gpm
EN_deletelink(P2)              then EN_solveQ returned   0 (expected 104)
EN_addlink(P5, J1 to J3)       then EN_solveQ returned   0 (expected 104)
EN_addnode(J4)                 then EN_solveQ returned   0 (expected 104)
EN_deletenode(J3)              then EN_solveQ returned   0 (expected 104)
EN_setlinknodes(P3, J1, J3)    then EN_solveQ returned   0 (expected 104)
EN_setlinktype(P3, EN_CVPIPE)  then EN_solveQ returned   0 (expected 104)
FAIL: after 6 of 6 structural edits, EN_solveQ ran on hydraulics saved for the old network
```

**With the fix**, `EN_solveQ` asks for new hydraulics:

```
EN_deletelink(P2)              then EN_solveQ returned 104 (expected 104)
...
EN_setlinktype(P3, EN_CVPIPE)  then EN_solveQ returned 104 (expected 104)
PASS: EN_solveQ refuses stale hydraulics after every structural edit
```

## The fix

Clear `SaveHflag` in each of the six functions once the edit has been made, so that `EN_openQ` and `EN_saveH` return 104 until the hydraulics are solved again:

```diff
+    // Saved hydraulic results no longer match the network
+    p->outfile.SaveHflag = FALSE;
     return 0;
```

This goes before the final `return 0` of `EN_addnode`, `EN_deletenode`, `EN_addlink`, `EN_deletelink` and `EN_setlinknodes`, and into the pipe/check-valve fast path of `EN_setlinktype`. The other type changes go through `EN_deletelink` and `EN_addlink`. `EN_setlinknodes` and the pipe/check-valve change keep the counts, so the records still line up, but the hydraulics were computed for a different network, and EPANET already treats both calls as structural edits (error 262). EPANET's own unit tests (6 suites) pass with the patch.

**Apply it together with [CRASH-11](../../2-crashes/CRASH-11-closeq-not-idempotent/).** When `EN_openQ` fails, `EN_solveQ` still calls `EN_closeQ`, and `closequal()` deletes the segment pool left over from the previous quality run a second time. With a quality analysis (`Quality Age` in this network), this patch alone turns the wrong results into a heap-use-after-free in `mempool_delete()`. The CRASH-11 patch sets the pool pointer to NULL. We ran this test with `Quality Age` against EPANET with both patches and it passes. The unpatched library already reaches the same crash today whenever `openqual()` fails after an earlier `EN_solveQ`: with `Quality Age`, the `EN_addnode` case of this test crashes there, because the new junction is unconnected (error 233).

**Related: [API-25](../API-25-report-misaligned-after-edit/).** `EN_report` has the same problem with the binary output file, which is guarded by `outfile.SaveQflag`. After `EN_deletelink(P2)` in this network, the report listed P1 with a velocity of 300.00 fps (P4's old flow), and P3 and P4 with the old flows of P2 and P3. The API-25 patch clears `SaveQflag` next to `SaveHflag` in the same six places, on top of this patch, so that `EN_report` returns 106 instead.

## Notes

- [API-03](../API-03-setlinktype-cv-keeps-controls/), [API-06](../API-06-addlink-self-loop/), [API-07](../API-07-deletelink-cv-pipe-count/), [CRASH-23](../../2-crashes/CRASH-23-setlinknodes-node-zero/) and [CRASH-24](../../2-crashes/CRASH-24-31-character-ids-unterminated/) also patch these functions. The changes are placed so that all the patches apply together.
- A stronger check would compare the scratch file's header (node and link counts) in `EN_openQ`, as `openhydfile()` does for an external file. That would still miss `EN_setlinknodes` and the pipe/check-valve change, which keep the counts.
