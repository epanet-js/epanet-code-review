# Toolkit API behaviour

[All issues](../README.md)

None of these crash. They leave the project in a different state from the one the call reports, accept input that the file format rejects, or return a value that does not match the documentation.

| ID | Issue | What happens | Fix |
|---|---|---|---|
| [API-01](API-01-addnode-shifts-wrong-rule-premises/) | EN_addnode shifts rule premises after incrementing the junction count | After a junction is added, a rule premise on the first tank or reservoir points at the new junction, with no error. | Call `adjusttankrules()` before `Njuncs` is incremented |
| [API-02](API-02-addnode-trace-node-not-shifted/) | EN_addnode does not shift the trace node (a reverted fix) | After a junction is added, a source trace silently runs from another node: in the test J1 reports 100 % instead of 0 %. | Restore [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829)'s line that shifts `TraceNode` in `EN_addnode` |
| [API-03](API-03-setlinktype-cv-keeps-controls/) | EN_setlinktype to CVPIPE keeps the link's controls and rules | A pipe given a check valve keeps the controls and rule actions that set it, and the saved file then fails to open (error 207). | Delete controls and rules that set a pipe given a check valve |
| [API-04](API-04-settankdata-diameter-zero/) | EN_settankdata with diameter 0 turns a tank into a reservoir | Diameter 0 with no volume curve silently makes the tank a fixed-head reservoir, which cannot be turned back (error 263). | Document the behaviour in the `EN_settankdata` docs; no code change |
| [API-05](API-05-stale-hydraulics-after-edit/) | Structural edits do not invalidate the saved hydraulics | After adding, deleting, reconnecting or retyping a node or link, EN_solveQ returns 0 using misaligned hydraulics; a 300 gpm pipe reads 41.7. | Clear `outfile.SaveHflag` in the six structural editing functions |
| [API-06](API-06-addlink-self-loop/) | EN_addlink accepts a link from a node to itself | EN_addlink returns 0 for a self-loop; the next EN_openH overruns a heap buffer in genmmd(), and a saved file will not reopen. | Return 222 when both end nodes are the same |
| [API-07](API-07-deletelink-cv-pipe-count/) | EN_deletelink does not decrement the pipe count for check-valve pipes | Deleting a check-valve pipe leaves the pipe count one too high; the report summary lists 3 pipes instead of 2. | Test `linkType <= PIPE` when decrementing `Npipes` |
| [API-25](API-25-report-misaligned-after-edit/) | EN_report after a structural edit prints misaligned results | After a node or link is added or deleted, EN_report returns 0 and prints shifted values: a 300.00 fps "velocity" is a flow. | Clear `outfile.SaveQflag` next to `SaveHflag` in the six editing functions |
| [API-27](API-27-results-on-wrong-objects-after-edit/) | Adding or deleting nodes and links leaves results attached to the wrong objects | After EN_addnode, EN_deletenode or EN_deletelink, nodes and links read their neighbours' results, and one new slot is uninitialised, until the next run. | Shift result arrays with the nodes and links; zero a new node's results |
| [API-29](API-29-addlink-leaves-new-link-fields-unset/) | EN_addlink leaves a new pump's diameter and a new link's flow, status and setting unset | A new link inherits its reused slot's values: a pump reports an 8 in diameter; a new PRV reads 100 gpm before any run. | Set a pump's diameter to 0; start new links at their initial state |
| [API-08](API-08-bad-rule-deletes-previous-rule/) | A bad EN_addrule string deletes the previous rule | A rejected EN_addrule string deletes or disables the last rule, or drops the rule count to -1 so later rules vanish. | Require `RULE` first; delete only a rule this call added |
| [API-09](API-09-malformed-rule-drops-later-rules/) | One malformed rule in a file drops every rule after it | EN_openX keeps only the rules before the first bad one; a bad RULE line deletes the previous rule; a stray clause makes the count -1. | Resume at the next `RULE` clause; charge errors to the right rule |
| [API-10](API-10-reindextanks-shifts-premises-repeatedly/) | reindextanks shifts rule premises once per tank | After EN_openX loads a file with a failed junction line, each tank rule premise moves by the gap times the tank count, onto another node. | Call `adjusttankrules()` once, after the tank loop |
| [API-11](API-11-bulk-setters-read-before-write/) | Bulk setters read every node before writing, so source and tank properties cannot be set | `EN_setnodevalues` fails at node 1, a junction, for every tank and quality-source property, so these can never be bulk-set. | Write only changed values; add a source only for a non-zero value |
| [API-12](API-12-sourcemass-cumulative/) | EN_SOURCEMASS returns cumulative mass x 60, not a rate | EN_SOURCEMASS returns the total mass added so far times 60: a 60 mg/min booster reads 216,000 after one hour. | Store the latest step's mass divided by its length in `Smass` |
| [API-13](API-13-reportstart-validates-old-value/) | EN_settimeparam(EN_REPORTSTART) validates the old value | A report start after the end is accepted, then every valid one is refused; a run with no reported period writes NaN statistics. | Test the new value; skip the statistic when no period was reported |
| [API-14](API-14-initstatus-char-cast/) | EN_INITSTATUS is cast to char before the range check | Link statuses of 256 and 257 wrap to 0 and 1 and are accepted with return code 0; 256 closes the link. | Check `value < 0 || value > 1` before the `char` cast |
| [API-15](API-15-pipe-setting-ignores-roughness-error/) | EN_SETTING on a pipe ignores the roughness error | An invalid pipe roughness set through `EN_INITSETTING` or `EN_SETTING` is ignored, but the call returns 0. | Return the error code of the `EN_ROUGHNESS` assignment |
| [API-16](API-16-options-not-reloadable/) | EN_CHECKFREQ, EN_MAXCHECK below 1 and tiny viscosity are accepted but cannot be reloaded | Accepted option values break the saved file: it fails to open (error 200) or reloads a viscosity 45 times too large. | Range checks in `EN_setoption` matching what the input file can hold |
| [API-17](API-17-haltflag-always-zero/) | EN_gettimeparam(EN_HALTFLAG) always returns 0 | A run halted by UNBALANCED STOP still reads EN_HALTFLAG = 0, so a program cannot detect the early stop. | Add the missing `break`; return `hyd->Haltflag` |
| [API-18](API-18-tanklevel-returns-initial-level/) | EN_TANKLEVEL returns the initial level but is documented as current | Documented as the current tank level, `EN_TANKLEVEL` returns the initial level; for a reservoir, set writes the head but get returns 0. | Correct the enum comment and guide; return a reservoir's head |
| [API-19](API-19-specific-gravity-call-order/) | EN_SP_GRAVITY does not rescale stored emitter and PDA values | Pressure values set before the gravity change their psi values: with gravity 2 a 40 psi PRV holds 80 psi. | Scale stored pressure heads by old/new gravity in `EN_SP_GRAVITY` |
| [API-20](API-20-reads-before-run-return-scratch-values/) | Reads before a run return solver scratch values | Before any run, a new emitter reads 448.83 gpm of flow and a tank holding 19,635 ft³ reads -196,350 ft³. | Seed emitter flow only while solving; set tank heads at load |
| [API-26](API-26-link-status-before-run/) | EN_STATUS, EN_SETTING and EN_DEMAND read leftover values before a run | Before a run every link reads closed, pump and valve settings read 0, and demands read 448.83 times the base demand. | Set initial link status and setting, and zero demands, on load |
| [API-21](API-21-disabled-controls-cut-time-steps/) | Controls disabled with link index 0 still cut time steps | A control de-activated with EN_setcontrol(..., linkIndex 0, ...) still shortens the hydraulic step to its trigger time (extra solution at 0:30 in Net1). | Skip controls with `Link <= 0` in `controltimestep()` |
| [API-22](API-22-timeofday-controls-not-wrapped/) | API TIMEOFDAY controls are not reduced modulo 24 h | A TIMEOFDAY control added through the API for 24:00 or later never fires; the same control from an input file does. | `if (type == TIMEOFDAY) t %= SECperDAY;` in `setcontrol()` |
| [API-23](API-23-nexthyd-drops-save-error/) | nexthyd overwrites a savehyd write error | A failed write to the hydraulics file is not reported; EN_nextH returns 0 and the quality run reads the damaged file. | `ERRCODE(savehydstep(...))` in `nexthyd()`; `ferror()` checks in `savehyd()`, `savehydstep()` |
| [API-28](API-28-control-times-truncated/) | Control times added through the API are truncated, not rounded | A computed control time such as 8.2 * 3600 is stored as 29519 s and acts a second early; 1e300 or NaN is undefined behaviour. | Refuse times outside 0 to `LONG_MAX/2` (and NaN); round with `+ 0.5` |
| [API-24](API-24-library-changes-host-locale/) | The library changes the host program's numeric locale | Under a decimal-comma locale, EN_createproject switches the whole process: the host's printf("%.2f", 1.25) then prints 1,25. | Per-thread `setnumericlocale()`/`restorenumericlocale()` instead of `setlocale()` |
| [API-30](API-30-report-file-left-open-after-failed-open/) | EN_deleteproject leaves the report file open after a failed EN_open | After a failed EN_open, EN_deleteproject skips EN_close: the report stays empty until exit, and handles and memory leak. | `EN_deleteproject` always calls `EN_close` |

Each issue's folder has the full write-up, a test program, its input files and a fix patch. [Reproducing the issues](../reproducing.md) explains how to run them.

## Editing the network

### API-01: EN_addnode shifts rule premises after incrementing the junction count

When `EN_addnode` adds a junction, every tank and reservoir moves up one index, and rule premises that refer to them must follow. The premise on the first tank or reservoir does not move, so it now refers to the new junction. In the test, a rule on reservoir R1's head tests the new junction J3 instead, and `EN_saveinpfile` writes it as `IF JUNCTION J3 HEAD > 100`.

`adjusttankrules()` shifts premises whose node index is greater than `Njuncs`, but `EN_addnode` calls it after it has already incremented `Njuncs`.

**Fix:** call `adjusttankrules()` before `Njuncs` is incremented.

[Write-up, test and patch](API-01-addnode-shifts-wrong-rule-premises/)

### API-02: EN_addnode does not shift the trace node (a reverted fix)

A source trace stores its node as an index. When `EN_addnode` adds a junction, tanks and reservoirs move up one place, but the trace node index does not, so the trace silently runs from the node below it. In the test, the trace from reservoir Lake becomes a trace from reservoir River: J1 reports 100 % and J2 0 %, the reverse of the truth, and `EN_solveQ` returns 0.

`EN_addnode` shifts tanks, link end nodes, control nodes and rule premises, but not `quality.TraceNode`. The shift was added in February 2025 ([#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829)) and lost two days later when another commit overwrote `epanet.c`, so no release has it.

**Fix:** shift `TraceNode` with the tanks, as [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) did.

[Write-up, test and patch](API-02-addnode-trace-node-not-shifted/)

### API-03: EN_setlinktype to CVPIPE keeps the link's controls and rules

A check valve cannot be controlled, and the parser, `EN_addcontrol` and `EN_addrule` all refuse it with error 207. Yet `EN_setlinktype(..., EN_CVPIPE, EN_UNCONDITIONAL)` on a controlled pipe returns 0 and leaves the control and the rule action in place, although the documentation says unconditional mode deletes them. A file saved afterwards cannot be opened: error 200, with two error 207 messages in the report.

A change between pipe and check-valve pipe takes a shortcut that only changes `Link.Type`. The other type changes delete and re-add the link, which removes its controls.

**Fix:** in that shortcut, delete the simple controls and rules that set the pipe.

[Write-up, test and patch](API-03-setlinktype-cv-keeps-controls/)

### API-04: EN_settankdata with diameter 0 turns a tank into a reservoir

`EN_settankdata` accepts a diameter of 0 when no volume curve is given. The tank's area becomes 0, which EPANET reads as a reservoir: in the test the tank holds a fixed head of 110 ft for 4 hours while an identical tank drains to 104.59 ft, and every later tank setter returns 263, so the change cannot be undone. `EN_setnodevalue(EN_TANKDIAM, 0)` refuses the same value with 209, and the documentation does not mention the conversion.

The function only rejects a negative diameter, and the node type is derived from `Tank.A == 0`. The maintainer ruled in 2021 that this is a feature, kept for backward compatibility, and an input file does the same.

**Fix:** document the conversion in the `EN_settankdata` reference; rejecting it with 209 is the alternative.

[Write-up, test and patch](API-04-settankdata-diameter-zero/)

### API-05: Structural edits do not invalidate the saved hydraulics

A toolkit program that solves a network, edits it and re-runs only the water quality step gets results computed from hydraulics saved for the old network, read out of alignment. `EN_solveQ` returns 0 after each of the six editing functions. In the test, after one of two parallel pipes is deleted, the pipe that carries J3's whole 300 gpm reads 41.7 gpm, the old flow of the other parallel pipe.

`EN_openQ` only checks `outfile.SaveHflag`, and none of the editing functions clears it. `readhyd()` then reads each saved record with the new node and link counts.

**Fix:** clear `SaveHflag` in `EN_addnode`, `EN_deletenode`, `EN_addlink`, `EN_deletelink`, `EN_setlinktype` and `EN_setlinknodes`, so `EN_solveQ` returns 104 until the hydraulics are solved again.

[Write-up, test and patch](API-05-stale-hydraulics-after-edit/)

### API-06: EN_addlink accepts a link from a node to itself

`EN_addlink` accepts a link whose start and end node are the same, although the input parser and `EN_setlinknodes` reject it with error 222. In the test network, the next `EN_solveH` reads past a 16-byte heap buffer while it orders the nodes for the sparse solver, and a file saved from such a project fails to open with error 222.

`EN_addlink` checks that both end nodes exist, and for valves that the connection is legal, but never compares the two nodes.

**Fix:** return 222 when the two end nodes are the same, as upstream did after this review's commit.

[Write-up, test and patch](API-06-addlink-self-loop/)

### API-07: EN_deletelink does not decrement the pipe count for check-valve pipes

Deleting a pipe with a check valve leaves EPANET's pipe count one too high. The count is only used in the input summary of a report, so the visible effect is a wrong "Number of Pipes" line: in the test, a network with 3 pipes still reports 3 after one is deleted.

The fix for [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790) added the decrement with `linkType == PIPE`. A check-valve pipe has type `CVPIPE` (0), which `EN_addlink` and the parser count as a pipe with `<= PIPE`.

**Fix:** use `linkType <= PIPE`, as `EN_addlink` does.

[Write-up, test and patch](API-07-deletelink-cv-pipe-count/)

### API-25: EN_report after a structural edit prints misaligned results

`EN_solveQ` saves each reporting period's results to the binary output file, and `EN_report` turns them into tables. If the network is edited in between, `EN_report` returns 0 and prints values that belong to other links or other variables. In the test, after one of two parallel pipes is deleted, pipe P1 is listed with a velocity of 300.00 fps, which is another pipe's old flow, and the remaining pipes show their neighbours' flows.

`EN_report` only checks `outfile.SaveQflag`, which no editing function clears, and `writeresults()` reads the file with the new node and link counts.

**Fix:** clear `SaveQflag` alongside `SaveHflag` in the six editing functions, so `EN_report` returns 106 until the network is solved again.

[Write-up, test and patch](API-25-report-misaligned-after-edit/)

### API-27: Adding or deleting nodes and links leaves results attached to the wrong objects

A program that reads results between runs, as a GUI does, gets values that belong to other objects after the network is edited. Results are kept in arrays indexed like the nodes and links, and the editing functions renumber nodes and links without moving their results. In the test, after a junction is added, reservoir R1 reads tank T1's 120 ft and T1 reads uninitialised memory. After junction J1 is deleted, three nodes and a pipe read their neighbours' values. After pipe P1 is deleted, P2 reads P1's 1712.31 gpm.

`EN_addnode`, `EN_deletenode` and `EN_deletelink` shift the `Node` or `Link` array, but not the head, demand, quality, flow, status and setting arrays, and `EN_addnode` never writes the new node's slot.

**Fix:** shift the result arrays with the nodes and links, and set a new node's results to 0.

[Write-up, test and patch](API-27-results-on-wrong-objects-after-edit/)

### API-29: EN_addlink leaves a new pump's diameter and a new link's flow, status and setting unset

`EN_addlink` places a new link in the last slot of the link arrays, which after a deletion still holds another link's values, and it does not overwrite all of them. A pump made with `EN_setlinktype` keeps the diameter of the pipe whose slot it reused: the report shows 8.00 in for a pump, where the parser gives pumps 0. A pressure-reducing valve added after a run reads the deleted pipe's 100 gpm, an open status and a 43.33 psi setting before it has run, and the stale flow becomes its starting flow.

`EN_addlink` sets `Link.Diam` only for pipes and valves, and never writes the new link's entries in the solver's flow, status and setting arrays.

**Fix:** set a new pump's diameter to 0, and start every new link with no flow at its initial status and setting.

[Write-up, test and patch](API-29-addlink-leaves-new-link-fields-unset/)

## Rules

### API-08: A bad EN_addrule string deletes the previous rule

EN_addrule deletes rule number Nrules whenever the text has an error, assuming the text started a new rule. If the error is in the first clause or in the RULE line itself, the previous rule is deleted instead, and with no rules the count drops to -1, after which new rules are stored where nothing can see them. A string that starts with DISABLED returns 0 and disables the previous rule.

The parser starts EN_addrule in the state that follows a complete rule, so DISABLED applies to the last rule, and Nrules only grows once a valid RULE line has been read.

**Fix:** return 221 unless the first clause is RULE, and delete a rule only if this call added one.

[Write-up, test and patch](API-08-bad-rule-deletes-previous-rule/)

### API-09: One malformed rule in a file drops every rule after it

EN_openX opens an input file with errors and is meant to drop only the bad items. In [RULES] it keeps only the rules before the first bad one; every later rule is skipped without a message. In the test, a file with five rules, two of them malformed, loads one rule instead of three. A RULE line with a bad label also deletes the rule before it, and a clause before the first RULE line leaves the rule count at -1.

ruledata() returns at once for every line once a rule has an error, and only the unreachable RULE case resets the state. The caller then deletes rule Nrules, which is the previous rule when the error was on a RULE line.

**Fix:** skip only up to the next RULE clause, start the new rule before checking its label, and ignore deleterule(0).

[Write-up, test and patch](API-09-malformed-rule-drops-later-rules/)

### API-10: reindextanks shifts rule premises once per tank

EN_openX opens files that have errors. If a junction line fails, for example with a duplicate ID, reindextanks() moves the tanks and reservoirs down to close the gap in the Node array. It moves each rule premise on a tank once per tank instead of once. In the test, one failed line and three tanks and reservoirs move a premise on tank T2 (node 4) to reservoir R1 (node 2), and the rule silently tests the wrong level.

adjusttankrules() shifts every tank premise at once, but reindextanks() calls it inside its loop over tanks.

**Fix:** move the adjusttankrules() call after the loop.

[Write-up, test and patch](API-10-reindextanks-shifts-premises-repeatedly/)

## Setters and getters

### API-11: Bulk setters read every node before writing, so source and tank properties cannot be set

`EN_setnodevalues` (new in PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908), not yet released) sets one property on every node, undoing everything on the first error. It stops at node 1, which is a junction in practically every network, for two groups of properties. Tank properties are rejected for a junction (error 263, or 209 for the placeholder value `EN_getnodevalues` returns), and source properties cannot be read for a node that has no source (error 240). So tank and source properties can never be bulk-set, not even by reading all values, changing the tank entries and writing them back.

Each node is read and then written, whether or not the property applies to it, and a missing source counts as an error. The undo re-runs the setter, which repeats side effects such as removing a tank's volume curve.

**Fix:** write (and undo) only values that change, treat a node without a source as having none, give it one only for a non-zero value, and remove any source the call created when it fails.

[Write-up, test and patch](API-11-bulk-setters-read-before-write/)

### API-12: EN_SOURCEMASS returns cumulative mass x 60, not a rate

`EN_SOURCEMASS` is documented as the source's computed mass inflow, in mass/minute. It actually returns the total mass the source has added since the start of the run, multiplied by 60, so it grows without limit and has no meaningful unit. A constant 60 mg/min booster reads 216,000 after one hour and 864,000 after four.

EPANET 2.0 and 2.1 divided the source's accumulated mass by the step length after each quality step. The v2.2 rewrite of the quality solver dropped that step, and `findsourcequal()` now only adds to `Smass`.

**Fix:** set `Smass` to the mass added over the latest quality step divided by the step length (0 when the source adds nothing), so the getter's `Smass * 60` is again mass/minute.

[Write-up, test and patch](API-12-sourcemass-cumulative/)

### API-13: EN_settimeparam(EN_REPORTSTART) validates the old value

`EN_settimeparam(EN_REPORTSTART)` checks the report start the project already has against the duration, not the new value. A report start after the end of the run is accepted, and from then on every value, including a valid 0, is refused with 213 until the duration is changed.

With no reported period, `STATISTIC AVERAGE` divides by zero periods in `savetimestat()` and writes NaN to the output file. Validating the new value closes one route to zero periods; a run that halts before `REPORT START` is another, and it also writes NaN.

**Fix:** test the new value, and compute the time statistic only when at least one period was reported.

[Write-up, test and patch](API-13-reportstart-validates-old-value/)

### API-14: EN_INITSTATUS is cast to char before the range check

`EN_setlinkvalue` with `EN_INITSTATUS` or `EN_STATUS` must refuse anything other than `EN_CLOSED` (0) and `EN_OPEN` (1). It rounds the value and casts it to `char` before the check, so 256 and 257 wrap to 0 and 1 and are accepted with return code 0. Setting an open pipe's status to 256 closes it.

The check `if (s < 0 || s > 1)` runs on the `char` copy, which keeps only the low 8 bits.

**Fix:** check the value itself before the conversion.

[Write-up, test and patch](API-14-initstatus-char-cast/)

### API-15: EN_SETTING on a pipe ignores the roughness error

For a pipe, `EN_INITSETTING` and `EN_SETTING` are its roughness, and `EN_setlinkvalue` passes the value on to the `EN_ROUGHNESS` case. That case refuses a roughness of 0 or less with error 211, but the setting cases drop the error and return 0, so the caller believes a roughness of -5 was applied.

Since a 2025 change that added `InitSetting`, the pipe branch calls `EN_setlinkvalue(..., EN_ROUGHNESS, ...)` without keeping its return code. In 2.2 it returned it.

**Fix:** return the error code of the roughness assignment.

[Write-up, test and patch](API-15-pipe-setting-ignores-roughness-error/)

### API-16: EN_CHECKFREQ, EN_MAXCHECK below 1 and tiny viscosity are accepted but cannot be reloaded

`EN_setoption` accepts option values that `EN_saveinpfile` cannot write in a form the parser reads back. `CHECKFREQ` or `MAXCHECK` of 0.25 is stored as 0 and the saved file fails to open with error 200. The parser reads a `VISCOSITY` of 0.001 or less as an absolute viscosity, so a relative viscosity of 0.0005 set through the API comes back 45 times that of water, and 1e-10 makes the file fail to open. Diffusivity has the same problem below 0.0001.

`EN_setoption` truncates `CHECKFREQ` and `MAXCHECK` without a lower bound, and only checks that viscosity and diffusivity are not negative, while the writer always saves the relative value.

**Fix:** refuse these values with 213, as `EN_TRIALS` already does below 1.

[Write-up, test and patch](API-16-options-not-reloadable/)

### API-17: EN_gettimeparam(EN_HALTFLAG) always returns 0

`EN_gettimeparam(EN_HALTFLAG)` is documented as the flag that says whether the simulation was halted, for example by `UNBALANCED STOP`. It always returns 0. In the test, Net1 with one trial allowed halts at t = 0 of a 24-hour run, and the flag still reads 0. This was reported in 2015 and closed without a fix.

The `EN_QTIME` case has no `break` and falls through into an `EN_HALTFLAG` case that has no body, so the value stays at the 0 it was initialised to.

**Fix:** add the missing `break` and return `hyd->Haltflag` for `EN_HALTFLAG`.

[Write-up, test and patch](API-17-haltflag-always-zero/)

### API-18: EN_TANKLEVEL returns the initial level but is documented as current

`EN_TANKLEVEL` is documented as the "current computed tank water level (read only)" and listed among computed results, but `EN_getnodevalue` returns the tank's initial level and `EN_setnodevalue` sets it. A tank that drains from 10 ft to 9.18 ft still reports 10 ft. In 2021 the maintainer ruled in PR [#633](https://github.com/OpenWaterAnalytics/EPANET/issues/633) that it is meant to be the initial level and that the documentation should be corrected; it never was. For a reservoir, setting `EN_TANKLEVEL` to 60 sets its head, but reading it back gives 0.

The getter returns `H0 - El`, which only the setters change, and which is always 0 for a reservoir because the setter sets both `H0` and `El` to the head.

**Fix:** document `EN_TANKLEVEL` as the initial level (current level is `EN_HEAD` minus `EN_ELEVATION`), and return a reservoir's head from the getter.

[Write-up, test and patch](API-18-tanklevel-returns-initial-level/)

### API-19: EN_SP_GRAVITY does not rescale stored emitter and PDA values

EPANET stores pressures as heads in feet. `EN_setoption(EN_SP_GRAVITY)` changes the psi-to-feet factor but not the heads already stored for values entered in psi, kPa or bar, so those values silently change. A network read with specific gravity 1 and then set to 2 has its 40 psi PRV holding 80 psi, a required PDA pressure of 40 psi instead of 20, a control level of 60 psi instead of 30, and an emitter coefficient of 7.07 instead of 10. The same network read from a file with `SPECIFIC GRAVITY 2` has the values as written.

The `EN_SP_GRAVITY` case rescales `Ucf[PRESSURE]` only. Emitter coefficients, `Pmin`/`Preq`, valve settings and control levels keep the heads computed with the old gravity.

**Fix:** scale those stored heads by the ratio of the old to the new gravity, so the API behaves like the input file.

[Write-up, test and patch](API-19-specific-gravity-call-order/)

### API-20: Reads before a run return solver scratch values

Programs often read values back before running a model. Right after `EN_setnodevalue(EN_EMITTER)`, `EN_EMITTERFLOW` reads 448.83 gpm (1 cfs) although nothing has been computed, and a tank's `EN_TANKVOLUME` before `EN_initH` is negative: -196,350 ft³ for a tank that holds 19,635 ft³. `EN_HEAD` and `EN_PRESSURE` of the tank are wrong for the same reason.

The emitter setter seeds the solver's emitter flow with 1 cfs (added by PR [#797](https://github.com/OpenWaterAnalytics/EPANET/issues/797) for an emitter added during a run), and the tank getters use `NodeHead`, which stays 0 until `inithyd()` sets the initial heads.

**Fix:** seed the emitter flow only while the solver is open, and set each tank's head to its initial head when the network is loaded and in `EN_settankdata`.

[Write-up, test and patch](API-20-reads-before-run-return-scratch-values/)

### API-26: EN_STATUS, EN_SETTING and EN_DEMAND read leftover values before a run

A program that opens a model and reads it before running gets values the solver has not set. Every link reads as closed (`EN_STATUS` 0, because the status array is still zero from `calloc`), pumps and valves read a setting of 0, and `EN_DEMAND` returns the parser's scratch value: a junction with 100 gpm reads 44,883 gpm, and one listed in `[DEMANDS]` reads -4.5e12 gpm.

The link status and setting arrays are only filled by `inithyd()`, and the parser uses the solver's demand array as a marker while reading `[JUNCTIONS]` and `[DEMANDS]` and never clears it.

**Fix:** when the network is loaded, give each link its initial status and setting and clear the junction demands; document that flows read between `EN_initH` and `EN_runH` are starting values.

[Write-up, test and patch](API-26-link-status-before-run/)

## Running a simulation

### API-21: Controls disabled with link index 0 still cut time steps

`EN_setcontrol` accepts a link index of 0 to de-activate a control. The control then never acts, but the solver still stops at its trigger time. In the test, a de-activated TIMER control at 0:30 makes Net1 solve at 0:00, 0:30, 1:00, ... instead of every hour, which changes every later result slightly.

`controls()` skips controls whose link index is 0, but `controltimestep()` does not. It shortens the step and compares the control's status with that of the unused link slot 0, which never matches.

**Fix:** skip controls with a link index of 0 in `controltimestep()`, as `controls()` does.

[Write-up, test and patch](API-21-disabled-controls-cut-time-steps/)

### API-22: API TIMEOFDAY controls are not reduced modulo 24 h

A time-of-day control added with `EN_addcontrol` or `EN_setcontrol` for a clock time of 24 hours or more, such as 24:00 for midnight, never fires and no error is returned. In the test, a control to close a pipe at midnight in a run that starts at 8:00 AM should act 16 hours in; it never does. Saved with `EN_saveinpfile` and read back, the same control fires at midnight.

The parser reduces clock times modulo 86400 s, but `setcontrol()` stores the time as given, and the solver compares it with the time of day, which is always below 86400 s. The modulo was lost when the API code was moved into `setcontrol()` in 2023.

**Fix:** reduce `TIMEOFDAY` times modulo `SECperDAY` in `setcontrol()`, as the parser does.

[Write-up, test and patch](API-22-timeofday-controls-not-wrapped/)

### API-23: nexthyd overwrites a savehyd write error

When a write to the scratch hydraulics file fails, for example because the disk is full, `EN_nextH` and `EN_solveH` still return 0. In the test, one write fails during the 4th hour: the hydraulics file ends up 3,532 bytes short, and `EN_solveQ` reads it from the wrong positions and returns 0. On Linux, if the disk stays full, the error only shows up later as error 307 from `EN_solveQ`.

`nexthyd()` stores the result of `savehyd()` and then overwrites it with the result of `savehydstep()`, which writes 4 bytes that usually succeed. `savehyd()` itself checks only the last of its six writes and never checks `fflush()`.

**Fix:** keep the first error in `nexthyd()` with `ERRCODE`, and check `ferror()` after the flush in `savehyd()` and `savehydstep()`.

[Write-up, test and patch](API-23-nexthyd-drops-save-error/)

### API-28: Control times added through the API are truncated, not rounded

A toolkit program that computes a control time in floating point can get a control that acts a second early. Closing a pipe after 8.2 hours, passed as `8.2 * 3600` = 29519.999999999996 s, is stored as 29519 s, and the pipe closes at 8:11:59; a clock time of 4:20 AM computed as `(4 + 20/60.0) * 3600` acts at 4:19:59. `EN_getcontrol` returns the short time, and saving the project loses another second. A time of 1e300 s or NaN is converted anyway, which is undefined behaviour, and accepted.

`setcontrol()`, used by `EN_addcontrol` and `EN_setcontrol`, converts the time with `(long)level`, which truncates and has no range check. Up to v2.2 the API rounded; the rounding was lost when the code moved into `setcontrol()` in 2023.

**Fix:** refuse times that are negative, not finite or at least half the range of a `long` with error 202, then round to the nearest second.

[Write-up, test and patch](API-28-control-times-truncated/)

## Side effects on the host program

### API-24: The library changes the host program's numeric locale

Pull request [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919), not yet released, made `EN_createproject` call `setlocale(LC_NUMERIC, "")` so that the report uses the system's decimal separator, and made `EN_saveinpfile` switch to "C" and back to "". `setlocale` is process-wide: under a German or French environment, a host program's own `printf("%.2f", 1.25)` prints `1,25` after it creates a project, and its `strtod("2.5")` returns 2. Other threads see the switches too. It also sends `getfloat()` down its slow path: `EN_open` on a 62,500-junction grid takes 163 ms instead of 99 ms.

`EN_createproject`, the legacy `createtmpfiles` and `saveinpfile` call `setlocale` instead of changing the locale of the calling thread for the duration of the work.

**Fix:** two helpers change the calling thread's numeric locale only (`uselocale` on POSIX, `_configthreadlocale` on Windows) and restore it: "C" around the input-file writer, the system's locale around the API calls that write numbers to the report.

[Write-up, test and patch](API-24-library-changes-host-locale/)

### API-30: EN_deleteproject leaves the report file open after a failed EN_open

When an input file has errors, EN_open writes the messages to the report file and returns 200 without opening the project. EN_deleteproject then calls EN_close only for an open project, so the report file is never closed: it stays empty on disk until the program exits (0 bytes instead of 655 in the test), and each failed open leaks a file handle and the network data. With the open-file limit at 64, the 60th failed open returns 303.

EN_deleteproject tests Openflag before calling EN_close, but openproject() opens the report and allocates the network before it knows whether the input is valid.

**Fix:** always call EN_close in EN_deleteproject; [CRASH-14](../2-crashes/CRASH-14-calls-after-close/) makes a second EN_close harmless.

[Write-up, test and patch](API-30-report-file-left-open-after-failed-open/)
