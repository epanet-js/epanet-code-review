# API-09: One malformed rule in a file drops every rule after it

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | `EN_openX` silently loses valid rules. After one bad rule, every later rule in [RULES] is skipped without an error message. A clause before the first `RULE` line leaves the rule count at -1. |
| **Reached from** | `EN_openX` on an input file with an error in [RULES]. (`EN_open` refuses such a file anyway.) |
| **Where** | `ruledata()` in [`src/rules.c:199`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L199) and [`:210-221`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L210-L221); the caller is `newline()` in [`src/input2.c:309-316`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L309-L316) |
| **Since** | v2.3: `EN_openX` (PR [#758](https://github.com/OpenWaterAnalytics/EPANET/pull/758), 2023) made the reader carry on after a bad rule. The early return in `ruledata()` dates from 2014. |
| **GitHub** | No related issue. PR [#472](https://github.com/OpenWaterAnalytics/EPANET/pull/472) (2019) added the missing-label check in the `RULE` case that this patch reorders. |
| **Fix** | Small, in `rules.c`: [`API-09_fix.patch`](API-09_fix.patch) |

## The problem

`EN_openX` opens an input file that has errors, so that a program or GUI can show the network and let the user repair it. For [RULES], the reader is meant to report a bad rule, delete it and keep the others. It keeps only the rules before the first bad one.

`API-09_five-rules.inp` has five rules. R2 has a misspelt status (`OPNE`), and the fourth rule's label has a space in it (`RULE Fill tank`). `EN_openX` returns 200, as it should, but loads only R1. Rules R3 and R5 are valid and are lost. The report lists only the error in R2, so nothing tells the user that the fourth rule is also wrong, or that R3 and R5 are gone.

Two more cases go wrong in the same code:

- **A `RULE` line with a bad label deletes the rule before it.** No new rule has been started, so the error is charged to the previous rule.
- **A clause before the first `RULE` line sets the rule count to -1.** `API-09_stray-clause.inp` starts its [RULES] section with an `IF` clause, then has two valid rules. `EN_getcount(EN_RULECOUNT)` returns -1.

## Why it happens

Once a rule has an error, `ruledata()` returns at once for every later line:

```c
// src/rules.c, ruledata()
    // Exit if current rule has an error
    if (rules->RuleState == r_ERROR) return 0;

    // Find the key word that begins the rule statement
    err = 0;
    key = findmatch(Tok[0], Ruleword);
    switch (key)
    {
        ...
        case r_RULE:
          // Missing the rule label
          if (parser->Ntokens != 2)
          {
              err = 201;
              break;
          }
          net->Nrules++;
          newrule(pr);
          rules->RuleState = r_RULE;
          rules->Errcode = 0;
          break;
```

Only the `r_RULE` case resets `RuleState`, and it can no longer be reached. Before `EN_openX` this did not matter, because any error stopped the file from opening.

PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) added the per-rule recovery in the caller:

```c
// src/input2.c, newline()
        case _RULES:
          if (ruledata(pr) > 0)
          {
              ruleerrmsg(pr);
              deleterule(pr, pr->network.Nrules);
              return 200;
          }
```

It deletes rule number `Nrules`, assuming that the error belongs to it. For a bad `RULE` line, `Nrules` has not been incremented, so the previous rule is deleted. For a clause before the first `RULE` line, `Nrules` is 0, and `deleterule(pr, 0)` decrements it to -1.

## How to reproduce

| File | What it is |
|---|---|
| [`API-09_five-rules.inp`](API-09_five-rules.inp) | Rules R1 to R5 on a reservoir, junction and tank. R2 has a misspelt status; the fourth rule's label has a space. |
| [`API-09_stray-clause.inp`](API-09_stray-clause.inp) | The same network. An `IF` clause before the first `RULE` line, then valid rules R1 and R2. |
| [`API-09_test.c`](API-09_test.c) | Opens each file with `EN_openX` and compares the rules loaded with the valid ones |
| [`API-09_fix.patch`](API-09_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-09            # unpatched EPANET: FAIL
tools/run-test.sh API-09 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-09_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-09_test
./API-09_test
```

**Without the fix:**

```
API-09_five-rules.inp: EN_openX returned 200, rule count 1
  expected: R1 R3 R5
  loaded:   R1
API-09_stray-clause.inp: EN_openX returned 200, rule count -1
  expected: R1 R2
  loaded:  
FAIL: valid rules after a malformed one were dropped
```

**With the fix:**

```
API-09_five-rules.inp: EN_openX returned 200, rule count 3
  expected: R1 R3 R5
  loaded:   R1 R3 R5
API-09_stray-clause.inp: EN_openX returned 200, rule count 2
  expected: R1 R2
  loaded:   R1 R2
PASS: only the malformed rules were dropped
```

The report for the first file now lists both errors:

```
  Input Error 202: illegal numeric value in following line of Rule R2:
  THEN PIPE P2 STATUS = OPNE
...
  Input Error 201: syntax error in following line of Rule Fill:
  RULE Fill tank
```

## The fix

The patch changes four places in `rules.c`:

1. `ruledata()` skips the lines of a bad rule only up to the next `RULE` clause:

   ```diff
   -    // Exit if current rule has an error
   -    if (rules->RuleState == r_ERROR) return 0;
   -
        // Find the key word that begins the rule statement
        err = 0;
        key = findmatch(Tok[0], Ruleword);
   +
   +    // Skip the rest of a rule that has an error, up to the next RULE clause
   +    if (rules->RuleState == r_ERROR && key != r_RULE) return 0;
   ```

2. A `RULE` line starts its rule before the label is checked, so an error in the line is charged to the new rule, which the caller then deletes. The error message then names the bad rule ("Rule Fill") rather than the rule before it.
3. `newrule()` gives a `RULE` line with no label an empty label, since `Tok[1]` is NULL in that case.
4. `deleterule()` ignores an index outside `1..Nrules`, so an error before the first `RULE` line deletes nothing.

Resuming at the next `RULE` clause on its own is not enough. Built with only the first change, the test loads R1 and R5 from the first file (the bad label now deletes R3) and only R2 from the second (R1 goes into the unused entry `Rule[0]`).

`EN_addrule` uses `ruledata()` too. It stops at the first error, so the resume does not affect it, and the other changes make it delete the right rule when a `RULE` line has a bad label. [API-08](../API-08-bad-rule-deletes-previous-rule/) fixes the rest of its error handling.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Parsing errors in a rule still drop the whole rule, as intended.
- [API-08](../API-08-bad-rule-deletes-previous-rule/) is the same mistake in `EN_addrule`: its error path also deletes rule `Nrules`. The two patches change different files and apply together.
