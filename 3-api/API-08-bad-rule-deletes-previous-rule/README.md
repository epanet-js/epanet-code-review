# API-08: A bad EN_addrule string deletes the previous rule

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A rejected rule string changes the existing rules: the last rule is deleted or disabled, or the rule count drops to -1 and later rules become invisible. The return code says only that the new rule was rejected. |
| **Reached from** | Toolkit API: `EN_addrule` with text whose first clause is not a valid `RULE` line, or that starts with `DISABLED` |
| **Where** | `EN_addrule()` in [`src/epanet.c:5641`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5641) and [`:5669`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5669); `ruledata()` in [`src/rules.c:210-221`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L210-L221) and [`:277-285`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L277-L285) |
| **Since** | v2.2, when `EN_addrule` was added (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018). The `DISABLED` clause was added in v2.3 (PR [#827](https://github.com/OpenWaterAnalytics/EPANET/pull/827)). |
| **GitHub** | No related issue |
| **Fix** | About 10 lines, needs CRASH-17: [`API-08_fix.patch`](API-08_fix.patch) |

## The problem

A program that passes a bad string to `EN_addrule` gets an error code back and expects nothing else to change. Starting from a project with one rule, R1:

- `"DISABLED"` on its own returns 0 and disables R1.
- Text with no `RULE` line returns 221 and deletes R1.
- A `RULE` line with two labels returns 250 and deletes the last rule. With no rules left, the rule count becomes -1.
- A valid rule added after that returns 0, but is stored in the unused entry `Rule[0]`, so `EN_getcount` reports 0 rules and `EN_getruleID(ph, 1, ...)` returns 257.

While the count is -1, a further `"DISABLED"` string writes before the start of the Rule array (a heap-buffer-overflow in `ruledata()` under AddressSanitizer, checked by running).

## Why it happens

`EN_addrule` deletes rule number `Nrules` after any error, assuming that the text has started a new rule:

```c
// src/epanet.c, EN_addrule()
    rules->RuleState = 6;  // = r_PRIORITY
    ...
    // Delete new rule entry if there was an error
    if (rules->Errcode) deleterule(p, net->Nrules);
```

`ruledata()` only increments `Nrules` once a `RULE` line with exactly one label has been read. An error in an earlier line, or in the `RULE` line itself, leaves `Nrules` pointing at the previous rule, which is then deleted. With no rules, `deleterule(p, 0)` decrements the count to -1.

The parser starts in the state it is in after a complete rule (`r_PRIORITY`), and in that state it accepts a `DISABLED` clause, which applies to the current rule:

```c
// src/rules.c, ruledata()
        case r_DISABLED:
          if (rules->RuleState != r_THEN && rules->RuleState != r_ELSE &&
              rules->RuleState != r_PRIORITY)
          {
              err = 221;
              break;
          }
          net->Rule[net->Nrules].isEnabled = FALSE;
```

In an input file, a `DISABLED` line after a rule's last clause belongs to that rule. In `EN_addrule`, the "current rule" is the previous one.

## How to reproduce

| File | What it is |
|---|---|
| [`API-08_one-rule.inp`](API-08_one-rule.inp) | Reservoir R1 fills tank T1; rule R1 closes pipe P2 when T1 is nearly full |
| [`API-08_test.c`](API-08_test.c) | Passes three bad strings and then a valid rule R4 to `EN_addrule`, listing the rules after each call |
| [`API-08_fix.patch`](API-08_fix.patch) | The fix (apply [`CRASH-17_fix.patch`](../../2-crashes/CRASH-17-addrule-multiple-rules/CRASH-17_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-08            # unpatched EPANET: FAIL
tools/run-test.sh API-08 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-08_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-08_test
./API-08_test
```

**Without the fix** (a disabled rule is marked `(off)`):

```
At start                                    rules:  1 [R1]
DISABLED on its own          returned   0   rules:  1 [R1(off)]
No RULE clause               returned 221   rules:  0 []
Two labels on the RULE line  returned 250   rules: -1 []
Valid rule R4                returned   0   rules:  0 []
FAIL: bad rule strings changed the existing rules
```

**With the fix:**

```
At start                                    rules:  1 [R1]
DISABLED on its own          returned 221   rules:  1 [R1]
No RULE clause               returned 221   rules:  1 [R1]
Two labels on the RULE line  returned 250   rules:  1 [R1]
Valid rule R4                returned   0   rules:  2 [R1 R4]
PASS: bad rule strings leave the existing rules alone
```

## The fix

The patch builds on [CRASH-17](../../2-crashes/CRASH-17-addrule-multiple-rules/), which records the rule count on entry in `nrules`:

```diff
         if (parser->Ntokens > 0 && *parser->Tok[0] != ';')
         {
+            // The first clause must start the new rule
+            if (net->Nrules == nrules && !match(parser->Tok[0], w_RULE))
+            {
+                rules->Errcode = 221;
+                break;
+            }
+
             // Only one rule can be added at a time
...
     // Delete new rule entry if there was an error
-    if (rules->Errcode) deleterule(p, net->Nrules);
+    if (rules->Errcode && net->Nrules > nrules) deleterule(p, net->Nrules);
```

The first check rejects any text that does not start with `RULE`, including `DISABLED`, before the parser can apply it to the previous rule. Error 221 (mis-placed rule clause) is what the parser already returned for a leading `IF` or `THEN`. The second change deletes a rule only if this call added one.

An empty string, or one with only comments, still returns 0 and adds nothing. It could return 250 instead; that is left as it is.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- This patch needs [CRASH-17](../../2-crashes/CRASH-17-addrule-multiple-rules/) applied first (`Requires: CRASH-17` in its header), because it uses the `nrules` variable that CRASH-17 adds and changes the lines next to CRASH-17's check.
- The input-file reader has the same flaw in its per-rule recovery: a `RULE` line with a bad label deletes the rule before it, and a clause before the first `RULE` line sets the count to -1. [API-09](../API-09-malformed-rule-drops-later-rules/) fixes that in `rules.c`. With both patches, a bad `RULE` line in `EN_addrule` starts its own rule, which the error path then deletes; the result is the same.
- [CRASH-14](../../2-crashes/CRASH-14-calls-after-close/), [CRASH-18](../../2-crashes/CRASH-18-addrule-long-line/) and [CRASH-19](../../2-crashes/CRASH-19-addrule-edits-caller-string/) also change `EN_addrule`. All of them apply together.
