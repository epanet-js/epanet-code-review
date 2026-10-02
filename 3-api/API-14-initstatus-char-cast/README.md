# API-14: EN_INITSTATUS is cast to char before the range check

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | Out-of-range link statuses are accepted with return code 0. A status of 256 closes the link, 257 opens it. |
| **Reached from** | Toolkit API: `EN_setlinkvalue` with `EN_INITSTATUS` or `EN_STATUS` and a value of 256 or more (or below 0) that wraps to 0 or 1 as a `char` |
| **Where** | `EN_setlinkvalue()` in [`src/epanet.c:4209-4210`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4209-L4210) |
| **Since** | EPANET 2.2 (the cast dates from 2018). [PR #881](https://github.com/OpenWaterAnalytics/EPANET/pull/881) (v2.3.3, 2025) narrowed the range check to 0 and 1, but after the cast. |
| **GitHub** | No related issue. The range check was last changed by [PR #881](https://github.com/OpenWaterAnalytics/EPANET/pull/881). |
| **Fix** | One line moved and changed: [`API-14_fix.patch`](API-14_fix.patch) |

## The problem

A link status set through `EN_INITSTATUS` or `EN_STATUS` must be `EN_CLOSED` (0) or `EN_OPEN` (1). `EN_setlinkvalue` checks this, but on a `char` copy of the rounded value. 256 becomes 0 and 257 becomes 1, so both pass the check and are applied: setting an open pipe's status to 256 closes it, with return code 0.

It is a small validation gap, but the check, narrowed to 0 and 1 by PR [#881](https://github.com/OpenWaterAnalytics/EPANET/issues/881), exists to refuse such values. A caller that passes a wrong enum or an uninitialised value gets a closed link and no error.

## Why it happens

```c
// src/epanet.c, EN_setlinkvalue(), case EN_INITSTATUS and EN_STATUS
char s;
...
// Status 's' must be either EN_CLOSED (0) or EN_OPEN (1)
// (to set the status of a valve to ACTIVE, assign a value to its
// EN_INITSETTING or EN_SETTING property)
s = (char)ROUND(value);
if (s < 0 || s > 1) return 211;
```

The cast keeps only the low 8 bits of the rounded value, so the test runs on 256 mod 256 = 0. Values of 2^31 and more also overflow the `int` conversion inside `ROUND`, which is undefined behaviour.

## How to reproduce

| File | What it is |
|---|---|
| [`API-14_network.inp`](API-14_network.inp) | Reservoir R1 feeds junction J1 through open pipe P1 |
| [`API-14_test.c`](API-14_test.c) | Sets `EN_INITSTATUS` (before a run) and `EN_STATUS` (during a run) of P1 to 256 and 257 and reads the status back |
| [`API-14_fix.patch`](API-14_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-14            # unpatched EPANET: FAIL
tools/run-test.sh API-14 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-14_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-14_test
./API-14_test
```

**Without the fix:**

```
EN_INITSTATUS = 256: returned   0, P1 is now CLOSED   (expected 211, open)
EN_INITSTATUS = 257: returned   0, P1 is now open   (expected 211, open)
EN_STATUS     = 256: returned   0, P1 is now CLOSED   (expected 211, open)
EN_STATUS     = 257: returned   0, P1 is now open   (expected 211, open)
FAIL: 4 of 4 out-of-range statuses were accepted
```

**With the fix:**

```
EN_INITSTATUS = 256: returned 211, P1 is now open   (expected 211, open)
EN_INITSTATUS = 257: returned 211, P1 is now open   (expected 211, open)
EN_STATUS     = 256: returned 211, P1 is now open   (expected 211, open)
EN_STATUS     = 257: returned 211, P1 is now open   (expected 211, open)
PASS: out-of-range statuses are refused with error 211
```

## The fix

Check the value before converting it:

```diff
+        // (written so that NaN is refused too)
+        if (!(value >= 0.0 && value <= 1.0)) return 211;
         s = (char)ROUND(value);
-        if (s < 0 || s > 1) return 211;
```

This also avoids the undefined conversion of very large values, and refuses NaN, which an ordered comparison such as `value < 0.0` would let through. It is slightly stricter than the old intent: values such as 1.2 or -0.3, which round to 1 or 0, are now refused. To keep the rounding, check the rounded value as an `int` instead (`ROUND(value)` before the cast), and accept that values beyond the `int` range stay undefined.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Before PR [#881](https://github.com/OpenWaterAnalytics/EPANET/issues/881) the check was `s < 0 || s > 2`, so in 2.2 to 2.3.2 values 256 to 258 were accepted the same way.
- [API-15](../API-15-pipe-setting-ignores-roughness-error/) is another validation gap in the same function. The two patches change different `case` blocks and apply together.
