# CRASH-11: EN_closeQ after EN_solveQ frees the segment pool twice

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap use-after-free and double free. A second `EN_closeQ`, for example in a program's clean-up code after `EN_solveQ`, crashes the host process, with or without sanitizers. |
| **Reached from** | Toolkit API: `EN_closeQ` called when the quality solver is already closed, most often after `EN_solveQ` |
| **Where** | `closequal()` in [`src/quality.c:408`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L408), called from `EN_closeQ()` in [`src/epanet.c:834-849`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L834-L849) |
| **Since** | v2.2: the memory pool clean-up in commit [`3ebb183`](https://github.com/OpenWaterAnalytics/EPANET/commit/3ebb1831d77fd4b5f115001ed4c4a860e7bd847b) (2019) |
| **GitHub** | No related issue |
| **Fix** | One line: [`CRASH-11_fix.patch`](CRASH-11_fix.patch) |

## The problem

`EN_solveQ` opens the water quality solver, runs it and closes it with its own call to `EN_closeQ`. A program that calls `EN_closeQ` once more, which is a natural thing to do in clean-up code, makes the library delete the quality solver's segment memory pool a second time.

The header comment for `EN_closeQ` does say "Do not call this function if EN_solveQ is being used." But the matching hydraulic call is safe: `EN_closeH` after `EN_solveH` does nothing and returns 0. With `EN_closeQ` the same mistake takes down the host process instead of returning an error.

## Why it happens

`closequal()` deletes the pool but leaves the pointer set. The `FREE()` macro on the next lines clears the other pointers, which is why only the pool is freed twice. `EN_closeQ` calls `closequal()` whether or not the solver is open:

```c
// src/quality.c, closequal()
    if (qual->Qualflag != NONE)
    {
        if (qual->SegPool) mempool_delete(qual->SegPool);
        FREE(qual->FirstSeg);
        FREE(qual->LastSeg);
        ...

// src/epanet.c, EN_closeQ()
    if (!p->Openflag) return 102;
    closequal(p);
    p->quality.OpenQflag = FALSE;
```

`EN_closeH`, by contrast, only calls `closehyd()` when `OpenHflag` is set.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-11_Net1.inp`](CRASH-11_Net1.inp) | EPANET's Net1 example, unchanged |
| [`CRASH-11_test.c`](CRASH-11_test.c) | Runs `EN_solveH` and `EN_solveQ`, then calls `EN_closeH` and `EN_closeQ` once more |
| [`CRASH-11_fix.patch`](CRASH-11_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-11            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-11 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-11_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-11_test
./CRASH-11_test
```

**Without the fix**, the extra `EN_closeH` is harmless and the extra `EN_closeQ` reads the freed pool:

```
EN_solveH returned 0
EN_solveQ returned 0
EN_closeH returned 0
=================================================================
==...==ERROR: AddressSanitizer: heap-use-after-free on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in mempool_delete mempool.c:86
    #1 ... in closequal quality.c:408
    #2 ... in EN_closeQ epanet.c:844
    #3 ... in main CRASH-11_test.c:40
...
freed by thread T0 here:
    ...
    #1 ... in closequal quality.c:408
    #2 ... in EN_closeQ epanet.c:844
    #3 ... in EN_solveQ epanet.c:717
```

A build without sanitizers (macOS, Release) stops in the same call with exit code 133, a SIGTRAP from the system allocator.

**With the fix:**

```
EN_closeH returned 0
EN_closeQ returned 0
PASS: EN_closeQ after EN_solveQ is harmless
```

## The fix

Clear the pointer after deleting the pool:

```diff
         if (qual->SegPool) mempool_delete(qual->SegPool);
+        qual->SegPool = NULL;
```

Every other resource that `closequal()` and `EN_closeQ` release is already cleared or checked, so a second `EN_closeQ` now frees nothing. It still writes a second "Analysis ended" line to the report file.

The review first proposed an early return in `EN_closeQ` when `OpenQflag` is not set, as `EN_closeH` does. That would also stop the duplicate line, but `EN_solveQ` calls `EN_closeQ` even when `EN_openQ` fails, and the flag is not set in that case. `openqual()` may already have allocated the pool, the arrays and the adjacency lists by then, so the early return would leak them. It could also leave adjacency lists for an old node count in place, which is the problem behind [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789). Clearing the pointer has neither problem.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-10](../CRASH-10-adjacency-lists-freed-by-both-solvers/) changes the last lines of `closequal()`. The two patches apply together in either order.
