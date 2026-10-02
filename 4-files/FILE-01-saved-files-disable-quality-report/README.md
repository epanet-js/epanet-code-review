# FILE-01: Every saved file turns off the node Quality column when reloaded

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A model saved with `EN_saveinpfile` and opened again has no water-quality column in its report. Any `F-Factor` report setting is also lost. |
| **Reached from** | Toolkit API: `EN_saveinpfile` on any project, then reopening the file |
| **Where** | `saveinpfile()` in [`src/inpfile.c:821-837`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L821-L837); field names in [`src/text.h:302`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/text.h#L302) and [`:308`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/text.h#L308); reader in `reportdata()`, [`src/input3.c:1725-1730`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L1725-L1730) |
| **Since** | v2.0.12 (2014), the first release in this repository |
| **GitHub** | No related issue |
| **Fix** | A loop bound and one `continue`, plus a comment: [`FILE-01_fix.patch`](FILE-01_fix.patch) |

## The problem

The `[REPORT]` section of a saved file has one line per report field. For node quality, `EN_saveinpfile` writes the right setting, for example `Quality PRECISION 2`, and then writes a second line, `Quality NO`. The second line belongs to the link-quality field, but the reader takes it as the node field. When the saved file is opened again, node quality is switched off and the report has no quality column.

This happens for every saved file that reports quality, including Net1, Net2 and Net3 saved unchanged. A GUI or script that saves a model and reopens it loses the column without any message.

The same loop stops one field short, so a `F-Factor` setting (the link friction factor, which the reader accepts) is never written either.

## Why it happens

The report fields are indexed by `FieldType`. Node quality is `QUALITY` and link quality is `LINKQUAL`, and both are named "Quality":

```c
// src/text.h
#define   t_QUALITY     "Quality"
...
#define   t_LINKQUAL    "Quality"
```

The writer loops over every field below `FRICTION`, which includes `LINKQUAL`. That field is never enabled, so it is always written as `NO`:

```c
// src/inpfile.c, saveinpfile()
for (i = 0; i < FRICTION; i++)
{
    SField *field = &rpt->Field[i];
    if (field->Enabled == TRUE)
    {
        fprintf(f, "\n %-20sPRECISION %d", field->Name, field->Precision);
        ...
    }
    else fprintf(f, "\n %-20sNO",field->Name);
}
```

The reader looks the name up with `findmatch()`, which returns the first match, `QUALITY`, so `Quality NO` turns off node quality. `LINKQUAL` cannot be set from a file at all, because its name always resolves to the node field. The reader accepts every field up to and including `FRICTION`:

```c
// src/input3.c, reportdata()
else i = findmatch(parser->Tok[0], Fldname);
if (i >= 0)
{
    if (i > FRICTION) return setError(parser, 0, 213);
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-01_chlorine.inp`](FILE-01_chlorine.inp) | Reservoir R1 supplies J1 and J2. Chlorine is modelled, and `[REPORT]` adds `F-Factor Yes` to the default columns. |
| [`FILE-01_test.c`](FILE-01_test.c) | Writes a report for the file, saves the project, writes a report for the saved file, and checks for the "Chlorine" and "F-Factor" columns in each |
| [`FILE-01_fix.patch`](FILE-01_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-01            # unpatched EPANET: FAIL
tools/run-test.sh FILE-01 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-01_test
./FILE-01_test
```

**Without the fix**, the saved file switches node quality on and then off again, and has no `F-Factor` line:

```
[REPORT] lines in the saved file:
   Quality             PRECISION 2
   Quality             NO

Report column        original file   saved and reloaded
Chlorine (nodes)     yes             no
F-Factor (links)     yes             no
FAIL: after saving and reloading, the report loses the Chlorine column and the F-Factor column
```

**With the fix**:

```
[REPORT] lines in the saved file:
   Quality             PRECISION 2
   F-Factor            PRECISION 3

Report column        original file   saved and reloaded
Chlorine (nodes)     yes             yes
F-Factor (links)     yes             yes
PASS: the saved file reports the same columns as the original
```

## The fix

Skip `LINKQUAL`, which no input file can address, and include `FRICTION`:

```diff
     // Field formatting options
-    for (i = 0; i < FRICTION; i++)
+    // (LINKQUAL is skipped: it has the same name as node QUALITY,
+    // which is the field the reader assigns that name to)
+    for (i = 0; i <= FRICTION; i++)
     {
         SField *field = &rpt->Field[i];
+        if (i == LINKQUAL) continue;
```

No input file can turn `LINKQUAL` on, so skipping it loses nothing. Giving the link field its own name, and teaching the reader that name, would let files enable a link-quality column. That would be a new feature rather than this fix.

EPANET's own unit tests (6 suites) pass with the patch.
