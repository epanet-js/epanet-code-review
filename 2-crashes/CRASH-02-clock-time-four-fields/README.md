# CRASH-02: A clock time with four or more fields writes past a 3-element array

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | A stack buffer overflow while reading the input file. A plain macOS Release build aborts in `EN_open`. |
| **Reached from** | Input file: any time value with four or more colon-separated fields, in [TIMES], in a [CONTROLS] `AT CLOCKTIME` line or in a rule's `CLOCKTIME` premise. Also `EN_addrule` with such a premise. |
| **Where** | `hour()` in [`src/input2.c:640`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L640) |
| **Since** | EPANET 2.00.12 (2008) or earlier, so every release |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`CRASH-02_fix.patch`](CRASH-02_fix.patch) |

## The problem

EPANET reads clock times such as `DURATION 24:00` or `START CLOCKTIME 6:30:00 AM` with `hour()`, which splits the text at the colons into hours, minutes and seconds. A time with a fourth field, for example `DURATION 1:2:3:4`, is written one element past the end of the 3-element array on the stack.

A typing error in an input file is enough to trigger it. Under AddressSanitizer `EN_open` stops with a stack-buffer-overflow. A plain Release build on macOS aborts in `EN_open` (exit code 134), and on other platforms the overwritten stack slot decides what happens.

## Why it happens

The array has three elements, but the loop runs while `n <= 3`, so it accepts a fourth field:

```c
// src/input2.c, hour()
double y[3];
...
for (n = 0; n < 3; n++) y[n] = 0.0;
n = 0;
s = strtok(time, ":");
while (s != NULL && n <= 3)
{
    if (!getfloat(s, &y[n])) return -1.0;     // n == 3 writes y[3]
    s = strtok(NULL, ":");
    n++;
}
```

`hour()` is called for [TIMES] values ([`input3.c:1826`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1826)), for `AT TIME` and `AT CLOCKTIME` in [CONTROLS] ([`input3.c:1023`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1023-L1024)) and for time premises in rules ([`rules.c:824`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L824-L825)), which `EN_addrule` also uses.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-02_four-field-duration.inp`](CRASH-02_four-field-duration.inp) | Reservoir R1 feeds junction J1. [TIMES] has `Duration 1:2:3:4`. |
| [`CRASH-02_test.c`](CRASH-02_test.c) | Opens the file and checks that EPANET rejects it with error 200 |
| [`CRASH-02_fix.patch`](CRASH-02_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-02            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-02 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-02_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-02_test
./CRASH-02_test
```

The command-line runner shows it too: `runepanet CRASH-02_four-field-duration.inp CRASH-02.rpt`.

**Without the fix**, AddressSanitizer stops `EN_open` while it reads the [TIMES] section:

```
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 8 at ... thread T0
    #0 ... in getfloat input2.c:709
    #1 ... in hour input2.c:642
    #2 ... in timedata input3.c:1826
    #3 ... in readdata input2.c:253
...
    [32, 56) 'y' (line 633) <== Memory access at offset 56 overflows this variable
```

**With the fix**, the time is rejected like any other invalid time:

```
EN_open returned 200 (expected 200)
  report:   Error 213: invalid option value Duration in [TIMES] section:
  report:   Error 200: one or more errors in input file
PASS: the four-field clock time is rejected with an input error
```

## The fix

Stop at the fourth field and return -1, the value `hour()` already returns for an invalid time:

```diff
-    while (s != NULL && n <= 3)
+    while (s != NULL)
     {
+        if (n == 3) return -1.0;    // More than hh:mm:ss
         if (!getfloat(s, &y[n])) return -1.0;
```

The earlier write-up suggested only `n < 3`. That stops the overflow but silently ignores the fourth field, so `1:2:3:4` would be read as 1:02:03. Rejecting it is safer.

The error message names the keyword (`Duration`) rather than the value. That is how `timedata()` reports every invalid time today, and is not changed here.

EPANET's own unit tests (6 suites) pass with the patch.
