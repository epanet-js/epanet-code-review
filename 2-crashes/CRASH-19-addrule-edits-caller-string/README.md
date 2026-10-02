# CRASH-19: EN_addrule edits the caller's string, so a string literal faults

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Write to read-only memory. `EN_addrule(ph, "RULE ...")` with a string literal crashes the program (bus error on macOS). With a writable buffer, a rule with an error leaves the caller's text cut short. |
| **Reached from** | Toolkit API: `EN_addrule` (and the legacy `ENaddrule`) with a string literal or other read-only text |
| **Where** | `EN_addrule()` in [`src/epanet.c:5649`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5649) and [`:5664`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5664) |
| **Since** | v2.2, when `EN_addrule` was added (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue |
| **Fix** | Small, needs CRASH-18: [`CRASH-19_fix.patch`](CRASH-19_fix.patch) |

## The problem

The most natural way to call `EN_addrule` from C is with a string literal:

```c
EN_addrule(ph, "RULE R2\nIF TANK T1 LEVEL > 15\nTHEN PIPE P2 STATUS = CLOSED");
```

This crashes. `EN_addrule` splits the text into lines by writing `'\0'` over each newline, and string literals are in read-only memory. Nothing in the documentation says the text must be writable, and the C compiler gives no warning, because the parameter is `char *`. EPANET's own unit tests pass writable arrays, which is why they do not see it.

With a writable buffer the call works, but the text is not always restored. If a line has an error, `EN_addrule` returns before putting the newline back. In the test, a 69-character rule with an undefined link comes back as 58 characters, ending after the `IF` clause. A program that retries or logs the text sees the shortened version.

## Why it happens

```c
// src/epanet.c, EN_addrule()
    line = rule;
    while (line)
    {
        // Find where current line ends and next one begins
        nextline = strchr(line, '\n');
        if (nextline) *nextline = '\0';
        ...
        if (parser->Ntokens > 0 && *parser->Tok[0] != ';')
        {
            ruledata(p);  // Nrules gets updated in ruledata()
            if (rules->Errcode) break;
        }

        // Extract next line from the rule statement
        if (nextline) *nextline = '\n';
        line = nextline ? (nextline + 1) : NULL;
    }
```

The line is copied into `line2` before it is tokenized, so the temporary terminator in the caller's text is not needed at all.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-19_tank.inp`](CRASH-19_tank.inp) | Reservoir R1 fills tank T1 through pipe P2; no rules |
| [`CRASH-19_test.c`](CRASH-19_test.c) | Passes a writable copy of a rule that names a missing link and compares it with the original, then passes a valid rule as a string literal |
| [`CRASH-19_fix.patch`](CRASH-19_fix.patch) | The fix (apply [`CRASH-18_fix.patch`](../CRASH-18-addrule-long-line/CRASH-18_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-19            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-19 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-19_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-19_test
./CRASH-19_test
```

**Without the fix**, the failed call shortens the caller's text and the string literal faults:

```
Rule with an undefined link: EN_addrule returned 204
  caller's text is 58 characters (was 69): CHANGED
AddressSanitizer:DEADLYSIGNAL
=================================================================
==...==ERROR: AddressSanitizer: BUS on unknown address (pc ... bp ... sp ... T0)
==...==The signal is caused by a WRITE memory access.
...
    #0 ... in EN_addrule epanet.c:5649
    #1 ... in main CRASH-19_test.c:46
```

A build without sanitizers (macOS, Release) stops with a bus error, exit code 138.

**With the fix:**

```
Rule with an undefined link: EN_addrule returned 204
  caller's text is 69 characters (was 69): unchanged
Valid rule as a string literal: EN_addrule returned 0, rule count 1
PASS: EN_addrule leaves the caller's string alone
```

## The fix

`EN_addrule` finds the end of each line without marking it, and copies exactly that many characters, plus the newline the tokenizer needs, into its own buffer:

```diff
         nextline = strchr(line, '\n');
-        if (nextline) *nextline = '\0';
+        if (nextline == NULL) nextline = line + strlen(line);
 
         // The line and the newline added below must fit in line2
-        if (strlen(line) >= MAXLINE)
+        if (nextline - line >= MAXLINE)
...
-        // Copy and tokenize the current line
-        strcpy(line2, line);
-        strcat(line2, "\n");  // Tokenizer won't work without this
+        // Copy the current line and a newline (the tokenizer needs it)
+        // into line2 and tokenize it; the caller's string is only read
+        sprintf(line2, "%.*s\n", (int)(nextline - line), line);
...
         // Extract next line from the rule statement
-        if (nextline) *nextline = '\n';
-        line = nextline ? (nextline + 1) : NULL;
+        line = *nextline ? (nextline + 1) : NULL;
```

The length check comes from [CRASH-18](../CRASH-18-addrule-long-line/), which this patch requires. It keeps the copy within `line2`. The parsed text is the same as before, so every rule that was accepted before is accepted now.

**The prototype.** The patch keeps `int EN_addrule(EN_Project ph, char *rule)`. Since the function now only reads the text, the parameter could become `const char *`, like `EN_setnodeid` and the other setters that already take `const char *`. That change is source-compatible for C and C++ callers, since a `char *` argument converts to `const char *`, and binary-compatible. It would break only code that stores `EN_addrule` in a function pointer of the old type. The legacy `ENaddrule` in `epanet2.h` and the language bindings would follow. This is left to the maintainers, since it changes the published API.

Another way to stop the writes is to copy the whole text to the heap and split the copy. That costs an allocation per call and changes more lines, for the same result.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- This patch needs [CRASH-18](../CRASH-18-addrule-long-line/) applied first (`Requires: CRASH-18` in its header), because both change the lines that copy each line into `line2`.
- [CRASH-14](../CRASH-14-calls-after-close/), [CRASH-17](../CRASH-17-addrule-multiple-rules/) and [API-08](../../3-api/API-08-bad-rule-deletes-previous-rule/) also change `EN_addrule`. All of them apply together.
- By reading the declarations, the C# and VB bindings pass the rule as a managed string, which the runtime copies before the call, so callers in those languages do not see the fault.
