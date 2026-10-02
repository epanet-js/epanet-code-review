# FILE-08: Strings with spaces are written unquoted

| | |
|---|---|
| **Category** | [Input files, saved files and the output file](../README.md) |
| **Impact** | A chemical name, tag or file name with a space is split when the saved file is read: "Free Chlorine" reloads as chemical "Free" with units "Chlorine". |
| **Reached from** | Toolkit API: `EN_setqualtype`, `EN_settag` (and file names set in `[OPTIONS] HYDRAULICS` or `[REPORT] FILE`), then `EN_saveinpfile` and reopening the file |
| **Where** | `saveinpfile()` in [`src/inpfile.c:712-713`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L712-L713) (chemical), [`:847`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L847) and [`:853`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L853) (tags), [`:694`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L694), [`:697`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L697) and [`:771`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L771) (file names) |
| **Since** | Chemical and file names: v2.0.12 (2014). Tags: since they were added in commit [7a16739](https://github.com/OpenWaterAnalytics/EPANET/commit/7a1673994ccdae9fac9b29b1bcccaebd77b3baba) (February 2025). |
| **GitHub** | No related issue |
| **Fix** | A small helper and 6 calls to it: [`FILE-08_fix.patch`](FILE-08_fix.patch) |

## The problem

The toolkit accepts strings that contain spaces in several places, for example `EN_setqualtype(ph, EN_CHEM, "Free Chlorine", "mg/L", "")` or `EN_settag(ph, EN_NODE, j, "Pressure zone 2")`. `EN_saveinpfile` writes them as they are, as `QUALITY Free Chlorine mg/L` and `NODE J1 Pressure zone 2`.

The reader splits lines into tokens at blanks, so the reloaded project has the chemical name "Free" with units "Chlorine", and the tag "Pressure". The same happens to the file names in `HYDRAULICS SAVE`/`USE` and `[REPORT] FILE`. The tokenizer already reads a token in double quotes as one token, so the writer only has to quote.

## Why it happens

```c
// src/inpfile.c, saveinpfile()
case CHEM:
  fprintf(f, "\n QUALITY             %s %s",
          qual->ChemName, qual->ChemUnits);
...
fprintf(f, "\n %-8s\t%-31s\t%s", "NODE", node->ID, node->Tag);
```

```c
// src/input2.c, gettokens()
if (*s == '"')              // Token begins with quote
{
    s++;                           // Start token after quote
    m = (int)strcspn(s,"\"\n\r");  // Find end quote (or EOL)
}
```

## How to reproduce

| File | What it is |
|---|---|
| [`FILE-08_network.inp`](FILE-08_network.inp) | A reservoir and two junctions |
| [`FILE-08_test.c`](FILE-08_test.c) | Sets the chemical to "Free Chlorine" in "mg/L" and J1's tag to "Pressure zone 2", saves the project, prints the lines written, reopens the file and reads the strings back |
| [`FILE-08_fix.patch`](FILE-08_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh FILE-08            # unpatched EPANET: FAIL
tools/run-test.sh FILE-08 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc FILE-08_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o FILE-08_test
./FILE-08_test
```

**Without the fix** (the test shortens runs of blanks and tabs in the saved lines to one space):

```
Lines written:
  QUALITY Free Chlorine mg/L
  NODE J1 Pressure zone 2

After reloading:
  chemical name  'Free'  (expected 'Free Chlorine')
  chemical units 'Chlorine'  (expected 'mg/L')
  tag of J1      'Pressure'  (expected 'Pressure zone 2')
FAIL: strings with spaces were split into separate tokens on reload
```

**With the fix**:

```
Lines written:
  QUALITY "Free Chlorine" mg/L
  NODE J1 "Pressure zone 2"

After reloading:
  chemical name  'Free Chlorine'  (expected 'Free Chlorine')
  chemical units 'mg/L'  (expected 'mg/L')
  tag of J1      'Pressure zone 2'  (expected 'Pressure zone 2')
PASS: strings with spaces survive saving and reloading
```

## The fix

A helper quotes a string when it contains a space or a tab, and the six places that write free-form strings call it:

```c
static char *quotestr(const char *s, char *buf)
{
    if (strpbrk(s, " \t") == NULL) return (char *)s;
    sprintf(buf, "\"%s\"", s);
    return buf;
}
```

```diff
           fprintf(f, "\n QUALITY             %s %s",
-                  qual->ChemName, qual->ChemUnits);
+                  quotestr(qual->ChemName, s1), quotestr(qual->ChemUnits, s2));
```

Strings without blanks are written exactly as before, so existing files and the unit tests are unaffected. IDs set through the API cannot contain spaces (`namevalid()` rejects them), and comments, category names and titles are written after `;` or on lines of their own, so they need no quotes. A string that contains a double quote or a `;` still cannot be saved; the reader has no escape for either.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

The tokenizer has a bug with quoted tokens, [FILE-14](../FILE-14-quoted-tokens-mis-split/): after a quoted token that contains a space, it miscounts the length of the rest of the line. For the lines this patch writes, where the quoted string is last or followed by one short token, the tokens still come out right at 83e25bb, as the test shows. FILE-14's fix should go in with this one, so that any quoted token is read reliably.
