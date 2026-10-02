# FILE-10: nan, inf, hex and empty tokens are accepted as numbers

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Invalid values load without error. A NaN passes every range check, and the run then reports NaN results with return code 0. |
| **Reached from** | Input file: any numeric field. Also `EN_setreport`, `EN_addrule` and `EN_loadpatternfile`, which read numbers with the same function. |
| **Where** | `getfloat()` in [`src/input2.c:721`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L721) |
| **Since** | EPANET 2.00.12 (2008) or earlier, so every release. Pull request [#919](https://github.com/OpenWaterAnalytics/EPANET/pull/919) (2026) rewrote `getfloat` for the decimal point but kept the same end test. |
| **GitHub** | No related issue. Open pull request [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) would run the tests under UBSan, which flags the float-to-integer conversions that NaN and infinity lead to (see [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/)). |
| **Fix** | Four lines: [`FILE-10_fix.patch`](FILE-10_fix.patch) |

## The problem

EPANET reads every number in the input file with `getfloat()`, which uses `strtod`. `strtod` accepts more than decimal numbers: `nan`, `inf`, `infinity` and hexadecimal such as `0x64`. `getfloat()` also accepts an empty token as 0 and, where `char` is signed (x86, Windows and Apple platforms, but not Linux on ARM), a number followed by any byte above 127, such as `12°` in a UTF-8 file.

None of these is a valid EPANET number. They load without error, and NaN then passes every range check, because a check such as `x <= 0.0` is false for NaN. In the test, a pipe with diameter `nan` loads. In a plain build, `EN_solveH` on that network returns 0 and every pressure and flow is NaN.

## Why it happens

```c
// src/input2.c, getfloat()
char *endptr;
...
if (lc->decimal_point[0] == '.')
{
    *y = strtod(s, &endptr);
}
...
if (*endptr > 0) return 0;          // characters left after the number
return 1;
```

The end test has two faults:

- For an empty token, `strtod` reads nothing and `endptr` points at the terminating null, so the test passes and `*y` is 0.
- `*endptr` is a `char`. Where `char` is signed, a byte above 127 is negative, so `*endptr > 0` is false and the trailing characters are ignored.

And the range checks that follow cannot catch NaN, for example in `pipedata()` ([`input3.c:410-412`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L410-L412)):

```c
if (!getfloat(parser->Tok[4], &x) || x <= 0.0)       // NaN <= 0.0 is false
    return setError(parser, 4, 202);
link->Diam = x;
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-10_bad-numbers.inp`](FILE-10_bad-numbers.inp) | Five parallel pipes from reservoir R1 to junction J1. Each has one bad value: diameter `nan`, length `inf`, roughness `0x64`, minor loss `""` and diameter `12°`. |
| [`FILE-10_test.c`](FILE-10_test.c) | Opens the file. If it loads, prints the value stored for each bad token. Then counts the error 202 lines in the report. |
| [`FILE-10_fix.patch`](FILE-10_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-10            # unpatched EPANET: FAIL
tools/run-test.sh FILE-10 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-10_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-10_test
./FILE-10_test
```

The command-line runner shows it too: `runepanet FILE-10_bad-numbers.inp FILE-10.rpt` prints `EPANET ran successfully.`

**Without the fix**, all five tokens are accepted:

```
EN_open returned 0 (expected 200)
  P1 diameter   token nan    stored as nan
  P2 length     token inf    stored as inf
  P3 roughness  token 0x64   stored as 100
  P4 minor loss token ""     stored as 0
  P5 diameter   token 12°   stored as 12
Error 202 reported 0 times (expected 5)
FAIL: tokens that are not decimal numbers were accepted
```

**With the fix**, each is reported as `Error 202: illegal numeric value nan in [PIPES] section:` (and so on):

```
EN_open returned 200 (expected 200)
Error 202 reported 5 times (expected 5)
PASS: all five tokens are rejected as illegal numeric values
```

## The fix

Require the whole token to be read, and reject hexadecimal forms and results that are not finite:

```diff
-    if (*endptr > 0) return 0;
+    // Reject an empty token, characters after the number, and the
+    // hexadecimal, infinity and NaN forms that strtod also accepts
+    if (endptr == s || *endptr != '\0') return 0;
+    if (strpbrk(s, "xX") != NULL || !isfinite(*y)) return 0;
     return 1;
```

The finiteness test also rejects numbers that overflow a `double`, such as `1e999`, which `strtod` returns as infinity. The locale handling above it (the `strtod_l` path for a comma decimal point) is not changed; see [API-24](../../3-api/API-24-library-changes-host-locale/) for why that path is taken more often than it should be.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Valid numbers too large for the integer they are stored in, such as `DURATION 1e300` or `TRIALS 1e11`, still pass `getfloat()`. [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/) adds the range checks for those.
- A trailing newline left on a token by [FILE-14](../FILE-14-quoted-tokens-mis-split/) is rejected both before and after this change.
