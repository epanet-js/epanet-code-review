# FILE-09: The output-file reader cannot reach the last period and misreads pressure units

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | `ENR_getNodeSeries` and `ENR_getLinkSeries` cannot return the last reporting period. `ENR_getUnits` returns the engine's pressure-unit code, but the reader's enum names the codes differently, so a kPa file reads as metres and a metres file as kPa. |
| **Reached from** | The output-file library (`epanet_output.h`), on any binary output file |
| **Where** | `ENR_getNodeSeries()` and `ENR_getLinkSeries()` in [`src/outfile/src/epanet_output.c:581-582`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/outfile/src/epanet_output.c#L581-L582) and [`:617-618`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/outfile/src/epanet_output.c#L617-L618); `ENR_PressUnits` in [`src/outfile/include/epanet_output_enums.h:40-44`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/outfile/include/epanet_output_enums.h#L40-L44); the engine writes the code in `savenetdata()`, [`src/output.c:87`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/output.c#L87) |
| **Since** | v2.2 (2019), the first release with this reader |
| **GitHub** | No related issue. PR [#719](https://github.com/OpenWaterAnalytics/EPANET/pull/719) (2023) noted that the toolkit's pressure-unit enum differs from the reader's, but the reader's enum was not changed. |
| **Fix** | 2 conditions and the enum values: [`FILE-09_fix.patch`](FILE-09_fix.patch) |

## The problem

**Last period.** `ENR_getNodeSeries(handle, node, attr, startPeriod, endPeriod, ...)` returns `endPeriod - startPeriod` values, starting at `startPeriod`, so `endPeriod` is exclusive. But the range check rejects `endPeriod >= nPeriods` with error 422. The widest series a caller can get is periods 0 to `nPeriods - 2`; the last period can only be read one period at a time with the other functions. EPANET's own unit test asks for periods 0 to 10, so it never meets the limit.

**Pressure units.** The engine writes its own `PressureUnitsType` value into the output file header: 0 psi, 1 kPa, 2 metres, 3 bar, 4 feet. `ENR_getUnits(ENR_pressUnits)` returns that value unchanged, but the reader's enum and its comment say 0 psi, 1 metres, 2 kPa, and have no bar or feet. A program that compares the code with `ENR_KPA` gets the wrong answer for every SI file.

## Why it happens

```c
// src/outfile/src/epanet_output.c, ENR_getNodeSeries() (ENR_getLinkSeries() is the same)
else if (startPeriod < 0 || endPeriod >= p_data->nPeriods ||
        endPeriod <= startPeriod) errorcode = 422;
// Check memory for outValues
else if MEMCHECK(temp = newFloatArray(length = endPeriod - startPeriod)) errorcode = 411;
```

```c
// src/outfile/include/epanet_output_enums.h
typedef enum {
	ENR_PSI         = 0,
	ENR_MTR         = 1,
	ENR_KPA         = 2
} ENR_PressUnits;

// src/types.h (engine), written to the output file by savenetdata()
typedef enum {
  PSI,           // pounds per square inch
  KPA,           // kiloPascals
  METERS,        // meters
  BAR,           // bar
  FEET,          // feet
  ...
} PressureUnitsType;
```

The engine has used this order (psi, kPa, metres) since EPANET 2.0.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-09_kpa.inp`](FILE-09_kpa.inp) | Reservoir R1 supplies J1 for 4 hours with an hourly demand pattern (5 reporting periods), pressures in kPa |
| [`FILE-09_test.c`](FILE-09_test.c) | Runs the network to write `FILE-09.out`, then reads J1's pressure series over all periods and the pressure unit code with the output reader |
| [`FILE-09_fix.patch`](FILE-09_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-09            # unpatched EPANET: FAIL
tools/run-test.sh FILE-09 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-09_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -o FILE-09_test
./FILE-09_test
```

**Without the fix**:

```
Reporting periods in the file: 5
ENR_getNodeSeries(J1, pressure, 0, 5) returned 422, 0 values
ENR_getNodeSeries(J1, pressure, 0, 4) returned 0, 4 values
ENR_getUnits(ENR_pressUnits) returned 1: ENR_PSI is 0, ENR_KPA 2, ENR_MTR 1
FAIL: the last period cannot be read as a series; the kPa file has unit code 1, which the reader calls ENR_MTR (metres)
```

**With the fix**:

```
Reporting periods in the file: 5
ENR_getNodeSeries(J1, pressure, 0, 5) returned 0, 5 values
  last value 354.617, period 4 from ENR_getNodeAttribute 354.617
ENR_getUnits(ENR_pressUnits) returned 1: ENR_PSI is 0, ENR_KPA 1, ENR_MTR 2
PASS: the reader returns every period and identifies kPa
```

## The fix

Accept `endPeriod == nPeriods` in both series functions, and give the enum the engine's values:

```diff
-    else if (startPeriod < 0 || endPeriod >= p_data->nPeriods ||
+    else if (startPeriod < 0 || endPeriod > p_data->nPeriods ||
             endPeriod <= startPeriod) errorcode = 422;
```

```diff
 typedef enum {
 	ENR_PSI         = 0,
-	ENR_MTR         = 1,
-	ENR_KPA         = 2
+	ENR_KPA         = 1,
+	ENR_MTR         = 2,
+	ENR_BAR         = 3,
+	ENR_FEET        = 4
 } ENR_PressUnits;
```

The patch also corrects the list of codes in the comment above `ENR_getUnits()`.

Both changes alter the reader's interface. A caller that worked around the period limit with `endPeriod = nPeriods - 1` gets the same values as before, and can now ask for the last one too. A caller that compared the unit code with `ENR_MTR` or `ENR_KPA` was getting the wrong answer, and is right once it is rebuilt against the new header. The alternative for the units is to keep the enum and translate the engine's code inside `ENR_getUnits()`, as it already does for quality units. That keeps the enum values but still has no names for bar and feet.

EPANET's own unit tests (6 suites) pass with the patch.
