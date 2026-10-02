# API-27: Adding or deleting nodes and links leaves results attached to the wrong objects

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After `EN_addnode`, `EN_deletenode` or `EN_deletelink`, `EN_getnodevalue` and `EN_getlinkvalue` return the results of a neighbouring node or link, and after `EN_addnode` one node reads uninitialised memory, until the network is solved again. |
| **Reached from** | Toolkit API: a run, then `EN_addnode` (junction), `EN_deletenode` or `EN_deletelink`, then reads of results (`EN_HEAD`, `EN_PRESSURE`, `EN_DEMAND`, `EN_QUALITY`, `EN_TANKVOLUME`, `EN_FLOW`, `EN_STATUS`, `EN_SETTING`, ...) |
| **Where** | The `Node` and `Link` shifts in `EN_addnode()` [`src/epanet.c:1944-1964`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1944-L1964), `EN_deletenode()` [`:2103-2109`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2103-L2109) and `EN_deletelink()` [`:3584-3590`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3584-L3590), none of which moves the result arrays |
| **Since** | v2.2, when the editing functions were added. [#543](https://github.com/OpenWaterAnalytics/EPANET/pull/543) (October 2019) added the shift of the `Node` array in `EN_addnode`, but not of the results. |
| **GitHub** | No issue for the results. [#542](https://github.com/OpenWaterAnalytics/EPANET/issues/542) (closed) reported the original `Node` array problem that [#543](https://github.com/OpenWaterAnalytics/EPANET/issues/543) fixed. |
| **Fix** | About 40 lines in three functions: [`API-27_fix.patch`](API-27_fix.patch) |

## The problem

A toolkit program that shows results while the user edits the network, as a GUI does, reads results between runs. After a node or link is added or deleted, those reads are wrong.

Results are kept in arrays indexed like the nodes and links: `NodeHead`, `NodeDemand`, `DemandFlow`, `FullDemand`, `EmitterFlow`, `LeakageFlow` and `NodeQual` for nodes, and `LinkFlow`, `LinkStatus` and `LinkSetting` for links. The editing functions renumber the nodes and links but leave these arrays as they were:

- **`EN_addnode`** puts a new junction at index `Njuncs + 1`, and every tank and reservoir moves up one place. In the test, the new junction J3 then reads reservoir R1's 150 ft, R1 reads tank T1's 120 ft, and T1 reads the new, uninitialised slot (-1.83255e-06 ft under AddressSanitizer, which fills new memory with a fixed byte pattern; 0 in a build without sanitizers).
- **`EN_deletenode`** moves every later node down one place. After J1 is deleted, J2 reads J1's head, R1 reads J2's and T1 reads R1's. Pipe P3 reads the flow of P1, because the two links deleted with J1 were not shifted out either.
- **`EN_deletelink`** moves every later link down one place. After P1 is deleted, P2 reads P1's flow and P3 reads P2's.

By reading the code, the next run recomputes all of these values: `inithyd()` resets tank heads, statuses, settings and node outflows, and junction heads are solved for before they are used. So the wrong values are seen between the edit and the next run. One exception is a link's flow, which `inithyd()` keeps as the starting flow of the next solution unless it is near zero.

## Why it happens

```c
// src/epanet.c, EN_addnode()
    hyd->NodeHead = (double *)realloc(hyd->NodeHead, size);
    ...
        // shift indices of non-Junction nodes at end of Node array
        for (i = net->Nnodes; i > net->Njuncs; i--)
        {
            hashtable_update(net->NodeHashTable, net->Node[i].ID, i + 1);
            net->Node[i + 1] = net->Node[i];
        }

// src/epanet.c, EN_deletenode()
    // Shift position of higher entries in Node & Coord arrays down one
    for (i = index; i <= net->Nnodes - 1; i++)
    {
        net->Node[i] = net->Node[i + 1];
        ...
    }

// src/epanet.c, EN_deletelink()
    // Shift position of higher entries in Link array down one
    for (i = index; i <= net->Nlinks - 1; i++)
    {
        net->Link[i] = net->Link[i + 1];
        ...
    }
```

The structs move; `NodeHead[i]`, `LinkFlow[i]` and the rest stay where they were, and the slot a new node gets is never written.

## How to reproduce

| File | What it is |
|---|---|
| [`API-27_tank-network.inp`](API-27_tank-network.inp) | R1 feeds tank T1 through pipes P1, P2 and P3 and junctions J1 and J2 (nodes: J1, J2, R1, T1) |
| [`API-27_test.c`](API-27_test.c) | For each of three edits (add junction J3, delete J1, delete P1): solves, notes every head and flow, makes the edit and reads them again without solving |
| [`API-27_fix.patch`](API-27_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-27            # unpatched EPANET: FAIL
tools/run-test.sh API-27 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-27_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-27_test
./API-27_test
```

**Without the fix**, every object above the edit reads its neighbour's result:

```
After EN_addnode(J3), without solving again:
    J1  head      138.844 ft  (expected 138.844)
    J2  head      128.864 ft  (expected 128.864)
    R1  head          120 ft  (expected 150)  <-- wrong
    T1  head -1.83255e-06 ft  (expected 120)  <-- wrong
    J3  head          150 ft  (expected 0)  <-- wrong
    ...
After EN_deletenode(J1), without solving again:
    J2  head      138.844 ft  (expected 128.864)  <-- wrong
    R1  head      128.864 ft  (expected 150)  <-- wrong
    T1  head          150 ft  (expected 120)  <-- wrong
    P3  flow      1712.31 gpm (expected 1512.31)  <-- wrong
After EN_deletelink(P1), without solving again:
    ...
    P2  flow      1712.31 gpm (expected 1612.31)  <-- wrong
    P3  flow      1612.31 gpm (expected 1512.31)  <-- wrong
FAIL: after the edits, 9 results belong to another node or link, or are unset
```

**With the fix**, every node and link keeps its results and J3 has none yet:

```
After EN_addnode(J3), without solving again:
    ...
    R1  head          150 ft  (expected 150)
    T1  head          120 ft  (expected 120)
    J3  head            0 ft  (expected 0)
    ...
After EN_deletenode(J1), without solving again:
    J2  head      128.864 ft  (expected 128.864)
    R1  head          150 ft  (expected 150)
    T1  head          120 ft  (expected 120)
    P3  flow      1512.31 gpm (expected 1512.31)
After EN_deletelink(P1), without solving again:
    ...
    P2  flow      1612.31 gpm (expected 1612.31)
    P3  flow      1512.31 gpm (expected 1512.31)
PASS: every node and link keeps its own results after the edits
```

## The fix

Move the results with the objects in all three functions. In `EN_deletenode`, after the node shift:

```diff
+    // Move the results of the higher nodes down one place with the nodes
+    for (i = index; i <= net->Nnodes - 1; i++)
+    {
+        hyd->NodeHead[i] = hyd->NodeHead[i + 1];
+        hyd->NodeDemand[i] = hyd->NodeDemand[i + 1];
+        ...
+        qual->NodeQual[i] = qual->NodeQual[i + 1];
+    }
```

In `EN_deletelink`, after the link shift:

```diff
+    // Move the results of the higher links down one place with the links
+    for (i = index; i <= net->Nlinks - 1; i++)
+    {
+        hyd->LinkFlow[i] = hyd->LinkFlow[i + 1];
+        hyd->LinkSetting[i] = hyd->LinkSetting[i + 1];
+        hyd->LinkStatus[i] = hyd->LinkStatus[i + 1];
+    }
```

In `EN_addnode`, once the new node is in place, the same loop runs upwards for the nodes above it, and the new node's results are set to 0, which is what a node has before any run:

```diff
+    // Move the results of the nodes above the new one up one place
+    // (with the nodes themselves) and clear the new node's results
+    for (i = net->Nnodes; i > nIdx; i--)
+    {
+        hyd->NodeHead[i] = hyd->NodeHead[i - 1];
+        ...
+    }
+    hyd->NodeHead[nIdx] = 0.0;
+    ...
```

`EN_deletenode` and `EN_deletelink` each get a `Hydraul` (and, for nodes, `Quality`) pointer, like the other editing functions. `EN_deletenode` calls `EN_deletelink` for the node's links, so their results are removed too. Each block is placed away from the lines that [API-01](../API-01-addnode-shifts-wrong-rule-premises/), [API-02](../API-02-addnode-trace-node-not-shifted/), [API-05](../API-05-stale-hydraulics-after-edit/), [API-07](../API-07-deletelink-cv-pipe-count/) and [CRASH-24](../../2-crashes/CRASH-24-31-character-ids-unterminated/) change in the same functions; all the patches apply together. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `EN_addlink` adds a link at the end, so no results move, but the new link's slots are not written either. [API-29](../API-29-addlink-leaves-new-link-fields-unset/) fixes that: it starts a new link with no flow at its initial status and setting.
- [API-20](../API-20-reads-before-run-return-scratch-values/) gives tanks their initial head before the first run. A tank added with `EN_addnode` has an initial head of 0, so the 0 that this patch writes matches it.
- [API-25](../API-25-report-misaligned-after-edit/) is the same misalignment in the binary output file, seen through `EN_report`.
