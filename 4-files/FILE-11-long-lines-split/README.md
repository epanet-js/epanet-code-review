# FILE-11: Lines over 1,023 characters are split in two

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | The tail of a long line is read as a separate line. A long comment becomes a bogus data line and gives a misleading error. A long data line is cut silently and its tail can create an extra object: in the test, a 36-factor pattern loads as two patterns with no error. |
| **Reached from** | Input file: any line longer than 1,023 characters |
| **Where** | `netsize()` and `readdata()` in [`src/input2.c:87`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L87) and [`194`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L194); the unreachable check at [`220`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L220-L226) |
| **Since** | Every release. Up to v2.1 the line buffer was 255 characters; v2.2 raised it to 1,024. |
| **GitHub** | No related issue |
| **Fix** | A new 20-line function and four changes in `input2.c`: [`FILE-11_fix.patch`](FILE-11_fix.patch) |

## The problem

The documentation allows up to 1,024 characters per line. EPANET reads each line with `fgets` into a buffer that returns at most 1,023 characters. If a line is longer, the next `fgets` call returns the rest of it, and the parser treats that as a new line. There is a check for over-long lines (error 214), but it cannot fire.

The test shows both kinds of damage:

- **A long comment.** Junction J1 has a 1,100-character comment. The comment's tail is read as a [JUNCTIONS] line, so a valid file fails to load with `Error 202: illegal numeric value 4471-B. in [JUNCTIONS] section:`, an error about text the user wrote as a comment.
- **A long data line.** Pattern PAT1 has 36 factors in 30-character columns, 1,085 characters in all. The line is cut inside the 34th factor: PAT1 gets 34 factors, the 34th read as `1.` (1.0 instead of 1.36), and the tail `36 ... 1.38 ... 1.40` becomes a second pattern named `36`. `EN_open` returns 0.

## Why it happens

```c
// src/input2.c, readdata()
char line[MAXLINE + 1],  // Line from input data file
...
while (fgets(line, MAXLINE, parser->InFile) != NULL)      // at most MAXLINE-1 characters
{
    ...
    // Check if max. line length exceeded
    if (strlen(line) >= MAXLINE)                           // never true
    {
        sprintf(pr->Msg, "%s section: %s", geterrmsg(214, errmsg), SectTxt[sect]);
        ...
```

`fgets(line, MAXLINE, ...)` stores at most `MAXLINE - 1` characters, so `strlen(line)` is never `MAXLINE` or more. `netsize()`, the first pass that counts objects, reads the file the same way. Both passes split a long line at the same place, so no array is overrun, but the tail is counted and parsed as an object of its own.

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-11_long-comment.inp`](FILE-11_long-comment.inp) | R1 feeds J1 and J2. J1's line ends with a 1,100-character comment. |
| [`FILE-11_long-pattern.inp`](FILE-11_long-pattern.inp) | R1 feeds J1, whose demand follows PAT1: 36 factors in 30-character columns, on a line of 1,085 characters |
| [`FILE-11_test.c`](FILE-11_test.c) | Opens both files. Expects the first to load and the second to be rejected with error 214. |
| [`FILE-11_fix.patch`](FILE-11_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-11            # unpatched EPANET: FAIL
tools/run-test.sh FILE-11 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-11_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-11_test
./FILE-11_test
```

**Without the fix**:

```
FILE-11_long-comment.inp: EN_open returned 200 (expected 0)
  report:   Error 202: illegal numeric value 4471-B. in [JUNCTIONS] section:
  report:   Error 200: one or more errors in input file
FILE-11_long-pattern.inp: EN_open returned 0 (expected 200)
  2 patterns (the file has 1); PAT1 has 34 factors (the line has 36)
  pattern 2 is named "36", from the tail of the line
FAIL: lines over 1,023 characters are split and the tail is parsed as a new line
```

**With the fix**:

```
FILE-11_long-comment.inp: EN_open returned 0 (expected 0)
FILE-11_long-pattern.inp: EN_open returned 200 (expected 200)
  report:   Error 214: too many characters in input line in [PATTERNS] section:
  report:   Error 200: one or more errors in input file
PASS: a long comment is ignored and a long data line gives error 214
```

## The fix

A new function, `skiprestofline()`, reads and discards the rest of a line that did not fit, and returns 1 if anything other than blanks was lost. Both passes call it straight after `fgets`, so they see the same lines. `readdata()` reports error 214 when data was lost:

```diff
     while (fgets(line, MAXLINE, parser->InFile) != NULL)
     {
+        // Skip the rest of a line too long for the buffer; this is an
+        // error unless only blanks or part of a comment were lost
+        toolong = skiprestofline(parser->InFile, line) && !strchr(line, ';');
+
 ...
         // Check if max. line length exceeded
-        if (strlen(line) >= MAXLINE)
+        if (toolong && sect >= 0)
         {
-            sprintf(pr->Msg, "%s section: %s", geterrmsg(214, errmsg), SectTxt[sect]);
+            sprintf(pr->Msg, "Error 214: %s in %s section:", geterrmsg(214, errmsg),
+                    SectTxt[sect]);
```

Choices a maintainer may want to revisit:

- **Long comments are allowed.** If the part of the line that fits contains a `;`, everything lost is comment text, which EPANET cuts to 255 characters anyway, so the rest is dropped without an error. Strictly, the documented limit applies to the whole line.
- **Every other long line is an error, in any section.** That includes [TITLE] and [LABELS], whose long lines did no harm before (a title is cut to 79 characters). Those sections could be exempted.
- **The message format.** This code had never run. Its message lacked the `Error 214:` prefix, and the `sect >= 0` test is new: before the first section header `sect` is -1, and `SectTxt[-1]` would be read out of bounds.
- **1,023 or 1,024 characters.** The buffer holds 1,024 characters, but `fgets(line, MAXLINE, ...)` stops at 1,023, while the documentation says 1,024. Passing `MAXLINE + 1` would match the documentation; this patch leaves the limit where it is.
- **A larger limit.** Reading lines of any length would need a growing buffer and is a bigger change.

Two other readers use `fgets` with a fixed buffer and are not changed: `saveauxdata()` ([`inpfile.c:63`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L63)), which copies [LABELS] and [BACKDROP] from the input file when saving, and `EN_loadpatternfile` ([`epanet.c:4781`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L4781)), which reads one value per line.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [FILE-12](../FILE-12-tokens-past-40-dropped/) is the matching limit on tokens per line. Its patch makes room for every token a line of this length can hold.
- [CRASH-18](../../2-crashes/CRASH-18-addrule-long-line/) is a different long-line bug, in `EN_addrule`.
