# CRASH-01: An invalid token over 255 characters overruns the error-message buffers

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | A stack buffer overflow while EPANET reports an input error. Shorter tokens (about 200 characters or more) overwrite the project title instead, with no sanitizer report. |
| **Reached from** | Input file: any input error whose offending token is long, for example an over-long ID (error 252) or an invalid section keyword (error 299) |
| **Where** | `inperrmsg()` in [`src/input2.c:758-767`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input2.c#L758-L767) |
| **Since** | EPANET 2.00.12 (2008) or earlier, which already copied the token into a 256-byte buffer with `strcpy`, so every release. The current form dates from commit [f1175b1](https://github.com/OpenWaterAnalytics/EPANET/commit/f1175b1f92629f7a53e4f6cd15e4d39b3f4fcffd) (2018). |
| **GitHub** | No related issue |
| **Fix** | Five lines: [`CRASH-01_fix.patch`](CRASH-01_fix.patch) |

## The problem

When a line in the input file has an error, EPANET writes a message such as `Error 252: invalid ID name <token> in [JUNCTIONS] section:` to the report. The token can be as long as the input line (1,023 characters), but the message is built in two 256-byte buffers with no length check.

- **A token of 256 characters or more** overflows a buffer on the stack. AddressSanitizer stops `EN_open`. The review's input-file fuzzer hit this in 66 of 1,500 mutated files.
- **A token of about 200 to 255 characters** fits the first buffer, but the formatted message does not fit `Project.Msg`. It runs on into `Project.Title`, the next field of the struct. In the test, a 240-character ID replaces the first title line with `XXXXXXXXXXX in [JUNCTIONS] section:`. AddressSanitizer cannot see this, because the write stays inside the `Project` allocation.

## Why it happens

```c
// src/input2.c, inperrmsg()
char errStr[MAXMSG + 1] = "";
char tok[MAXMSG + 1];                                    // 256 bytes

// Get token associated with input error
if (parser->ErrTok >= 0) strcpy(tok, parser->Tok[parser->ErrTok]);
...
    sprintf(pr->Msg, "Error %d: %s %s in %s section:",    // Msg is 256 bytes
        err, geterrmsg(err, errStr), tok, SectTxt[sect]);
```

`MAXMSG` is 255, while a token can be up to `MAXLINE - 1` (1,023) characters. In the `Project` struct, `Msg[MAXMSG+1]` is followed directly by `Title[MAXTITLE][TITLELEN+1]` ([`types.h:918-919`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/types.h#L918-L919)).

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-01_id-240-chars.inp`](CRASH-01_id-240-chars.inp) | Reservoir R1 feeds junction J1. A second junction has a 240-character ID. |
| [`CRASH-01_id-300-chars.inp`](CRASH-01_id-300-chars.inp) | The same with a 300-character ID |
| [`CRASH-01_test.c`](CRASH-01_test.c) | Opens each file with `EN_openX`, then checks the return code and the title |
| [`CRASH-01_fix.patch`](CRASH-01_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-01            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-01 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-01_test
./CRASH-01_test
```

**Without the fix**, the 240-character ID overwrites the title, then the 300-character ID overflows the stack buffer:

```
CRASH-01_id-240-chars.inp
  EN_openX returned 200 (expected 200)
  title is "XXXXXXXXXXX in [JUNCTIONS] section:"   <-- overwritten
CRASH-01_id-300-chars.inp
=================================================================
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 301 at ... thread T0
    #0 ... in wrap_strcpy+0x510 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4d110)
    #1 ... in inperrmsg input2.c:758
    #2 ... in readdata input2.c:256
    #3 ... in getdata input1.c:64
...
    [352, 608) 'tok' (line 755) <== Memory access at offset 608 overflows this variable
```

**With the fix**, both files are rejected and the title is intact:

```
CRASH-01_id-240-chars.inp
  EN_openX returned 200 (expected 200)
  title is "CRASH-01: an invalid ID of 240 characters"
CRASH-01_id-300-chars.inp
  EN_openX returned 200 (expected 200)
  title is "CRASH-01: an invalid ID of 300 characters"
PASS: long tokens are reported without overrunning any buffer
```

The report line is cut at 255 characters, for example `Error 252: invalid ID name JUNCTION_XXXX...XXXX`. The input line is echoed in full on the next line, as before.

## The fix

Copy at most `MAXMSG` characters of the token and format the message with `snprintf`:

```diff
-    if (parser->ErrTok >= 0) strcpy(tok, parser->Tok[parser->ErrTok]);
+    // (cut short if needed: a token can be up to MAXLINE characters long)
+    if (parser->ErrTok >= 0) strncpy(tok, parser->Tok[parser->ErrTok], MAXMSG);
     else strcpy(tok, "");
+    tok[MAXMSG] = '\0';
 ...
-        sprintf(pr->Msg, "Error %d: %s %s in %s section:",
+        snprintf(pr->Msg, sizeof(pr->Msg), "Error %d: %s %s in %s section:",
```

The same change is made to the message for error 299. A long message loses its end (` in [JUNCTIONS] section:`) rather than overflowing. To keep the section name, the token could instead be shortened further, for example to 100 characters plus `...`.

`input2.c` uses the standard `snprintf`, which MSVC has provided since Visual Studio 2015. `rules.c` already relies on it. (`epanet.c` and `report.c` map `snprintf` to `_snprintf` on Windows, which does not add a terminator when it truncates, so this file must not copy that define.)

EPANET's own unit tests (6 suites) pass with the patch.
