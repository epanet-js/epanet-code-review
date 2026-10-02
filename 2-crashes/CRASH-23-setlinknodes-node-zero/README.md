# CRASH-23: EN_setlinknodes accepts node index 0

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | The call returns 0 and attaches the link to the unused slot `Node[0]`. The next hydraulic run that ends with a warning writes past a heap buffer while it checks for disconnected nodes. |
| **Reached from** | Toolkit API: `EN_setlinknodes` with 0 for either node, then `EN_solveH` or `EN_runH` with any hydraulic warning |
| **Where** | `EN_setlinknodes()` in [`src/epanet.c:3837-3838`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3837-L3838); the overflow is in `disconnected()` and `marknodes()` in [`src/report.c:1268-1303`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L1268-L1303) |
| **Since** | v2.2, the first release with `EN_setlinknodes` |
| **GitHub** | No related issue |
| **Fix** | Two characters: [`CRASH-23_fix.patch`](CRASH-23_fix.patch) |

## The problem

Node indices start at 1. `EN_setlinknodes` checks its node arguments with `< 0`, so 0 passes and the link is connected to `Node[0]`, an unused entry with an empty ID. Index 0 is easy to pass by mistake, for example after a failed `EN_getnodeindex`, which sets the index to 0.

The library does not fail at once. In the test, pipe P3 is moved to start at node 0 and the network is solved. The solution has negative pressures (warning 6), and after any warning `writehydwarn()` calls `disconnected()` to look for nodes cut off from every source. That check walks the network from the tanks and reservoirs and lists every node it reaches. Through P3 it also reaches node 0, so it lists one more node than the `Nnodes + 1` entries it allocated, and writes past the end of the list.

Built without sanitizers, the test runs to the end on our machine, with `EN_solveH` returning 6.

## Why it happens

```c
// src/epanet.c, EN_setlinknodes()
    // Check for valid link index
    if (index <= 0 || index > net->Nlinks) return 204;

    // Check that nodes exist
    if (node1 < 0 || node1 > net->Nnodes) return 203;
    if (node2 < 0 || node2 > net->Nnodes) return 203;

// src/report.c, disconnected()
    nodelist = (int *)calloc(net->Nnodes + 1, sizeof(int));
    marked = (char *)calloc(net->Nnodes + 1, sizeof(char));
    ...
    marknodes(pr, mcount, nodelist, marked);

// src/report.c, marknodes()
            // Mark connection node if link not closed
            if (hyd->LinkStatus[k] > CLOSED)
            {
                marked[j] = 1;
                m++;
                nodelist[m] = j;
            }
```

`nodelist` has room for nodes 1 to `Nnodes`. With node 0 reachable, `m` reaches `Nnodes + 1`. (`marknodes()` is inlined into `disconnected()` in the sanitizer build, so the report names line 1303, the call.)

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-23_parallel-pipes.inp`](CRASH-23_parallel-pipes.inp) | R1 feeds J1, and J1 feeds J2 through two parallel pipes, P2 and P3. J2 lies above R1's head, so every solution ends with a negative-pressure warning. |
| [`CRASH-23_test.c`](CRASH-23_test.c) | Calls `EN_setlinknodes(P3, 0, J2)`, then `EN_solveH` |
| [`CRASH-23_fix.patch`](CRASH-23_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-23            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-23 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-23_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-23_test
./CRASH-23_test
```

**Without the fix**, node 0 is accepted and `EN_solveH` overflows the node list:

```
EN_setlinknodes(P3, 0, J2) returned 0 (expected 203)
P3 now runs from node 0 to node 2 (J1 is 1, J2 is 2)
Solving hydraulics ...
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 4 at ... thread T0
    #0 ... in disconnected report.c:1303
    #1 ... in writehydwarn report.c:1212
    #2 ... in runhyd hydraul.c:234
    #3 ... in EN_runH epanet.c:567
...
... is located 0 bytes after 16-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in disconnected report.c:1268
```

**With the fix**, node 0 is refused, P3 keeps its nodes, and the run ends with the expected warning:

```
EN_setlinknodes(P3, 0, J2) returned 203 (expected 203)
P3 now runs from node 1 to node 2 (J1 is 1, J2 is 2)
Solving hydraulics ...
EN_solveH returned 6 (6 = negative pressures warning)
PASS: node index 0 is rejected with error 203
```

## The fix

Test `<= 0`, as the link index check above does:

```diff
-    if (node1 < 0 || node1 > net->Nnodes) return 203;
-    if (node2 < 0 || node2 > net->Nnodes) return 203;
+    if (node1 <= 0 || node1 > net->Nnodes) return 203;
+    if (node2 <= 0 || node2 > net->Nnodes) return 203;
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-14](../CRASH-14-calls-after-close/) adds the missing `Openflag` check at the top of the same function, and [API-05](../../3-api/API-05-stale-hydraulics-after-edit/) adds a line at its end. The three patches apply together.
- [API-06](../../3-api/API-06-addlink-self-loop/) is the matching gap in `EN_addlink`, which accepted a link from a node to itself.
