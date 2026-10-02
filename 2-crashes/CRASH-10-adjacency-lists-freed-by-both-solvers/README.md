# CRASH-10: Both solvers free the shared adjacency lists

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Segmentation fault. Closing one solver while the other is open frees the node adjacency lists that the other solver still uses. The close call returns 0, so the crash comes later, in `EN_runQ` or `EN_runH`. |
| **Reached from** | Toolkit API: both solvers open at once, then one of them closed before the other is used again |
| **Where** | `closehyd()` in [`src/hydraul.c:312`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L312) and `closequal()` in [`src/quality.c:415`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L415); the lists are shared by `openqual()` at [`src/quality.c:64`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L64) |
| **Since** | v2.3: commits [`9307913`](https://github.com/OpenWaterAnalytics/EPANET/commit/9307913c5bb584c7916c49e595915c1aa4c895db) and [`c79f125`](https://github.com/OpenWaterAnalytics/EPANET/commit/c79f12560893be18743bba2984173c6afdbbb3e7) (2024) |
| **GitHub** | Regression from the fix for [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789), PRs [#791](https://github.com/OpenWaterAnalytics/EPANET/pull/791) and [#798](https://github.com/OpenWaterAnalytics/EPANET/pull/798) |
| **Fix** | Two lines: [`CRASH-10_fix.patch`](CRASH-10_fix.patch) |

## The problem

Both solvers walk the network through one set of node adjacency lists, `net->Adjlist`. The hydraulic solver builds them in `EN_openH`. `EN_openQ` builds them only if they do not exist yet, so when the hydraulic solver is open the quality solver uses the same lists.

Issue [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) reported that the lists outlived the solvers and went stale when nodes were added or deleted. The fix added `freeadjlists()` to both `closehyd()` and `closequal()`. Each close now frees lists the other solver may still need:

- **`EN_closeH` with the quality solver open.** The next `EN_runQ` that re-sorts the nodes reads `net->Adjlist[i]` with `net->Adjlist == NULL` and crashes in `sortnodes()`.
- **`EN_closeQ` with the hydraulic solver open.** The next `EN_runH` that ends with any warning calls `disconnected()`, which crashes in `marknodes()` for the same reason.

Both close calls return 0, so a program that closes the solvers in an unexpected order gets no error to explain the later crash.

## Why it happens

```c
// src/quality.c, openqual()
    // Build nodal adjacency lists if they don't already exist
    if (net->Adjlist == NULL)
    {
        ...
        errcode = buildadjlists(net);

// src/hydraul.c, closehyd()
    freesparse(pr);
    freematrix(pr);
    freeadjlists(&pr->network);

// src/quality.c, closequal()
    freeadjlists(&pr->network);
```

`freeadjlists()` sets `net->Adjlist` to NULL, so this is not a double free. It is a NULL dereference in whichever solver is used next.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-10_Net1.inp`](CRASH-10_Net1.inp) | EPANET's Net1 example, unchanged |
| [`CRASH-10_test.c`](CRASH-10_test.c) | Part A runs the hydraulics with `EN_SAVE`, opens the quality solver, closes the hydraulic solver and runs quality from the saved file. Part B runs both solvers together with `TRIALS 1`, closes the quality solver and runs the hydraulics again. |
| [`CRASH-10_fix.patch`](CRASH-10_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-10            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-10 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-10_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-10_test
./CRASH-10_test
```

**Without the fix**, part A stops in the first `EN_runQ`. UndefinedBehaviorSanitizer reports the NULL array; a build without sanitizers gets a segmentation fault (exit code 139):

```
A: EN_closeH done; running quality from the saved hydraulics
.../src/qualroute.c:487:26: runtime error: applying non-zero offset 80 to null pointer
    #0 ... in sortnodes qualroute.c
    #1 ... in runqual quality.c:246
    #2 ... in EN_runQ epanet.c:783
    #3 ... in quality_after_closeH CRASH-10_test.c:48
```

Part B fails the same way at [`src/report.c:1364`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L1364) in `marknodes()`, called from `disconnected()`. This was confirmed by running part B on its own; the test stops at part A first.

**With the fix**, both parts complete:

```
A: EN_closeH done; running quality from the saved hydraulics
A: quality run ended at 24 h with code 0
B: EN_closeQ done; running the hydraulics again
B: hydraulic run ended at 24 h, highest EN_runH code 2 (a warning)
PASS: each solver keeps working after the other one is closed
```

## The fix

Each close function frees the lists only when the other solver is not open:

```diff
 // closehyd()
-    freeadjlists(&pr->network);
+
+    // Keep the adjacency lists if the quality solver is still using them
+    if (!pr->quality.OpenQflag) freeadjlists(&pr->network);

 // closequal()
-    freeadjlists(&pr->network);
+
+    // Keep the adjacency lists if the hydraulic solver is still using them
+    if (!pr->hydraul.OpenHflag) freeadjlists(&pr->network);
```

`EN_closeH` and `EN_closeQ` clear their own flag after calling these functions, so the solver that closes last frees the lists. The [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) fix still holds: the functions that add or delete nodes and links return error 262 while either solver is open, so the lists are always freed before the network can change.

The alternative in [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) was to keep the lists until the network changes and free them in every function that adds or deletes nodes or links. That is a larger change for the same result.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-11](../CRASH-11-closeq-not-idempotent/) also changes `closequal()`, a few lines above this change. The two patches apply together in either order.
