# API-01: EN_addnode shifts rule premises after incrementing the junction count

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A rule premise on the first tank or reservoir moves to the new junction. The rule then tests the wrong node, with no error, and a saved file keeps the wrong node. |
| **Reached from** | Toolkit API: `EN_addnode` with `EN_JUNCTION`, in a project with a rule premise on the first tank or reservoir (the node right after the last junction) |
| **Where** | `EN_addnode()` in [`src/epanet.c:1967`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1967) and [`:1991`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1991); `adjusttankrules()` in [`src/rules.c:428-434`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/rules.c#L428-L434) |
| **Since** | v2.2, the first release with `EN_addnode` |
| **GitHub** | No related issue |
| **Fix** | One call moved: [`API-01_fix.patch`](API-01_fix.patch) |

## The problem

EPANET stores junctions first in its node list, then tanks and reservoirs. `EN_addnode` puts a new junction at index `Njuncs + 1` and moves every tank and reservoir up one place. Links, simple controls and rule premises that refer to a tank or reservoir must move with it.

Rule premises do not all move. The premise on the first tank or reservoir keeps its old index, which is now the new junction's index. In the test network, the rule

```
IF RESERVOIR R1 HEAD > 100 AND TANK T1 LEVEL > 15 THEN PIPE P3 STATUS IS CLOSED
```

tests the head of the new junction J3 after `EN_addnode(ph, "J3", EN_JUNCTION, ...)`. Nothing reports it. `EN_saveinpfile` then writes the rule as `IF JUNCTION J3 HEAD > 100.0000`. Premises on the other tanks and reservoirs move correctly.

## Why it happens

`adjusttankrules()` shifts the premises whose node index is greater than `Njuncs`. `EN_addnode` calls it at the end of the junction branch, after it has already incremented `Njuncs`. The first tank or reservoir had index `Njuncs + 1` before the call, which equals the new `Njuncs`, so its premise fails the `>` test. The link and control loops in the same branch allow for this by testing `> net->Njuncs - 1`; the rule call does not.

```c
// src/epanet.c, EN_addnode()
        // set index of new Junction node
        net->Njuncs++;
        ...
        // shift indices of Links, if necessary
        for (i = 1; i <= net->Nlinks; i++)
        {
            if (net->Link[i].N1 > net->Njuncs - 1) net->Link[i].N1 += 1;
            ...
        }
        ...
        // adjust indices of tanks/reservoirs in Rule premises (see RULES.C)
        adjusttankrules(p, 1);

// src/rules.c, adjusttankrules()
    njuncs = net->Njuncs;
    ...
            if (p->object == r_NODE && p->index > njuncs)
                p->index += ndiff;
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-01_tank-rule.inp`](API-01_tank-rule.inp) | Junctions J1 and J2 (nodes 1 and 2), reservoir R1 (node 3), tank T1 (node 4), and one rule with a premise on R1 and one on T1 |
| [`API-01_test.c`](API-01_test.c) | Adds junction J3 with `EN_addnode` and prints the node each premise refers to, before and after |
| [`API-01_fix.patch`](API-01_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-01            # unpatched EPANET: FAIL
tools/run-test.sh API-01 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-01_test
./API-01_test
```

**Without the fix**, premise 1 stays at node 3, which is now J3:

```
Before adding a junction:
  premise 1 refers to node 3 (R1), expected R1
  premise 2 refers to node 4 (T1), expected T1
After adding junction J3 (now node 3):
  premise 1 refers to node 3 (J3), expected R1
  premise 2 refers to node 5 (T1), expected T1
FAIL: after EN_addnode the rule tests the head of the new junction instead of reservoir R1
```

**With the fix**, both premises move up one place:

```
After adding junction J3 (now node 3):
  premise 1 refers to node 4 (R1), expected R1
  premise 2 refers to node 5 (T1), expected T1
PASS: both rule premises still refer to R1 and T1
```

## The fix

Call `adjusttankrules()` before `Njuncs` is incremented, while every tank and reservoir index is still greater than `Njuncs`:

```diff
+        // adjust indices of tanks/reservoirs in Rule premises (see RULES.C)
+        // (before Njuncs is incremented, since it shifts indices > Njuncs)
+        adjusttankrules(p, 1);
+
         // set index of new Junction node
         net->Njuncs++;
 ...
-        // adjust indices of tanks/reservoirs in Rule premises (see RULES.C)
-        adjusttankrules(p, 1);
```

`adjusttankrules()` itself is unchanged, so its other caller, `reindextanks()` in `validate.c`, is not affected. EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [API-02](../API-02-addnode-trace-node-not-shifted/) is the same kind of bug in the same branch of `EN_addnode`: the water quality trace node is not shifted at all. [API-10](../API-10-reindextanks-shifts-premises-repeatedly/) is another `adjusttankrules()` problem, in `reindextanks()`. The three patches apply together.
- The rule premises only point at nodes. Rule actions refer to links, and links keep their indices when a node is added.
