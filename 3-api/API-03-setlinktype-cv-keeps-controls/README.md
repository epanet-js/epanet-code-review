# API-03: EN_setlinktype to CVPIPE keeps the link's controls and rules

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A pipe given a check valve stays the target of simple controls and rule actions, which EPANET forbids for check valves. A file saved afterwards cannot be opened (error 200, with error 207 in the report). |
| **Reached from** | Toolkit API: `EN_setlinktype(..., EN_CVPIPE, EN_UNCONDITIONAL)` on a pipe that a simple control or a rule action sets |
| **Where** | The pipe/check-valve fast path of `EN_setlinktype()` in [`src/epanet.c:3773-3778`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L3773-L3778) |
| **Since** | v2.2 (the fast path dates from January 2019) |
| **GitHub** | No related issue |
| **Fix** | About 20 lines: [`API-03_fix.patch`](API-03_fix.patch) |

## The problem

A check valve cannot be controlled. The input file parser rejects a control or rule action on a CV pipe with error 207, and so do `EN_addcontrol`, `EN_setcontrol` and `EN_addrule`. The documentation of `EN_setlinktype` says:

> If `actionCode` is `EN_UNCONDITIONAL` then all simple and rule-based controls that contain the link are deleted when the link's type is changed.

For a change between `EN_PIPE` and `EN_CVPIPE` nothing is deleted. In the test network, pipe P2 is closed by a simple control and by the action of rule 1. After `EN_setlinktype(ph, &p2, EN_CVPIPE, EN_UNCONDITIONAL)` returns 0, both still set P2. `EN_saveinpfile` writes them out, and opening the saved file fails with error 200:

```
Error 207: attempt to control CV/GPV link P2 in [CONTROLS] section:
Input Error 207: attempt to control a CV in following line of Rule 1:
```

With `EN_CONDITIONAL` the call correctly returns 261.

## Why it happens

Every other type change deletes the link and adds a new one, and `EN_deletelink` removes the controls and rules that contain the link. A change between pipe and CV pipe keeps the link and its data, so it takes a shortcut that only changes the type:

```c
// src/epanet.c, EN_setlinktype()
    // Type change will be cancelled if link appears in any controls
    if (actionCode == EN_CONDITIONAL)
    {
        actionCode = incontrols(p, LINK, i);
        if (actionCode > 0) return 261;
    }

    // Pipe changing from or to having a check valve
    if (oldType <= PIPE && linkType <= PIPE)
    {
        net->Link[i].Type = linkType;
        if (linkType == CVPIPE) net->Link[i].InitStatus = OPEN;
        return 0;
    }
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-03_controlled-pipe.inp`](API-03_controlled-pipe.inp) | Pipe P2 is closed by a simple control at 2 h and by rule 1. Rule 2 tests P2's flow in its premise and sets P1. |
| [`API-03_test.c`](API-03_test.c) | Gives P2 a check valve, counts the controls and rule actions that still set it, saves the project and opens the saved file |
| [`API-03_fix.patch`](API-03_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-03            # unpatched EPANET: FAIL
tools/run-test.sh API-03 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-03_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-03_test
./API-03_test
```

**Without the fix**, the control and rule 1 stay, and the saved file does not open:

```
Before the type change:
  1 control(s), 2 rule(s); 2 of them set P2
EN_setlinktype(P2, EN_CVPIPE, EN_UNCONDITIONAL) returned 0
  1 control(s), 2 rule(s); 2 of them set P2
Opening the saved file returned 200
    Error 207: attempt to control CV/GPV link P2 in [CONTROLS] section:
    Input Error 207: attempt to control a CV in following line of Rule 1:
FAIL: 2 control(s) or rule action(s) still set check valve pipe P2, and opening the saved file returned 200
```

**With the fix**, the control and rule 1 are deleted, rule 2 is kept, and the saved file opens:

```
EN_setlinktype(P2, EN_CVPIPE, EN_UNCONDITIONAL) returned 0
  0 control(s), 1 rule(s); 0 of them set P2
Opening the saved file returned 0
PASS: no control or rule sets the check valve pipe, and the saved file opens
```

## The fix

Before the fast path, when a pipe is given a check valve, delete the simple controls whose link is the pipe and the rules with a THEN or ELSE action on it:

```diff
+    // A check valve cannot be controlled, so delete any simple controls
+    // and rules that set a pipe which is given one
+    if (oldType == PIPE && linkType == CVPIPE)
+    {
+        for (k = net->Ncontrols; k >= 1; k--)
+        {
+            if (net->Control[k].Link == i) EN_deletecontrol(p, k);
+        }
+        for (k = net->Nrules; k >= 1; k--)
+        {
+            a = net->Rule[k].ThenActions;
+            while (a != NULL && a->link != i) a = a->next;
+            if (a == NULL)
+            {
+                a = net->Rule[k].ElseActions;
+                while (a != NULL && a->link != i) a = a->next;
+            }
+            if (a != NULL) deleterule(p, k);
+        }
+    }
```

This runs only with `EN_UNCONDITIONAL`: with `EN_CONDITIONAL` the call has already returned 261 if the link is in any control. A whole rule is deleted, as `EN_deletelink` does, because a rule without that action would mean something else. EPANET's own unit tests (6 suites) pass with the patch.

There are two other ways to fix this; a maintainer may prefer either:

- **Keep the documented promise literally.** Delete every control and rule that contains the link, including rules that only test it in a premise, and do so for CV pipe to pipe as well. That matches the other type changes, but it deletes rules such as rule 2 in the test, which are still valid for a check valve.
- **Refuse the change.** Return 207 when the pipe is set by a control. Simple, but it breaks the documented `EN_UNCONDITIONAL` behaviour.

The patch takes the narrow route: it removes only what a check valve cannot have. If it is accepted, the `EN_setlinktype` documentation could say that a change between `EN_PIPE` and `EN_CVPIPE` keeps rules that only test the link.

## Notes

- [API-05](../API-05-stale-hydraulics-after-edit/) also edits this fast path (it clears the saved-hydraulics flag before `return 0`). The two patches apply together.
- [API-07](../API-07-deletelink-cv-pipe-count/) is another place where `CVPIPE` is left out of a test written for `PIPE`.
