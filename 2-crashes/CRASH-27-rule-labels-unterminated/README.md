# CRASH-27: EN_addrule leaves rule labels of 31 characters or more unterminated

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Stack buffer overflow in the caller. A rule added with a label of 31 characters or more has no terminating zero, so `EN_getruleID` copies more than the documented `EN_MAXID + 1` characters into the caller's buffer, and `EN_saveinpfile` writes stray bytes after the label. |
| **Reached from** | Toolkit API: `EN_addrule` with a label of 31 characters or more, then `EN_getruleID`, `EN_saveinpfile` or anything else that reads the label |
| **Where** | `newrule()` in [`src/rules.c:699`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L699), for the entry that `EN_addrule()` adds with `realloc` ([`src/epanet.c:5639`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5639)); the copy is in `EN_getruleID()`, [`src/epanet.c:5754`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5754) |
| **Since** | v2.2, when `EN_addrule` was added (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | Related: [#135](https://github.com/OpenWaterAnalytics/EPANET/issues/135) (open since 2017) asks for bounded string functions such as `strncpy`; `strncpy` on its own is what leaves this label unterminated |
| **Fix** | One line: [`CRASH-27_fix.patch`](CRASH-27_fix.patch) |

## The problem

EPANET IDs can be up to 31 characters (`EN_MAXID`), and the API documents that a buffer for a rule ID needs `EN_MAXID + 1` characters. A program that adds a rule with a 31-character label through `EN_addrule`, and reads it back with `EN_getruleID` into such a buffer, gets 33 bytes written into its 32-byte buffer. Labels longer than 31 characters, which the parser cuts to 31, do the same.

This is the rule-label version of [CRASH-24](../CRASH-24-31-character-ids-unterminated/), which affects node and link IDs.

## Why it happens

`newrule()` copies the label with `strncpy`, which adds no terminating zero when the source has `MAXID` characters or more:

```c
// src/rules.c, newrule()
    Srule *rule = &net->Rule[net->Nrules];

    strncpy(rule->label, Tok[1], MAXID);
```

`label` is `char label[MAXID+1]`, so its last byte keeps whatever was in memory. When the rules come from an input file, the Rule array is allocated with `calloc`, the byte is zero and the label ends after 31 characters. `EN_addrule` grows the array with `realloc`, which does not clear the new entry:

```c
// src/epanet.c, EN_addrule()
    net->Rule = (Srule *)realloc(net->Rule, (net->Nrules + 2)*sizeof(Srule));
```

`EN_getruleID` then copies with `strcpy(id, p->network.Rule[index].label)` up to the next zero byte, which here is in the `priority` field that follows the label.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-27_tank.inp`](CRASH-27_tank.inp) | Reservoir R1 fills tank T1 through pipe P2; no rules |
| [`CRASH-27_test.c`](CRASH-27_test.c) | Adds a rule with the 31-character label `PUMP_STATION_7_NIGHT_TARIFF_OFF` and reads it back with `EN_getruleID` into a buffer of `EN_MAXID + 1` characters |
| [`CRASH-27_fix.patch`](CRASH-27_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-27            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-27 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-27_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-27_test
./CRASH-27_test
```

**Without the fix**, `EN_getruleID` writes 33 bytes into the 32-byte buffer:

```
EN_addrule returned 0 for a 31-character label
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 33 at ... thread T0
    #0 ... in wrap_strcpy+0x510 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4d110)
    #1 ... in EN_getruleID epanet.c:5754
    #2 ... in main CRASH-27_test.c:40
...
  This frame has 3 object(s):
    [32, 40) 'ph' (line 24)
    [64, 214) 'rule' (line 26)
    [288, 320) 'id' (line 26) <== Memory access at offset 320 overflows this variable
```

AddressSanitizer fills new heap memory with a non-zero byte (`0xbe`), so the missing terminator shows every time. Without sanitizers the result depends on what the reallocated memory held. In a macOS Release build the byte happened to be zero, and the test passed.

**With the fix:**

```
EN_addrule returned 0 for a 31-character label
EN_getruleID returned 0: 31 characters
  expected: PUMP_STATION_7_NIGHT_TARIFF_OFF
  read:     PUMP_STATION_7_NIGHT_TARIFF_OFF
PASS: the rule label reads back unchanged
```

## The fix

Write the terminator in `newrule()`, as [CRASH-24](../CRASH-24-31-character-ids-unterminated/) does for node and link IDs:

```diff
     rule->priority = 0.0;
     rule->isEnabled = TRUE;
+    rule->label[MAXID] = '\0';  // strncpy() above may not terminate it
     pr->rules.LastPremise = NULL;
```

A label longer than 31 characters is now cut to 31, as it already was when read from an input file. Refusing such a label with error 252 (invalid ID name), as `EN_addnode` does for node IDs, would be stricter, but would also make input files with long rule labels fail to open.

The natural place for the new line is straight after the `strncpy`. [API-09](../../3-api/API-09-malformed-rule-drops-later-rules/) changes that line (to handle a `RULE` line with no label), so the terminator is placed a few lines further down, where the two patches do not touch the same lines and can be applied in either order.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `EN_saveinpfile` writes the label with `fprintf(f, "\nRULE %s", ...)` ([`src/inpfile.c:534`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L534)), so without the fix the saved file gets the stray byte too. Saving the test's project under AddressSanitizer writes `RULE PUMP_STATION_7_NIGHT_TARIFF_OFF` followed by the byte `0xbe`.
