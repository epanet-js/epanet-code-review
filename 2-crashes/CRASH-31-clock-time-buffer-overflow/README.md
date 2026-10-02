# CRASH-31: A run of 1,000,000 hours or more overflows the 13-byte clock-time buffers

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | With a status report, a stack buffer overflow; a plain macOS build aborts in `EN_solveH`. Without one, the report file's stored name is overwritten silently, and `EN_copyreport` then fails with error 303. |
| **Reached from** | Input file or toolkit: any run that reaches 1,000,000 hours (114 years). `EN_solveH` overflows on every such run; `EN_runH` only with a status report; `EN_report` when it writes a period at or after 1,000,000 hours. `EN_saveinpfile` overflows the same buffer when a [TIMES] value is 1,000,000 hours or more. |
| **Where** | `clocktime()` in [`src/report.c:1530`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L1530), `Report.Atime` in [`src/types.h:667`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/types.h#L667), and `writehydstat()` in [`src/report.c:352-358`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L352-L358) |
| **Since** | EPANET 2.00.12 (2008) or earlier, so every release |
| **GitHub** | No related issue |
| **Fix** | Two buffer sizes and one format: [`CRASH-31_fix.patch`](CRASH-31_fix.patch) |

## The problem

EPANET writes times as hours, minutes and seconds, for example `24:00:00`, using `clocktime()`. The text goes into `Report.Atime`, a 13-byte array: room for 12 characters and the terminator. From 1,000,000 hours on, the text is `1000000:00:00`, 13 characters, so the terminator is written one byte past the array.

- **The report file's name is lost.** The next field in the `Report` struct is `Rpt1Fname`, the name of the report file. `EN_solveH` formats the current time for its progress message at every step, so a run that reaches 1,000,000 hours sets the name to an empty string. Nothing fails at once and no sanitizer reports it, because the write stays inside the `Project` allocation. Later, `EN_copyreport` fails with error 303 when it reopens the report file by name (and so, by reading the code, does `EN_clearreport`). Longer times write further into the name.
- **A stack overflow with a status report.** `writehydstat()` copies the text into its own `char atime[13]` with `strcpy`. AddressSanitizer stops the run there, and a plain macOS build aborts (exit code 133).

Such a run is not physical, but EPANET accepts it from an input file or `EN_settimeparam`, and the test input runs in under 2 s with 100,000-hour steps. This was found while testing [CRASH-30](../CRASH-30-api-out-of-range-conversions/), where a huge `EN_DURATION` led to the same overflow after a million one-hour steps.

## Why it happens

```c
// src/types.h, Report
char
  Atime[13],             // Clock time (hrs:min:sec)
  Rpt1Fname[MAXFNAME+1], // Primary report file name

// src/report.c, clocktime()
h = seconds / 3600;
m = seconds % 3600 / 60;
s = seconds - 3600 * h - 60 * m;
sprintf(atime, "%01d:%02d:%02d", (int)h, (int)m, (int)s);

// src/report.c, writehydstat()
char atime[13];
...
strcpy(atime, clocktime(rpt->Atime, time->Htime));
```

The buffers allow six digits of hours, 999,999 hours (114 years). A `long` time can need up to 25 characters. The `(int)h` cast also gives the wrong hours once they exceed `INT_MAX`.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-31_million-hours.inp`](CRASH-31_million-hours.inp) | Reservoir R1 feeds junction J1. The run lasts 1,000,000 hours in 100,000-hour steps, with a status report. |
| [`CRASH-31_test.c`](CRASH-31_test.c) | Solves the file twice, with the status report off and then on. After each run it copies the report with `EN_copyreport` and saves the input file, then checks both for the time `1000000:00:00`. |
| [`CRASH-31_fix.patch`](CRASH-31_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-31            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-31 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-31_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-31_test
./CRASH-31_test
```

The command-line runner shows the crash too: `runepanet CRASH-31_million-hours.inp CRASH-31.rpt`.

**Without the fix**, the first run loses the report file's name with no sanitizer report, and the second overflows the stack:

```
Status report off:
  EN_solveH returned 0, EN_copyreport returned 303 (expected 0, 0)
  Saved input file has DURATION 1000000:00:00: yes
Status report on:
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 14 at ... thread T0
    #0 ... in wrap_strcpy+0x510 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4d110)
    #1 ... in writehydstat report.c:358
    #2 ... in runhyd hydraul.c:224
    #3 ... in EN_runH epanet.c:567
...
    [1200, 1213) 'atime' (line 352) <== Memory access at offset 1213 overflows this variable
```

**With the fix**:

```
Status report off:
  EN_solveH returned 0, EN_copyreport returned 0 (expected 0, 0)
  Saved input file has DURATION 1000000:00:00: yes
Status report on:
  EN_solveH returned 0, EN_copyreport returned 0 (expected 0, 0)
  Report has the status line for 1000000:00:00: yes
  Saved input file has DURATION 1000000:00:00: yes
PASS: a run of 1,000,000 hours is reported and saved correctly
```

## The fix

Size the buffer for any `long` number of seconds, print the hours as a `long`, and size `writehydstat()`'s copy from `Atime` so the two cannot drift apart:

```diff
-    Atime[13],             // Clock time (hrs:min:sec)
+    Atime[26],             // Clock time (hrs:min:sec), long enough
+                           // for any long number of seconds
 ...
-  char atime[13];
+  char atime[sizeof(rpt->Atime)];
 ...
-    sprintf(atime, "%01d:%02d:%02d", (int)h, (int)m, (int)s);
+    sprintf(atime, "%01ld:%02ld:%02ld", h, m, s);
```

The longest text a 64-bit `long` gives is 25 characters: 17 for the hours of `LONG_MIN` seconds, and up to 3 each for minutes and seconds, which are negative for a negative time, plus the two colons. With [CRASH-04](../CRASH-04-inf-nan-time-values/) and [CRASH-30](../CRASH-30-api-out-of-range-conversions/), times are at least 0 and at most half of `LONG_MAX` seconds, so at most 22 characters are needed. The size here does not depend on those patches.

Making `clocktime()` cut its output to 12 characters would also stop the overflow. But `EN_saveinpfile` writes the [TIMES] section with `clocktime()`, so a long duration would be saved wrong.

EPANET's own unit tests (6 suites) pass with the patch.
