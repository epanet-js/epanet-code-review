# CRASH-08: xstrcpy leaves over-long names unterminated

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap over-read. Getters return the stored name followed by whatever is next in memory, and `EN_saveinpfile` writes those bytes into the file. |
| **Reached from** | Input file: a [DEMANDS] category name (the line's comment) over 31 characters, or a [TAGS] tag over 255. Toolkit: `EN_setcomment`, `EN_settag` or `EN_setdemandname` with a string over the limit. |
| **Where** | `xstrcpy()` in [`src/project.c:1630`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L1630) |
| **Since** | v2.2 (2019), when comments and demand names became dynamic strings |
| **GitHub** | No related issue. Open issue [#817](https://github.com/OpenWaterAnalytics/EPANET/issues/817) asks to raise the 255-character comment limit; the truncation path needs this fix either way. |
| **Fix** | Five lines: [`CRASH-08_fix.patch`](CRASH-08_fix.patch) |

## The problem

EPANET stores demand category names, comments and tags as heap strings with a length limit: 31 characters (`MAXID`) for demand names and 255 (`MAXMSG`) for comments and tags. A longer string is meant to be cut to the limit. The cut copy is stored without a terminating null, so every later read runs past the end of the allocation.

In the test, the input file names a demand category `Residential customers - north pressure zone 5` (45 characters). Under AddressSanitizer, `EN_getdemandname` stops with a heap-buffer-overflow. In a plain macOS build it returns `Residential customers - north prID   Multipliers`: the 31 stored characters followed by text from a neighbouring heap block. `EN_saveinpfile` writes the same bytes into the saved file, and a comment of 300 characters set with `EN_setcomment` reads back as 256 characters.

The [DEMANDS] comment is where `EN_saveinpfile` itself writes a category name ([`inpfile.c:349`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L349)), so a descriptive name of more than 31 characters is enough.

## Why it happens

```c
// src/project.c, xstrcpy()
// Find size of source string
if (s2) n2 = strlen(s2);
if (n2 > n) n2 = n;                                   // cut to the limit
...
if (n2 > n1) *s1 = realloc(*s1, (n2 + 1) * sizeof(char));

// Copy the source string into the destination string
if (*s1) strncpy(*s1, s2, n2+1);                      // no terminator if s2 was cut
```

When the source fits, character `n2+1` of `s2` is its terminator and the copy is correct. When the source was cut, character `n2+1` is an ordinary character. `strncpy` copies it into the last byte and writes no null.

Commit [38278f7](https://github.com/OpenWaterAnalytics/EPANET/commit/38278f7d46f6f2c225e0b5362b1ca4bec460cdec) (2019, "Fixed use of strncpy in xstrcpy()") changed the count from `n2` to `n2+1`. That fixed strings that fit, but not strings that are cut.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-08_long-demand-name.inp`](CRASH-08_long-demand-name.inp) | R1 feeds J1 and J2. J1 has two demand categories; the first is named by a 45-character comment. |
| [`CRASH-08_test.c`](CRASH-08_test.c) | Reads the demand name back, then sets a 300-character comment on P1 and reads it back |
| [`CRASH-08_fix.patch`](CRASH-08_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-08            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-08 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-08_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-08_test
./CRASH-08_test
```

**Without the fix**, AddressSanitizer stops the first read of the name:

```
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 35 at ... thread T0
    #0 ... in wrap_strcpy+0x4e4 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4d0e4)
    #1 ... in EN_getdemandname epanet.c:3293
    #2 ... in main CRASH-08_test.c:53

... is located 0 bytes after 32-byte region [...)
allocated by thread T0 here:
    #0 ... in wrap_realloc+0x9c (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x52e28)
    #1 ... in xstrcpy project.c:1627
    #2 ... in demanddata input3.c:919
```

**With the fix**, both strings are cut to their limit and read back intact:

```
Demand name: "Residential customers - north p"
Demand name: 31 characters read back (expected 31)
Comment: 255 characters read back (expected 255)
PASS: over-long strings are cut to their limit and stay terminated
```

## The fix

Copy `n2` characters and write the terminator explicitly. This is `2-performance/load-build/patches/bugfix_xstrcpy.diff` from the performance work:

```diff
     // Copy the source string into the destination string
-    if (*s1) strncpy(*s1, s2, n2+1);
+    // (always null-terminated, even when s2 was truncated to n characters)
+    if (*s1)
+    {
+        memcpy(*s1, s2, n2);
+        (*s1)[n2] = '\0';
+    }
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The getters copy into the caller's buffer with `strcpy`, so a caller still needs a buffer of `EN_MAXID+1` or `EN_MAXMSG+1` characters. Open issue [#814](https://github.com/OpenWaterAnalytics/EPANET/issues/814) is about that separate problem.
- [CRASH-24](../CRASH-24-31-character-ids-unterminated/) is a similar missing terminator for 31-character IDs added with `EN_addnode` and `EN_addlink`.
