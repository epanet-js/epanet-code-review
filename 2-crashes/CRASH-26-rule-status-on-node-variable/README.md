# CRASH-26: A rule premise that compares a node variable with a status reads past the link arrays

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap read out of bounds. A rule such as `IF TANK T1 LEVEL IS OPEN` loads without error, and every rule check then reads the link status array with the tank's node index. On most networks the index is in range and the rule silently tests some link's status instead. |
| **Reached from** | Input file: a premise on a node or on `SYSTEM` whose value is `OPEN`, `CLOSED` or `ACTIVE`. Toolkit API: the same premise in `EN_addrule`. |
| **Where** | `newpremise()` in [`src/rules.c:828`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L828); the read is in `checkstatus()`, [`src/rules.c:1067`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1067) |
| **Since** | The first commit of this repository (2014) |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`CRASH-26_fix.patch`](CRASH-26_fix.patch) |

## The problem

Only links have a status. The manual's rule format allows `STATUS IS OPEN`, `CLOSED` or `ACTIVE` for a link, and a number for everything else. The parser does not enforce this: it accepts a status word as the value of any premise, including one on a node's level, pressure or demand. A typo or a misread manual is enough, for example `IF TANK T1 LEVEL IS OPEN`.

The file opens without an error. During the run, the rule check treats the premise as a link status test and reads the link status array at the node's index. In the test network, tank T1 is node 3 and there are only two links, so the read is one past the end of the array. In a larger network the index usually falls inside the array and the rule tests the status of whichever link has that index, without any warning.

## Why it happens

`newpremise()` takes a status word whatever the object and variable are:

```c
// src/rules.c, newpremise()
    else if ((k = findmatch(Tok[parser->Ntokens - 1], Value)) > IS_NUMBER) s = k;
```

`checkpremise()` then sends every premise that has a status to `checkstatus()`, which assumes that the index is a link:

```c
// src/rules.c, checkpremise()
    else if (p->status > IS_NUMBER) return (checkstatus(pr,p));

// src/rules.c, checkstatus()
      case IS_OPEN:
      case IS_CLOSED:
      case IS_ACTIVE:
        i = hyd->LinkStatus[p->index];
```

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-26_level-is-open.inp`](CRASH-26_level-is-open.inp) | Three nodes and two links. Rule R1 is `IF TANK T1 LEVEL IS OPEN THEN PIPE P2 STATUS = CLOSED`. |
| [`CRASH-26_test.c`](CRASH-26_test.c) | Opens the file and runs the hydraulics, then opens it with `EN_openX` and passes the same premise to `EN_addrule` |
| [`CRASH-26_fix.patch`](CRASH-26_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-26            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-26 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-26_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-26_test
./CRASH-26_test
```

The command-line runner shows the same read: `runepanet CRASH-26_level-is-open.inp CRASH-26.rpt`.

**Without the fix**, the file opens and the first rule check reads past `LinkStatus`:

```
EN_open returned 0 (expected 200)
=================================================================
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 4 at ... thread T0
    #0 ... in checkpremise rules.c:983
    #1 ... in checkrules rules.c:548
    #2 ... in ruletimestep hydraul.c:880
    #3 ... in timestep hydraul.c:682
...
... is located 0 bytes after 12-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in allocdata project.c:419
```

`project.c:419` allocates `LinkStatus` (3 entries for 2 links); `checkstatus()` is inlined into `checkpremise()`. A build without sanitizers (macOS, Release) does not crash on this small overrun: the run completes, `EN_addrule` also accepts the premise, and the test prints `FAIL: a status was accepted for a node variable`.

**With the fix:**

```
EN_open returned 200 (expected 200)
EN_addrule returned 202 (expected 202)
PASS: a status for a node variable is rejected
```

The report names the rule and the clause:

```
  Input Error 202: illegal numeric value in following line of Rule R1:
  IF TANK T1 LEVEL IS OPEN
```

## The fix

After the value has been parsed, refuse a status unless the premise is on a link:

```diff
         if (v == r_FILLTIME || v == r_DRAINTIME) x = x * 3600.0;
     }
 
+    // Only a link has a status to compare with
+    if (s > IS_NUMBER && i != r_LINK) return 202;
+
     // Create new premise structure
```

Error 202 (illegal numeric value) fits: a node or system premise needs a number. `EN_open` reports it as error 200 and `EN_addrule` returns it as is. None of the example networks or test files has such a premise.

A link premise may still pair a status with `FLOW` or `SETTING` (`IF PIPE P1 FLOW IS OPEN`), which is then evaluated as a status test. That is odd but reads only valid link data, so it is left alone.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-20](../CRASH-20-rule-setters-unchecked-index/) adds the same rule to the toolkit setters: `EN_setpremise` and `EN_setpremisestatus` refuse a status on a premise that is not on a link. This patch closes the input-file and `EN_addrule` route.
- [FILE-18](../../4-files/FILE-18-rule-times-lose-a-second/) changes the time lines a few lines above in `newpremise()`. The patches apply together.
