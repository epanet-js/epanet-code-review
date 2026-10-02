# CRASH-05: PRECISION 7 or more overflows a 16-byte report buffer

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Stack buffer overflow when the report is written. AddressSanitizer stops the run, a plain macOS Release build aborts in the fortified `sprintf`, and a build without fortification overwrites the stack. |
| **Reached from** | Input file: `[REPORT] <field> PRECISION 7` or more, or the same command through `EN_setreport`, then `EN_report` (or `runepanet`) with node or link tables |
| **Where** | `writenodetable()` and `writelinktable()` in [`src/report.c:668`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L668), [`:697`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L697), [`:729`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L729) and [`:767`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/report.c#L767); the precision is read, unchecked, in `reportdata()`, [`src/input3.c:1755`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1755) |
| **Since** | v2.0.12 (2014), the first release in this repository |
| **GitHub** | No related issue |
| **Fix** | 2 lines and a comment: [`CRASH-05_fix.patch`](CRASH-05_fix.patch) |

## The problem

The node and link tables of the report print each value into a 16-byte buffer with the field's `PRECISION` as the number of decimals. Nothing limits that number. A head of 799.68 ft with `PRECISION 12` is `799.683837890625`, 16 characters, which needs 17 bytes with its terminator.

The higher the precision, the smaller the values that overflow: with 12 decimals any value of 100 or more, with 9 decimals any value of 100,000 or more. 7 is the lowest precision that can overflow, and only for a value of -1,000,000.

`runepanet` and every program that calls `EN_report` are affected. The command can come from the input file or from `EN_setreport`, which uses the same parser.

## Why it happens

```c
// src/report.c, writenodetable() (writelinktable() is the same)
char s[MAXLINE + 1], s1[16];
...
if (fabs(y[j]) > 1.e6) sprintf(s1, "%10.2e", y[j]);
else sprintf(s1, "%10.*f", rpt->Field[j].Precision, y[j]);

// src/input3.c, reportdata()
if (j == PREC)
{
    rpt->Field[i].Enabled = TRUE;
    rpt->Field[i].Precision = ROUND(y);
}
```

Values above 1e6 use exponent format, so the longest fixed-point value is `-1000000.` plus the decimals. With 6 decimals that is 15 characters, which fits with the terminator; with 7 it does not.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-05_precision12.inp`](CRASH-05_precision12.inp) | A reservoir at 800 ft and two junctions, with `Head Precision 12` in `[REPORT]` |
| [`CRASH-05_test.c`](CRASH-05_test.c) | Runs the network and calls `EN_report`, then looks for the longest number in the report |
| [`CRASH-05_fix.patch`](CRASH-05_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-05            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-05 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-05_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-05_test
./CRASH-05_test
```

The input file alone shows it too: `runepanet CRASH-05_precision12.inp CRASH-05.rpt`.

**Without the fix**, AddressSanitizer reports a 17-byte write into the 16-byte `s1` (`writenodetable()` is inlined into `writeresults()`):

```
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 17 at ... thread T0
    ...
    #2 ... in writeresults report.c:630
    #3 ... in writereport report.c
    #4 ... in EN_report epanet.c:916
    #5 ... in main CRASH-05_test.c:30
...
    [2640, 2656) 's1.i' (line 668)
```

A plain macOS Release build aborts in the fortified `sprintf` (exit code 133). Without sanitizers or fortification, the test reports the 16-character values it finds in the report as `FAIL`.

**With the fix**:

```
EN_report returned 0
Longest value in the report: 799.683838 (10 characters, buffer holds 15)
PASS: EN_report succeeds and every value fits the buffer
```

## The fix

Clamp the precision to 0 to 6 where it is read, so both the input file and `EN_setreport` are covered:

```diff
         if (j == PREC)
         {
+            // Report tables print values into a 16-byte buffer,
+            // which holds at most 6 decimal places (see report.c)
+            y = MAX(y, 0.0);
+            y = MIN(y, 6.0);
             rpt->Field[i].Enabled = TRUE;
             rpt->Field[i].Precision = ROUND(y);
         }
```

Clamping happens before `ROUND`, so a huge or NaN value cannot overflow the conversion to `int` either. The report values come from the binary output file, which stores 4-byte floats with about 7 significant digits, so more than 6 decimals would only print noise.

Alternatives:

- **Reject the value** with error 213, as `reportdata()` does for a `PAGESIZE` above 255. This is stricter, but files that work today with `PRECISION 7` and small values would stop loading.
- **Enlarge `s1`** and allow more decimals. This needs a limit anyway, since the precision is a free integer.

With 6 decimals a value of 100 or more fills the whole 10-character column, so it runs into the previous column in the table. That is a layout limit of the report, not a memory error, and it exists at 83e25bb for any value that is too wide.

EPANET's own unit tests (6 suites) pass with the patch.
