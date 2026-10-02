# Input files, saved files and the output file

[All issues](../README.md)

Most of these were found by opening a file, saving it, reopening the saved file, saving again, and comparing the two saved files and about 70 properties per object. A saved file should reload to the same model; FILE-01 to FILE-08 break that. FILE-09 is in the output-file reader, and FILE-10 to FILE-19 are in the input-file parser.

| ID | Issue | What happens | Fix |
|---|---|---|---|
| [FILE-01](FILE-01-saved-files-disable-quality-report/) | Every saved file turns off the node Quality column when reloaded | A saved file writes `Quality NO` after the node quality setting, so the reloaded model's report has no quality column. | Skip `LINKQUAL` in the `[REPORT]` writer loop and include `FRICTION` |
| [FILE-02](FILE-02-save-over-input-loses-labels/) | Saving over the input file loses [LABELS] and [BACKDROP] | `EN_saveinpfile` to the project's own input file deletes its map labels and backdrop, and returns 0. | Copy both sections to a scratch file before truncating the output |
| [FILE-03](FILE-03-filltime-rules-not-reloadable/) | FILLTIME and DRAINTIME rule values are written in a form the parser rejects | A saved rule testing a tank's FILLTIME or DRAINTIME is written as h:mm:ss; the parser wants hours, so the file will not open. | Write `FILLTIME` and `DRAINTIME` values as decimal hours |
| [FILE-04](FILE-04-pumps-gpvs-without-curve-not-reloadable/) | Pumps and GPVs without a curve are written in a form the parser rejects | A pump with no curve or power, or a GPV with no curve, saves without error but the file will not reopen. | Write `PU1 R1 J1` (plus PATTERN/SPEED) and a GPV line without setting |
| [FILE-05](FILE-05-timer-controls-lose-a-second/) | Timer controls lose one second per save | Control times are truncated when read, so 45 % of saved timer controls reload a second early; some clock times already are. | Round instead of truncate in `controldata()`: `(long)(3600.0 * time + 0.5)` |
| [FILE-06](FILE-06-zero-demand-categories-dropped/) | Demand categories with zero base demand are dropped on save | Saving drops every zero-demand category with its name and pattern, so named placeholder categories vanish and category counts change. | Proposal: keep zero categories that have a name or pattern |
| [FILE-07](FILE-07-four-decimal-formats-truncate/) | Fixed four-decimal formats truncate small values | Saved files round values to four decimals, so a pattern factor of 0.00005 reloads as 0.0001 and a 0.00045 m³/s flow as 0.0004. | Write roughness, settings, patterns, curves and controls with `%.10g` |
| [FILE-08](FILE-08-strings-with-spaces-unquoted/) | Strings with spaces are written unquoted | A chemical name, tag or file name with a space is split on reload: "Free Chlorine" becomes chemical "Free" with units "Chlorine". | Write such strings in double quotes via a `quotestr()` helper |
| [FILE-09](FILE-09-output-reader-last-period-and-units/) | The output-file reader cannot reach the last period and misreads pressure units | Series reads reject the last reporting period (error 422), and the reader's enum calls the engine's kPa code metres and vice versa. | Accept `endPeriod == nPeriods`; set `ENR_PressUnits` to the engine's codes |
| [FILE-10](FILE-10-nan-inf-accepted-as-numbers/) | nan, inf, hex and empty tokens are accepted as numbers | A pipe diameter of nan (or inf, 0x64, an empty token, 12°) loads without error; NaN passes every range check and results are NaN. | In `getfloat()`: `*endptr != '\0'`, reject empty, hex and non-finite |
| [FILE-11](FILE-11-long-lines-split/) | Lines over 1,023 characters are split in two | A long line's tail is parsed as a new line: a long comment gives a bogus error; a 36-factor pattern loads as two patterns. | Skip the rest of over-long lines; report error 214 |
| [FILE-12](FILE-12-tokens-past-40-dropped/) | Tokens past the 40th on a line are dropped | A line with more than 40 tokens is silently cut: a 168-factor weekly pattern on one line loads with 39 factors. | `MAXTOKS` = `MAXLINE / 2 + 1`; clear only used token slots |
| [FILE-13](FILE-13-fractional-trials-zero-iterations/) | TRIALS below 1 gives zero solver iterations | TRIALS 0.5 is stored as 0, so the solver runs no trials and returns its initial guess: -43.33 psi instead of 25.51 psi. | Reject `TRIALS` below 1 (error 213); initialise `relerr` |
| [FILE-14](FILE-14-quoted-tokens-mis-split/) | A quoted token corrupts the rest of the line | After a quoted token, the last token keeps its newline (spurious error 205); spaces inside quotes make the scan read past the line. | Recompute `len` from the closing quote in `gettokens()` (one line) |
| [FILE-15](FILE-15-valve-check-skips-previous-link/) | The illegal-valve check skips the link read just before the valve | Two PRVs sharing a downstream node load without error when they are on consecutive [VALVES] lines; apart, or via EN_addlink, they give error 220. | Pass 0, not `net->Nlinks`, to `valvecheck()` in `valvedata()` |
| [FILE-16](FILE-16-short-link-lines-dropped/) | [PIPES], [PUMPS] and [VALVES] lines with too few fields are silently dropped | A pipe or pump line with under 3 fields, or a valve line with under 5, is skipped silently; the model runs without it. | Return error 201 instead of the unset `errcode` (0) |
| [FILE-17](FILE-17-options-load-but-not-reload/) | [OPTIONS] values that load but cannot be saved and reloaded | CHECKFREQ 0.5, SPECIFIC GRAVITY 1e-7 and similar values load, but the saved copy fails to open or reopens with 0. | Refuse `CHECKFREQ`/`MAXCHECK` below 1; write eight options with `%.10g` |
| [FILE-18](FILE-18-rule-times-lose-a-second/) | Clock times in rule premises are truncated and lose a second | Rule times like 8:00:01 are stored as 28800.999 s and truncated, so the rule fires a rule step early and saves as 8:00:00. | Round the parsed time to whole seconds in `newpremise()` |
| [FILE-19](FILE-19-trials-default-documented-wrong/) | The documented default for TRIALS is 40; the code uses 200 | The input file reference says TRIALS defaults to 40, but EPANET uses 200, as the 2.2 Users Manual also states. | Change "The default is 40" to 200 in `toolkit-input.dox` |

Each issue's folder has the full write-up, a test program, its input files and a fix patch. [Reproducing the issues](../reproducing.md) explains how to run them.

## Saving a project

### FILE-01: Every saved file turns off the node Quality column when reloaded

Every file written by `EN_saveinpfile` that reports water quality reloads with the quality column switched off, including Net1, Net2 and Net3 saved unchanged. A GUI or script that saves and reopens a model loses the column without any message. A `F-Factor` report setting is lost the same way.

The `[REPORT]` writer loops over every field below `FRICTION`. That includes the link-quality field, which is never enabled and is also named "Quality", so the file gets `Quality NO` after the node quality line. The reader matches the first field with that name, node quality. The loop also stops before `FRICTION`.

**Fix:** skip `LINKQUAL`, which no file can address, and loop up to and including `FRICTION`.

[Write-up, test and patch](FILE-01-saved-files-disable-quality-report/)

### FILE-02: Saving over the input file loses [LABELS] and [BACKDROP]

Saving a project over the file it was opened from, the normal "Save" in a script or a toolkit-based GUI, deletes the map labels and the backdrop settings. `EN_saveinpfile` returns 0. In the test, 2 label lines and 4 backdrop lines become none.

EPANET keeps no labels or backdrop in memory. `saveinpfile()` opens the new file with `"wt"`, which empties it, writes the network, and only then copies `[LABELS]` and `[BACKDROP]` from the input file, which by then is the empty new file. Issue [#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522) reported this in 2019 and was closed as fixed by [#549](https://github.com/OpenWaterAnalytics/EPANET/issues/549), which only covered vertices.

**Fix:** copy the two sections to a scratch file before opening the new file, and append the copy at the end.

[Write-up, test and patch](FILE-02-save-over-input-loses-labels/)

### FILE-03: FILLTIME and DRAINTIME rule values are written in a form the parser rejects

The manual and the parser treat a FILLTIME or DRAINTIME premise value as a number of hours, for example "FILLTIME BELOW 1.5". EN_saveinpfile writes it as a clock time, "FILLTIME < 1:30:00", so the saved file fails to open with error 202. The save itself reports nothing.

writepremise() sends these two variables through gettimetxt() along with TIME and CLOCKTIME, but newpremise() reads them with getfloat() and multiplies by 3600.

**Fix:** write FILLTIME and DRAINTIME values as decimal hours with "%.4f".

[Write-up, test and patch](FILE-03-filltime-rules-not-reloadable/)

### FILE-04: Pumps and GPVs without a curve are written in a form the parser rejects

A model being built through the toolkit often has a pump or a GPV with no curve yet; `EN_addlink` creates both that way, and the parser accepts a pump line with no `HEAD` or `POWER`. Saving such a project returns 0, but the saved file does not open: error 201 for the pump and error 206 for the GPV.

For the pump, `saveinpfile()` falls back to the numeric pump-curve format of EPANET 1.x, which `pumpdata()` stopped reading in v2.3. For the GPV it writes the curve index 0 as `0.0000` where a curve ID belongs.

**Fix:** write the forms the parser already accepts: `PU1 R1 J1` with its usual `PATTERN` and `SPEED` keywords, and the GPV line without a setting.

[Write-up, test and patch](FILE-04-pumps-gpvs-without-curve-not-reloadable/)

### FILE-05: Timer controls lose one second per save

A timer control saved by `EN_saveinpfile` as `AT TIME 0.3333 HOURS` (1,200 s) is read back as 1,199 s, so it can fire one time step early. This happens to 45 % of whole-second times, and some lose a second again on the next save. Clock times in the original file are affected too: `0:01:01` is read as 60 s.

`controldata()` converts hours to seconds with `(long)(3600.0 * time)`, which truncates. Neither four-decimal hours nor `h:mm:ss` converted through floating point gives an exact whole second. The `[TIMES]` reader rounds the same conversion.

**Fix:** round in `controldata()`, as `timedata()` does.

[Write-up, test and patch](FILE-05-timer-controls-lose-a-second/)

### FILE-06: Demand categories with zero base demand are dropped on save

Since v2.3, `EN_saveinpfile` skips every demand category whose base demand is zero, as asked in issue [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174). The category's name and pattern go with it, so a named placeholder such as a zero "Fire flow" category with its own pattern disappears, and the junction has one category fewer after reloading. Results do not change; the saved model does.

`saveinpfile()` tests only `demand->Base == 0.0`. In [#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174) eladsal suggested keeping zero demands for categories unless there is only one category with no name.

**Fix (proposal):** skip a zero-demand category only when it has neither a name nor a pattern.

[Write-up, test and patch](FILE-06-zero-demand-categories-dropped/)

### FILE-07: Fixed four-decimal formats truncate small values

`EN_saveinpfile` writes most numbers with four decimals. For inputs that are routinely small, such as flows in CMS or MGD, near-zero pattern factors and smooth-pipe D-W roughness, that keeps only one or two significant digits. In the test, a pump curve flow of 0.00045 m³/s reloads as 0.0004 and a pattern factor of 0.00005 as 0.0001. Values below 0.00005 become 0.

The writer uses `%.4f` for pipe roughness, valve settings, pattern factors, curve points and control values, whatever their size.

**Fix:** write those values with `%.10g`, which never keeps fewer digits than `%.4f` below a million and drops trailing zeros.

[Write-up, test and patch](FILE-07-four-decimal-formats-truncate/)

### FILE-08: Strings with spaces are written unquoted

The toolkit accepts a chemical name, units, tags and file names that contain spaces, and `EN_saveinpfile` writes them unquoted. The reader splits them at the blank, so "Free Chlorine" in "mg/L" reloads as chemical "Free" with units "Chlorine", and the tag "Pressure zone 2" as "Pressure".

The writer prints these strings with a bare `%s`, although the tokenizer already reads a double-quoted token as one token.

**Fix:** quote a string that contains a space or tab when writing it.

[Write-up, test and patch](FILE-08-strings-with-spaces-unquoted/)

## Reading the output file

### FILE-09: The output-file reader cannot reach the last period and misreads pressure units

The output-file library cannot return the last reporting period as a series: `ENR_getNodeSeries` and `ENR_getLinkSeries` treat `endPeriod` as exclusive but reject `endPeriod >= nPeriods`, so on a 5-period file the widest series is 4 values. Its pressure-unit enum also disagrees with the engine, so a kPa file is reported as metres.

The range check should be `endPeriod > nPeriods`. The engine writes its own unit codes (0 psi, 1 kPa, 2 metres, 3 bar, 4 feet), but `ENR_PressUnits` says 1 is metres and 2 is kPa.

**Fix:** accept `endPeriod == nPeriods`, and give `ENR_PressUnits` the engine's values, adding bar and feet.

[Write-up, test and patch](FILE-09-output-reader-last-period-and-units/)

## Reading input files

### FILE-10: nan, inf, hex and empty tokens are accepted as numbers

EPANET reads every number with `getfloat()`, built on `strtod`, which also accepts `nan`, `inf` and hexadecimal such as `0x64`. `getfloat()` also accepts an empty token as 0, and a number followed by a byte above 127 (such as `12°`) where `char` is signed. In the test a pipe with diameter `nan` loads without error, because a NaN passes every range check such as `x <= 0.0`; the hydraulic run then returns 0 with NaN for every pressure and flow.

The end-of-token test is `*endptr > 0`, which is false for an empty token and for a negative `char`, and nothing checks for NaN, infinity or hexadecimal.

**Fix:** require `*endptr == '\0'` and a non-empty token, and reject hexadecimal forms and non-finite results.

[Write-up, test and patch](FILE-10-nan-inf-accepted-as-numbers/)

### FILE-11: Lines over 1,023 characters are split in two

EPANET reads input lines with `fgets` into a buffer that returns at most 1,023 characters, and parses the rest of a longer line as a new line. A long comment then becomes a bogus data line: in the test a valid file fails with `Error 202: illegal numeric value 4471-B.`. A long data line is cut silently: a 36-factor pattern loads as a 34-factor pattern plus a second pattern named `36`, and `EN_open` returns 0.

The "line too long" check (error 214) tests `strlen(line) >= MAXLINE`, which `fgets(line, MAXLINE, ...)` can never produce, in both passes over the file.

**Fix:** after each `fgets`, skip the rest of a line that did not fit, and report error 214 if data (not only comment text) was lost.

[Write-up, test and patch](FILE-11-long-lines-split/)

### FILE-12: Tokens past the 40th on a line are dropped

The input format allows 1,024 characters per line and sets no limit on the number of values, but the tokenizer stops after 40 tokens and drops the rest of the line without an error. In the test, a weekly demand pattern with 168 hourly factors on one line loads with only 39, and the run repeats those 39 hours for the whole week. [REPORT] `NODES` lists are cut the same way.

`gettokens()` loops `while (len > 0 && n < MAXTOKS)` with `MAXTOKS` 40, and the caller cannot tell that tokens were left over.

**Fix:** set `MAXTOKS` to `MAXLINE / 2 + 1`, enough for any line, and clear only the token slots the previous line used so reading is not slower.

[Write-up, test and patch](FILE-12-tokens-past-40-dropped/)

### FILE-13: TRIALS below 1 gives zero solver iterations

`TRIALS` is the maximum number of solver iterations per time step. The parser accepts any value above 0 and stores it as an integer, so `TRIALS 0.5` becomes 0 and the solver runs no trials. It returns its initial guess as the solution with no error: in the test, a junction 100 ft below a reservoir shows -43.33 psi instead of 25.51 psi. `EN_setoption` already rejects the same value.

`optionvalue()` stores `MaxIter = (int)y` after checking only `y > 0`. With no trials, `hydsolve()` never sets the convergence error `relerr`, which `runhyd()` then reads.

**Fix:** reject `TRIALS` below 1 with error 213, and initialise `relerr` to 0.

[Write-up, test and patch](FILE-13-fractional-trials-zero-iterations/)

### FILE-14: A quoted token corrupts the rest of the line

The tokenizer treats text in double quotes as one token, as used for map labels in [LABELS]. After a quoted token the rest of the line is split wrongly. With no spaces in the quotes, the last token keeps its newline: the junction line `"J1" 100 5 PAT1` fails with error 205 because the pattern looked up is `PAT1\n`. With spaces in the quotes, the scan runs past the end of the line by about the length of the quoted text; a long label takes it past the 1,025-byte line buffer.

`gettokens()` reduces its count of remaining characters by the length up to the first separator, but then moves past the whole quoted token, so the count and the position disagree.

**Fix:** recompute the remaining length from the position of the closing quote.

[Write-up, test and patch](FILE-14-quoted-tokens-mis-split/)

### FILE-15: The illegal-valve check skips the link read just before the valve

EPANET forbids some valve combinations, for example two PRVs that share a downstream node, with error 220. When an input file is read, the check misses a pair on consecutive lines: in the test the illegal PRV pair loads without error, while the same pair with a pipe line between them, or added with `EN_addlink`, is rejected. Files saved by EPANET list all valves together, so the missed case is the common one.

`valvecheck()` skips the valve whose index it is given. `valvedata()` passes `net->Nlinks` before the new valve is added, which is the index of the link read before it.

**Fix:** pass 0 (nothing to skip), as `EN_addlink` does.

[Write-up, test and patch](FILE-15-valve-check-skips-previous-link/)

### FILE-16: [PIPES], [PUMPS] and [VALVES] lines with too few fields are silently dropped

PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) made most fields of a link line optional so that partly built models can be opened. A line too short even for that (a pipe or pump with no second end node, a valve with no type) used to give error 201. Now it is skipped without any message: in the test, three of seven links are missing, `EN_open` and `EN_solveH` return 0, and `runepanet` reports a successful run.

The rewritten `pipedata()`, `pumpdata()` and `valvedata()` return `setError(parser, -1, errcode)` while `errcode` is still 0.

**Fix:** return error 201 for these lines, as `tankdata()` does.

[Write-up, test and patch](FILE-16-short-link-lines-dropped/)

### FILE-17: [OPTIONS] values that load but cannot be saved and reloaded

A network that EPANET opens without complaint should survive `EN_saveinpfile` and a reload, but some [OPTIONS] values do not. `CHECKFREQ` or `MAXCHECK` of 0.5 is accepted and stored as 0, which the saved copy writes as 0 and the parser then refuses. Eight real options are written with 4 to 8 fixed decimals, so small values become 0. For `SPECIFIC GRAVITY`, `DEMAND MULTIPLIER` and `EMITTER EXPONENT` the saved copy then fails to open. For the others it opens with 0, which turns `TOLERANCE`, `DAMPLIMIT`, `HEADERROR` and `FLOWCHANGE` off. `CHECKFREQ` or `MAXCHECK` at 0 also stops the periodic status checks.

`optionvalue()` checks only `y > 0` before storing `(int)y`, and `saveinpfile()` uses formats such as `%-.4f` and `%-.6f` for values that can be smaller than their last decimal.

**Fix:** refuse `CHECKFREQ` and `MAXCHECK` below 1, and write the eight real options with `%-.10g`.

[Write-up, test and patch](FILE-17-options-load-but-not-reload/)

### FILE-18: Clock times in rule premises are truncated and lose a second

A rule premise on SYSTEM TIME or SYSTEM CLOCKTIME written as h:mm:ss, such as 8:00:01, is stored as 28,800.999999999996 s. The rule check truncates it to 28,800 s, so the rule fires one rule time step early (8:00 instead of 8:06 in the test), and EN_saveinpfile writes 8:00:00, so the second is lost for good. 10,545 of the 86,400 clock times in a day are affected.

newpremise() stores hour() * 3600 without rounding, and both checktime() and gettimetxt() truncate.

**Fix:** round the time to the nearest second in newpremise(), as the [TIMES] reader does.

[Write-up, test and patch](FILE-18-rule-times-lose-a-second/)

### FILE-19: The documented default for TRIALS is 40; the code uses 200

The input file reference says the default for `TRIALS` is 40. EPANET has used 200 since version 2.00.12: `EN_getoption(EN_TRIALS)` returns 200 for a file without a `TRIALS` line, and the report summary says so. The EPANET 2.2 Users Manual also gives 200 in its input file appendix.

The sentence in `doc/toolkit-input.dox` was written in 2019, probably from the GUI's suggested value of 40. `MAXITER` in `input1.c` is marked as a deliberate 2.00.12 change.

**Fix:** state the default as 200 in the documentation.

[Write-up, test and patch](FILE-19-trials-default-documented-wrong/)
