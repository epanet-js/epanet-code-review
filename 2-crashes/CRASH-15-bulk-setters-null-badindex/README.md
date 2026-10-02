# CRASH-15: EN_setnodevalues and EN_setlinkvalues write through a NULL badIndex

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | The caller crashes (null pointer write) whenever a bulk assignment fails and `badIndex` is NULL, which the functions otherwise allow. |
| **Reached from** | Toolkit API: `EN_setnodevalues` or `EN_setlinkvalues` with `badIndex = NULL`, when any node or link rejects its value |
| **Where** | `EN_setnodevalues()` in [`src/epanet.c:2807`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2807) and [`:2815`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2815); `EN_setlinkvalues()` in [`src/epanet.c:4420`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4420) and [`:4428`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4428) |
| **Since** | [PR #908](https://github.com/OpenWaterAnalytics/EPANET/pull/908) (March 2026), which is not in a release yet. The guards were dropped by commit [8a3b5f0](https://github.com/OpenWaterAnalytics/EPANET/commit/8a3b5f0) inside that PR. |
| **GitHub** | No related issue. Introduced by [PR #908](https://github.com/OpenWaterAnalytics/EPANET/pull/908). |
| **Fix** | Four lines: [`CRASH-15_fix.patch`](CRASH-15_fix.patch) |

## The problem

`EN_setnodevalues` and `EN_setlinkvalues`, added in PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908), set one property for every node or link. If an assignment fails, they undo the ones already made and report the index of the failing node or link in `badIndex`. Both functions treat `badIndex` as optional, and start with

```c
if (badIndex) *badIndex = 0;
```

The error paths then store the index without the check, so a caller that passes NULL crashes on the first rejected value instead of getting the error code. In the test, a negative emitter coefficient (error 209) and a negative pipe diameter (error 211) are enough. Without sanitizers the store is a segmentation fault.

## Why it happens

```c
// src/epanet.c, EN_setnodevalues() (EN_setlinkvalues() is the same)
errcode = EN_getnodevalue(p, i, property, &old[i - 1]);
if (errcode != 0)
{
    *badIndex = i;                       // line 2807
    j = i - 1;        // Need to restore values for nodes 1 to i-1
}
else
{
    errcode = EN_setnodevalue(p, i, property, values[i - 1]);
    if (errcode != 0)
    {
        *badIndex = i;                   // line 2815
        j = i;        // Need to restore values for nodes 1 to i
    }
}
```

The commit in PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908) that added `badIndex` (50d5b29) had `if (badIndex) *badIndex = i;` in every place. Commit 8a3b5f0, "Fix rollback in set*values", restructured the loop and dropped the checks.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-15_network.inp`](CRASH-15_network.inp) | Reservoir R1 feeds junctions J1 and J2 through pipes P1 and P2 |
| [`CRASH-15_test.c`](CRASH-15_test.c) | Calls `EN_setnodevalues(EN_EMITTER)` and `EN_setlinkvalues(EN_DIAMETER)` with invalid values and `badIndex = NULL` |
| [`CRASH-15_fix.patch`](CRASH-15_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-15            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-15 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-15_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-15_test
./CRASH-15_test
```

**Without the fix**, the first call crashes:

```
EN_setnodevalues(EN_EMITTER, -1 for every node, NULL) ...
.../src/epanet.c:2815:17: runtime error: store to null pointer of type 'int'
    #0 ... in EN_setnodevalues epanet.c:2815
    #1 ... in main CRASH-15_test.c:31
```

The link version crashes the same way at line 4428 when it is called on its own.

**With the fix:**

```
EN_setnodevalues(EN_EMITTER, -1 for every node, NULL) ...
  returned 209 (expected 209)
EN_setlinkvalues(EN_DIAMETER, -1 for every link, NULL) ...
  returned 211 (expected 211); P1 diameter is still 12.0 in
PASS: both bulk setters return an error code when badIndex is NULL
```

## The fix

Guard the four stores the same way as the initialisation:

```diff
-            *badIndex = i;
+            if (badIndex) *badIndex = i;
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [API-11](../../3-api/API-11-bulk-setters-read-before-write/) is a second problem in the same two functions: tank and source properties cannot be bulk-set at all. Its patch changes the same loop and is written on top of this one (`Requires: CRASH-15`).
- Both functions are unreleased (they are not in v2.3.5), so this can be fixed before the next release.
