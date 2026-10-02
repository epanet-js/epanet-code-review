# CRASH-09: EN_saveinpfile overflows a 20-byte buffer for rule values of 1e15 or more

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Stack buffer overflow while saving. A rule whose premise value or action setting is 1e15 or more (or -1e14 or less) crashes `EN_saveinpfile`, or overwrites the stack without warning in a build without fortified `sprintf`. |
| **Reached from** | Toolkit API: `EN_saveinpfile` on a project with such a rule. The rule can come from an input file, `EN_addrule` or the premise and action setters. |
| **Where** | `writepremise()` and `writeaction()` in [`src/rules.c:1385`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1385) and [`:1422`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1422), [`:1439`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1439) and [`:1453`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1453) |
| **Since** | v2.2, when `EN_saveinpfile` started writing rules from the parsed data (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`CRASH-09_fix.patch`](CRASH-09_fix.patch) |

## The problem

`EN_saveinpfile` writes each rule's numbers with `"%.4f"` into a 20-character buffer. 1234567890123456 becomes `1234567890123456.0000`, which is 21 characters plus the terminator. Any value of 1e15 or more, or -1e14 or less, overflows the buffer.

Such values are rare, but they do occur: the review found this when the saved-file check crashed on one of the benchmark models, on a rule action setting. The test here reproduces it on a small network.

## Why it happens

Both writers use the same fixed buffer:

```c
// src/rules.c, writepremise()
    char s_value[20];
    ...
        default: sprintf(s_value, "%.4f", p->value);

// src/rules.c, writeaction()
    char s_value[20];
    ...
        sprintf(s_value, "%.4f", a->setting);
```

`"%.4f"` prints every digit of the integer part, so the length grows with the value. The largest double needs 315 characters.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-09_valve-tank.inp`](CRASH-09_valve-tank.inp) | Reservoir R1 feeds tank T1 through pressure-reducing valve V1 |
| [`CRASH-09_test.c`](CRASH-09_test.c) | Adds a rule that tests T1's level against 1234567890123456 and a rule that sets V1 to that value, saves the project, opens the saved file and reads both values back |
| [`CRASH-09_fix.patch`](CRASH-09_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-09            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-09 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-09_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-09_test
./CRASH-09_test
```

**Without the fix**, the first rule's premise overflows the buffer:

```
EN_addrule returned 0 and 0
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 4 at ... thread T0
    ...
    #6 ... in writerule rules.c:493
    #7 ... in saveinpfile inpfile.c:535
    #8 ... in main CRASH-09_test.c:45
...
  This frame has 3 object(s):
    [32, 52) 's_obj.i' (line 1383)
    [96, 128) 's_id.i' (line 1384)
    [160, 180) 's_value.i' (line 1385) <== Memory access at offset 180 overflows this variable
```

`writepremise()` is inlined into `writerule()` here. A build without sanitizers (macOS, Release) stops in the fortified `sprintf` with exit code 133 (SIGTRAP) before `EN_saveinpfile` returns. Without fortification the stack is overwritten silently.

**With the fix**, both values are written in full and read back unchanged:

```
EN_addrule returned 0 and 0
EN_saveinpfile returned 0
EN_open(saved file) returned 0
Premise value:  expected 1234567890123456.0, read back 1234567890123456.0
Action setting: expected 1234567890123456.0, read back 1234567890123456.0
PASS: rule values of 1e15 or more are saved and read back unchanged
```

The saved rules:

```
RULE BIGLEVEL
IF   TANK T1 LEVEL > 1234567890123456.0000
THEN VALVE V1 STATUS = OPEN

RULE BIGSETTING
IF   SYSTEM  CLOCKTIME >= 1:00:00
THEN VALVE V1 SETTING = 1234567890123456.0000
```

## The fix

Size both buffers for any number that `"%.4f"` can produce:

```diff
-    char s_value[20];
+    char s_value[MAXLINE + 1];  // fits "%.4f" of any double
```

`MAXLINE + 1` is 1,025 characters. A value longer than an input line could not be read back anyway, and the largest double needs 315. The written text is exactly what the code meant to write, so files that saved correctly before are unchanged.

Other options:

- `snprintf(s_value, sizeof(s_value), ...)` with the old size stops the overflow but writes a truncated, wrong number without any error. `rules.c` also maps `snprintf` to `_snprintf` on Windows, which does not terminate a truncated string.
- `"%.4g"` or `"%.4e"` for large values keeps the buffer small, but changes how some values are written.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `gettimetxt()` writes into the same buffer. For a time premise of more than 168 hours it uses `"%.4f"` of hours, which the larger buffer also covers. It also casts the time in seconds to `int` first (`hours = (int)secs / 3600`), which is undefined for times of 2^31 seconds (about 68 years) or more. That is [CRASH-29](../CRASH-29-huge-rule-time-to-text/).
- [FILE-03](../../4-files/FILE-03-filltime-rules-not-reloadable/) changes the `switch` in `writepremise()` a few lines below. The two patches apply together in either order.
