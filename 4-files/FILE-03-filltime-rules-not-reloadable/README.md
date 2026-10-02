# FILE-03: FILLTIME and DRAINTIME rule values are written in a form the parser rejects

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A project with a rule that tests a tank's fill or drain time saves without error, but the saved file does not open (error 200). |
| **Reached from** | `EN_saveinpfile` (and so any GUI or script that saves a project) followed by opening the saved file, for any rule with a `FILLTIME` or `DRAINTIME` premise |
| **Where** | `writepremise()` in [`src/rules.c:1414-1421`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1414-L1421); the reader is `newpremise()` in [`src/rules.c:828-833`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L828-L833) |
| **Since** | v2.2, when `EN_saveinpfile` started writing rules from the parsed data (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue |
| **Fix** | Small: two case labels move to a new branch. [`FILE-03_fix.patch`](FILE-03_fix.patch) |

## The problem

The manual describes `FILLTIME` and `DRAINTIME` as the hours needed to fill or empty a tank, written as a number: `TANK T200 FILLTIME BELOW 3.5`. EPANET reads them that way. When it saves the project, it writes them as a clock time instead: `FILLTIME < 1:30:00`. The number parser rejects `1:30:00`, so the saved file fails to open with error 202 in the rule.

The save itself reports no error. The problem only shows when someone opens the saved file, possibly much later or on another machine. Files saved by EPANET 2.2 and 2.3 with such a rule are already in this state.

## Why it happens

The writer treats fill and drain times like the two clock variables:

```c
// src/rules.c, writepremise()
        // For time values convert from seconds to hr:min:sec
        switch (p->variable)
        {
        case r_CLOCKTIME:
        case r_DRAINTIME:
        case r_FILLTIME:
        case r_TIME:
            gettimetxt(p->value, s_value);
            break;
        default: sprintf(s_value, "%.4f", p->value);
        }
```

The reader parses `TIME` and `CLOCKTIME` values with `hour()`, which accepts `h:mm:ss`, but reads `FILLTIME` and `DRAINTIME` as a plain number of hours:

```c
// src/rules.c, newpremise()
        if (!getfloat(Tok[parser->Ntokens - 1], &x))
        return (202);
        if (v == r_FILLTIME || v == r_DRAINTIME) x = x * 3600.0;
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-03_filltime-rules.inp`](FILE-03_filltime-rules.inp) | Reservoir R1 fills tank T1. Rule R1 tests `FILLTIME BELOW 1.5` and rule R2 tests `DRAINTIME > 2.25`. |
| [`FILE-03_test.c`](FILE-03_test.c) | Opens the file, saves it with `EN_saveinpfile`, prints the saved premises, opens the saved file and compares the premise values |
| [`FILE-03_fix.patch`](FILE-03_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-03            # unpatched EPANET: FAIL
tools/run-test.sh FILE-03 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-03_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-03_test
./FILE-03_test
```

**Without the fix**, the saved file does not open. `EN_getpremise` returns the values in seconds:

```
EN_open(original) returned 0
  saved: IF   TANK T1 FILLTIME < 1:30:00
  saved: IF   TANK T1 DRAINTIME > 2:15:00
EN_open(saved) returned 200
FILLTIME premise:  5400 s before, -1 s after
DRAINTIME premise: 8100 s before, -1 s after
FAIL: the saved FILLTIME and DRAINTIME rules do not load
```

The report for the saved file:

```
  Input Error 202: illegal numeric value in following line of Rule R1:
  IF TANK T1 FILLTIME < 1:30:00
```

Rule R2 has the same problem but is not reported, because the parser skips every rule after the first bad one ([API-09](../../3-api/API-09-malformed-rule-drops-later-rules/)).

**With the fix:**

```
EN_open(original) returned 0
  saved: IF   TANK T1 FILLTIME < 1.5000
  saved: IF   TANK T1 DRAINTIME > 2.2500
EN_open(saved) returned 0
FILLTIME premise:  5400 s before, 5400 s after
DRAINTIME premise: 8100 s before, 8100 s after
PASS: the saved rules load with the same values
```

## The fix

Write fill and drain times as decimal hours, the format the manual gives and the parser reads:

```diff
         case r_CLOCKTIME:
-        case r_DRAINTIME:
-        case r_FILLTIME:
         case r_TIME:
             gettimetxt(p->value, s_value);
             break;
+
+        // Fill and drain times are read back as decimal hours
+        case r_DRAINTIME:
+        case r_FILLTIME:
+            sprintf(s_value, "%.4f", p->value / 3600.0);
+            break;
         default: sprintf(s_value, "%.4f", p->value);
```

Four decimals of an hour is 0.36 s, so a value can move by up to 0.18 s on a save. `gettimetxt()` truncated to whole seconds, which moved it by up to 1 s.

An alternative, or an addition, is to make the reader accept both forms by parsing these values with `hour()`, as it does for `TIME` and `CLOCKTIME`. That would also open files that EPANET 2.2 and 2.3 have already saved with `h:mm:ss`. It would not help other programs that read the saved file and follow the manual, so the writer should be fixed either way. If the reader is changed, note that `hour()` has its own out-of-bounds write for times with four or more fields ([CRASH-02](../../2-crashes/CRASH-02-clock-time-four-fields/)).

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-09](../../2-crashes/CRASH-09-saveinp-rule-value-overflow/) enlarges `s_value` in the same function. The two patches apply together in either order. Without CRASH-09, a fill time of 1e15 hours or more would still overflow the 20-character buffer.
