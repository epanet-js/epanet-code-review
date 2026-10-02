# CRASH-20: Rule premise and action setters accept any object index

| | |
|---|---|
| **Category** | [Crashes and invalid memory access](../README.md) |
| **Impact** | Heap reads out of bounds. A rule edited with a bad node or link index, or with a variable that does not suit its object, is accepted with return code 0. The next hydraulic run reads past the end of the node or link arrays, and `EN_saveinpfile` indexes its keyword tables with the unchecked codes. |
| **Reached from** | Toolkit API: `EN_setpremise`, `EN_setpremiseindex`, `EN_setpremisestatus`, `EN_setthenaction` and `EN_setelseaction` with an out-of-range index or code, followed by `EN_solveH`, `EN_runH` or `EN_saveinpfile` |
| **Where** | The setters in [`src/epanet.c:5796-6020`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L5796-L6020); the reads in `checkstatus()` and `checkvalue()` ([`src/rules.c:1052-1174`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L1052-L1174)) |
| **Since** | v2.2, when the rule getters and setters were added |
| **GitHub** | No related issue |
| **Fix** | About 40 lines of checks: [`CRASH-20_fix.patch`](CRASH-20_fix.patch) |

## The problem

The [RULES] parser looks up each node and link by ID, and checks that the variable suits the object. The toolkit setters that edit a rule check that the rule and the clause exist, and nothing else. In the test network (3 nodes, 2 links), each of these calls returns 0:

- `EN_setpremise` with tank T1 and the link variable `EN_R_FLOW`
- `EN_setpremise` and `EN_setpremiseindex` with node 4
- `EN_setthenaction` and `EN_setelseaction` with link 3

The rule is checked at every hydraulic time step. A premise on node 4 reads `NodeHead[4]`, one past the end. A flow premise on a node reads `LinkFlow` with a node index, which is out of bounds when that index is larger than the number of links. An action on link 3 reads `LinkStatus[3]` when it fires. `EN_saveinpfile` reads `Node[index]` or `Link[index]` for the ID, and uses the variable, operator and status codes as indexes into its keyword tables.

## Why it happens

All five setters follow the same pattern. `EN_setpremiseindex`:

```c
// src/epanet.c, EN_setpremiseindex()
    if (ruleIndex < 1 || ruleIndex > p->network.Nrules) return 257;

    premises = p->network.Rule[ruleIndex].Premises;
    premise = getpremise(premises, premiseIndex);
    if (premise == NULL)  return 258;

    premise->index = objIndex;
    return 0;
```

The rule checks trust the stored values:

```c
// src/rules.c, checkpremise()
    if (p->variable == r_TIME ||
        p->variable == r_CLOCKTIME) return (checktime(pr,p));
    else if (p->status > IS_NUMBER) return (checkstatus(pr,p));
    else return (checkvalue(pr,p));

// src/rules.c, checkvalue()
    i = p->index;
    v = p->variable;
    switch (v)
    {
      ...
      case r_HEAD:
      case r_GRADE:
        x = hyd->NodeHead[i] * Ucf[HEAD];
```

A status test reads `LinkStatus[p->index]`, whatever the premise's object is.

## How to reproduce

| File | What it is |
|---|---|
| [`CRASH-20_tank.inp`](CRASH-20_tank.inp) | Reservoir R1, junction J1 and tank T1 (3 nodes), pipes P1 and P2 (2 links) |
| [`CRASH-20_test.c`](CRASH-20_test.c) | Adds rule R1 (IF T1 level > 15 THEN close P2 ELSE open P2), makes the five setter calls above and runs `EN_solveH` |
| [`CRASH-20_fix.patch`](CRASH-20_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh CRASH-20            # unpatched EPANET: CRASH
tools/run-test.sh CRASH-20 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc CRASH-20_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o CRASH-20_test
./CRASH-20_test
```

**Without the fix**, every call returns 0 and the first rule check reads past `NodeHead`:

```
EN_setpremise(node T1, EN_R_FLOW)              returned   0 (expected 251)
EN_setpremise(node 4, EN_R_LEVEL)              returned   0 (expected 203)
EN_setpremiseindex(node 4)                     returned   0 (expected 203)
EN_setthenaction(link 3)                       returned   0 (expected 204)
EN_setelseaction(link 3)                       returned   0 (expected 204)
=================================================================
==...==ERROR: AddressSanitizer: heap-buffer-overflow on address ... at pc ... bp ... sp ...
READ of size 8 at ... thread T0
    #0 ... in checkpremise rules.c:984
    #1 ... in checkrules rules.c:548
    #2 ... in ruletimestep hydraul.c:880
    #3 ... in timestep hydraul.c:682
...
0x... is located 0 bytes after 32-byte region [...)
allocated by thread T0 here:
    ...
    #1 ... in allocdata project.c:396
```

`project.c:396` allocates `NodeHead`; `checkvalue()` is inlined into `checkpremise()`. A build without sanitizers (macOS, Release) does not crash on this small overrun: the run completes and the test prints `FAIL: 5 of 5 calls accepted an invalid premise or action`.

**With the fix:**

```
EN_setpremise(node T1, EN_R_FLOW)              returned 251 (expected 251)
EN_setpremise(node 4, EN_R_LEVEL)              returned 203 (expected 203)
EN_setpremiseindex(node 4)                     returned 203 (expected 203)
EN_setthenaction(link 3)                       returned 204 (expected 204)
EN_setelseaction(link 3)                       returned 204 (expected 204)
EN_solveH returned 0
PASS: invalid premises and actions are rejected
```

## The fix

Each setter now checks what it stores, after the existing rule and clause checks, using the error codes of the rest of the API (203 undefined node, 204 undefined link, 251 invalid parameter code):

- `EN_setpremise`: `logop` 1 to 3, `relop` `EN_R_EQ` to `EN_R_ABOVE`, `status` 0 to `EN_R_IS_ACTIVE`, and a status only for a link. The object must be `EN_R_NODE`, `EN_R_LINK` or `EN_R_SYSTEM`, the index must exist, and the variable must be one the parser accepts for that object:

  ```c
  case EN_R_NODE:
      if (objIndex < 1 || objIndex > p->network.Nnodes) return 203;
      if (variable < EN_R_DEMAND || variable > EN_R_DRAINTIME) return 251;
      if (variable > EN_R_PRESSURE && variable < EN_R_FILLTIME) return 251;
      break;
  case EN_R_LINK:
      if (objIndex < 1 || objIndex > p->network.Nlinks) return 204;
      if (variable < EN_R_FLOW || variable > EN_R_SETTING) return 251;
      break;
  case EN_R_SYSTEM:
      if (variable != EN_R_DEMAND && variable != EN_R_TIME &&
          variable != EN_R_CLOCKTIME) return 251;
      break;
  ```

- `EN_setpremiseindex`: the index must exist for the premise's object.
- `EN_setpremisestatus`: `status` 0 to `EN_R_IS_ACTIVE`, and a status only on a link premise.
- `EN_setthenaction` and `EN_setelseaction`: the link must exist, and an action with no setting (`EN_MISSING`) needs a status of `EN_R_IS_OPEN` to `EN_R_IS_ACTIVE`, since `EN_saveinpfile` writes that status as a keyword.

`EN_setpremisestatus` was not in the original finding. It is included because a status on a node premise makes `checkstatus()` read `LinkStatus` with a node index.

`EN_R_POWER` is now refused for a link premise, as the parser refuses it (`checkvalue()` has no case for it, so such a premise was never true). The setters still accept a few things the parser rejects without any memory risk: a fill or drain time on a junction, and an action on a check-valve pipe (error 207 in the parser, see [API-03](../../3-api/API-03-setlinktype-cv-keeps-controls/)).

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [CRASH-14](../CRASH-14-calls-after-close/) adds an `Openflag` check at the top of the same setters. The two patches change different lines and apply together.
- The input-file parser has a similar gap, found while checking this fix: `newpremise()` accepts a status value for any variable, so `IF TANK T1 LEVEL IS OPEN` loads, and the rule check then reads `LinkStatus` with the tank's node index. That is [CRASH-26](../CRASH-26-rule-status-on-node-variable/).
