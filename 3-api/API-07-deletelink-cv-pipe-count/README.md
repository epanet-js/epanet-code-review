# API-07: EN_deletelink does not decrement the pipe count for check-valve pipes

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | Each deleted check-valve pipe leaves the pipe count one too high. The count is only used in the report's input summary, so the effect is a wrong "Number of Pipes" line. |
| **Reached from** | Toolkit API: `EN_deletelink` on an `EN_CVPIPE` link (also `EN_deletenode` on a node with one, and `EN_setlinktype` from `EN_CVPIPE` to a pump or valve), followed by a report with an input summary |
| **Where** | `EN_deletelink()` in [`src/epanet.c:3602-3606`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3602-L3606) |
| **Since** | v2.3, with the fix for [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790) |
| **GitHub** | Incomplete fix of [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790), fixed by [#792](https://github.com/OpenWaterAnalytics/EPANET/pull/792) (commit [d50c673](https://github.com/OpenWaterAnalytics/EPANET/commit/d50c6733213d8952daddfd823531527df5e1bbe4)) for `PIPE` only |
| **Fix** | One character, plus a comment: [`API-07_fix.patch`](API-07_fix.patch) |

## The problem

A pipe with a check valve is a link of type `EN_CVPIPE`. The input parser and `EN_addlink` count it as a pipe in `Npipes`. `EN_deletelink` only decrements `Npipes` for type `EN_PIPE`, so deleting a check-valve pipe leaves the count one too high.

`Npipes` is not available through `EN_getcount`. Its only use is the "Number of Pipes" line of the input summary that `writesummary()` writes to a report file. In the test, a network with three pipes still reports 3 after its check-valve pipe is deleted.

This was found by reading the code. The review's first test only checked the return code of `EN_deletelink`; the test here reads the count from a report.

## Why it happens

The fix for [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790) added the decrement with an equality test. `EN_CVPIPE` is 0 and `EN_PIPE` is 1, and the rest of the code tests `<= PIPE` for "pipe with or without a check valve":

```c
// src/epanet.c, EN_addlink()
    if (linkType <= PIPE) net->Npipes++;

// src/epanet.c, EN_deletelink()
    // Reduce the number of pipes count by one if it is a pipe.
    if (linkType == PIPE)
    {
        net->Npipes--;
    }
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-07_cv-pipe.inp`](API-07_cv-pipe.inp) | Three pipes; P3 has a check valve (status `CV`) |
| [`API-07_test.c`](API-07_test.c) | Deletes P3, solves, writes a full report to a second report file with `EN_report`, and reads "Number of Pipes" from it |
| [`API-07_fix.patch`](API-07_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-07            # unpatched EPANET: FAIL
tools/run-test.sh API-07 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-07_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-07_test
./API-07_test
```

The second report file (`EN_setreport(ph, "FILE API-07_after.rpt")`) is needed because `EN_report` writes the input summary only to a secondary report file; the primary one gets it at `EN_open`.

**Without the fix**, the summary still counts the deleted pipe:

```
Deleting P3 (link type 0, EN_CVPIPE = 0)
Summary written by EN_open:       3 pipes
Summary written after the delete: 3 pipes (expected 2)
FAIL: deleting a check-valve pipe left the pipe count at 3
```

**With the fix**:

```
Summary written after the delete: 2 pipes (expected 2)
PASS: the pipe count drops when a check-valve pipe is deleted
```

## The fix

Use the same test as `EN_addlink`:

```diff
-    // Reduce the number of pipes count by one if it is a pipe.
-    if (linkType == PIPE)
+    // Reduce the number of pipes count by one if it is a pipe
+    // (with or without a check valve).
+    if (linkType <= PIPE)
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `EN_setlinktype` between `EN_PIPE` and `EN_CVPIPE` keeps the link and does not touch the count, which is correct. A change from `EN_CVPIPE` to a pump or valve goes through `EN_deletelink`, so it is also fixed by this patch.
- [WQ-02](../../5-water-quality/WQ-02-check-valve-pipes-have-no-volume/) is the same slip in the water quality code: tests for `PIPE` that leave out `CVPIPE` give check-valve pipes no volume. [API-03](../API-03-setlinktype-cv-keeps-controls/) is another check-valve gap in the toolkit.
