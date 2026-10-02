# API-15: EN_SETTING on a pipe ignores the roughness error

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | An invalid pipe roughness set through `EN_INITSETTING` or `EN_SETTING` is ignored, but the call returns 0, so the caller believes it was applied. |
| **Reached from** | Toolkit API: `EN_setlinkvalue` with `EN_INITSETTING` or `EN_SETTING` on a pipe or check-valve pipe, with a value of 0 or less |
| **Where** | `EN_setlinkvalue()` in [`src/epanet.c:4227`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4227) |
| **Since** | v2.3: commit [30c2077](https://github.com/OpenWaterAnalytics/EPANET/commit/30c2077) (April 2025, "InitSetting added to Link struct"). In 2.2 the error was returned. |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`API-15_fix.patch`](API-15_fix.patch) |

## The problem

A pipe's setting is its roughness, so `EN_setlinkvalue(EN_INITSETTING)` and `EN_setlinkvalue(EN_SETTING)` on a pipe hand the value to the `EN_ROUGHNESS` case. That case refuses a roughness of 0 or less with error 211 and changes nothing. The setting cases drop that error and return 0. In the test, setting P1's setting to -5 returns 0 and the roughness stays 100; the same value through `EN_ROUGHNESS` returns 211.

## Why it happens

```c
// src/epanet.c, EN_setlinkvalue()
case EN_INITSETTING:
case EN_SETTING:
    if (Link[index].Type == PIPE || Link[index].Type == CVPIPE)
    {
        EN_setlinkvalue(p, index, EN_ROUGHNESS, value);   // return code dropped
        if (property == EN_INITSETTING) Link[index].InitSetting = Link[index].Kc;
    }
```

In 2.2 this branch was `return EN_setlinkvalue(p, index, EN_ROUGHNESS, value);`. Commit 30c2077 added the `InitSetting` line after the call and the return code was lost.

## How to reproduce

| File | What it is |
|---|---|
| [`API-15_network.inp`](API-15_network.inp) | Reservoir R1 feeds junction J1 through pipe P1 (roughness 100) |
| [`API-15_test.c`](API-15_test.c) | Sets P1's `EN_ROUGHNESS`, `EN_INITSETTING` and `EN_SETTING` to -5 and checks the return codes |
| [`API-15_fix.patch`](API-15_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-15            # unpatched EPANET: FAIL
tools/run-test.sh API-15 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-15_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-15_test
./API-15_test
```

**Without the fix:**

```
EN_ROUGHNESS   = -5: returned 211, roughness is 100   (expected 211, 100)
EN_INITSETTING = -5: returned   0, roughness is 100   (expected 211, 100)
EN_SETTING     = -5: returned   0, roughness is 100   (expected 211, 100)
FAIL: 2 of 3 calls returned 0 for an invalid roughness
```

**With the fix:**

```
EN_ROUGHNESS   = -5: returned 211, roughness is 100   (expected 211, 100)
EN_INITSETTING = -5: returned 211, roughness is 100   (expected 211, 100)
EN_SETTING     = -5: returned 211, roughness is 100   (expected 211, 100)
PASS: every call reports the invalid roughness with error 211
```

## The fix

Keep the return code and pass it on:

```diff
-            EN_setlinkvalue(p, index, EN_ROUGHNESS, value);
+            int errcode = EN_setlinkvalue(p, index, EN_ROUGHNESS, value);
+            if (errcode > 0) return errcode;
             if (property == EN_INITSETTING) Link[index].InitSetting = Link[index].Kc;
```

The variable is declared at the start of the block, so the change does not touch the function's declarations.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [API-14](../API-14-initstatus-char-cast/) is another validation gap in the same function. The two patches change different `case` blocks and apply together.
