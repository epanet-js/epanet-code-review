# API-10: reindextanks shifts rule premises once per tank

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | Wrong results with no warning. After `EN_openX` loads a file with a bad junction line, every rule premise on a tank or reservoir points at a different node, so the rules test the wrong levels. |
| **Reached from** | `EN_openX` on an input file where a counted [JUNCTIONS] line did not create a junction (a duplicate ID, for example), and the file has rules on tanks or reservoirs |
| **Where** | `reindextanks()` in [`src/validate.c:396`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/validate.c#L396), which calls `adjusttankrules()` ([`src/rules.c:418-439`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L418-L439)) |
| **Since** | v2.3: `reindextanks()` was added with `EN_openX` in PR [#758](https://github.com/OpenWaterAnalytics/EPANET/pull/758) (2023) |
| **GitHub** | No related issue |
| **Fix** | Moves one line: [`API-10_fix.patch`](API-10_fix.patch) |

## The problem

`EN_openX` opens an input file even if some lines have errors, so that a user can repair the network. The Node array is sized from the number of junction lines in the file. If a junction line fails, for example because its ID is a duplicate, there are fewer junctions than slots, and `reindextanks()` moves every tank and reservoir down to close the gap.

It moves rule premises too, but once for every tank instead of once. In the test network one junction line fails and there are three tanks and reservoirs, so a premise on tank T2 (node 4) moves by 3 and ends up on reservoir R1 (node 2). The rule then tests R1's level instead of T2's. Nothing reports the change: `EN_openX` returns 200 for the duplicate ID, as expected.

## Why it happens

`adjusttankrules()` shifts the index of every tank premise in every rule. `reindextanks()` calls it inside the loop over tanks:

```c
// src/validate.c, reindextanks()
    ndiff = parser->MaxJuncs - net->Njuncs;
    if (ndiff > 0)
    {
        for (i = 1; i <= net->Ntanks; ++i)
        {
            ...
            for (j = 1; j <= net->Ncontrols; ++j)
            {
                control = &net->Control[j];
                if (control->Node == n1) control->Node = n2;
            }
            adjusttankrules(pr, -ndiff);
            if (qual->TraceNode == n1) qual->TraceNode = n2;
        }
```

The link ends, simple controls and trace node are updated for one tank at a time, which is correct. `adjusttankrules()` is not per tank, so each premise moves by `ndiff` times the number of tanks.

## How to reproduce

| File | What it is |
|---|---|
| [`API-10_duplicate-junction.inp`](API-10_duplicate-junction.inp) | Junction J1 is listed twice. Reservoir R1 and tanks T1 and T2 follow. Rule RA tests T2's level. |
| [`API-10_test.c`](API-10_test.c) | Opens the file with `EN_openX` and compares the node in rule RA's premise with T2's index |
| [`API-10_fix.patch`](API-10_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-10            # unpatched EPANET: FAIL
tools/run-test.sh API-10 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-10_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-10_test
./API-10_test
```

**Without the fix:**

```
EN_openX returned 200 (200: errors in the file, project still opened)
Tank T2 is node 4
Rule RA premise refers to node 2 (R1)
FAIL: the premise moved from T2 to R1
```

**With the fix:**

```
EN_openX returned 200 (200: errors in the file, project still opened)
Tank T2 is node 4
Rule RA premise refers to node 4 (T2)
PASS: the premise still refers to T2
```

## The fix

Call `adjusttankrules()` once, after the loop:

```diff
-            adjusttankrules(pr, -ndiff);
             if (qual->TraceNode == n1) qual->TraceNode = n2;
         }
+
+        // Shift tank indexes in rule premises (all tanks at once)
+        adjusttankrules(pr, -ndiff);
```

`adjusttankrules()` tests `p->index > Njuncs`, which holds for every tank premise before the shift, so one call moves each of them by `ndiff`.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [API-01](../API-01-addnode-shifts-wrong-rule-premises/) is a different mistake with the same function: `EN_addnode` calls `adjusttankrules()` after it has incremented `Njuncs`, so the premise on the first tank or reservoir is not shifted.
