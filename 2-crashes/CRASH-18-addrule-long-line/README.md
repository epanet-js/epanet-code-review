# CRASH-18: EN_addrule with a line of 1,024 characters or more overflows the stack

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Stack buffer overflow. Any line of 1,024 characters or more in the text passed to `EN_addrule`, including a comment line, writes past a 1,025-byte stack buffer. |
| **Reached from** | Toolkit API: `EN_addrule` |
| **Where** | `EN_addrule()` in [`src/epanet.c:5652-5653`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5652-L5653) |
| **Since** | v2.2, when `EN_addrule` was added (commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1), PR [#335](https://github.com/OpenWaterAnalytics/EPANET/pull/335), 2018) |
| **GitHub** | No related issue |
| **Fix** | Five lines: [`CRASH-18_fix.patch`](CRASH-18_fix.patch) |

## The problem

`EN_addrule` copies each line of the rule text into a fixed buffer of `MAXLINE + 1` (1,025) characters and adds a newline. A line of 1,023 characters fits exactly. A line of 1,024 characters writes one byte past the end, and longer lines write further.

Rule clauses are short, but a comment line is not limited, and text built by a program (a description, or several clauses joined without newlines) can be long. Nothing in the API documentation mentions a limit.

## Why it happens

The line is copied without a length check:

```c
// src/epanet.c, EN_addrule()
    char line2[MAXLINE+1];
    ...
        // Find where current line ends and next one begins
        nextline = strchr(line, '\n');
        if (nextline) *nextline = '\0';

        // Copy and tokenize the current line
        strcpy(line2, line);
        strcat(line2, "\n");  // Tokenizer won't work without this
```

For a line of `n` characters, `strcpy` and `strcat` write `n + 2` bytes, so the line must be 1,023 characters or less. The input-file reader cannot overflow in the same way, because `fgets()` reads at most `MAXLINE - 1` characters into its buffer (it splits longer lines instead: [FILE-11](../../4-files/FILE-11-long-lines-split/)).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-18_tank.inp`](CRASH-18_tank.inp) | Reservoir R1 fills tank T1 through pipe P2; no rules |
| [`CRASH-18_test.c`](CRASH-18_test.c) | Adds rule R1, whose last line is a 1,023-character comment, and rule R2, whose last line is a 1,024-character comment |
| [`CRASH-18_fix.patch`](CRASH-18_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-18            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-18 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-18_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-18_test
./CRASH-18_test
```

**Without the fix**, the 1,023-character line is fine and the 1,024-character line overflows `line2` by one byte:

```
Rule R1, last line 1023 characters: EN_addrule returned 0
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 2 at ... thread T0
    #0 ... in EN_addrule epanet.c:5653
    #1 ... in main CRASH-18_test.c:55
...
  This frame has 1 object(s):
    [32, 1057) 'line2' (line 5636) <== Memory access at offset 1057 overflows this variable
```

A build without sanitizers (macOS, Release) stops in the fortified `strcat` with exit code 133 (SIGTRAP). Without fortification the stack is overwritten.

**With the fix:**

```
Rule R1, last line 1023 characters: EN_addrule returned 0
Rule R2, last line 1024 characters: EN_addrule returned 214
Rule count: 1
PASS: a 1023-character line is accepted, a 1024-character one rejected
```

## The fix

Refuse a line that does not fit, before copying it:

```diff
         if (nextline) *nextline = '\0';
 
+        // The line and the newline added below must fit in line2
+        if (strlen(line) >= MAXLINE)
+        {
+            rules->Errcode = 214;
+            break;
+        }
+
         // Copy and tokenize the current line
         strcpy(line2, line);
```

Error 214 (too many characters in input line) is the existing code for this, and is listed as applying to API functions as well as input files. The rule being added is deleted, as for any other error, so the project is unchanged.

The limit stays the same as for an input file. An alternative is to tokenize each line in place in a heap copy of the text, which would remove the limit altogether; the rule parser does not need lines longer than this.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-19](../CRASH-19-addrule-edits-caller-string/) changes how `EN_addrule` finds and copies each line, so that it no longer writes into the caller's string. It rewrites the lines around this check and changes `strlen(line)` to `nextline - line`, so its patch requires this one.
- [CRASH-14](../CRASH-14-calls-after-close/), [CRASH-17](../CRASH-17-addrule-multiple-rules/) and [API-08](../../3-api/API-08-bad-rule-deletes-previous-rule/) also change `EN_addrule`. All of them apply together.
