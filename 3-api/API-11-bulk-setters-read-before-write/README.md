# API-11: Bulk setters read every node before writing, so source and tank properties cannot be set

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | `EN_setnodevalues` fails at node 1 for every tank-only property and every quality-source property, on any network, so those properties cannot be bulk-set at all. |
| **Reached from** | Toolkit API: `EN_setnodevalues` with `EN_TANKLEVEL`, `EN_TANKDIAM`, `EN_MINVOLUME`, `EN_MINLEVEL`, `EN_MAXLEVEL`, `EN_MIXMODEL` and the other tank properties, or with `EN_SOURCEQUAL`, `EN_SOURCEPAT` or `EN_SOURCETYPE` |
| **Where** | `EN_setnodevalues()` in [`src/epanet.c:2802-2827`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2802-L2827) |
| **Since** | [PR #908](https://github.com/OpenWaterAnalytics/EPANET/pull/908) (March 2026), which is not in a release yet |
| **GitHub** | No related issue. Introduced by [PR #908](https://github.com/OpenWaterAnalytics/EPANET/pull/908). |
| **Fix** | About 30 lines in the two bulk setters: [`API-11_fix.patch`](API-11_fix.patch), on top of [CRASH-15](../../2-crashes/CRASH-15-bulk-setters-null-badindex/) |

## The problem

`EN_setnodevalues(property, values, badIndex)` takes one value for every node. For each node in turn it reads the current value (to be able to undo), writes the new one, and on the first error undoes everything and returns the error with `badIndex` set.

Junctions always come first in the node list, so node 1 is a junction in practically every network. That stops two groups of properties at node 1:

- **Tank properties.** `EN_setnodevalue` rejects them for a junction: with 263 ("node is not a tank"), or with 209 when the junction's entry is a value the setter checks first, such as the diameter 0 that `EN_getnodevalues(EN_TANKDIAM)` returns for a junction. Setting new diameters for the two tanks in the test returns 209 with `badIndex` 1 and changes nothing.
- **Source properties.** `EN_getnodevalue` returns 240 for a node without a quality source, so the read fails before anything is written. `EN_setnodevalue` would create the source, but is never called. Setting source strengths returns 240 with `badIndex` 1.

So the natural use fails for every tank property: read all values with `EN_getnodevalues`, change the tank entries, and write them back.

## Why it happens

```c
// src/epanet.c, EN_setnodevalues()
for (i = 1; i <= n; i++)
{
    errcode = EN_getnodevalue(p, i, property, &old[i - 1]);
    if (errcode != 0)                 // 240: node i has no source
    {
        *badIndex = i;
        j = i - 1;        // Need to restore values for nodes 1 to i-1
    }
    else
    {
        errcode = EN_setnodevalue(p, i, property, values[i - 1]);
        if (errcode != 0)             // 263 or 209: node i is a junction
        {
            ...
```

Every node is written, even where the property does not apply, and a missing source counts as an error. The undo has a related weakness, found by reading the code: it restores each node by calling the setter again, so it repeats the setter's side effects. Restoring `EN_TANKDIAM` removes a tank's volume curve, restoring `EN_MINVOLUME` with 0 recomputes the minimum volume, and restoring `EN_TANKLEVEL` on a reservoir writes 0 as its head (see [API-18](../API-18-tanklevel-returns-initial-level/)).

## How to reproduce

| File | What it is |
|---|---|
| [`API-11_network.inp`](API-11_network.inp) | Junctions J1 and J2 (nodes 1 and 2), reservoir R1 (3), tanks T1 and T2 (4 and 5), no quality sources |
| [`API-11_test.c`](API-11_test.c) | Calls `EN_setnodevalues` with new tank diameters, with a source pattern that is invalid at node 3, and with source strengths for every node |
| [`API-11_fix.patch`](API-11_fix.patch) | The fix (apply [`CRASH-15_fix.patch`](../../2-crashes/CRASH-15-bulk-setters-null-badindex/CRASH-15_fix.patch) first) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-11            # unpatched EPANET: FAIL
tools/run-test.sh API-11 --patched  # with CRASH-15 and API-11: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-11_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-11_test
./API-11_test
```

**Without the fix**, all three calls stop at node 1:

```
A. EN_TANKDIAM:   returned 209, badIndex 1; T1 50.0 ft, T2 50.0 ft   (expected 0, 0; 60.0, 70.0)
B. EN_SOURCEPAT:  returned 240, badIndex 1; nodes with a source: 0   (expected 205, 3; 0)
C. EN_SOURCEQUAL: returned 240, badIndex 1; J2 0.0, T2 0.0               (expected 0, 0; 1.0, 2.5)
FAIL: 3 of 3 bulk assignments did not behave as expected
```

**With the fix:**

```
A. EN_TANKDIAM:   returned   0, badIndex 0; T1 60.0 ft, T2 70.0 ft   (expected 0, 0; 60.0, 70.0)
B. EN_SOURCEPAT:  returned 205, badIndex 3; nodes with a source: 0   (expected 205, 3; 0)
C. EN_SOURCEQUAL: returned   0, badIndex 0; J2 1.0, T2 2.5               (expected 0, 0; 1.0, 2.5)
PASS: tank and source properties can be bulk-set, and a failure is undone
```

In B, node 3 (the reservoir) gets a new source before its pattern index is rejected; the undo removes it again.

## The fix

Two rules, both in the existing loop:

1. **Only values that change are written** (and only those are restored on an error). The entries that `EN_getnodevalues` returns for nodes a property does not apply to are therefore left alone, and an unchanged node no longer goes through the setter's side effects.
2. **A node without a source has none to save.** It is marked `MISSING`, gets a source only if its new value is not 0, and the undo removes any source the call created.

```diff
         errcode = EN_getnodevalue(p, i, property, &old[i - 1]);
+
+        // A node without a quality source has no source value to save
+        // (error 240). Mark it as MISSING; it only gets a source if its
+        // new value is not 0.
+        if (errcode == 240)
+        {
+            errcode = 0;
+            old[i - 1] = MISSING;
+            if (values[i - 1] == 0.0) continue;
+        }
         if (errcode != 0)
         {
             if (badIndex) *badIndex = i;
             j = i - 1;        // Need to restore values for nodes 1 to i-1
         }
-        else
+        else if (values[i - 1] != old[i - 1])
         {
...
-                EN_setnodevalue(p, k, property, old[k - 1]);
+                if (old[k - 1] == MISSING)
+                {
+                    // Remove any source created by this call
+                    free(p->network.Node[k].S);
+                    p->network.Node[k].S = NULL;
+                }
+                else if (values[k - 1] != old[k - 1])
+                {
+                    EN_setnodevalue(p, k, property, old[k - 1]);
+                }
```

`EN_setlinkvalues` gets rule 1 too. Its type-specific setters mostly ignore links they do not apply to, but in the unpatched code `EN_INITSTATUS` on a check valve (207) and `EN_VALVE_TYPE` on a pipe (264) stop it in the same way. With rule 1, the values `EN_getlinkvalues` returns for those links pass through.

The patch is written on top of CRASH-15, which changes the same lines (`Requires: CRASH-15` in its header).

What changes for callers, and alternatives:

- Passing a node's current value is now a no-op. Before, it re-ran the setter, which for example removed a tank's volume curve for `EN_TANKDIAM` or reset a tank's head for `EN_TANKLEVEL`. Passing the current values of a read-only property now returns 0 instead of 251.
- A value that does change is still rejected on a node it does not apply to (for example a non-zero diameter for a junction), with `badIndex` pointing at it.
- **Skip by node type instead.** The bulk setter could skip junctions for tank properties, with a list of which properties apply to which node types. That accepts any placeholder for junctions, but duplicates knowledge that lives in `EN_setnodevalue`.
- **Exact undo.** Rule 1 avoids the side effects of the undo for unchanged nodes only. An exact undo would snapshot and restore the node, tank and source data instead of calling the setter, which needs deep copies of the demand lists.
- **Documentation only.** State that tank and source properties must be set node by node. Since PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908) is unreleased, fixing the functions seems better.

EPANET's own unit tests (6 suites) pass with CRASH-15 and this patch.

## Notes

- Restoring `EN_TANKLEVEL` on a changed reservoir still writes 0 as its head, because `EN_getnodevalue(EN_TANKLEVEL)` returns 0 for a reservoir. The [API-18](../API-18-tanklevel-returns-initial-level/) patch makes the getter return the head, which fixes this.
