# FILE-06: Demand categories with zero base demand are dropped on save

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | Saving drops every zero-demand category, with its name and pattern. A named placeholder category disappears, and `EN_getnumdemands` changes after reloading. Results do not change. |
| **Reached from** | Toolkit API: `EN_saveinpfile` on a project with zero-demand categories |
| **Where** | `saveinpfile()` in [`src/inpfile.c:344`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L344) |
| **Since** | v2.3, by design: PR [#702](https://github.com/OpenWaterAnalytics/EPANET/pull/702) (commit [f601021](https://github.com/OpenWaterAnalytics/EPANET/commit/f60102105aa8fc2aa82be549e60c20216873fba9), January 2023) for issue [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174) |
| **GitHub** | [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174), implemented by [#702](https://github.com/OpenWaterAnalytics/EPANET/pull/702) |
| **Fix** | A proposal, 4 lines: [`FILE-06_fix.patch`](FILE-06_fix.patch) |

## The problem

This is behaviour that was chosen on purpose, and this item proposes a narrower rule.

Issue [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174) (2018) asked `EN_saveinpfile` to stop writing zero demands, which add nothing to the file. PR [#702](https://github.com/OpenWaterAnalytics/EPANET/pull/702) (2023) did this by skipping every category whose base demand is 0. A category also has a name and a pattern, and those are lost with it:

- A model that keeps a named zero-demand category as a placeholder, for example a "Fire flow" category with its own pattern, loses the category. Its junction has one category fewer after reloading.
- A zero category whose only content is its pattern loses the pattern.

The simulation results are the same, because the demand is zero. What changes is the model the user saved, without any message.

In [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174), eladsal suggested keeping zero demands for categories "unless there is only one category with no name". In the review of [#702](https://github.com/OpenWaterAnalytics/EPANET/issues/702) he asked whether a second zero-demand category would still be written; the test that followed showed it is not, and the change was merged.

## Why it happens

```c
// src/inpfile.c, saveinpfile(), [DEMANDS]
for (demand = node->D; demand != NULL; demand = demand->next)
{
    if (demand->Base == 0.0) continue;
    sprintf(s, " %-31s\t%-14.6f", node->ID, ucf * demand->Base);
    if ((j = demand->Pat) > 0) sprintf(s1, "%-31s", net->Pattern[j].ID);
    ...
    if (demand->Name) fprintf(f, "\t;%s", demand->Name);
}
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-06_categories.inp`](FILE-06_categories.inp) | J1 has a 10 gpm "Domestic" category and a zero "Fire flow" category with pattern FIRE. J2 has a zero category with no name or pattern. J3 has a zero category with pattern FIRE and no name. |
| [`FILE-06_test.c`](FILE-06_test.c) | Saves the project, reopens it, and lists each junction's categories before and after |
| [`FILE-06_fix.patch`](FILE-06_fix.patch) | The proposed change |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-06            # unpatched EPANET: FAIL
tools/run-test.sh FILE-06 --patched  # with the proposed change: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-06_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-06_test
./FILE-06_test
```

**Without the change**, J1 loses its "Fire flow" category and J3 loses its pattern:

```
Input file:
  J1: 2 categories  [10 gpm, pattern 'DAILY', name 'Domestic']  [0 gpm, pattern 'FIRE', name 'Fire flow']
  J2: 1 category  [0 gpm, pattern '', name '']
  J3: 1 category  [0 gpm, pattern 'FIRE', name '']
Saved and reloaded:
  J1: 1 category  [10 gpm, pattern 'DAILY', name 'Domestic']
  J2: 1 category  [0 gpm, pattern '', name '']
  J3: 1 category  [0 gpm, pattern '', name '']
FAIL: zero-demand categories lost their name or pattern (J1 keeps 1 of 2 categories, J3's pattern is '')
```

**With the change**:

```
...
Saved and reloaded:
  J1: 2 categories  [10 gpm, pattern 'DAILY', name 'Domestic']  [0 gpm, pattern 'FIRE', name 'Fire flow']
  J2: 1 category  [0 gpm, pattern '', name '']
  J3: 1 category  [0 gpm, pattern 'FIRE', name '']
PASS: zero-demand categories with a name or pattern are kept
```

J2's category is not written in either case. On reload J2 gets the category the parser creates from its `[JUNCTIONS]` line, which is the same: zero demand, no pattern, no name.

## The fix

The patch is a proposal. It keeps the purpose of [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174) (no lines for zero demands that carry no information) and skips a zero-demand category only when it has neither a name nor a pattern:

```diff
-            if (demand->Base == 0.0) continue;
+            // Skip a zero demand unless it has a name or a pattern
+            // (a placeholder category the user wants to keep)
+            if (demand->Base == 0.0 && demand->Pat == 0 &&
+                (demand->Name == NULL || demand->Name[0] == '\0')) continue;
```

A pattern here means one assigned to the category; a category with no pattern of its own follows the default pattern. Files saved from models without named or patterned zero categories, such as Net1, Net2 and Net3, are unchanged.

The alternatives are to keep the current rule and document that zero-demand categories are not saved, or to write every category, as before [#702](https://github.com/OpenWaterAnalytics/EPANET/issues/702). The second keeps the model exactly but brings back the redundant lines that [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174) removed.

EPANET's own unit tests (6 suites) pass with the patch.
