# API-12: EN_SOURCEMASS returns cumulative mass x 60, not a rate

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | `EN_getnodevalue(EN_SOURCEMASS)` returns a number that grows through the run and has no meaning in any unit: the total mass injected so far, times 60. A 60 mg/min booster reads 216,000 after one hour. |
| **Reached from** | Toolkit API: `EN_getnodevalue` with `EN_SOURCEMASS` during or after a chemical quality run |
| **Where** | `EN_getnodevalue()` in [`src/epanet.c:2324`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2324) and `findsourcequal()` in [`src/quality.c:530`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/quality.c#L530) |
| **Since** | v2.2: the water quality rewrite in PR [#297](https://github.com/OpenWaterAnalytics/EPANET/pull/297) (commit [`7c021cf`](https://github.com/OpenWaterAnalytics/EPANET/commit/7c021cf533c247b66eb1b5c562856e88e8dc7a4b), 2018) dropped the conversion to a rate |
| **GitHub** | No related issue |
| **Fix** | Three lines: [`API-12_fix.patch`](API-12_fix.patch). Apply [WQ-04](../../5-water-quality/WQ-04-reaction-averages-low-with-report-start/)'s patch first. |

## The problem

`EN_SOURCEMASS` is documented as the "current computed quality source mass inflow", and the toolkit's units page lists source mass injection in mass/minute. The getter returns `Smass * 60`, and `Smass` is declared in `types.h` as the "actual mass flow rate", so `Smass` is meant to be in mass/second.

Since v2.2, `Smass` is instead the total mass the source has added since the start of the run. `EN_SOURCEMASS` therefore grows without limit, and the factor of 60 turns mass into a number that is neither a mass nor a rate. For a constant 60 mg/min booster it reads 216,000 after one hour, 432,000 after two, and so on.

## Why it happens

```c
// src/quality.c, findsourcequal()
    // Source mass added over time step = source concen. * outflow volume
    massadded = c * volout;

    // Update source's total mass added
    source->Smass += massadded;

// src/epanet.c, EN_getnodevalue()
        else if (property == EN_SOURCEMASS) v = source->Smass * 60.0;
```

`Smass` is only set to 0 in `initqual()`. EPANET 2.0 and 2.1 had the same `+=`, but `updatesourcenodes()` then divided `Smass` by the step length after every quality step ("Normalize mass added at source to time step"), so `EN_SOURCEMASS` read close to the rate in mass/minute. (By reading the 2.1 code: the total was never reset, so a 60 mg/min source read about 60.2 with 5-minute steps.) The v2.2 rewrite removed `updatesourcenodes()` and the division with it.

## How to reproduce

| File | What it is |
|---|---|
| [`API-12_mass-source.inp`](API-12_mass-source.inp) | R1 feeds J2 (200 gpm) through J1, which has a constant 60 mg/min mass booster |
| [`API-12_test.c`](API-12_test.c) | Runs the quality solver step by step and reads `EN_SOURCEMASS` for J1 every hour |
| [`API-12_fix.patch`](API-12_fix.patch) | The fix, to apply after [`WQ-04_fix.patch`](../../5-water-quality/WQ-04-reaction-averages-low-with-report-start/WQ-04_fix.patch) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-12            # unpatched EPANET: FAIL
tools/run-test.sh API-12 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-12_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-12_test
./API-12_test
```

**Without the fix:**

```
Hour   EN_SOURCEMASS   Expected (mg/min)
   1        216000.0                60.0   <-- wrong
   2        432000.0                60.0   <-- wrong
   3        648000.0                60.0   <-- wrong
   4        864000.0                60.0   <-- wrong
FAIL: EN_SOURCEMASS grows with time instead of giving the 60 mg/min rate
```

**With the fix:**

```
Hour   EN_SOURCEMASS   Expected (mg/min)
   1            60.0                60.0   
   2            60.0                60.0   
   3            60.0                60.0   
   4            60.0                60.0   
PASS: EN_SOURCEMASS gives the source's mass inflow rate
```

## The fix

There were two ways to resolve the mismatch between the code and the documentation:

1. **Return a rate**, as documented: the mass the source added over the latest quality step, divided by the step length.
2. **Document the cumulative value**: drop the factor of 60 and describe `EN_SOURCEMASS` as the total mass added since the start of the run.

The patch takes option 1. The enum comment, the units page and the `Smass` declaration all describe a rate, and EPANET 2.0 and 2.1 returned one, so this restores the documented behaviour instead of redefining it. A program that uses the current value as a total has to divide it by 60 first, which nothing documents. The total that sources add over a run is still available as the output file's average source inflow rate times the length of the reporting period.

`findsourcequal()` sets `Smass` to 0 for every source it visits, and to the mass added divided by the step length when the source adds mass:

```diff
     if (source == NULL)    return 0.0;
+    source->Smass = 0.0;
     if (source->C0 == 0.0) return 0.0;
     if (volout / tstep <= Q_STAGNANT) return 0.0;
 ...
-    // Update source's total mass added
-    source->Smass += massadded;
+    // Update source's mass inflow rate (mass/sec)
+    source->Smass = massadded / tstep;
```

The getter's `Smass * 60` then gives mass/minute and does not change. `findsourcequal()` runs once per node per quality step, so `EN_SOURCEMASS` is the rate over the latest step. It is 0 while no water leaves the source node, because EPANET adds no mass then. After `EN_runQ` at a given time, the latest step is the one that ended at that time.

The patch changes the line directly above the `Wsource` test that [WQ-04](../../5-water-quality/WQ-04-reaction-averages-low-with-report-start/) changes, so its header says `Requires: WQ-04` and it applies after that patch.

EPANET's own unit tests (6 suites) pass with both patches.
