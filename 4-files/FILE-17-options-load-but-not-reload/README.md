# FILE-17: [OPTIONS] values that load but cannot be saved and reloaded

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A file that EPANET opens without complaint is saved by `EN_saveinpfile` in a form that fails to open (error 200), or that opens with a different value. With `CHECKFREQ` or `MAXCHECK` below 1, the periodic status checks during each solve also stop. |
| **Reached from** | Input file: `CHECKFREQ` or `MAXCHECK` above 0 and below 1, or a small value of `SPECIFIC GRAVITY`, `DEMAND MULTIPLIER`, `EMITTER EXPONENT`, `PRESSURE EXPONENT`, `TOLERANCE`, `DAMPLIMIT`, `HEADERROR` or `FLOWCHANGE`, followed by `EN_saveinpfile` |
| **Where** | `optionvalue()` in [`src/input3.c:2181-2182`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2181-L2182); `saveinpfile()` in [`src/inpfile.c:729-756`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L729-L756) |
| **Since** | EPANET 2.00.12 (2008) or earlier for the parser and most of the formats, so every release. `HEADERROR`, `FLOWCHANGE` and the PDA options were added in v2.2. |
| **GitHub** | No related issue. [API-16](../../3-api/API-16-options-not-reloadable/) fixes the same class of problem in `EN_setoption`. |
| **Fix** | A lower limit for two options and eight output formats: [`FILE-17_fix.patch`](FILE-17_fix.patch) |

## The problem

A network that EPANET reads should survive `EN_saveinpfile` and a reload. These [OPTIONS] values do not:

- **`CHECKFREQ` or `MAXCHECK` between 0 and 1.** The parser only requires the value to be above 0, then stores `(int)0.5`, which is 0. The saved file says `CHECKFREQ 0`, and the same parser refuses that with `Error 213: invalid option value 0 in [OPTIONS] section:`. `TRIALS` has the same pattern; [FILE-13](../FILE-13-fractional-trials-zero-iterations/) adds a lower limit of 1 for it.
- **Small real values.** The writer uses a fixed number of decimals: 4 for `DEMAND MULTIPLIER`, `EMITTER EXPONENT` and `PRESSURE EXPONENT`, 6 for `SPECIFIC GRAVITY` and 8 for `TOLERANCE`, `DAMPLIMIT`, `HEADERROR` and `FLOWCHANGE`. A smaller value is saved as 0. For the first three in the test, 0 is not allowed, so the copy fails to open (`Error 213: invalid option value 0.000000`). For the others the copy opens with 0. For `TOLERANCE`, `DAMPLIMIT`, `HEADERROR` and `FLOWCHANGE`, 0 turns the setting off. A `PRESSURE EXPONENT` of 0 gives NaN results ([SIM-15](../../1-wrong-results/SIM-15-pressure-exponent-zero/)), and with SIM-15's fix the copy fails to open instead.

Most of these values are unusual, but EPANET accepts them, and the failure only shows when the saved file is used.

`CHECKFREQ` or `MAXCHECK` at 0 also changes the solver. By reading `hydsolve()`, `nextcheck` starts at `CheckFreq` (0) and the trial counter starts at 1, so `*iter == nextcheck` never holds. With `MAXCHECK` 0, `*iter <= MaxCheck` never holds. Either way the periodic status checks before convergence never run. In a plain build, Net1 with `CHECKFREQ 0.5` takes 62 trials over its 24-hour run instead of 69. The heads are the same there.

## Why it happens

```c
// src/input3.c, optionvalue()
// All other options must be > 0
if (y <= 0.0) return setError(parser, nvalue, 213);
...
else if (match(tok0, w_CHECKFREQ)) hyd->CheckFreq = (int)y;    // 0.5 -> 0
else if (match(tok0, w_MAXCHECK))  hyd->MaxCheck = (int)y;

// src/inpfile.c, saveinpfile()
fprintf(f, "\n DEMAND MULTIPLIER   %-.4f", hyd->Dmult);          // 0.00001 -> 0.0000
fprintf(f, "\n EMITTER EXPONENT    %-.4f", 1.0 / hyd->Qexp);
...
fprintf(f, "\n SPECIFIC GRAVITY    %-.6f", hyd->SpGrav);         // 1e-7 -> 0.000000
...
fprintf(f, "\n TOLERANCE           %-.8f", qual->Ctol * pr->Ucf[QUALITY]);
...
fprintf(f, "\n CHECKFREQ           %-d", hyd->CheckFreq);        // 0
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-17_network.inp`](FILE-17_network.inp) | Reservoir R1 feeds junction J1. It has no `[END]` line, so an [OPTIONS] section appended to it is still read. |
| [`FILE-17_test.c`](FILE-17_test.c) | For each of ten values, appends one [OPTIONS] line (as `FILE-17_case.inp`), opens the file, saves it, and opens the saved copy |
| [`FILE-17_fix.patch`](FILE-17_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-17            # unpatched EPANET: FAIL
tools/run-test.sh FILE-17 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-17_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o FILE-17_test
./FILE-17_test
```

**Without the fix**, every value loads, and none survives the save:

```
[OPTIONS] value                  first open             saved copy
CHECKFREQ          0.5           0                      error 200   <-- not reloaded
MAXCHECK           0.5           0                      error 200   <-- not reloaded
SPECIFIC GRAVITY   0.0000001     1e-07                  error 200   <-- not reloaded
DEMAND MULTIPLIER  0.00001       1e-05                  error 200   <-- not reloaded
EMITTER EXPONENT   0.00001       1e-05                  error 200   <-- not reloaded
TOLERANCE          0.000000001   1e-09                  0   <-- not reloaded
DAMPLIMIT          0.000000001   1e-09                  0   <-- not reloaded
HEADERROR          0.000000001   1e-09                  0   <-- not reloaded
FLOWCHANGE         0.000000001   1e-09                  0   <-- not reloaded
PRESSURE EXPONENT  0.00001       1e-05                  0   <-- not reloaded
FAIL: 10 of 10 option values load but do not survive a save and reload
```

**With the fix**, the two integer options are refused when the file is first read, and the rest come back unchanged:

```
[OPTIONS] value                  first open             saved copy
CHECKFREQ          0.5           error 200              -
MAXCHECK           0.5           error 200              -
SPECIFIC GRAVITY   0.0000001     1e-07                  1e-07
DEMAND MULTIPLIER  0.00001       1e-05                  1e-05
EMITTER EXPONENT   0.00001       1e-05                  1e-05
TOLERANCE          0.000000001   1e-09                  1e-09
DAMPLIMIT          0.000000001   1e-09                  1e-09
HEADERROR          0.000000001   1e-09                  1e-09
FLOWCHANGE         0.000000001   1e-09                  1e-09
PRESSURE EXPONENT  0.00001       1e-05                  1e-05
PASS: every option value is either refused or survives a save and reload
```

## The fix

Refuse `CHECKFREQ` and `MAXCHECK` below 1 when the file is read, as FILE-13 does for `TRIALS` and API-16 does for `EN_setoption`:

```diff
-    else if (match(tok0, w_CHECKFREQ)) hyd->CheckFreq = (int)y;
-    else if (match(tok0, w_MAXCHECK))  hyd->MaxCheck = (int)y;
+    else if (match(tok0, w_CHECKFREQ))
+    {
+        if (y < 1.0) return setError(parser, nvalue, 213);
+        hyd->CheckFreq = (int)y;
+    }
+    else if (match(tok0, w_MAXCHECK))
+    {
+        if (y < 1.0) return setError(parser, nvalue, 213);
+        hyd->MaxCheck = (int)y;
+    }
```

Write the eight real values with `%-.10g`, which keeps ten significant digits at any size, as [FILE-07](../FILE-07-four-decimal-formats-truncate/) does for curve points, pattern factors and settings. For example:

```diff
-    fprintf(f, "\n SPECIFIC GRAVITY    %-.6f", hyd->SpGrav);
+    fprintf(f, "\n SPECIFIC GRAVITY    %-.10g", hyd->SpGrav);
```

The saved files change slightly in appearance: `DEMAND MULTIPLIER 1.0000` becomes `DEMAND MULTIPLIER 1`. The parser reads both.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- **Not changed.** `VISCOSITY` and `DIFFUSIVITY` are written as values relative to water and chlorine, but the parser reads a `VISCOSITY` of 0.001 or less (`DIFFUSIVITY` 0.0001 or less) as an actual value. A file with an actual viscosity below about 1.1e-8 ft²/s therefore reloads with a different one: `VISCOSITY 1e-9` comes back as 8.27 times that of water. Such values are not physical, and changing the rule is a format decision. API-16 refuses the matching relative values in `EN_setoption`.
- The patches for this issue, [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/) and FILE-13 all change `optionvalue()`, in separate hunks, and apply together.
