# CRASH-03: A bare PRIORITY with no value dereferences a null token

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Null-pointer read. Opening an input file whose rule has a `PRIORITY` line with no value crashes the host process instead of reporting an input error. `EN_addrule` crashes the same way. |
| **Reached from** | Input file: a `PRIORITY` clause with no value in [RULES]. Toolkit API: the same clause in the text passed to `EN_addrule`. |
| **Where** | `newpriority()` in [`src/rules.c:943`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L943) |
| **Since** | The first commit of this repository (2014) for input files; v2.2 added `EN_addrule` |
| **GitHub** | No related issue |
| **Fix** | One line: [`CRASH-03_fix.patch`](CRASH-03_fix.patch) |

## The problem

A rule's `PRIORITY` clause takes one number. If the number is missing, for example because of a typo or a tool that writes an empty field, `EN_open`, `EN_openX` and `runepanet` crash while reading the file. The user gets no report and no line number. Every other malformed rule clause gives a syntax error.

`EN_addrule` parses rule text with the same code, so a rule string that ends with a bare `PRIORITY` crashes the calling program too.

## Why it happens

`gettokens()` sets every unused token pointer to NULL. `newpriority()` reads the second token without checking how many tokens the line has:

```c
// src/rules.c, newpriority()
    double x;
    char **Tok = pr->parser.Tok;

    if (!getfloat(Tok[1], &x)) return 202;
    net->Rule[net->Nrules].priority = x;
```

For a bare `PRIORITY`, `Tok[1]` is NULL and `getfloat()` passes it to `strtod()`, which reads address 0. The parsers for the other clauses check the token count first: `newpremise()` wants 5 or 6 tokens, `newaction()` wants 6 and a `RULE` line wants 2.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-03_bare-priority.inp`](CRASH-03_bare-priority.inp) | A reservoir, a junction and a tank. Rule R1 ends with `PRIORITY` and no value. |
| [`CRASH-03_test.c`](CRASH-03_test.c) | Opens the file with `EN_open`, then opens it with `EN_openX` and passes a rule with a bare `PRIORITY` to `EN_addrule` |
| [`CRASH-03_fix.patch`](CRASH-03_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-03            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-03 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-03_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-03_test
./CRASH-03_test
```

The command-line runner shows the same crash: `runepanet CRASH-03_bare-priority.inp CRASH-03.rpt`.

**Without the fix**, `EN_open` crashes while reading the rule:

```
==...==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000 (pc ... bp ... sp ... T0)
==...==The signal is caused by a READ memory access.
==...==Hint: address points to the zero page.
    ...
    #2 ... in strtod+0x40 (libsystem_c.dylib:arm64e+0x254f4)
    #3 ... in getfloat input2.c:709
    #4 ... in ruledata rules.c:274
    #5 ... in readdata input2.c:253
    #6 ... in getdata input1.c:64
```

(`newpriority()` is inlined into `ruledata()` in this build.) A build without sanitizers (macOS, Release) stops with a segmentation fault, exit code 139.

**With the fix**, the file is reported like any other bad rule:

```
EN_open returned 200 (expected 200)
EN_addrule returned 250 (expected 250), rule count 0
PASS: a bare PRIORITY line is reported as an error
```

The report file names the rule and the clause:

```
  Input Error 201: syntax error in following line of Rule R1:
  PRIORITY
```

## The fix

Check the token count before reading the value:

```diff
     char **Tok = pr->parser.Tok;
 
+    if (pr->parser.Ntokens < 2) return 201;
     if (!getfloat(Tok[1], &x)) return 202;
```

Error 201 (syntax error) is what the other clause parsers return. `EN_open` reports it as error 200, and `EN_addrule` returns it as 250 (invalid format).

The check is `< 2` rather than `!= 2`, so files that have extra tokens after the priority value still load as before.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- With `EN_openX`, an error in one rule currently drops every rule after it in the file. That is [API-09](../../3-api/API-09-malformed-rule-drops-later-rules/).
