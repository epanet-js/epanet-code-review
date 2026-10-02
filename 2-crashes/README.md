# Crashes and invalid memory access

[All issues](../README.md)

Each of these crashes, or reads or writes memory it does not own. All were reproduced with AddressSanitizer or UndefinedBehaviorSanitizer. Without a sanitizer, many of them corrupt memory silently and fail later somewhere else, so the tests are meant to be run against a sanitizer build (see [Reproducing the issues](../reproducing.md)). Each needs a bounds check or an early error code.

| ID | Issue | What happens | Fix |
|---|---|---|---|
| [CRASH-01](CRASH-01-long-token-error-message/) | An invalid token over 255 characters overruns the error-message buffers | Reporting an input error with a long token overflows a stack buffer; tokens of about 200 characters silently overwrite the project title. | Bounded `strncpy` of the token and `snprintf` into `Project.Msg` |
| [CRASH-02](CRASH-02-clock-time-four-fields/) | A clock time with four or more fields writes past a 3-element array | A time such as DURATION 1:2:3:4 writes past a 3-element stack array while the file is read; a plain macOS build aborts. | Return -1 from `hour()` when a fourth field is present |
| [CRASH-03](CRASH-03-bare-priority/) | A bare PRIORITY with no value dereferences a null token | A rule whose PRIORITY line has no value crashes EN_open, runepanet and EN_addrule with a null-pointer read. | Return 201 when `PRIORITY` has no value token |
| [CRASH-04](CRASH-04-inf-nan-time-values/) | inf, nan or huge values in [TIMES] and [OPTIONS] overflow integer conversions | DURATION 1e300, inf or nan, and TRIALS, CHECKFREQ or MAXCHECK out of range, are converted to integers with undefined behaviour. | Range checks (NaN-safe) before the `(long)` and `(int)` conversions |
| [CRASH-05](CRASH-05-report-precision-overflow/) | PRECISION 7 or more overflows a 16-byte report buffer | A large report PRECISION makes the node and link tables overflow a 16-byte stack buffer; PRECISION 12 does for any value of 100 or more. | Clamp PRECISION to 0..6 in `reportdata()` |
| [CRASH-06](CRASH-06-gpv-one-point-curve/) | A one-point GPV head loss curve reads out of bounds | A GPV whose head loss curve has one point makes `curvecoeff()` read before or past the curve's arrays on every trial. | Reject GPV curves with fewer than 2 points at `EN_openH` |
| [CRASH-07](CRASH-07-quality-isolated-component/) | A network with an isolated component overflows SortedNodes in a quality run | A quality run on a network with more nodes than links plus tanks writes past the end of SortedNodes. | Allocate `SortedNodes` with `Nnodes + 1` entries |
| [CRASH-08](CRASH-08-long-names-unterminated/) | xstrcpy leaves over-long names unterminated | A demand name over 31 characters, or a comment or tag over 255, is stored without a terminator; getters and EN_saveinpfile read past it. | `memcpy` n2 characters, then write the terminator |
| [CRASH-09](CRASH-09-saveinp-rule-value-overflow/) | EN_saveinpfile overflows a 20-byte buffer for rule values of 1e15 or more | Saving a rule whose premise value or action setting is 1e15 or more overflows a 20-byte stack buffer in EN_saveinpfile. | Size `s_value` as `MAXLINE + 1` in both rule writers |
| [CRASH-26](CRASH-26-rule-status-on-node-variable/) | A rule premise that compares a node variable with a status reads past the link arrays | The parser accepts IF TANK T1 LEVEL IS OPEN; each rule check then reads LinkStatus with the tank's node index. | Return 202 for a status value on a non-link premise |
| [CRASH-29](CRASH-29-huge-rule-time-to-text/) | Rule time values of 2^31 seconds or more are converted with undefined behaviour | Huge rule times are cast to long when checked and to int when saved: undefined; x86-64 saves a garbled time. | Compare the double in `checktime()`; range-test before casting in `gettimetxt()` |
| [CRASH-31](CRASH-31-clock-time-buffer-overflow/) | A run of 1,000,000 hours or more overflows the 13-byte clock-time buffers | From 1,000,000 hours, clocktime() overruns the 13-byte Report.Atime, blanking the report file's name; with a status report, a stack overflow. | `Atime[26]`, copy sized from it, hours printed as `long` |
| [CRASH-10](CRASH-10-adjacency-lists-freed-by-both-solvers/) | Both solvers free the shared adjacency lists | Closing one solver frees the adjacency lists the other still uses; its next run step dereferences NULL. | Free the lists only when the other solver is closed |
| [CRASH-11](CRASH-11-closeq-not-idempotent/) | EN_closeQ after EN_solveQ frees the segment pool twice | A second EN_closeQ, for example after EN_solveQ, deletes the quality segment pool again and crashes the host. | Set `qual->SegPool = NULL` after deleting the pool |
| [CRASH-12](CRASH-12-inith-initq-shared-save-flag/) | EN_initH and EN_initQ share one save flag | Stepping both solvers with EN_initH(EN_NOSAVE) and EN_initQ(EN_SAVE) crashes in savehyd(); the opposite combination saves no hydraulics. | New `OutSaveflag` for the output file; `Saveflag` stays for hydraulics |
| [CRASH-13](CRASH-13-report-closes-output-file/) | EN_report before EN_closeQ closes the live output file | EN_report closes the project's open output file but keeps the pointer: the next report prints zeros or closes another file, and EN_closeQ closes it again. | In `writeresults()`, close the file only if it opened it |
| [CRASH-14](CRASH-14-calls-after-close/) | Rule, control, ID and link-node functions work on freed memory after EN_close | After EN_close, 17 rule, control, ID and link-node functions use the freed arrays and return 0; a second EN_close double-frees. | `Openflag` guards in 17 functions; `initpointers()` in `EN_close` |
| [CRASH-15](CRASH-15-bulk-setters-null-badindex/) | EN_setnodevalues and EN_setlinkvalues write through a NULL badIndex | Any failed bulk assignment with `badIndex = NULL` crashes the caller with a null pointer write instead of returning the error code. | Guard the four `*badIndex = i` stores with `if (badIndex)` |
| [CRASH-16](CRASH-16-gpv-without-curve/) | A GPV with no head loss curve crashes the solver | Solving a network with a GPV that has no curve (added through the API, or a curveless [VALVES] line) reads the uninitialised `Curve[0]`. | New `validatevalves()` at `EN_openH`: error 206, `EN_openH` returns 110 |
| [CRASH-17](CRASH-17-addrule-multiple-rules/) | EN_addrule with two RULE clauses overflows the rule array | EN_addrule makes room for one rule but parses every RULE clause, so a second rule is written past the array. | Record the rule count; return 250 on a second `RULE` clause |
| [CRASH-18](CRASH-18-addrule-long-line/) | EN_addrule with a line of 1,024 characters or more overflows the stack | A line of 1,024 characters or more in EN_addrule's text, even a comment, overflows the 1,025-byte stack buffer line2. | Return 214 for a line of `MAXLINE` characters or more |
| [CRASH-19](CRASH-19-addrule-edits-caller-string/) | EN_addrule edits the caller's string, so a string literal faults | EN_addrule writes '\0' over each newline of the caller's text: a string literal faults, and a failed call leaves the text cut short. | Copy each line into `line2` by length; leave the caller's text alone |
| [CRASH-20](CRASH-20-rule-setters-unchecked-index/) | Rule premise and action setters accept any object index | Premise and action setters accept any node or link index, variable or code; the next run reads past the node and link arrays. | Check index, variable and codes in the five premise/action setters |
| [CRASH-21](CRASH-21-deletedemand-index-zero/) | EN_deletedemand with index 0 leaves a dangling demand list | Demand index 0 returns 0 but frees the junction's first demand while the list still points to it; the next read is a use-after-free. | Return 253 for a demand index below 1 |
| [CRASH-22](CRASH-22-setcurvevalue-out-of-range/) | EN_setcurvevalue reads past the curve before range-checking the point index | A point index two or more past the end reads beyond the curve's arrays and can append a point with a decreasing x-value. | Clamp `pointIndex` to `Npts + 1` before the x-value check |
| [CRASH-23](CRASH-23-setlinknodes-node-zero/) | EN_setlinknodes accepts node index 0 | Node index 0 is accepted; after any hydraulic warning, the disconnected-node check writes past the end of its node list. | Reject node indices below 1 (`<= 0` instead of `< 0`) |
| [CRASH-24](CRASH-24-31-character-ids-unterminated/) | EN_addnode and EN_addlink leave 31-character IDs unterminated | A 31-character ID set through the API is stored without a terminator: look-ups by ID fail and EN_getnodeid overflows the caller's buffer. | Write `ID[MAXID] = '\0'` after each of the four `strncpy` calls |
| [CRASH-25](CRASH-25-setflowunits-out-of-range/) | EN_setflowunits does not check its argument | An out-of-range flow units code returns 0, then EPANET reads past the end of its unit-name table. | Return 251 for codes outside `EN_CFS` to `EN_CMS`, as `EN_init` does |
| [CRASH-27](CRASH-27-rule-labels-unterminated/) | EN_addrule leaves rule labels of 31 characters or more unterminated | A rule added with a 31-character label has no terminator; EN_getruleID writes 33 bytes into the documented 32-byte buffer. | Write `label[MAXID] = '\0'` in `newrule()` |
| [CRASH-28](CRASH-28-setcurve-shrinks-gpv-curve-while-open/) | EN_setcurve can shrink a GPV's curve to one point while the solver is open | Setting a GPV's curve to one point with `EN_setcurve` during a run makes the next `EN_runH` read outside the curve's arrays. | Refuse one-point GPV curves in `EN_setcurve` and `EN_GPV_CURVE` while open |
| [CRASH-30](CRASH-30-api-out-of-range-conversions/) | Time and option setters convert out-of-range values to integers with undefined behaviour | Setters convert NaN or huge values to int before range-checking; TRIALS near INT_MAX plus UNBALANCED CONTINUE overflows, so no trials run. | Saturating `ROUND()`, int and time range checks, saturating trial sum |

Each issue's folder has the full write-up, a test program, its input files and a fix patch. [Reproducing the issues](../reproducing.md) explains how to run them.

## Reading and saving input files

### CRASH-01: An invalid token over 255 characters overruns the error-message buffers

When a line in the input file has an error, EPANET writes the offending token into the report message. The token can be up to 1,023 characters, but the message is built in two 256-byte buffers with no length check. A 300-character ID overflows a stack buffer and AddressSanitizer stops `EN_open`. A 240-character ID overflows `Project.Msg` into the project title instead, which no sanitizer detects.

`inperrmsg()` copies the token with `strcpy` into `char tok[MAXMSG + 1]` and formats the message with `sprintf` into `Project.Msg[MAXMSG + 1]`.

**Fix:** copy at most `MAXMSG` characters of the token and use `snprintf` for the message.

[Write-up, test and patch](CRASH-01-long-token-error-message/)

### CRASH-02: A clock time with four or more fields writes past a 3-element array

`hour()` reads clock times such as `DURATION 24:00` by splitting them at the colons into hours, minutes and seconds. A time with a fourth field, for example `DURATION 1:2:3:4`, is written one element past the end of a 3-element array on the stack. AddressSanitizer stops `EN_open`, and a plain macOS Release build aborts in it.

The loop that fills `double y[3]` runs while `n <= 3` instead of `n < 3`. The same function reads [TIMES] values, `AT CLOCKTIME` controls and rule time premises.

**Fix:** return -1 (an invalid time) when a fourth field is present.

[Write-up, test and patch](CRASH-02-clock-time-four-fields/)

### CRASH-03: A bare PRIORITY with no value dereferences a null token

A rule's PRIORITY clause needs a number. When the number is missing, opening the input file crashes the host process with a null-pointer read instead of reporting an input error. EN_addrule crashes the same way on the same clause.

newpriority() reads the second token without checking the token count. For a bare PRIORITY that token is NULL, and getfloat() passes it to strtod().

**Fix:** return error 201 (syntax error) when the clause has no value, as the other rule clause parsers do.

[Write-up, test and patch](CRASH-03-bare-priority/)

### CRASH-04: inf, nan or huge values in [TIMES] and [OPTIONS] overflow integer conversions

EPANET converts [TIMES] values to a whole number of seconds in a `long`, and `TRIALS`, `CHECKFREQ` and `MAXCHECK` to an `int`, without checking that the value fits. `inf`, `nan` and `1e300` all parse as numbers, so the conversion is undefined behaviour: UBSan stops `EN_open`, and a plain arm64 build stores `DURATION 1e300` as 9,223,372,036,854,775,807 s, which later overflows the hydraulic clock. The review's fuzzer found 29 crashes of this kind.

`timedata()` does `t = (long)(3600.0 * y + 0.5)` and `optionvalue()` does `MaxIter = (int)y` (and the same for `CheckFreq` and `MaxCheck`) with no upper bound, and NaN passes the existing `y <= 0` check.

**Fix:** reject times outside 0 to half the range of a `long`, and integer options above `INT_MAX`, with tests that NaN also fails.

[Write-up, test and patch](CRASH-04-inf-nan-time-values/)

### CRASH-05: PRECISION 7 or more overflows a 16-byte report buffer

The report's node and link tables print each value into `char s1[16]` with the field's `PRECISION` as the number of decimals, and nothing limits it. A head of 799.68 ft with `PRECISION 12` needs 17 bytes. The command can come from `[REPORT]` or from `EN_setreport`, and the overflow happens in `EN_report` and `runepanet`.

`reportdata()` stores `ROUND(y)` as the precision without a range check. Values over 1e6 are printed in exponent form, so 6 decimals is the most the buffer can hold.

**Fix:** clamp the precision to 0..6 where it is read.

[Write-up, test and patch](CRASH-05-report-precision-overflow/)

### CRASH-06: A one-point GPV head loss curve reads out of bounds

A GPV's head loss is interpolated between the two curve points that bracket the flow. EPANET accepts a GPV curve with a single point, because one point is valid for a pump curve, and the interpolation then reads one element before or after the curve's arrays. In the test the sanitizer build stops with a heap-buffer-overflow on the first trial; without it, the head loss comes from neighbouring memory.

`validatecurves()` only requires one point, and `curvecoeff()` assumes at least two.

**Fix:** report a GPV curve with one point as an error at `EN_openH`, in the valve check added by [CRASH-16](CRASH-16-gpv-without-curve/).

[Write-up, test and patch](CRASH-06-gpv-one-point-curve/)

### CRASH-07: A network with an isolated component overflows SortedNodes in a quality run

A water quality run on a network with an isolated component, such as a pipe joining two junctions that connect to nothing else, writes past the end of a heap array. The hydraulic solver accepts the network with warnings, so this happens in an ordinary `runepanet` run. In the 4-node test network the sort writes index 4 of a 4-entry array.

`openqual()` allocates `SortedNodes` with the segment-array size, `Nlinks + Ntanks + 1`, but `sortnodes()` stores one entry per node. Connected networks have enough links to hide the mismatch.

**Fix:** allocate `SortedNodes` with `Nnodes + 1` entries.

[Write-up, test and patch](CRASH-07-quality-isolated-component/)

### CRASH-08: xstrcpy leaves over-long names unterminated

EPANET cuts demand category names to 31 characters and comments and tags to 255, but stores the cut copy without a terminating null. Every later read runs past the allocation: in the test, `EN_getdemandname` on a 45-character [DEMANDS] category name returns the 31 stored characters followed by text from a neighbouring heap block, and `EN_saveinpfile` writes those bytes into the file.

`xstrcpy()` sizes the buffer for `n2 = min(strlen(s2), n)` characters but copies `n2+1` with `strncpy`. When the source was cut, the last character copied is not the terminator.

**Fix:** copy `n2` characters with `memcpy` and set `(*s1)[n2] = '\0'`.

[Write-up, test and patch](CRASH-08-long-names-unterminated/)

### CRASH-09: EN_saveinpfile overflows a 20-byte buffer for rule values of 1e15 or more

EN_saveinpfile writes each rule value with "%.4f" into a 20-character buffer. A value of 1e15 or more (or -1e14 or less) needs 21 characters or more, so saving the project overflows the stack. The review hit this on a benchmark model with a large rule action setting.

writepremise() and writeaction() both declare char s_value[20], and "%.4f" prints every digit of the integer part.

**Fix:** declare both buffers as MAXLINE + 1 characters, enough for "%.4f" of any double.

[Write-up, test and patch](CRASH-09-saveinp-rule-value-overflow/)

### CRASH-26: A rule premise that compares a node variable with a status reads past the link arrays

Only links have a status, but the [RULES] parser accepts OPEN, CLOSED or ACTIVE as the value of any premise, so a rule such as IF TANK T1 LEVEL IS OPEN loads without error. Each rule check then reads the link status array at the tank's node index: past the end when the node index is larger than the number of links, and some unrelated link's status otherwise. EN_addrule accepts the same premise.

newpremise() stores a status word for any object, and checkpremise() sends every premise with a status to checkstatus(), which reads LinkStatus[p->index].

**Fix:** reject a status value with error 202 unless the premise is on a link.

[Write-up, test and patch](CRASH-26-rule-status-on-node-variable/)

### CRASH-29: Rule time values of 2^31 seconds or more are converted with undefined behaviour

A rule's time premise is a double number of seconds, but two places convert it to an integer type without checking that it fits. checktime() compares (long)value at every rule check, which is undefined for a premise such as IF SYSTEM TIME > 1e300 (and for any time of 2^31 s or more on Windows). EN_saveinpfile's gettimetxt() computes (int)secs / 3600 first, which is undefined for 2^31 s (596,523 hours) or more; x86-64 then saves a garbled time that reloads as a different value.

Both functions convert before testing the value.

**Fix:** compare the double in checktime(), and test the range before converting in gettimetxt().

[Write-up, test and patch](CRASH-29-huge-rule-time-to-text/)

### CRASH-31: A run of 1,000,000 hours or more overflows the 13-byte clock-time buffers

EPANET writes times as hours, minutes and seconds into `Report.Atime`, a 13-byte buffer with room for six digits of hours. From 1,000,000 hours (114 years) the text is 13 characters, so the terminator lands in the next field, the report file's name. A run that reaches that time loses the name silently, and `EN_copyreport` then fails with error 303. With a status report, `writehydstat()` also copies the text into a 13-byte array on the stack, which AddressSanitizer reports and a plain macOS build aborts on.

`clocktime()` prints `"%01d:%02d:%02d"` into `Atime[13]`, and `writehydstat()` copies it into `char atime[13]`, with no room for hours of seven or more digits.

**Fix:** size `Atime` for any `long` time (26 bytes), size `writehydstat()`'s copy from it, and print the hours as a `long`.

[Write-up, test and patch](CRASH-31-clock-time-buffer-overflow/)

## Opening, running and closing the solvers

### CRASH-10: Both solvers free the shared adjacency lists

The hydraulic and water quality solvers share one set of node adjacency lists. Closing either solver frees them, even when the other solver is still open and using them. The close call returns 0; the crash comes later, in `EN_runQ` after `EN_closeH`, or in `EN_runH` after `EN_closeQ` as soon as a step raises a warning.

The fix for [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) added `freeadjlists()` to both `closehyd()` and `closequal()`, while `openqual()` reuses the lists when the hydraulic solver built them.

**Fix:** free the lists in each close function only when the other solver is not open.

[Write-up, test and patch](CRASH-10-adjacency-lists-freed-by-both-solvers/)

### CRASH-11: EN_closeQ after EN_solveQ frees the segment pool twice

`EN_solveQ` closes the water quality solver itself. A program that calls `EN_closeQ` again, a natural step in clean-up code, makes the library delete the segment memory pool a second time. The process crashes with a heap use-after-free, with or without sanitizers. `EN_closeH` after `EN_solveH` is harmless.

`closequal()` deletes the pool but does not clear `qual->SegPool`, and `EN_closeQ` calls it whether or not the solver is open.

**Fix:** set `qual->SegPool` to NULL after deleting the pool, as `FREE()` does for the other arrays.

[Write-up, test and patch](CRASH-11-closeq-not-idempotent/)

### CRASH-12: EN_initH and EN_initQ share one save flag

When a toolkit program steps the hydraulic and water quality solvers together, `EN_initH(EN_NOSAVE)` followed by `EN_initQ(EN_SAVE)` crashes at the first `EN_nextH`, which writes to a hydraulics file that was never opened. The opposite, `EN_initH(EN_SAVE)` with `EN_initQ(EN_NOSAVE)`, runs but saves no hydraulics, so `EN_savehydfile` returns 104.

Both functions set the one field `outfile.Saveflag`, which the hydraulic solver reads to write the hydraulics file and the quality solver reads to write the output file. Whichever is initialised last wins.

**Fix:** keep `Saveflag` for the hydraulics file and add `OutSaveflag`, set by `EN_initQ`, for the output file.

[Write-up, test and patch](CRASH-12-inith-initq-shared-save-flag/)

### CRASH-13: EN_report before EN_closeQ closes the live output file

A program that steps through water quality and calls `EN_report` before `EN_closeQ` gets a correct first report, but `writeresults()` then closes the project's live output file and leaves `out->OutFile` pointing at it. A second `EN_report` prints all-zero tables with return code 0; in the test, a file the program opened in between got the recycled `FILE` and was closed by `EN_report`. `EN_closeQ` then closes the stream a second time, a double free with glibc.

`writeresults()` uses `out->OutFile` when it is open, or opens the file by name, and closes whichever handle it used.

**Fix:** close the file only when `writeresults()` opened it itself.

[Write-up, test and patch](CRASH-13-report-closes-output-file/)

### CRASH-14: Rule, control, ID and link-node functions work on freed memory after EN_close

EN_close frees the network arrays but keeps the node, link, control and rule counts. Seventeen toolkit functions have no Openflag check, so after EN_close their index checks pass and they read and write the freed memory; without sanitizers several of them return 0 as if they had worked. A second EN_close frees the arrays again, and EN_addrule before EN_open dereferences a NULL hash table.

EN_setnodeid, EN_setlinkid, EN_setlinknodes, EN_deletecontrol and the rule functions go straight to their index checks, which use counts that EN_close never resets.

**Fix:** return 102 from those functions when no project is open, and call initpointers() in EN_close to zero the counts and pointers.

[Write-up, test and patch](CRASH-14-calls-after-close/)

## Toolkit calls with bad arguments

### CRASH-15: EN_setnodevalues and EN_setlinkvalues write through a NULL badIndex

`EN_setnodevalues` and `EN_setlinkvalues` (new in PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908), not yet released) report the failing node or link in an optional `badIndex` argument. They check it for NULL when they initialise it, but store the failing index through it unchecked, so any rejected value (a negative emitter coefficient, a negative diameter) crashes a caller that passed NULL.

The error paths in both functions do `*badIndex = i;` without the `if (badIndex)` guard that the initialisation uses. An earlier commit in the same PR had the guards; a later rework of the rollback dropped them.

**Fix:** guard the four stores with `if (badIndex)`.

[Write-up, test and patch](CRASH-15-bulk-setters-null-badindex/)

### CRASH-16: A GPV with no head loss curve crashes the solver

A GPV created through the toolkit has curve index 0 until a curve is assigned. If the network is solved first, the hydraulic solver looks up curve 0, a slot that is never initialised, and crashes in `curvecoeff()` or computes head losses from leftover memory. An input file with a GPV line that has no curve field, or deleting the curve a GPV uses, leads to the same read.

`validateproject()` checks tanks, pumps, patterns and curves at `EN_openH` but not valves, and `gpvcoeff()` passes the index to `curvecoeff()` unchecked.

**Fix:** add `validatevalves()`, which reports a GPV without a valid curve as error 206 so that `EN_openH` returns 110.

[Write-up, test and patch](CRASH-16-gpv-without-curve/)

### CRASH-17: EN_addrule with two RULE clauses overflows the rule array

EN_addrule grows the Rule array by one entry, but the parser starts a new rule at every RULE clause. Passing two rules in one string, for example a block copied from another input file, writes the second rule past the end of the array. Without sanitizers the call returns 0 and corrupts the heap.

The realloc in EN_addrule allows Nrules + 1 rules, while ruledata() increments Nrules and calls newrule() for each RULE line.

**Fix:** record the rule count on entry and return 250 when a second RULE clause arrives.

[Write-up, test and patch](CRASH-17-addrule-multiple-rules/)

### CRASH-18: EN_addrule with a line of 1,024 characters or more overflows the stack

EN_addrule copies each line of the rule text into a 1,025-byte stack buffer and adds a newline. A line of 1,023 characters fits; a line of 1,024 characters or more writes past the end. A long comment line is enough, and nothing documents a limit.

The copy is strcpy(line2, line) followed by strcat(line2, "\n"), with no length check.

**Fix:** return error 214 (too many characters in input line) for a line that does not fit, as for an input file.

[Write-up, test and patch](CRASH-18-addrule-long-line/)

### CRASH-19: EN_addrule edits the caller's string, so a string literal faults

EN_addrule splits the rule text into lines by writing '\0' over each newline of the caller's string, and puts the newline back after the line is parsed. Called with a string literal, the most natural way from C, it faults on the first write. With a writable buffer, a rule with an error returns before the newline is restored, so the caller's text comes back cut short.

The terminator is not needed: each line is copied into the local buffer line2 before it is tokenized.

**Fix:** find each line's end without marking it and copy that many characters into line2; the text is only read.

[Write-up, test and patch](CRASH-19-addrule-edits-caller-string/)

### CRASH-20: Rule premise and action setters accept any object index

The toolkit setters that edit a rule's premises and actions check only that the rule and the clause exist. A node index past the last node, a link index past the last link, or a link variable on a node is stored with return code 0. The next hydraulic run reads past the end of NodeHead, LinkFlow or LinkStatus when it checks the rule, and EN_saveinpfile indexes its keyword tables with the stored codes.

EN_setpremise, EN_setpremiseindex, EN_setpremisestatus, EN_setthenaction and EN_setelseaction store their arguments unchecked, and checkvalue() and checkstatus() trust them.

**Fix:** check the object index (203, 204), that the variable suits the object, and the operator and status codes (251), as the parser does.

[Write-up, test and patch](CRASH-20-rule-setters-unchecked-index/)

### CRASH-21: EN_deletedemand with index 0 leaves a dangling demand list

Demand categories are numbered from 1. `EN_deletedemand` with index 0 returns 0, but it has freed the junction's first demand while the junction's demand list still starts with it. The next call that reads the demands uses freed memory, and closing the project frees the demand again. In the test, AddressSanitizer stops the next read of the junction's two demands (100 and 50 gpm) with a heap-use-after-free.

With index 0 neither the "head of the list" branch (index 1) nor the search loop runs, so the code "unlinks" the head from itself and frees it.

**Fix:** return 253 for an index below 1, as the other demand functions do.

[Write-up, test and patch](CRASH-21-deletedemand-index-zero/)

### CRASH-22: EN_setcurvevalue reads past the curve before range-checking the point index

`EN_setcurvevalue` is meant to append a point when the point index is past the end of the curve. For an index two or more past the end, it first checks the new x-value against a value read from beyond the curve's arrays, and only then clamps the index. AddressSanitizer reports a heap-buffer-overflow. Without sanitizers, the test's point (500, 60) is accepted after (2000, 50), so the curve's x-values stop increasing.

The function computes `n = pointIndex - 1` and reads `X[n-1]` before it clamps `pointIndex` to `Npts + 1`.

**Fix:** clamp `pointIndex` and set `n` before the x-value check.

[Write-up, test and patch](CRASH-22-setcurvevalue-out-of-range/)

### CRASH-23: EN_setlinknodes accepts node index 0

`EN_setlinknodes` accepts node index 0, which is an unused slot, and returns 0. A later hydraulic run that ends with any warning checks the network for disconnected nodes. That check reaches node 0 through the moved link, lists one node more than it allocated room for, and AddressSanitizer reports a heap-buffer-overflow.

The node arguments are checked with `node < 0` instead of `node <= 0`, unlike the link index check above them.

**Fix:** test `<= 0` for both nodes.

[Write-up, test and patch](CRASH-23-setlinknodes-node-zero/)

### CRASH-24: EN_addnode and EN_addlink leave 31-character IDs unterminated

IDs may have up to 31 characters. When a node or link gets a 31-character ID through `EN_addnode`, `EN_addlink`, `EN_setnodeid` or `EN_setlinkid`, it is stored without a terminating zero, so the ID runs on into the next bytes. The object cannot be found by its ID, `EN_saveinpfile` writes a stray byte after it, and `EN_getnodeid` writes 33 bytes into a caller's 32-byte buffer (a stack-buffer-overflow under AddressSanitizer).

`strncpy(ID, id, MAXID)` does not write a terminator for a 31-character source, and the slot's last byte comes from `realloc`, so it can be anything.

**Fix:** set `ID[MAXID] = '\0'` after each of the four copies.

[Write-up, test and patch](CRASH-24-31-character-ids-unterminated/)

### CRASH-25: EN_setflowunits does not check its argument

`EN_setflowunits` accepts any integer. With 11, one past `EN_CMS`, it returns 0, stores 11 as the flow units, and `initunits()` copies the units label from one entry past the end of `RptFlowUnitsTxt[]` (an AddressSanitizer global buffer overflow). The project is left with a units code no input file can produce, converted with the CFS factor.

`EN_init` rejects the same out-of-range codes with error 251; `EN_setflowunits` has no such check.

**Fix:** add `if (units < 0 || units > CMS) return 251;`.

[Write-up, test and patch](CRASH-25-setflowunits-out-of-range/)

### CRASH-27: EN_addrule leaves rule labels of 31 characters or more unterminated

A rule added through EN_addrule with a label of 31 characters, the longest ID EPANET allows, or more, is stored without a terminating zero. EN_getruleID then copies 33 or more bytes into the caller's buffer, which the API documents as EN_MAXID + 1 (32) characters, and EN_saveinpfile writes stray bytes after the label. This is the rule-label version of [CRASH-24](CRASH-24-31-character-ids-unterminated/).

newrule() copies the label with strncpy(label, Tok[1], MAXID), which writes no terminator for a long label, and EN_addrule's realloc leaves the new entry's last byte uninitialised.

**Fix:** write label[MAXID] = '\0' in newrule(), so long labels are cut to 31 characters as in input files.

[Write-up, test and patch](CRASH-27-rule-labels-unterminated/)

### CRASH-28: EN_setcurve can shrink a GPV's curve to one point while the solver is open

[CRASH-06](CRASH-06-gpv-one-point-curve/) checks at `EN_openH` that a GPV's head loss curve has at least two points, but a program can change the curve while the solver is open, and `EN_setcurve` accepts a single point. On the next `EN_runH`, `curvecoeff()` reads before the start of the curve's arrays. With [SIM-21](../1-wrong-results/SIM-21-gpv-curve-index-three-copies/) applied, assigning a one-point curve through `EN_GPV_CURVE` during a run does the same.

Nothing re-checks a GPV's curve after `EN_openH`.

**Fix:** while the solver is open, `EN_setcurve` refuses fewer than two points for a GPV's curve (error 202), and `EN_GPV_CURVE` refuses a one-point curve (error 211).

[Write-up, test and patch](CRASH-28-setcurve-shrinks-gpv-curve-while-open/)

### CRASH-30: Time and option setters convert out-of-range values to integers with undefined behaviour

The toolkit setters for integer-valued options and properties convert the double they are given with `(int)` or `ROUND()` before checking its range. For NaN or a value outside the `int` range that is undefined behaviour, and UBSan stops the call. In a plain arm64 build some values get through: `EN_TRIALS` 1e10 becomes 2,147,483,647 trials and `EN_MIXMODEL` NaN becomes model 0. A `TRIALS` value that large, with `UNBALANCED CONTINUE`, overflows the solver's trial count. The solver then runs no trials and reports -307.64 psi instead of 127.54 psi at a Net1 junction. `EN_settimeparam` accepts a `PATTERN START` of `LONG_MAX`, which overflows on the first time step.

The conversions come first and the range checks afterwards, `EN_settimeparam` has no upper limit, and `hydsolve()` adds `ExtraIter` to `MaxIter` without a check.

**Fix:** make `ROUND()` saturate (NaN gives `INT_MIN`), refuse integer options that do not fit in an `int`, limit times to half the range of a `long`, and saturate the trial count.

[Write-up, test and patch](CRASH-30-api-out-of-range-conversions/)
