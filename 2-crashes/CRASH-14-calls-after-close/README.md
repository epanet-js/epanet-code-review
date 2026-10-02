# CRASH-14: Rule, control, ID and link-node functions work on freed memory after EN_close

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Use after free and double free. After `EN_close`, 17 toolkit functions still read and write the freed network arrays and return 0, and a second `EN_close` frees them again. `EN_addrule` before `EN_open` dereferences a NULL hash table. |
| **Reached from** | Toolkit API: the functions listed below, called after `EN_close` (or, for `EN_addrule`, before `EN_open`); a second `EN_close` |
| **Where** | `EN_close()` in [`src/epanet.c:351-394`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L351-L394), which calls `freedata()` ([`src/project.c:467`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/project.c#L467)), and the 17 functions in `src/epanet.c` |
| **Since** | v2.2, when these functions were added without the check |
| **GitHub** | No related issue |
| **Fix** | 17 one-line guards and one call in `EN_close`: [`CRASH-14_fix.patch`](CRASH-14_fix.patch) |

## The problem

Most toolkit functions start with `if (!p->Openflag) return 102;` (no network data available). These do not:

- `EN_setnodeid`, `EN_setlinkid`, `EN_setlinknodes`
- `EN_deletecontrol`
- `EN_addrule`, `EN_deleterule`, `EN_getrule`, `EN_setrulepriority`
- `EN_getpremise`, `EN_setpremise`, `EN_setpremiseindex`, `EN_setpremisestatus`, `EN_setpremisevalue`
- `EN_getthenaction`, `EN_setthenaction`, `EN_getelseaction`, `EN_setelseaction`

`EN_close` frees the network arrays but leaves the node, link, control and rule counts as they were, and leaves the pointers dangling. So after `EN_close` these functions pass their index checks and work on freed memory. In a build without sanitizers (macOS, Release), `EN_setnodeid`, `EN_setlinkid`, `EN_setlinknodes`, `EN_deletecontrol`, `EN_getrule`, `EN_setrulepriority` and `EN_deleterule` return 0, as if they had worked. A second `EN_close` then aborts with a double free (exit code 134).

`EN_getcomment`, `EN_setcomment`, `EN_gettag`, `EN_settag` and `EN_timetonextevent` have no `Openflag` check either. They are not in the original finding, but `EN_getcomment` after `EN_close` is also a heap use-after-free (checked by running it).

`EN_addrule` on a project that was never opened reallocates the empty rule array, then looks up the rule's node in a hash table that does not exist yet.

## Why it happens

`EN_close` frees everything and resets only the flags:

```c
// src/epanet.c, EN_close()
    // Free all project data
    freedata(p);
    ...
    // Reset system flags
    p->Openflag = FALSE;
```

`freedata()` frees `Node`, `Link`, `Control`, the hash tables and so on, but sets only `Rule` back to NULL and leaves `Nnodes`, `Nlinks`, `Ncontrols` and `Nrules` unchanged. A function without the `Openflag` check then goes straight to its index check, which passes:

```c
// src/epanet.c, EN_setnodeid()
    // Check for valid arguments
    if (index <= 0 || index > net->Nnodes) return 203;
    if (!namevalid(newid)) return 252;

    // Check if another node with same name exists
    if (hashtable_find(net->NodeHashTable, newid) > 0) return 215;
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-14_rule-and-control.inp`](CRASH-14_rule-and-control.inp) | A reservoir, a junction and a tank, with one simple control and one rule (with THEN and ELSE actions) |
| [`CRASH-14_test.c`](CRASH-14_test.c) | Opens and closes the project, calls each function, calls `EN_close` again, then calls `EN_addrule` on a new project that was never opened |
| [`CRASH-14_fix.patch`](CRASH-14_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-14            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-14 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-14_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-14_test
./CRASH-14_test
```

**Without the fix**, the first call after `EN_close` reads the freed node hash table:

```
After EN_close:
=================================================================
==...==ERROR: AddressSanitizer: heap-use-after-free on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in hashtable_find hash.c
    #1 ... in EN_setnodeid epanet.c:2214
    #2 ... in main CRASH-14_test.c:45
...
freed by thread T0 here:
    ...
    #1 ... in freedata project.c:554
    #2 ... in EN_close epanet.c:361
```

**With the fix:**

```
After EN_close:
EN_setnodeid           returned 102
EN_setlinkid           returned 102
EN_setlinknodes        returned 102
EN_deletecontrol       returned 102
EN_addrule             returned 102
EN_getrule             returned 102
EN_getruleID           returned 102
...
EN_setrulepriority     returned 102
EN_deleterule          returned 102
EN_close again returned 0
Before EN_open:
EN_addrule             returned 102
PASS: every call on a closed project returns 102
```

## The fix

Two parts:

1. Each of the 17 functions returns 102 when no project is open. The check is the first statement after the declarations, for example:

   ```diff
        Saction *action;
    
   +    if (!p->Openflag) return 102;
        if (ruleIndex < 1 || ruleIndex > p->network.Nrules) return 257;
   ```

2. `EN_close` calls `initpointers()` after `freedata()`:

   ```diff
        // Free all project data
        freedata(p);
    
   +    // Reset the counts and pointers to the freed data, so that a closed
   +    // project looks the same as one that was never opened
   +    initpointers(p);
   ```

   `initpointers()` is what `EN_open` and `EN_init` call before they allocate anything. It sets the counts to 0 and the array pointers to NULL. Every index check in a function without an `Openflag` check then fails safely (`EN_getcomment` now returns 251), and a second `EN_close` frees only NULL pointers. It also clears the report callback, but `EN_open` and `EN_init` already do that, so a callback had to be set after opening anyway.

`EN_getruleID` already had the check, so the test only confirms it. The test does not cover the comment, tag and time-to-next-event functions, which the second part makes safe.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Other patches change the bodies of some of these functions: [CRASH-23](../CRASH-23-setlinknodes-node-zero/) (`EN_setlinknodes`), [CRASH-24](../CRASH-24-31-character-ids-unterminated/) (`EN_setnodeid`, `EN_setlinkid`), [CRASH-17](../CRASH-17-addrule-multiple-rules/), [CRASH-18](../CRASH-18-addrule-long-line/), [CRASH-19](../CRASH-19-addrule-edits-caller-string/) and [API-08](../../3-api/API-08-bad-rule-deletes-previous-rule/) (`EN_addrule`), and [CRASH-20](../CRASH-20-rule-setters-unchecked-index/) (the premise and action setters). The guards sit at the top of each function so that the patches apply together.
- [API-30](../../3-api/API-30-report-file-left-open-after-failed-open/) makes `EN_deleteproject` call `EN_close` even after a failed `EN_open`, so `EN_close` then runs twice in the usual close-then-delete sequence. It relies on the second part of this patch and requires it.
- `EN_close` does not close an open hydraulic or quality solver, so their work arrays leak if a program closes the project without `EN_closeH` or `EN_closeQ`. That was already so, and `initpointers()` does not change it.
