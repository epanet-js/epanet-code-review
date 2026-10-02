# CRASH-24: EN_addnode and EN_addlink leave 31-character IDs unterminated

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | A node or link given an ID of exactly 31 characters through the API cannot be found by that ID, and `EN_getnodeid` or `EN_getlinkid` writes past the end of a caller's buffer of the documented size. |
| **Reached from** | Toolkit API: `EN_addnode`, `EN_addlink`, `EN_setnodeid` or `EN_setlinkid` with an ID of `EN_MAXID` (31) characters, when the reallocated slot does not already hold a zero in its last byte |
| **Where** | `strncpy(..., MAXID)` in `EN_addnode()` [`src/epanet.c:2028`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2028), `EN_setnodeid()` [`:2218`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2218), `EN_addlink()` [`:3449`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3449) and `EN_setlinkid()` [`:3703`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3703) |
| **Since** | v2.2, the first release with these functions |
| **GitHub** | No related issue |
| **Fix** | Four lines: [`CRASH-24_fix.patch`](CRASH-24_fix.patch) |

## The problem

An ID may have up to `EN_MAXID` = 31 characters, and `namevalid()` accepts 31. Node and link IDs are stored in `char ID[MAXID+1]`, copied with `strncpy(ID, id, MAXID)`. For a 31-character ID, `strncpy` copies the 31 characters and no terminating zero, so the ID ends wherever the next zero byte happens to be. The node and link arrays grow with `realloc`, so the last byte of a new slot is whatever was in memory.

When that byte is not zero:

- The hash table stores the longer string, so `EN_getnodeindex` or `EN_getlinkindex` with the real 31-character ID does not find the object (203 or 204). Nor does `EN_addlink` when the node is named as an end node: it returns 203.
- `EN_getnodeid` and `EN_getlinkid` copy the longer string with `strcpy`. The documentation asks callers for a buffer of `EN_MAXID + 1` characters, so the copy overflows it. In the test, 33 bytes are written into a 32-byte stack buffer.
- `EN_saveinpfile` writes the longer ID, with the stray byte, to the file.

All four ways of setting such an ID go wrong: a tank added with `EN_addnode`, a pipe added with `EN_addlink`, and a tank and a pipe renamed with `EN_setnodeid` and `EN_setlinkid`. The test stops at the first one under AddressSanitizer; in a copy of the test with a 256-byte buffer, all four look-ups return index 0 and the IDs read back as 32 or 33 characters. Whether the bug shows depends on the memory allocator. AddressSanitizer fills new memory with a non-zero byte, so under the sanitizer build it shows every time. Built without sanitizers on macOS, the reallocated slots happened to be zero, and the same test passes.

IDs read from an input file are not affected, because the parser fills arrays that were allocated with `calloc`.

## Why it happens

```c
// src/project.c, namevalid()
    size_t n = strlen(name);
    if (n < 1 || n > MAXID || strpbrk(name, " ;") || name[0] == '"') return FALSE;

// src/epanet.c, EN_addnode()
    // Grow node-related arrays to accommodate the new node
    size = (net->Nnodes + 2) * sizeof(Snode);
    net->Node = (Snode *)realloc(net->Node, size);
    ...
    strncpy(node->ID, id, MAXID);

// src/epanet.c, EN_getnodeid()
    strcpy(id, p->network.Node[index].ID);
```

`strncpy` writes a terminating zero only when the source is shorter than the count. The same pattern is in `EN_addlink`, `EN_setnodeid` and `EN_setlinkid`. A renamed object is affected when its slot was created by `EN_addnode` or `EN_addlink`: a shorter first ID is padded with zeros only up to index 30, so byte 31 still holds whatever `realloc` left there.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-24_test.c`](CRASH-24_test.c) | Builds a network with `EN_init`, gives two new objects and two renamed objects 31-character IDs, looks each one up by ID and reads its ID back into a `char[EN_MAXID + 1]` buffer |
| [`CRASH-24_fix.patch`](CRASH-24_fix.patch) | The fix |

No input file is needed. With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-24            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-24 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-24_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-24_test
./CRASH-24_test
```

**Without the fix**, the first look-up fails, and reading the ID back overflows the buffer:

```
Tank_added_with_31_characters_X: found at index 0 (expected 3)
==...==ERROR: AddressSanitizer: stack-buffer-overflow on address ... at pc ... bp ... sp ...
WRITE of size 33 at ... thread T0
    #0 ... in wrap_strcpy+0x510 (libclang_rt.asan_osx_dynamic.dylib:arm64e+0x4d110)
    #1 ... in EN_getnodeid epanet.c:2193
    #2 ... in check_id CRASH-24_test.c:30
...
  This frame has 2 object(s):
    [32, 64) 'buf' (line 23)
```

**With the fix**, all four IDs are found and read back:

```
Tank_added_with_31_characters_X: found at index 3 (expected 3)
  ID read back: 31 characters
Pipe_added_with_31_characters_X: found at index 2 (expected 2)
  ID read back: 31 characters
Tank_renamed_to_31_characters_X: found at index 2 (expected 2)
  ID read back: 31 characters
Pipe_renamed_to_31_characters_X: found at index 1 (expected 1)
  ID read back: 31 characters
PASS: all four 31-character IDs are found and read back unchanged
```

## The fix

Write the terminator after each of the four copies:

```diff
     strncpy(node->ID, id, MAXID);
+    node->ID[MAXID] = '\0';
```

and the same for `net->Node[index].ID`, `link->ID` and `net->Link[index].ID`. EPANET's own unit tests (6 suites) pass with the patch.

`EN_addpattern`, `EN_addcurve`, `EN_setpatternid` and `EN_setcurveid` use `strcpy` after the same `namevalid()` check, which is also correct. Replacing the four `strncpy` calls with `strcpy` would work too; the explicit terminator keeps the copy bounded if `namevalid()` ever changes.

## Notes

- [CRASH-08](../CRASH-08-long-names-unterminated/) is the same mistake in `xstrcpy()`, which leaves demand category names, comments and tags unterminated when they are cut to their limit.
- [CRASH-14](../CRASH-14-calls-after-close/) adds an `Openflag` check at the top of `EN_setnodeid` and `EN_setlinkid`, and [API-05](../../3-api/API-05-stale-hydraulics-after-edit/) adds a line at the end of `EN_addnode` and `EN_addlink`. The patches apply together.
