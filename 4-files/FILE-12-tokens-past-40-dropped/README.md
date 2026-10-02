# FILE-12: Tokens past the 40th on a line are dropped

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Data is lost with no warning. A weekly demand pattern written on one line keeps 39 of its 168 factors, and the run repeats those 39 hours. |
| **Reached from** | Input file: any line with more than 40 tokens, in practice [PATTERNS] lines and [REPORT] `NODES` or `LINKS` lists. Also `EN_setreport` and `EN_addrule`. |
| **Where** | `gettokens()` in [`src/input2.c:594`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L594); `MAXTOKS` in [`src/types.h:44`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/types.h#L44) |
| **Since** | `MAXTOKS` has been 40 since EPANET 2.00.12 (2008) or earlier. v2.2 raised the line limit from 255 to 1,024 characters, which made the problem much easier to hit. |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`FILE-12_fix.patch`](FILE-12_fix.patch) |

## The problem

The input file format allows 1,024 characters per line ([`toolkit-input.dox`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/doc/toolkit-input.dox#L21)) and does not mention a limit on the number of values. A [PATTERNS] line can carry as many factors as fit, for example a whole weekly pattern.

The tokenizer stops after 40 tokens and drops the rest of the line without an error. In the test, a weekly pattern with 168 hourly factors on one 845-character line loads as a 39-factor pattern. `EN_open` returns 0, and an extended-period run cycles through hours 1 to 39 for the whole week.

## Why it happens

```c
// src/types.h
#define   MAXLINE   1024     // Max. # characters read from input line
#define   MAXTOKS   40       // Max. items per line of input

// src/input2.c, gettokens()
// Scan s for tokens until nothing left
while (len > 0 && n < MAXTOKS)
{
    ...
}
return n;
```

The loop ends at 40 tokens whether or not the line has more, and the caller cannot tell the difference.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-12_weekly-pattern.inp`](FILE-12_weekly-pattern.inp) | Reservoir R1 feeds junction J1, whose demand follows pattern WEEK: 168 hourly factors on one line |
| [`FILE-12_test.c`](FILE-12_test.c) | Opens the file and checks the length and last factor of WEEK |
| [`FILE-12_fix.patch`](FILE-12_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-12            # unpatched EPANET: FAIL
tools/run-test.sh FILE-12 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-12_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-12_test
./FILE-12_test
```

**Without the fix**:

```
EN_open returned 0
Pattern WEEK: 39 factors (expected 168), last factor 1.00 (expected 0.70)
FAIL: 129 of the 168 factors on the line were silently dropped
```

**With the fix**:

```
EN_open returned 0
Pattern WEEK: 168 factors (expected 168), last factor 0.70 (expected 0.70)
PASS: all 168 factors on the line are read
```

## The fix

Make the token array large enough for any line the reader accepts. A line of `MAXLINE` characters holds at most `MAXLINE / 2 + 1` tokens (one-character tokens with single separators), so no token can be dropped:

```diff
-#define   MAXTOKS   40       // Max. items per line of input
+#define   MAXTOKS   (MAXLINE / 2 + 1) // Max. items per line of input
```

`gettokens()` set every one of the `MAXTOKS` pointers to NULL at the start of each line. With 513 slots that alone made `EN_open` about 7 % slower. Since only `gettokens()` writes the array, the slots after the previous line's tokens are already NULL, so the patch clears only up to the first NULL:

```diff
-    // Begin with no tokens
-    for (n=0; n<maxToks; n++) Tok[n] = NULL;
+    // Begin with no tokens (only the slots used by the previous
+    // line need clearing; the rest are still NULL)
+    for (n = 0; n < maxToks && Tok[n] != NULL; n++) Tok[n] = NULL;
```

Timing `EN_open` on a synthetic 62,500-junction grid (about 250,000 lines, best of 7, plain Release build on Apple arm64): 99.9 ms unpatched, 107 to 108 ms with only the larger `MAXTOKS`, and 97 to 98 ms with this patch. The `Project` struct grows by about 4 KB (473 more pointers), and the parser's scratch array `X` by the same amount while a file is read.

The alternative is to keep `MAXTOKS` at 40 and report an error for a line with more tokens. That avoids silent data loss, but rejects files that follow the documented format.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The single-pass tokenizer from the performance work (`2-performance/load-build/patches/tok.diff`) keeps the full clearing loop. If both are applied, it should take the same one-line change.
- [FILE-11](../FILE-11-long-lines-split/) is the related limit on characters per line; [FILE-14](../FILE-14-quoted-tokens-mis-split/) also changes `gettokens()`. The three patches touch different lines and apply together.
