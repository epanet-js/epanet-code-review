# CRASH-04: inf, nan or huge values in [TIMES] and [OPTIONS] overflow integer conversions

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Undefined behaviour while the file is read. In practice the value is stored as a saturated or wrapped integer, for example a duration of 9,223,372,036,854,775,807 s, and the later time arithmetic overflows. |
| **Reached from** | Input file: a [TIMES] value, or `TRIALS`, `CHECKFREQ` or `MAXCHECK` in [OPTIONS], written as `inf`, `nan` or a number too large for the integer it is stored in |
| **Where** | `timedata()` in [`src/input3.c:1834`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1834) and `optionvalue()` in [`src/input3.c:2167`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2167) and [`2181-2182`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2181-L2182) |
| **Since** | EPANET 2.00.12 (2008) or earlier, so every release |
| **GitHub** | No related issue. Open pull request [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) would run the tests under UBSan, which flags these conversions. |
| **Fix** | Two range checks: [`CRASH-04_fix.patch`](CRASH-04_fix.patch) |

## The problem

EPANET converts each [TIMES] value to a whole number of seconds stored in a `long`, and stores `TRIALS`, `CHECKFREQ` and `MAXCHECK` in an `int`. It never checks that the value fits. Converting a floating-point value that is out of range, infinite or NaN to an integer is undefined behaviour in C.

The review's input-file fuzzer found 29 crashes with this signature. In practice the result depends on the processor. In a plain build on Apple arm64 the conversion saturates: `DURATION 1e300` is stored as 9,223,372,036,854,775,807 s (`LONG_MAX`), `TRIALS 1e11` and `CHECKFREQ 1e11` as 2,147,483,647, and `PATTERN START nan` and `MAXCHECK nan` as 0. x86-64 processors return `LONG_MIN` for an out-of-range conversion instead, which gives a negative duration. With a saturated duration the hydraulic clock (`Htime += hydstep`, [`hydraul.c:281`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydraul.c#L281)) and the pattern arithmetic in `timestep()` overflow later (review log F5).

## Why it happens

`getfloat()` accepts any spelling that `strtod` accepts, including `inf`, `nan` and `1e300` (see [FILE-10](../../4-files/FILE-10-nan-inf-accepted-as-numbers/)). The time and option code then converts without a range check:

```c
// src/input3.c, timedata()
if (!getfloat(parser->Tok[n], &y))
{
    if ((y = hour(parser->Tok[n], "")) < 0.0)
    ...
}
t = (long)(3600.0 * y + 0.5);

// src/input3.c, optionvalue()
// All other options must be > 0
if (y <= 0.0) return setError(parser, nvalue, 213);       // NaN passes
...
else if (match(tok0, w_TRIALS))   hyd->MaxIter = (int)y;
...
else if (match(tok0, w_CHECKFREQ)) hyd->CheckFreq = (int)y;
else if (match(tok0, w_MAXCHECK))  hyd->MaxCheck = (int)y;
```

`1e300` is a valid number, so the FILE-10 fix alone does not prevent this.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-04_network.inp`](CRASH-04_network.inp) | Reservoir R1 feeds junction J1. It has no `[END]` line, so a section appended to it is still read. |
| [`CRASH-04_test.c`](CRASH-04_test.c) | For each of six out-of-range values, appends one [TIMES] or [OPTIONS] line to the network (as `CRASH-04_case.inp`) and opens it |
| [`CRASH-04_fix.patch`](CRASH-04_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-04            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-04 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-04_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-04_test
./CRASH-04_test
```

**Without the fix**, UndefinedBehaviorSanitizer stops at the first case:

```
[TIMES] Duration            1e300
.../src/input3.c:1834:9: runtime error: 3.6e+303 is outside the range of representable values of type 'long'
    #0 ... in timedata input3.c:1834
    #1 ... in readdata input2.c:253
    #2 ... in getdata input1.c:64
    #3 ... in openproject project.c:70
```

Without sanitizers the test prints `FAIL: 6 of 6 out-of-range values were accepted`, with the stored values given above.

**With the fix**, every case is rejected:

```
[TIMES] Duration            1e300
    EN_open returned 200
[TIMES] Hydraulic Timestep  inf
    EN_open returned 200
...
[OPTIONS] MaxCheck            nan
    EN_open returned 200
PASS: every out-of-range value is rejected with error 200
```

The report shows, for example, `Error 213: invalid option value nan in [OPTIONS] section:`.

## The fix

Check the value before converting it. Both checks are written as `!(in range)`, so that NaN, which fails every comparison, is rejected too:

```diff
+    // Reject a time that is negative, too large or not a number: its
+    // number of seconds must fit in half the range of a long, so that
+    // two times can be added without overflow
+    if (!(y >= 0.0 && y < LONG_MAX / 7200.0)) return setError(parser, n, 213);
     t = (long)(3600.0 * y + 0.5);
 ...
     // All other options must be > 0
     if (y <= 0.0) return setError(parser, nvalue, 213);
 
+    // Options stored as integers must fit in an int (NaN fails this test)
+    if ((match(tok0, w_TRIALS) || match(tok0, w_CHECKFREQ) ||
+         match(tok0, w_MAXCHECK)) && !(y <= INT_MAX))
+        return setError(parser, nvalue, 213);
```

`input3.c` also gets `#include <limits.h>`.

Choices a maintainer may want to revisit:

- **The time limit.** Half the range of a `long` leaves room for sums such as `Htime + Hstep`. Where `long` is 64 bits the limit is far beyond any real run. On Windows `long` is 32 bits and the limit is 298,261 hours (34 years); without the patch, times above 68 years already overflow there. The check also rejects negative times, which the parser accepted before. `EN_settimeparam` already rejects negative values.
- **Not covered.** A `TRIALS` value close to `INT_MAX` with `UNBALANCED CONTINUE n` still overflows `maxtrials += hyd->ExtraIter` in [`hydsolver.c:112`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydsolver.c#L112) (by reading). The toolkit has the same gaps: `EN_settimeparam` has no upper limit, and `EN_setoption` converts `EN_TRIALS`, `EN_CHECKFREQ`, `EN_MAXCHECK` and `EN_UNBALANCED` with `(int)value` and no upper limit (review log F5 addendum).

## Notes

- [FILE-10](../../4-files/FILE-10-nan-inf-accepted-as-numbers/) makes `getfloat()` reject `inf` and `nan`. With both patches, `inf` and `nan` are rejected by `getfloat()` and `1e300` by this check. The patches change different files and apply in either order.
- [FILE-13](../../4-files/FILE-13-fractional-trials-zero-iterations/) adds a lower limit of 1 to `TRIALS`, and [SIM-15](../../1-wrong-results/SIM-15-pressure-exponent-zero/) changes the `PRESSURE EXPONENT` check a few lines above. The hunks do not overlap.

EPANET's own unit tests (6 suites) pass with the patch.
