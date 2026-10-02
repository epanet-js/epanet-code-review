# FILE-14: A quoted token corrupts the rest of the line

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A valid line with a quoted token gets a spurious input error. A quoted token that contains spaces makes the tokenizer read past the end of the line, which AddressSanitizer reports when it leaves the line buffer. |
| **Reached from** | Input file: any line with a token in double quotes that is not the last token, for example a quoted ID or a [LABELS] line. Also `EN_addrule` and `EN_setreport`, which use the same tokenizer. |
| **Where** | `gettokens()` in [`src/input2.c:603-615`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L603-L615) |
| **Since** | The over-read with spaces in quotes: EPANET 2.00.12 (2008) or earlier. The spurious error: v2.2, from commit [6a4b95f](https://github.com/OpenWaterAnalytics/EPANET/commit/6a4b95f4d517942ce750bb5abaa07dadb6ff5c9f) (2019), which added a last-token shortcut that relies on the count. |
| **GitHub** | No related issue |
| **Fix** | One line: [`FILE-14_fix.patch`](FILE-14_fix.patch) |

## The problem

The tokenizer treats text in double quotes as one token. The documented use is the text of a map label in [LABELS], which may contain spaces, for example `1000.00 2000.00 "Pump Station A" J1`. Quotes around an ID are accepted too.

After a quoted token, the rest of the line is split wrongly:

- **No spaces in the quotes.** The last token on the line keeps its newline character. In the test, the junction line `"J1"  100  5  PAT1` fails with `Error 205: undefined time pattern PAT1`, because the pattern name it looks up is `PAT1\n`. A number in the last place fails with error 202 in the same way.
- **Spaces in the quotes.** The scan carries on past the end of the line, by about the length of the quoted text. With a short label such as `"Pump Station A"` it reads about 10 bytes past the end of the line, into what earlier lines left in the same buffer (by reading the code; no sanitizer flags this). In the test a 923-character label takes it past the end of the 1,025-byte buffer, and AddressSanitizer stops `EN_open`.

## Why it happens

`len` counts the characters left on the line. It is reduced by the length of the next token as found by `strcspn(s, SEPSTR)`, before the code checks for a quote. For a quoted token that is only the part up to the first separator, but the pointer then moves past the whole quoted text and the closing quote:

```c
// src/input2.c, gettokens()
while (len > 0 && n < MAXTOKS)
{
    m = (int)strcspn(s,SEPSTR);     // Find token length
    if (m == len)                   // s is last token
    {
        Tok[n] = s;                 // (not terminated: relies on len)
        n++;
        break;
    }
    len -= m+1;                     // Update length of s
    if (m == 0) s++;                // No token found
    else
    {
        if (*s == '"')              // Token begins with quote
        {
            s++;                           // Start token after quote
            m = (int)strcspn(s,"\"\n\r");  // Find end quote (or EOL)
        }
        s[m] = '\0';                 // Null-terminate the token
        Tok[n] = s;                  // Save pointer to token
        n++;                         // Update token count
        s += m+1;                    // Begin next token
    }
}
```

For `"J1"`, `len` drops by 5 (the token and one separator) while `s` moves 4 (to the character after the closing quote), so `len` ends one short. The last-token shortcut then fires one character early, while the newline is still part of the token. For `"a b"`, `len` drops by 3 while `s` moves 5, so `len` ends two too long and the loop keeps scanning beyond the terminating null.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-14_quoted-id.inp`](FILE-14_quoted-id.inp) | R1 feeds J1 and J2, both with pattern PAT1. J1's ID is written as `"J1"`. |
| [`FILE-14_long-label.inp`](FILE-14_long-label.inp) | R1 feeds J1. [LABELS] has one label whose quoted text is 923 characters with spaces. |
| [`FILE-14_test.c`](FILE-14_test.c) | Opens each file and checks that it loads |
| [`FILE-14_fix.patch`](FILE-14_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-14            # unpatched EPANET: CRASH
tools/run-test.sh FILE-14 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-14_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-14_test
./FILE-14_test
```

**Without the fix**, the quoted ID gives a spurious error, then the long label overruns the line buffer:

```
FILE-14_quoted-id.inp
  EN_open returned 200 (expected 0)
  report:   Error 205: undefined time pattern PAT1
  report:   Error 200: one or more errors in input file
FILE-14_long-label.inp
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 7 at ... thread T0
    #0 ... in wrap_strcspn+0x480 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x19a10)
    #1 ... in gettokens input2.c:596
    #2 ... in readdata input2.c:198
    #3 ... in getdata input1.c:64
...
    [1200, 2225) 'wline' (line 158) <== Memory access at offset 2225 overflows this variable
```

**With the fix**:

```
FILE-14_quoted-id.inp
  EN_open returned 0 (expected 0)
FILE-14_long-label.inp
  EN_open returned 0 (expected 0)
PASS: both files with quoted tokens load
```

## The fix

After finding the closing quote, recompute `len` from where the scan really is:

```diff
                 s++;                           // Start token after quote
                 m = (int)strcspn(s,"\"\n\r");  // Find end quote (or EOL)
+                len = (int)strlen(s + m) - 1;  // Length after end quote
             }
```

`s + m` is the closing quote (or the end of the line), and the `- 1` accounts for it. If the quote is never closed and the string ends, `len` becomes -1 and the loop stops.

**Alternative.** The single-pass tokenizer from the performance work, `2-performance/load-build/patches/tok.diff`, rewrites the loop without a length counter, which removes this bug as well and halves the time spent tokenizing. It is a larger change, so this patch keeps to the one line.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The review's original over-read example has a 900-character quoted tag in [TAGS]. With this fix it gets past the tokenizer and then hits the unterminated-string bug [CRASH-08](../../2-crashes/CRASH-08-long-names-unterminated/), because a tag is cut to 255 characters.
- EPANET's own writer does not quote strings that contain spaces ([FILE-08](../FILE-08-strings-with-spaces-unquoted/)). If it starts to, this fix is needed first.
- [FILE-12](../FILE-12-tokens-past-40-dropped/) also changes `gettokens()`, at different lines; the two patches apply together.
