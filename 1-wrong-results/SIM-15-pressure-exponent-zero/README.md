# SIM-15: PRESSURE EXPONENT 0 is accepted and gives NaN results

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | An input file with `PRESSURE EXPONENT 0` loads and runs with return code 0, and every junction head, flow and demand is NaN. |
| **Reached from** | Input files with `[OPTIONS] PRESSURE EXPONENT 0` and pressure-driven demand |
| **Where** | `optionvalue()` in [`src/input3.c:2156`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2156) |
| **Since** | v2.2, when pressure-driven demand was added (commit [b5e3986](https://github.com/OpenWaterAnalytics/EPANET/commit/b5e3986e6bc43526a3e5f3b53c63dcf7ee268ada), 2018) |
| **GitHub** | No related issue |
| **Fix** | One character: [`SIM-15_fix.patch`](SIM-15_fix.patch) |

## The problem

A pressure-driven demand is `D = Dfull × ((p − Pmin) / (Preq − Pmin))^e`, where `e` is the pressure exponent. An exponent of 0 has no physical meaning, and `EN_setdemandmodel` rejects it with error 208. The input file parser accepts it.

The run then produces NaN for every junction's head and demand in the test network, and `EN_solveH` returns 0, so nothing tells the user that the results are meaningless.

## Why it happens

The parser checks only for a negative exponent:

```c
// src/input3.c, optionvalue()
    else if (match(tok0, w_PRESSURE))
    {
        if (y < 0.0) return setError(parser, nvalue, 213);
        hyd->Pexp = y;
        return 0;
    }

// src/epanet.c, EN_setdemandmodel()
        if (pexp <= 0.0) return 208;
```

The solver works with the inverted demand function, whose exponent is `n = 1.0 / hyd->Pexp` (in `demandcoeffs()` and `newdemandflows()`). With `Pexp = 0` that is infinite, the demand head loss and its gradient become infinite or NaN, and NaN enters the solution matrix. The ill-conditioning check in `linsolve()`, `if (diagj <= 0.0)`, is false for NaN, so the factorisation carries on and the solver reports success.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-15_pexp-zero.inp`](SIM-15_pexp-zero.inp) | Two 500 gpm junctions fed from a reservoir, pressure-driven demand with `PRESSURE EXPONENT 0` |
| [`SIM-15_test.c`](SIM-15_test.c) | Opens the file; if it loads, runs it and counts NaN results |
| [`SIM-15_fix.patch`](SIM-15_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-15            # unpatched EPANET: FAIL
tools/run-test.sh SIM-15 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-15_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-15_test
./SIM-15_test
```

`runepanet SIM-15_pexp-zero.inp SIM-15.rpt` shows the same: a normal run with NaN in the results tables.

**Without the fix:**

```
EN_open returned 0
EN_solveH returned 0
  J1  head = nan  demand = nan
  J2  head = nan  demand = nan
  R1  head = 30.000000  demand = nan
FAIL: PRESSURE EXPONENT 0 was accepted; 3 of 3 nodes have NaN results
```

**With the fix**, the file is rejected and the report says why:

```
EN_open returned 200
PASS: PRESSURE EXPONENT 0 is rejected as an input error
```

```
  Error 213: invalid option value 0 in [OPTIONS] section:
  Error 200: one or more errors in input file
```

## The fix

Reject values `<= 0`, as `EN_setdemandmodel` does and as the parser does for most other options:

```diff
     else if (match(tok0, w_PRESSURE))
     {
-        if (y < 0.0) return setError(parser, nvalue, 213);
+        if (y <= 0.0) return setError(parser, nvalue, 213);
```

A file that uses demand-driven analysis but still carries `PRESSURE EXPONENT 0` will now also be rejected. `EN_saveinpfile` writes the PDA options only for PDA projects, so EPANET itself does not produce such files.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The NaN gets through because `linsolve()` tests `diagj <= 0.0`. Writing the test as `!(diagj > 0.0)` would also catch NaN pivots, so by reading the code any future NaN in the matrix would end the solve as ill-conditioned (error 110) instead of returning NaN results. That is a wider change and is not part of this patch.
- [FILE-13](../../4-files/FILE-13-fractional-trials-zero-iterations/) covers the other half of the earlier item P16 (`TRIALS 0.9` gives zero trials), a few lines further down in the same function. The two patches do not overlap.
