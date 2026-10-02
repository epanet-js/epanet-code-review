# CRASH-17: EN_addrule with two RULE clauses overflows the rule array

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap buffer overflow. Passing two or more rules to `EN_addrule` in one string writes past the end of the Rule array. Without sanitizers the call returns 0 and the heap is silently corrupted. |
| **Reached from** | Toolkit API: `EN_addrule` with text that has more than one `RULE` clause, for example the [RULES] section of another file |
| **Where** | `EN_addrule()` in [`src/epanet.c:5639`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5639) and `ruledata()` in [`src/rules.c:217-218`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L217-L218) |
| **Since** | v2.2, when `EN_addrule` was added (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue |
| **Fix** | About 10 lines: [`CRASH-17_fix.patch`](CRASH-17_fix.patch) |

## The problem

`EN_addrule` takes rule text in the [RULES] format. A natural thing to try is to pass several rules at once, for example a block copied from another input file. The function makes room for one rule only, but parses every rule in the text, so the second rule is written past the end of the array.

In the test, the project has one rule and the text holds two more. Under AddressSanitizer the call stops with a heap-buffer-overflow. In a build without sanitizers (macOS, Release) it returns 0 and reports three rules, with the third written past the end of the array.

## Why it happens

`EN_addrule` grows the array by one entry (entries 0 to `Nrules + 1`):

```c
// src/epanet.c, EN_addrule()
    // Resize rules array
    net->Rule = (Srule *)realloc(net->Rule, (net->Nrules + 2)*sizeof(Srule));
```

`ruledata()` starts a new rule at every `RULE` clause:

```c
// src/rules.c, ruledata()
        case r_RULE:
          ...
          net->Nrules++;
          newrule(pr);
```

The second `RULE` clause makes `Nrules` two more than before, and `newrule()` writes the label and pointers of `Rule[Nrules]`, one entry past the end. When reading an input file this cannot happen, because the array is sized from a count of the `RULE` lines.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-17_one-rule.inp`](CRASH-17_one-rule.inp) | Reservoir R1 fills tank T1; rule R1 closes pipe P2 when T1 is nearly full |
| [`CRASH-17_test.c`](CRASH-17_test.c) | Passes rules AA and BB to `EN_addrule` in one string, then adds them in two separate calls |
| [`CRASH-17_fix.patch`](CRASH-17_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-17            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-17 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-17_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-17_test
./CRASH-17_test
```

**Without the fix**, `newrule()` (inlined into `ruledata()`) copies the second label past the end of the array:

```
Rules at start: 1
=================================================================
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 31 at ... thread T0
    #0 ... in wrap_strncpy+0x538 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4da04)
    #1 ... in ruledata rules.c:218
    #2 ... in EN_addrule epanet.c:5659
    #3 ... in main CRASH-17_test.c:43
... is located 0 bytes after 216-byte region [...)
allocated by thread T0 here:
    #0 ... in wrap_realloc+0x9c (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x52e28)
    #1 ... in EN_addrule epanet.c:5639
```

The 216-byte region is three `Srule` entries (0, 1 and 2). The second new rule would be entry 3.

**With the fix**, the two-rule string is refused and nothing changes; one rule per call still works:

```
Rules at start: 1
EN_addrule(AA and BB) returned 250, rule count 1
EN_addrule(AA), EN_addrule(BB) returned 0 and 0, rule count 3
PASS: two rules in one call are rejected; one per call works
```

## The fix

`EN_addrule` records the rule count on entry and refuses a `RULE` clause once its own rule has been started:

```diff
+    int  nrules;      // number of rules before this one is added
     char *line;
...
     rules->RuleState = 6;  // = r_PRIORITY
+    nrules = net->Nrules;
...
         if (parser->Ntokens > 0 && *parser->Tok[0] != ';')
         {
+            // Only one rule can be added at a time
+            if (net->Nrules > nrules && match(parser->Tok[0], w_RULE))
+            {
+                rules->Errcode = 250;
+                break;
+            }
             ruledata(p);  // Nrules gets updated in ruledata()
```

The error path then deletes the partly built first rule, as it does for any other error, so a failed call adds nothing. Error 250 (invalid format) is what `EN_addrule` already returns for a syntax error. The function is documented as adding "a new rule-based control", so one rule per call is the intended use.

The alternative is to accept several rules per call, by growing the array at each `RULE` clause. That would need a decision about what a failed call leaves behind (all the rules before the bad one, or none), so this patch keeps to one rule.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [API-08](../../3-api/API-08-bad-rule-deletes-previous-rule/) uses `nrules` to fix the rest of `EN_addrule`'s error handling, so its patch requires this one.
- Four other patches change `EN_addrule`: [CRASH-14](../CRASH-14-calls-after-close/) (`Openflag` check), [CRASH-18](../CRASH-18-addrule-long-line/) (long lines), [CRASH-19](../CRASH-19-addrule-edits-caller-string/) (writes to the caller's string) and API-08. They apply together.
- `newrule()` copies the label with `strncpy(rule->label, Tok[1], MAXID)` into memory that `realloc` did not clear, so a label of 31 characters or more is left without a terminator. In a quick check, a 40-character label added with `EN_addrule` read back through `EN_getruleID` as 32 characters. That is [CRASH-27](../CRASH-27-rule-labels-unterminated/), the rule-label version of [CRASH-24](../CRASH-24-31-character-ids-unterminated/).
