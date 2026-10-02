# API-02: EN_addnode does not shift the trace node (a reverted fix)

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After a junction is added, a source trace from a tank or reservoir runs from a different node. The results are wrong and no error is reported. |
| **Reached from** | Toolkit API: `EN_addnode` with `EN_JUNCTION` in a project whose quality analysis is a trace from a tank or reservoir |
| **Where** | `EN_addnode()` in [`src/epanet.c:1957-1992`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1957-L1992) has no trace node update; `EN_deletenode()` has one at [`src/epanet.c:2110`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2110) |
| **Since** | v2.2. Fixed on `dev` on 17 February 2025 and lost two days later, so missing from every release |
| **GitHub** | [#828](https://github.com/OpenWaterAnalytics/EPANET/issues/828) (closed), fixed by [#829](https://github.com/OpenWaterAnalytics/EPANET/pull/829) (commit [a5bce31](https://github.com/OpenWaterAnalytics/EPANET/commit/a5bce31b13ea21f7794e2f34d97a40e2158ff843)) and undone by [#831](https://github.com/OpenWaterAnalytics/EPANET/pull/831) (commit [7a16739](https://github.com/OpenWaterAnalytics/EPANET/commit/7a1673994ccdae9fac9b29b1bcccaebd77b3baba)). The `EN_deletenode` half: [#859](https://github.com/OpenWaterAnalytics/EPANET/issues/859), fixed by [#860](https://github.com/OpenWaterAnalytics/EPANET/pull/860) |
| **Fix** | One line, the one from [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829): [`API-02_fix.patch`](API-02_fix.patch) |

## The problem

A source trace (`EN_TRACE`) stores the trace node as a node index, `quality.TraceNode`. Tanks and reservoirs come after the junctions in the node list, so when `EN_addnode` adds a junction every tank and reservoir moves up one place. The trace node index stays where it was and now names the node that was one place below the trace node: another tank or reservoir, or the new junction itself.

In the test network, reservoir River supplies J1 and reservoir Lake supplies J2, and the project traces water from Lake. After `EN_addnode(ph, "J3", EN_JUNCTION, ...)` the trace runs from River. J1 reports 100 % "from Lake" and J2 0 %, the reverse of the truth. `EN_getqualtype` returns the new index without complaint, and `EN_solveQ` returns 0.

## Why it happens

`EN_addnode` shifts the tank array, link end nodes, control nodes and rule premises, but not `TraceNode`:

```c
// src/epanet.c, EN_addnode()
        // shift indices of Tank array
        for (i = 1; i <= net->Ntanks; i++)
        {
            net->Tank[i].Node += 1;
        }
        // shift indices of Links, if necessary
        for (i = 1; i <= net->Nlinks; i++)
        {
            if (net->Link[i].N1 > net->Njuncs - 1) net->Link[i].N1 += 1;
            if (net->Link[i].N2 > net->Njuncs - 1) net->Link[i].N2 += 1;
        }
        ...
```

`EN_deletenode` has the matching update (`if (index < p->quality.TraceNode) (p->quality.TraceNode)--;`).

The shift was reported by the maintainer as [#828](https://github.com/OpenWaterAnalytics/EPANET/issues/828) and added by [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) on 17 February 2025. Two days later, commit 7a16739 ("Set/Get node & link tags added", [#831](https://github.com/OpenWaterAnalytics/EPANET/issues/831)) replaced `epanet.c` with a copy made before [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829), which removed the line again. The `EN_deletenode` half was re-reported as [#859](https://github.com/OpenWaterAnalytics/EPANET/issues/859) and fixed again by [#860](https://github.com/OpenWaterAnalytics/EPANET/issues/860). The `EN_addnode` half was not, and [#828](https://github.com/OpenWaterAnalytics/EPANET/issues/828) is closed.

## How to reproduce

| File | What it is |
|---|---|
| [`API-02_two-sources.inp`](API-02_two-sources.inp) | Reservoirs River (node 3) and Lake (node 4) at the same head. River supplies J1 and Lake supplies J2. The quality option is `Trace Lake`. |
| [`API-02_test.c`](API-02_test.c) | Runs the trace, adds junction J3 (and pipe P4 to connect it), and runs the trace again |
| [`API-02_fix.patch`](API-02_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-02            # unpatched EPANET: FAIL
tools/run-test.sh API-02 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-02_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-02_test
./API-02_test
```

**Without the fix**, the trace node index stays at 4, which is now River:

```
Before adding a junction (expected: Lake, J1 0 %, J2 100 %):
  trace node is node 4 (Lake); J1 gets 0 % and J2 100 % from it
After adding junction J3 as node 3:
  trace node is node 4 (River); J1 gets 100 % and J2 0 % from it
FAIL: after EN_addnode the trace runs from a different node, with no error
```

**With the fix**, the trace node moves to index 5 with Lake:

```
After adding junction J3 as node 3:
  trace node is node 5 (Lake); J1 gets 0 % and J2 100 % from it
PASS: the trace node is still Lake after EN_addnode
```

## The fix

Restore the line from [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829), next to the shift of the tank array and with the same test that the link loop uses:

```diff
         {
             net->Tank[i].Node += 1;
         }
+        // shift index of water quality trace node, if necessary
+        if (qual->TraceNode > net->Njuncs - 1) qual->TraceNode += 1;
         // shift indices of Links, if necessary
```

A trace node of 0 (no trace) is never shifted, because `Njuncs - 1` is at least 0. [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) placed the line after the `adjusttankrules()` call; it sits a few lines higher here so that it does not overlap the [API-01](../API-01-addnode-shifts-wrong-rule-premises/) patch, which moves that call. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) changed three things in `epanet.c`: the `EN_addnode` shift (a5bce31), an `EN_deletenode` shift (054ebf3), and resetting `TraceNode` to 0 in `EN_setqualtype` for the other analysis types. 7a16739, whose copy of `epanet.c` is headed "Last Updated: 02/14/2025", removed all three; it removed no other code from that file. [#860](https://github.com/OpenWaterAnalytics/EPANET/issues/860) restored the `EN_deletenode` shift, and `EN_setqualtype` at 83e25bb sets `qual->TraceNode = traceNodeIndex`, which is 0 unless the type is `EN_TRACE`. Only the `EN_addnode` shift is still missing.
- [API-01](../API-01-addnode-shifts-wrong-rule-premises/) is a second index-shift bug in the same branch of `EN_addnode`, for rule premises.
