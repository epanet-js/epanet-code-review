# CRASH-07: A network with an isolated component overflows SortedNodes in a quality run

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap buffer overflow during any water quality run on a network that has more nodes than links plus tanks. The write goes past the end of an `int` array, so it can corrupt the heap without any visible error. |
| **Reached from** | Input file or toolkit: a quality run (`EN_solveQ`, or `EN_openQ` and `EN_runQ`) on a network with a disconnected component, for example an isolated pipe |
| **Where** | `openqual()` in [`src/quality.c:90-95`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L90-L95), written by `sortnodes()` at [`src/qualroute.c:483`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualroute.c#L483) |
| **Since** | v2.2: the water quality rewrite in commit [`7c021cf`](https://github.com/OpenWaterAnalytics/EPANET/commit/7c021cf533c247b66eb1b5c562856e88e8dc7a4b) (2018) |
| **GitHub** | No related issue |
| **Fix** | One line: [`CRASH-07_fix.patch`](CRASH-07_fix.patch) |

## The problem

The quality solver sorts the nodes from upstream to downstream and keeps the order in `SortedNodes`, one entry per node. The array is allocated with the size of the segment arrays instead, `Nlinks + Ntanks + 1`.

In a connected network every junction is reached by at least one link, so `Nlinks + Ntanks >= Nnodes` and the array is large enough. A network with an isolated component breaks that: each separate tree of pipes has one more node than it has links. The test network has 4 nodes but only 2 links and 1 reservoir, so `SortedNodes` has 4 entries (indices 0 to 3) and the sort writes index 4.

The hydraulic solver accepts such a network with warnings ("Node J2 disconnected"), so the overflow happens in an ordinary run of an input file, including `runepanet`.

## Why it happens

```c
// src/quality.c, openqual()
    // Allocate arrays used for volume segments in links & tanks
    n = net->Nlinks + net->Ntanks + 1;
    qual->FirstSeg = (Pseg *)calloc(n, sizeof(Pseg));
    qual->LastSeg = (Pseg *)calloc(n, sizeof(Pseg));

    // Allocate memory for topologically sorted nodes
    qual->SortedNodes = (int *)calloc(n, sizeof(int));

// src/qualroute.c, sortnodes()
        while (numsorted < net->Nnodes)
        {
            ...
            numsorted++;
            qual->SortedNodes[numsorted] = i;
```

`sortnodes()` itself allocates its work arrays (`indegree`, `stack`) with `Nnodes + 1` entries. Only `SortedNodes` uses the wrong size.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-07_isolated-pipe.inp`](CRASH-07_isolated-pipe.inp) | Reservoir R1 feeds junction J1 through P1. Pipe P2 joins J2 and J3 and is connected to nothing else. An emitter at J2 lets the hydraulic solver handle the isolated pipe with warnings. |
| [`CRASH-07_test.c`](CRASH-07_test.c) | Prints the node, link and tank counts, then runs `EN_solveH` and `EN_solveQ` |
| [`CRASH-07_fix.patch`](CRASH-07_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-07            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-07 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build (with AddressSanitizer, or the overflow goes unnoticed), from this folder:

```sh
cc -fsanitize=address CRASH-07_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-07_test
./CRASH-07_test
```

The command-line runner shows the same thing with a sanitizer build: `runepanet CRASH-07_isolated-pipe.inp CRASH-07.rpt`.

**Without the fix**, AddressSanitizer stops the run in the first `EN_runQ` call:

```
Nodes 4, links 2, tanks and reservoirs 1
SortedNodes entries needed:     Nnodes + 1          = 5
SortedNodes entries at 83e25bb: Nlinks + Ntanks + 1 = 4
EN_solveH returned 6
=================================================================
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 4 at ... thread T0
    #0 ... in sortnodes qualroute.c:483
    #1 ... in runqual quality.c:246
    #2 ... in EN_runQ epanet.c:783
    #3 ... in EN_solveQ epanet.c:711
...
0x... is located 0 bytes after 16-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in openqual quality.c:95
```

Code 6 from `EN_solveH` is the negative-pressure warning; the report also lists "Node J2 disconnected".

**With the fix**, the quality run completes:

```
EN_solveH returned 6
EN_solveQ returned 0
PASS: the quality run completes on a network with an isolated component
```

## The fix

Allocate `SortedNodes` for every node:

```diff
     // Allocate memory for topologically sorted nodes
+    n = net->Nnodes + 1;
     qual->SortedNodes = (int *)calloc(n, sizeof(int));
```

EPANET's own unit tests (6 suites) pass with the patch.
