# CRASH-21: EN_deletedemand with index 0 leaves a dangling demand list

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | The call returns 0 but frees the junction's first demand while the junction still points to it. The next read of the demands is a use-after-free, and closing the project frees the demand again. |
| **Reached from** | Toolkit API: `EN_deletedemand` on a junction with a demand index of 0 or below |
| **Where** | `EN_deletedemand()` in [`src/epanet.c:3106-3145`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3106-L3145) |
| **Since** | v2.2 (the demand functions were rewritten in May 2019) |
| **GitHub** | No related issue |
| **Fix** | One line: [`CRASH-21_fix.patch`](CRASH-21_fix.patch) |

## The problem

Demand categories are numbered from 1. A caller that passes 0, for example by mixing up 0- and 1-based indexing, gets a return code of 0 from `EN_deletedemand`. The junction's first demand has been freed, but the junction's demand list still starts with it. Every later call that walks the list (`EN_getnumdemands`, `EN_getbasedemand`, a hydraulic run) reads freed memory, and `EN_close` frees the demand a second time.

In the test, AddressSanitizer stops the next call, `EN_getnumdemands`. Built without sanitizers on macOS, the test reads one demand of 0.0 gpm with an empty name in place of the two real ones, and the process then aborts in the system allocator when the project is closed.

`EN_getbasedemand` and the other demand getters return 253 ("nonexistent demand category") for an index below 1.

## Why it happens

With `demandIndex` = 0, neither branch that unlinks a demand applies. The "head of list" branch tests for exactly 1, and the search loop does not run because `n` starts at 1. So `d` and `dprev` both still point to the head, the "unlink" assigns `head->next` to itself, and the head is freed while `node->D` keeps pointing to it:

```c
// src/epanet.c, EN_deletedemand()
        d = node->D;
        if (d == NULL) return 253;
        dprev = d;

        // Check if target demand is head of demand list
        if (demandIndex == 1)
        {
            ...
        }

        // Otherwise locate target demand in demand list
        while (d != NULL && n < demandIndex)
        {
            ...
        }

        // Return error if target demand not found
        if (d == NULL) return 253;

        // Link the demands that precede and follow the target
        dprev->next = d->next;

        // Delete the target demand
        free(d->Name);
        free(d);
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-21_two-demands.inp`](CRASH-21_two-demands.inp) | Junction J1 with two demand categories, Domestic (100 gpm) and Industrial (50 gpm) |
| [`CRASH-21_test.c`](CRASH-21_test.c) | Calls `EN_deletedemand(J1, 0)`, then reads J1's demands back |
| [`CRASH-21_fix.patch`](CRASH-21_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-21            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-21 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-21_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-21_test
./CRASH-21_test
```

**Without the fix**, the call returns 0 and the next read uses freed memory:

```
EN_deletedemand(J1, 0) returned 0 (expected 253)
==...==ERROR: AddressSanitizer: heap-use-after-free on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in EN_getnumdemands epanet.c
    #1 ... in main CRASH-21_test.c:34
...
freed by thread T0 here:
    #0 ... in wrap_free+0x98 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x52ce0)
    #1 ... in EN_deletedemand epanet.c
```

**With the fix**, index 0 is refused and both demands are kept:

```
EN_deletedemand(J1, 0) returned 253 (expected 253)
J1 now has 2 demand categories:
  1: Domestic    100.0 gpm
  2: Industrial   50.0 gpm
PASS: index 0 is rejected with 253 and both demands are kept
```

## The fix

Reject an index below 1 with the error the other demand functions use:

```diff
     if (nodeIndex <= 0 || nodeIndex > p->network.Nnodes) return 203;
+    if (demandIndex <= 0) return 253;
```

The check sits with the other argument checks, so a call on a tank or reservoir with demand index 0 also gets 253. With any other index the function still returns 0 for those nodes and does nothing. EPANET's own unit tests (6 suites) pass with the patch.
