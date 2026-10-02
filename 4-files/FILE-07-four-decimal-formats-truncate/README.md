# FILE-07: Fixed four-decimal formats truncate small values

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Saved files round small values to four decimals: a pattern factor of 0.00005 comes back as 0.0001, and a flow of 0.00045 m³/s as 0.0004. Values below 0.00005 become 0. |
| **Reached from** | Toolkit API: `EN_saveinpfile`, then reopening the file. Most visible with flows in CMS or MGD, small pattern factors and D-W roughness of smooth pipe. |
| **Where** | `saveinpfile()` in `src/inpfile.c`: pipe roughness ([`:221`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L221)), valve settings ([`:325-327`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L325-L327)), pattern factors ([`:435`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L435)), curve points ([`:449-454`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L449-L454)), control settings and levels ([`:495`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L495), [`:508`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L508)); rule values in `src/rules.c` ([`:1422`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1422), [`:1453`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1453)) |
| **Since** | v2.0.12 (2014), the first release in this repository |
| **GitHub** | No related issue |
| **Fix** | Formats on 8 lines: [`FILE-07_fix.patch`](FILE-07_fix.patch) |

## The problem

`EN_saveinpfile` writes most numbers with `%.4f`, four decimals whatever their size. That is fine for elevations, lengths and diameters, but some inputs are routinely below 0.01, where four decimals keep only one or two significant digits:

- **Flows in CMS or MGD.** A small pump curve or FCV setting in m³/s, such as 0.00045 (0.45 L/s), comes back as 0.0004, 11 % lower.
- **Pattern factors.** A factor close to zero, used to switch a demand or pump almost off, becomes 0.0001 or 0.
- **D-W roughness of smooth pipe.** Plastic pipe at 0.0015 mm is 0.0049 millifeet in US units, written as 0.0049.
- **Control settings and levels**, which are in the same units as the values above.

Each save rounds again, and the values are changed without any message.

## Why it happens

```c
// src/inpfile.c, saveinpfile()
...
            fprintf(f, "\t%-12.4f", net->Pattern[i].F[j]);           // [PATTERNS]
...
            fprintf(f, "\n %-31s\t%-12.4f\t%-12.4f\t%s", curve->ID,   // [CURVES]
                curve->X[0], curve->Y[0], CurveTypeTxt[curve->Type]);
...
            sprintf(s, " LINK %s %.4f", link->ID, kc);                // [CONTROLS]
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-07_small-values.inp`](FILE-07_small-values.inp) | A small booster in CMS units with four values below 0.001: a pump curve flow of 0.00045, an FCV setting of 0.00025, a control setting of 0.00012 and a pattern factor of 0.00005 |
| [`FILE-07_test.c`](FILE-07_test.c) | Reads the four values, saves the project, reopens the saved file and reads them again |
| [`FILE-07_fix.patch`](FILE-07_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-07            # unpatched EPANET: FAIL
tools/run-test.sh FILE-07 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-07_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o FILE-07_test
./FILE-07_test
```

**Without the fix**:

```
Value                     Input file   Saved and reloaded
pump curve flow (m3/s)       0.00045       0.0004   <-- changed
FCV setting (m3/s)           0.00025       0.0003   <-- changed
control setting (m3/s)       0.00012       0.0001   <-- changed
pattern factor                 5e-05       0.0001   <-- changed
FAIL: 4 of 4 small values changed when the file was saved
```

**With the fix**:

```
Value                     Input file   Saved and reloaded
pump curve flow (m3/s)       0.00045      0.00045   
FCV setting (m3/s)           0.00025      0.00025   
control setting (m3/s)       0.00012      0.00012   
pattern factor                 5e-05        5e-05   
PASS: small values survive saving and reloading
```

## The fix

Write these values with `%.10g` at the same column width, for example:

```diff
-            fprintf(f, "\t%-12.4f", net->Pattern[i].F[j]);
+            fprintf(f, "\t%-12.10g", net->Pattern[i].F[j]);
...
-            sprintf(s, " LINK %s %.4f", link->ID, kc);
+            sprintf(s, " LINK %s %.10g", link->ID, kc);
```

The patch changes pipe roughness, valve settings, pattern factors, curve points, and control settings and levels. Elevations, lengths, diameters, minor-loss coefficients and the other fields keep `%.4f`, since four decimals are enough for them.

**Why `%.10g` and not `%.6g`.** `%.6g` keeps six significant digits, which is fewer than `%.4f` keeps for any value of 100 or more: a curve flow of 12345.6789 gpm would be saved as 12345.7. `%.10g` keeps ten, which is never fewer than `%.4f` for values below a million, and is still far from the noise in the 16th digit of a double. It also drops the trailing zeros, so files read more easily: Net3 saves its control levels as `17.1` instead of `17.1000`, and patterns as `1.2` instead of `1.2000`. Saving Net1, Net2, Net3 and an SI test network with every feature showed no values with floating-point noise, including converted ones such as SI D-W roughness. `%g` switches to exponent form below 0.0001 (`5e-05`), which the reader (`strtod`) accepts.

**Rule values** in `src/rules.c` (premise values at line 1422, action settings at line 1453) use `%.4f` as well and need the same change. This patch leaves them alone because [FILE-03](../FILE-03-filltime-rules-not-reloadable/) changes the lines around line 1422 and [CRASH-09](../../2-crashes/CRASH-09-saveinp-rule-value-overflow/) changes the buffers they print into; the change is best made on top of those two. `%.10g` needs at most 17 characters, so it would also fit the original 20-byte buffers.

EPANET's own unit tests (6 suites) pass with the patch.
