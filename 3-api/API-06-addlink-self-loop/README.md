# API-06: EN_addlink accepts a link from a node to itself

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | The call returns 0. The next `EN_openH` (or `EN_solveH`) can read past a heap buffer while it orders the nodes, and a file saved from the project cannot be opened. |
| **Reached from** | Toolkit API: `EN_addlink` with the same node ID for `fromNode` and `toNode` |
| **Where** | `EN_addlink()` in [`src/epanet.c:3424-3433`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3424-L3433); compare `EN_setlinknodes()` at [`src/epanet.c:3841`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3841) and the parser at [`src/input3.c:358`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L358) |
| **Since** | v2.2, the first release with `EN_addlink` |
| **GitHub** | [#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937) (September 2026, reports the heap overflow). Fixed on `dev` after 83e25bb by [#940](https://github.com/OpenWaterAnalytics/EPANET/pull/940), with the same line as this patch. |
| **Fix** | One line: [`API-06_fix.patch`](API-06_fix.patch) |

## The problem

A link must join two different nodes. The input file parser rejects a pipe, pump or valve whose two ends are the same node with error 222 ("same start and end nodes for link"), and so does `EN_setlinknodes`. `EN_addlink` does not check, and returns 0.

What happens next depends on the network. In the test network, a chain of three junctions fed by a reservoir, a pipe from J3 to J3 makes the next `EN_solveH` read past a 16-byte heap buffer in the minimum-degree node ordering (`genmmd()`, called from `createsparse()` in `EN_openH`). [#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937) reports the same overflow, and intermittent SIGBUS crashes, in an application that embeds 2.3.5. The self-loop puts the junction in its own adjacency list, which the ordering does not expect. With other networks (a self-loop on node 10 of Net1, for example) the solver runs and returns warning 4 instead.

If the project is saved with `EN_saveinpfile`, the file cannot be opened again: `EN_open` returns 200, and the report shows `Error 222: same start and end nodes for link P4 in [PIPES] section`.

## Why it happens

`EN_addlink` looks up both end nodes and checks that they exist and, for valves, that the connection is legal, but it never compares them:

```c
// src/epanet.c, EN_addlink()
    // Lookup the link's from and to nodes
    n1 = hashtable_find(net->NodeHashTable, fromNode);
    n2 = hashtable_find(net->NodeHashTable, toNode);
    if (n1 == 0 || n2 == 0) return 203;

    // Check that valve link has legal connections
    if (linkType > PUMP)
    {
        errcode = valvecheck(p, 0, linkType, n1, n2);
        if (errcode) return errcode;
    }

// src/epanet.c, EN_setlinknodes()
    // Check that nodes are not the same
    if (node1 == node2) return 222;
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-06_three-junctions.inp`](API-06_three-junctions.inp) | Reservoir R1 feeds junctions J1, J2 and J3 in a chain |
| [`API-06_test.c`](API-06_test.c) | Adds pipe P4 from J3 to J3, then solves the hydraulics |
| [`API-06_fix.patch`](API-06_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-06            # unpatched EPANET: CRASH
tools/run-test.sh API-06 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-06_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-06_test
./API-06_test
```

**Without the fix**, `EN_addlink` returns 0 and AddressSanitizer stops `EN_solveH`:

```
EN_addlink(P4 from J3 to J3) returned 0 (expected 222)
Solving hydraulics ...
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 4 at ... thread T0
    #0 ... in genmmd genmmd.c:113
    #1 ... in createsparse smatrix.c:105
    #2 ... in openhyd hydraul.c:63
    #3 ... in EN_openH epanet.c:495
...
... is located 0 bytes after 16-byte region [...)
```

Built without sanitizers, the same test runs to the end on our machine (`EN_solveH` returns 0) and prints `FAIL: EN_addlink accepted a pipe whose two ends are the same node`.

**With the fix**, the self-loop is refused and the network solves:

```
EN_addlink(P4 from J3 to J3) returned 222 (expected 222)
Solving hydraulics ...
EN_solveH returned 0
PASS: EN_addlink refuses a link from a node to itself with error 222
```

## The fix

Return 222 when the two end nodes are the same, before the valve check:

```diff
     if (n1 == 0 || n2 == 0) return 203;
 
+    // Check that nodes are not the same
+    if (n1 == n2) return 222;
+
     // Check that valve link has legal connections
```

This is the change that [#940](https://github.com/OpenWaterAnalytics/EPANET/issues/940) made on `dev` after 83e25bb (merged on 1 October 2026), with the same comment and position, so the patch only matters for a tree at 83e25bb or a backport to 2.3.x. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937) found this through an application that passed stale node indices after `EN_addnode` had shifted the tanks and reservoirs. [API-01](../API-01-addnode-shifts-wrong-rule-premises/) and [API-02](../API-02-addnode-trace-node-not-shifted/) are the same kind of shift going wrong inside the library.
- `EN_setlinknodes` has a related gap: it accepts node index 0. See [CRASH-23](../../2-crashes/CRASH-23-setlinknodes-node-zero/).
