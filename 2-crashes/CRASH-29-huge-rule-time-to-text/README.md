# CRASH-29: Rule time values of 2^31 seconds or more are converted with undefined behaviour

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Undefined behaviour when a rule with a very large `TIME` or `CLOCKTIME` value is checked during a run, and when it is saved. On x86-64 the saved file gets a garbled time; arm64 happens to give the expected results. UndefinedBehaviorSanitizer stops the program. |
| **Reached from** | A rule premise on `SYSTEM TIME` or `SYSTEM CLOCKTIME` (or, before [FILE-03](../../4-files/FILE-03-filltime-rules-not-reloadable/), `FILLTIME` or `DRAINTIME` when saving) from an input file, `EN_addrule`, `EN_setpremise` or `EN_setpremisevalue`. Saving needs 2^31 s (596,523 hours) or more; the run check needs more than the range of a `long` (2^31 s on Windows, about 9.2e18 s on Linux and macOS). |
| **Where** | `checktime()` in [`src/rules.c:1012`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1012) and `gettimetxt()` in [`src/rules.c:1492`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1492) |
| **Since** | `checktime()`: the first commit of this repository (2014). `gettimetxt()`: v2.2, when `EN_saveinpfile` started writing rules from the parsed data (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue. PR [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) (open) adds a build flag to run the tests under UBSan, which would report this in a test with such a rule. |
| **Fix** | A few lines in two functions: [`CRASH-29_fix.patch`](CRASH-29_fix.patch) |

## The problem

A rule's time premise is stored as a `double` number of seconds. Two places convert it to an integer type without checking that it fits, which is undefined behaviour in C:

- **During a run**, `checktime()` compares `(long)value` with the simulation time. `IF SYSTEM TIME > 1e300` (hours) is accepted by the parser and `EN_addrule`, and is checked at every rule time step. On Windows, where `long` has 32 bits, any time of 2^31 s (about 68 years) or more is enough.
- **When saving**, `gettimetxt()` decides between `h:mm:ss` and decimal hours by first computing `(int)secs / 3600`. Any time of 2^31 s or more, such as `IF SYSTEM TIME >= 600000`, is undefined here. arm64 saturates the conversion and happens to write `600000.0000`. x86-64 gives `INT_MIN` and writes `-596523:71791380:-2147483648`, which `hour()` reads back as about 3,477 hours, so the rule silently gets a different time. That x86-64 result was checked with a copy of `gettimetxt()` built for x86-64 and run under Rosetta.

Such times have no use in a model, but nothing rejects them, and `EN_setpremisevalue` accepts any number, so a unit mistake in a program is enough.

## Why it happens

```c
// src/rules.c, checktime()
    char flag;
    long t1, t2, x;
    ...
    // Test premise's time
    x = (long)(p->value);

// src/rules.c, gettimetxt()
    int hours = 0, minutes = 0, seconds = 0;
    hours = (int)secs / 3600;
    if (hours > 24 * 7) sprintf(timetxt, "%.4f", secs / 3600.0);
    else
    {
        minutes = (int)((secs - 3600 * hours) / 60);
        ...
```

In both functions the conversion comes before any test of the value.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-29_tank.inp`](CRASH-29_tank.inp) | Reservoir R1 fills tank T1 through pipe P2; no rules |
| [`CRASH-29_test.c`](CRASH-29_test.c) | Adds rule LATE (`TIME >= 600000` hours, 2.16e9 s) and rule NEVER (`TIME > 1e300` hours), both closing pipe P1, runs the 6-hour simulation, then deletes NEVER, saves the project and reads LATE's time back from the saved file |
| [`CRASH-29_fix.patch`](CRASH-29_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-29            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-29 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-29_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-29_test
./CRASH-29_test
```

**Without the fix**, UndefinedBehaviorSanitizer stops at the first rule check:

```
EN_addrule returned 0 and 0, LATE time 2160000000 s
.../src/rules.c:1012:9: runtime error: 3.6e+303 is outside the range of representable values of type 'long'
    #0 ... in checkpremise rules.c:982
    #1 ... in checkrules rules.c:548
    #2 ... in ruletimestep hydraul.c:880
    #3 ... in timestep hydraul.c:682
```

(`checktime()` is inlined into `checkpremise()`.) Without the run, the save stops in `gettimetxt()` instead. This is the output of an earlier version of the test, which added only rule LATE and saved:

```
EN_addrule returned 0, premise time 2160000000 s
.../src/rules.c:1492:13: runtime error: 2.16e+09 is outside the range of representable values of type 'int'
    #0 ... in writerule rules.c:493
    #1 ... in saveinpfile inpfile.c:535
```

A build without sanitizers on arm64 (macOS, Release) saturates both conversions, and the test passes there.

**With the fix:**

```
EN_addrule returned 0 and 0, LATE time 2160000000 s
EN_solveH returned 0; at the end P1 is open
EN_saveinpfile returned 0
EN_open(saved file) returned 0
LATE time: expected 2160000000 s, read back 2160000000 s
PASS: rule times of 2^31 s or more are checked, saved and read back
```

The saved premise is `IF   SYSTEM  TIME >= 600000.0000`.

## The fix

`checktime()` compares the `double` itself, so no conversion is needed:

```diff
-    long t1, t2, x;
+    long t1, t2;
+    double x;
...
-    // Test premise's time
-    x = (long)(p->value);
+    // Test premise's time (as a double: converting a very large time to
+    // long would be undefined)
+    x = p->value;
```

`gettimetxt()` tests the range on the `double` and converts only in the `h:mm:ss` branch:

```diff
     int hours = 0, minutes = 0, seconds = 0;
-    hours = (int)secs / 3600;
-    if (hours > 24 * 7) sprintf(timetxt, "%.4f", secs / 3600.0);
+
+    // Times of 169 hours or more are written in decimal hours. Test the
+    // double, since (int)secs is undefined for 2^31 s or more.
+    if (secs < 0.0 || secs >= (24 * 7 + 1) * 3600.0)
+    {
+        sprintf(timetxt, "%.4f", secs / 3600.0);
+    }
     else
     {
+        hours = (int)secs / 3600;
         minutes = (int)((secs - 3600 * hours) / 60);
```

For whole-second times both functions behave exactly as before: `(int)secs / 3600 > 168` holds exactly when `secs >= 169 * 3600`, and comparing a whole number as a `double` gives the same result as comparing it as a `long`. Two things change:

- A time with a fraction of a second is now compared as it is, not truncated. The parser produces such values for many clock times (8:00:01 is stored as 28,800.999999999996 s), so a rule that fired a rule step early because of the truncation now fires on time. [FILE-18](../../4-files/FILE-18-rule-times-lose-a-second/) rounds those values when they are read, which also fixes how they are saved.
- A negative time, which only the setters can store, is written in decimal hours instead of a garbled `h:mm:ss` value.

Fixing the two consumers covers every way a time can be stored, including `EN_setpremise` and `EN_setpremisevalue`. Rejecting very large times where they are parsed would cover only the input file and `EN_addrule`. It could be added as well, but it would change the same lines in `newpremise()` as FILE-18.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Saving rule NEVER would write 1e300 hours with `"%.4f"`, which overflows the 20-character buffer of the rule writer ([CRASH-09](../CRASH-09-saveinp-rule-value-overflow/)), so the test deletes it before saving. With CRASH-09 applied it is written in full.
- [CRASH-09](../CRASH-09-saveinp-rule-value-overflow/), [FILE-03](../../4-files/FILE-03-filltime-rules-not-reloadable/) and [FILE-18](../../4-files/FILE-18-rule-times-lose-a-second/) change other lines of `rules.c`. All the patches apply together.
- The UBSan report for `checktime()` was also found independently in another part of this review, with `IF SYSTEM TIME > 1e300` in an input file.
