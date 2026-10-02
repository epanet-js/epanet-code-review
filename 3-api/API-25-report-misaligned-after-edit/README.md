# API-25: EN_report after a structural edit prints misaligned results

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After a node or link is added or deleted, `EN_report` returns 0 and writes a report whose values belong to other nodes, links or variables, for example a flow in the velocity column. |
| **Reached from** | Toolkit API: `EN_solveQ` (or a saved `EN_initQ`/`EN_runQ`/`EN_nextQ` run), then any of `EN_addnode`, `EN_deletenode`, `EN_addlink`, `EN_deletelink`, `EN_setlinktype`, `EN_setlinknodes`, then `EN_report` without solving again |
| **Where** | `EN_report()` checks only `outfile.SaveQflag` at [`src/epanet.c:912`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L912); `writeresults()` reads the output file with the current counts at [`src/report.c:628`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L628) and [`:635`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L635) |
| **Since** | v2.2, when the network editing functions were added |
| **GitHub** | No related issue |
| **Fix** | One statement in each of six functions, on top of API-05: [`API-25_fix.patch`](API-25_fix.patch) (`Requires: API-05`) |

## The problem

`EN_solveQ` saves the results of every reporting period to the binary output file, and `EN_report` turns them into the tables of the report file. If the network is edited in between, `EN_report` does not notice that the saved results belong to a different network. It returns 0 and writes a report in which the values no longer belong to the rows and columns they are printed in.

In the test network, reservoir R1 supplies J3 (300 gpm) through pipe P1, two parallel pipes P2 (12 in) and P3 (6 in), and pipe P4. P1 and P4 both carry 300 gpm at 0.85 fps. After `EN_deletelink(P2)`, `EN_report` writes:

```
  Link                  gpm       fps   /1000ft
  ----------------------------------------------
  P1                 300.00    300.00      0.47
  P3                 258.28      0.85      0.85
  P4                  41.72      0.73      0.44
```

P3 shows P2's old flow, P4 shows P3's, and P1's velocity of 300.00 fps is P4's old flow. This was found while writing up [API-05](../API-05-stale-hydraulics-after-edit/), which is the same problem for the saved hydraulics.

## Why it happens

The output file holds, for each reporting period, one array per node variable with a value for each node, then one array per link variable with a value for each link. `writeresults()` reads them back with the counts the network has now:

```c
// src/report.c, writeresults()
            for (j = DEMAND; j <= QUALITY; j++)
            {
                fread((x[j - DEMAND]) + 1, sizeof(REAL4), net->Nnodes, outFile);
            }
            ...
                fread((x[j - FLOW]) + 1, sizeof(REAL4), net->Nlinks, outFile);
```

After one link is deleted, each read stops one value short, so every link after the deleted one takes the value of the link before it, and every later array starts one value early. `EN_report` only checks that results were saved:

```c
// src/epanet.c, EN_report()
    // Check if results have been saved to binary output file
    if (!p->outfile.SaveQflag) return 106;
```

None of the six editing functions clears `SaveQflag`.

## How to reproduce

| File | What it is |
|---|---|
| [`API-25_loop.inp`](API-25_loop.inp) | R1 supplies J3 (300 gpm) through P1, two parallel pipes P2 (12 in) and P3 (6 in), and P4. `[REPORT] Links All` writes the link table. |
| [`API-25_test.c`](API-25_test.c) | For each of the six editing functions: open, `EN_solveH`, `EN_solveQ`, one edit, then `EN_report`. Prints what `EN_report` returns, and for the first edit the link table it wrote. |
| [`API-25_fix.patch`](API-25_fix.patch) | The fix. It applies after [`API-05_fix.patch`](../API-05-stale-hydraulics-after-edit/API-05_fix.patch). |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-25            # unpatched EPANET: FAIL
tools/run-test.sh API-25 --patched  # with API-05 and this fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-25_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-25_test
./API-25_test
```

**Without the fix**, every edit is followed by a report that `EN_report` calls successful:

```
  report written after EN_deletelink(P2):
      Link Results at 0:00:00 hrs:
      ...
      P1                 300.00    300.00      0.47
      P3                 258.28      0.85      0.85
      P4                  41.72      0.73      0.44
EN_deletelink(P2)              then EN_report returned   0 (expected 106)
EN_addlink(P5, J1 to J3)       then EN_report returned   0 (expected 106)
EN_addnode(J4)                 then EN_report returned   0 (expected 106)
EN_deletenode(J3)              then EN_report returned   0 (expected 106)
EN_setlinknodes(P3, J1, J3)    then EN_report returned   0 (expected 106)
EN_setlinktype(P3, EN_CVPIPE)  then EN_report returned   0 (expected 106)
FAIL: after 6 of 6 structural edits, EN_report printed results saved for the old network
```

**With the fix**:

```
EN_deletelink(P2)              then EN_report returned 106 (expected 106)
...
EN_setlinktype(P3, EN_CVPIPE)  then EN_report returned 106 (expected 106)
PASS: EN_report refuses stale results after every structural edit
```

## The fix

[API-05](../API-05-stale-hydraulics-after-edit/) clears `SaveHflag` in the six editing functions. This patch clears `SaveQflag` next to it, so `EN_report` returns 106 ("no results saved to report on") until the network is solved again:

```diff
-    // Saved hydraulic results no longer match the network
+    // Saved hydraulic and output results no longer match the network
     p->outfile.SaveHflag = FALSE;
+    p->outfile.SaveQflag = FALSE;
     return 0;
```

The patch edits the lines that API-05 adds, so its header says `Requires: API-05` and it applies after that patch. EPANET's own unit tests (6 suites) pass with both patches.

`EN_setlinknodes` and the pipe/check-valve change of `EN_setlinktype` keep the node and link counts, so after them the report is not misaligned: it shows the results of the network as it was before the edit. The patch refuses the report in those cases too, so that one rule holds for every structural edit, as in API-05. If the maintainers prefer to keep reporting the previous run in that case, the line can be left out of those two functions.

## Notes

- Reading the output file with the toolkit's output API (`epanet_output.h`) is not affected: it takes the counts from the file's own header.
- [API-27](../API-27-results-on-wrong-objects-after-edit/) is the same kind of problem for the result arrays in memory: after a node or link is added or deleted, `EN_getnodevalue` and `EN_getlinkvalue` return a neighbour's results.
