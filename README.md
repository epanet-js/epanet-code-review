# EPANET 2.3 bugs found in the code review

Sep 30, 2026 · @Luke

## Summary

The review of the EPANET 2.3 engine (`dev` at [83e25bb](https://github.com/OpenWaterAnalytics/EPANET/commit/83e25bbe2b0b460803e71df03bc989afe9557ea3)) found 91 distinct bugs, and writing tests and fixes for them found 18 more: 109 in all. The most serious are the 29 that change simulation results without any error or warning (25 in the simulation itself and 4 in water quality), and 31 crashes or invalid memory accesses.

Every issue has its own folder with a write-up, a test program, input files and a fix patch:

- Each test shows the bug on 83e25bb and passes with its patch. Three patches only correct documentation, so their tests pass either way.
- All 109 patches also apply together, and EPANET's own unit tests pass with each patch and with all of them.

[verification.md](verification.md) has the results, and [Reproducing the issues](reproducing.md) explains how to run the tests.

**How they were found.** The engine was built with AddressSanitizer and UndefinedBehaviorSanitizer and run against the Boost tests, every bundled network and two fuzzers: 1,500 mutated input files and random API call sequences. Each module was then reviewed by hand, and every candidate was re-checked independently by reading the code and running a repro. Finally, each finding was cross-checked against upstream issues, pull requests and branches. Writing a test and a fix for every finding then turned up the 18 further bugs; [id-map.md](id-map.md) marks them as new.

**Suggested order for the release:**

1. Wrong results that users see without any error: [WQ-02](5-water-quality/WQ-02-check-valve-pipes-have-no-volume/), [SIM-02](1-wrong-results/SIM-02-gpv-pbv-flow-through-empty-tank/), [WQ-03](5-water-quality/WQ-03-fifo-lifo-tanks-book-inflow-as-overflow/), [SIM-23](1-wrong-results/SIM-23-output-headloss-float-precision/), [SIM-03](1-wrong-results/SIM-03-si-power-pumps-converted-twice/), [SIM-06](1-wrong-results/SIM-06-demand-charge-squared/)/[SIM-07](1-wrong-results/SIM-07-energy-averages-computed-in-place/), [SIM-08](1-wrong-results/SIM-08-reservoir-level-controls-always-fire/), [SIM-12](1-wrong-results/SIM-12-hydraulic-step-under-10s/), [FILE-01](4-files/FILE-01-saved-files-disable-quality-report/), [WQ-01](5-water-quality/WQ-01-fractional-reaction-order-nan/).
2. Bugs introduced after v2.3.5, before they ship: [API-24](3-api/API-24-library-changes-host-locale/), and [CRASH-15](2-crashes/CRASH-15-bulk-setters-null-badindex/) and [API-11](3-api/API-11-bulk-setters-read-before-write/) in the bulk setters. The [SIM-04](1-wrong-results/SIM-04-leak-expansion-units/) documentation fix also belongs here, while the expanded leakage docs from [#906](https://github.com/OpenWaterAnalytics/EPANET/issues/906) are unreleased.
3. The [crashes](2-crashes/README.md).
4. Everything else.

## Upstream changes since the review

Upstream `dev` moved from 83e25bb to [8bca02d](https://github.com/OpenWaterAnalytics/EPANET/commit/8bca02d57af9225e4dd8bba4d9d51b8431a38588) on 1 October 2026:

- [#940](https://github.com/OpenWaterAnalytics/EPANET/pull/940) fixes [SIM-01](1-wrong-results/SIM-01-tank-keeps-supplying-after-empty/) and [API-06](3-api/API-06-addlink-self-loop/), with the same changes as the patches here. Both had been reported independently after the review, as [#936](https://github.com/OpenWaterAnalytics/EPANET/issues/936) and [#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937).
- [#941](https://github.com/OpenWaterAnalytics/EPANET/pull/941) validates tank volume curves ([#883](https://github.com/OpenWaterAnalytics/EPANET/issues/883)), which this review did not cover.

Against 8bca02d:

- SIM-01 and API-06 pass without a patch.
- The other 107 tests give the same results as on 83e25bb.
- All 107 remaining patches still apply, alone and together. With them applied, every test and EPANET's unit tests pass.

The write-ups' source links and line numbers refer to 83e25bb.

## How this is organised

| Category | Issues | |
|---|---|---|
| [Wrong simulation results](1-wrong-results/README.md) | 25 | `SIM-01` to `SIM-25` |
| [Crashes and invalid memory access](2-crashes/README.md) | 31 | `CRASH-01` to `CRASH-31` |
| [Toolkit API behaviour](3-api/README.md) | 30 | `API-01` to `API-30` |
| [Input files, saved files and the output file](4-files/README.md) | 19 | `FILE-01` to `FILE-19` |
| [Water quality](5-water-quality/README.md) | 4 | `WQ-01` to `WQ-04` |

Each category page describes its issues in a few paragraphs. Each issue folder has the full write-up (`README.md`), the test (`<ID>_test.c`), its input files (`<ID>_*.inp`) and the fix (`<ID>_fix.patch`). The `epanet` folder is a submodule holding EPANET at the reviewed commit. [Reproducing the issues](reproducing.md) explains how to run the tests, and [verification.md](verification.md) records the results. Earlier drafts of this review used other IDs; [id-map.md](id-map.md) maps them to the current ones.

## Wrong simulation results (25)

These change the numbers in a report or read through the toolkit, and nothing warns the user.

| ID | Bug | What happens | Fix |
| --- | --- | --- | --- |
| [SIM-01](1-wrong-results/SIM-01-tank-keeps-supplying-after-empty/) | Sign error in the "tank empties within the next second" test (`tanklevels`) | A draining tank keeps supplying for up to a full step after it is empty: 526 ft³ more than the tank held in the test | `V + NodeDemand <= Vmin` (fixed upstream by [#940](https://github.com/OpenWaterAnalytics/EPANET/issues/940)) |
| [SIM-02](1-wrong-results/SIM-02-gpv-pbv-flow-through-empty-tank/) | GPV and PBV ignore the solver's TEMPCLOSED status | Flow passes through an empty tank while the tank level never changes, and the report's flow balance no longer balances | Test `<= CLOSED` as the other coefficient routines do |
| [SIM-03](1-wrong-results/SIM-03-si-power-pumps-converted-twice/) | SI constant-power pumps converted from kW twice (regression from PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758)) | 34 % too much head; power grows 1.341x on each save and reload | Keep one internal unit |
| [SIM-04](1-wrong-results/SIM-04-leak-expansion-units/) | Leak expansion is per metre of head in both unit systems; the docs say per ft or m | US users following the docs get 0.3048x the leakage | Correct the docs; the code matches the maintainer's stated design |
| [SIM-05](1-wrong-results/SIM-05-emitter-coefficient-pressure-units/) | Emitter coefficient re-converted with the pressure-unit factor | With kPa pressure units, setting the emitter exponent changes the coefficient from 10 to 26.26 | Use the parser's conversion |
| [SIM-06](1-wrong-results/SIM-06-demand-charge-squared/) | Demand charge multiplied by the price twice, and compounds across runs | Demand charge 386.83, then 773.66, then 1547.31, instead of 193.42 | Keep `Emax` unscaled; reset it in `inithyd` |
| [SIM-07](1-wrong-results/SIM-07-energy-averages-computed-in-place/) | Energy averages computed in place | A second `EN_solveQ` reports 240 % pump usage and 130 % efficiency | Compute into locals |
| [SIM-08](1-wrong-results/SIM-08-reservoir-level-controls-always-fire/) | Level controls on a reservoir compare volumes, and a reservoir's volume never changes | A BELOW or ABOVE control fires at every step, whatever the head | Compare the reservoir's head with the control grade |
| [SIM-09](1-wrong-results/SIM-09-report-start-mislabels-rows/) | REPORT START not a multiple of REPORT STEP | Every row is labelled with the wrong time | Align the first reporting time |
| [SIM-10](1-wrong-results/SIM-10-pattern-start-boundary-missed/) | PATTERN START not a multiple of PATTERN STEP | Pattern changes are missed until the next hydraulic step | Subtract `Pstart` in the step calculation |
| [SIM-11](1-wrong-results/SIM-11-duration-overshoot/) | Duration not a multiple of the hydraulic step | An extra solution past the end; 133 % pump usage | Clip the last step |
| [SIM-12](1-wrong-results/SIM-12-hydraulic-step-under-10s/) | Hydraulic step under 10 s | Quality step and rule step become 0: a quality run never ends; division by zero in rule timing | `MAX(1, Hstep/10)` |
| [SIM-13](1-wrong-results/SIM-13-pcv-curve-extrapolation/) | PCV curve extrapolation uses 1 instead of 100 % (regression from PR [#747](https://github.com/OpenWaterAnalytics/EPANET/issues/747)) | Wrong head loss; a two-point curve gives NaN and a fully open valve | Use 100 |
| [SIM-14](1-wrong-results/SIM-14-pda-kpa-minimum-pressure-difference/) | PDA minimum pressure difference in mixed units with kPa | Two different demand curves; the run fails to converge after 211 trials | Drop the redundant clamp |
| [SIM-15](1-wrong-results/SIM-15-pressure-exponent-zero/) | `PRESSURE EXPONENT 0` accepted | All results NaN, return code 0 | Reject at load |
| [SIM-16](1-wrong-results/SIM-16-negative-pump-speed-nan/) | Negative pump-speed pattern factor | NaN that persists for the rest of the run | Treat a negative speed as closed |
| [SIM-17](1-wrong-results/SIM-17-tank-volumes-stale-after-edit/) | `EN_MINLEVEL` or `EN_setcurve` leave tank volumes stale | Tank level jumps 13 ft instead of 3 ft in the first hour | Recompute the volumes |
| [SIM-18](1-wrong-results/SIM-18-setqualtype-scales-initial-quality/) | `EN_setqualtype` scales initial quality by 1/28.3 switching to CHEM | 0.5 becomes 0.0177; confirmed in a real chlorine run | Convert in both directions |
| [SIM-19](1-wrong-results/SIM-19-gpv-curve-type-not-set/) | GPV curves set through the API never typed as head-loss curves | `EN_setflowunits` does not convert them | Type the curve in the setter |
| [SIM-20](1-wrong-results/SIM-20-volcurve-area-user-units/) | `EN_VOLCURVE` stores the tank area in user units | Wrong diameter, also in the saved file | Convert as `EN_settankdata` does |
| [SIM-21](1-wrong-results/SIM-21-gpv-curve-index-three-copies/) | A GPV's curve index is stored in three places; one is kept current | After deleting a curve the valve uses a different one | Have the solver read one copy |
| [SIM-22](1-wrong-results/SIM-22-curve-change-while-solver-open/) | Changing a GPV or pump curve while the solver is open (regression of [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550)) | Getters show the new curve; the solver keeps the old one | Apply the change, as 2.2 did |
| [SIM-23](1-wrong-results/SIM-23-output-headloss-float-precision/) | Output-file head loss computed from heads rounded to 4-byte floats | On Net2, 25 % of head losses off by more than 1 %; 17.5 % stored as exactly 0 on a 33k-junction model | Compute in double precision before rounding |
| [SIM-24](1-wrong-results/SIM-24-deletecurve-leaves-tank-volumes/) | `EN_deletecurve` leaves a tank's volumes computed from the deleted curve | A tank draining 20 gpm falls 7.90 ft in an hour instead of 0.40 ft | Recompute the volumes, as `EN_TANKDIAM` does |
| [SIM-25](1-wrong-results/SIM-25-pump-power-change-while-solver-open/) | `EN_PUMP_POWER` changed while the solver is open (regression from PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758)) | The new power is ignored; a curve pump switched to constant power adds no head | Recompute the pump's parameters, as 2.2 did |

## Crashes and invalid memory access (31)

All 31 were reproduced: 30 under AddressSanitizer or UBSan, and CRASH-13 through its effects, because macOS never frees a closed file's structure. Each needs a bounds check or an early error code.

| ID | Trigger | Failure | Reached from |
| --- | --- | --- | --- |
| [CRASH-01](2-crashes/CRASH-01-long-token-error-message/) | Invalid token over 255 characters, e.g. a long ID | Buffer overrun in the error message; 66 of 1,500 fuzzed files. Tokens of about 200 characters silently overwrite the project title. | File |
| [CRASH-02](2-crashes/CRASH-02-clock-time-four-fields/) | Clock time with 4+ fields, e.g. `DURATION 1:2:3:4` | Write past a 3-element array in `hour()` | File |
| [CRASH-03](2-crashes/CRASH-03-bare-priority/) | Bare `PRIORITY` with no value in a rule | Null dereference in `strtod` | File or API |
| [CRASH-04](2-crashes/CRASH-04-inf-nan-time-values/) | `inf`, `nan` or huge values in \[TIMES\] or \[OPTIONS\] | Undefined float-to-integer conversion; time arithmetic overflows | File |
| [CRASH-05](2-crashes/CRASH-05-report-precision-overflow/) | A large `PRECISION` in \[REPORT\] or `EN_setreport` (12 overflows for any value of 100 or more) | Stack overflow in a 16-byte format buffer | File or API |
| [CRASH-06](2-crashes/CRASH-06-gpv-one-point-curve/) | GPV head-loss curve with one point | Heap read out of bounds in `curvecoeff` | File or API |
| [CRASH-07](2-crashes/CRASH-07-quality-isolated-component/) | Network with an isolated component, in a quality run | Heap overflow: `SortedNodes` sized by links, filled by nodes | File or API |
| [CRASH-08](2-crashes/CRASH-08-long-names-unterminated/) | A name over its length limit, e.g. a \[DEMANDS\] category name over 31 characters | Stored without a terminator; the getter and `EN_saveinpfile` read past it | File or API |
| [CRASH-09](2-crashes/CRASH-09-saveinp-rule-value-overflow/) | `EN_saveinpfile` with a rule value of 1e15 or more | 20-byte stack buffer overflow; found on a real benchmark model | API |
| [CRASH-10](2-crashes/CRASH-10-adjacency-lists-freed-by-both-solvers/) | `EN_closeQ`, then `EN_runH` with any hydraulic warning; or `EN_openQ`, `EN_closeH`, `EN_runQ` | Null dereference: both solvers free the shared adjacency lists (regression from the [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) fix) | API |
| [CRASH-11](2-crashes/CRASH-11-closeq-not-idempotent/) | `EN_solveQ` then `EN_closeQ` | Use after free of the segment pool | API |
| [CRASH-12](2-crashes/CRASH-12-inith-initq-shared-save-flag/) | `EN_initH` and `EN_initQ` with different save flags (one shared flag) | Null `HydFile` write; or hydraulics silently never saved | API |
| [CRASH-13](2-crashes/CRASH-13-report-closes-output-file/) | `EN_report` before `EN_closeQ` | Output file closed twice; later tables all zero | API |
| [CRASH-14](2-crashes/CRASH-14-calls-after-close/) | Rule, control, ID and link-node functions after `EN_close` or before `EN_open` | Use after free or null dereference (no `Openflag` check; counts survive `EN_close`) | API |
| [CRASH-15](2-crashes/CRASH-15-bulk-setters-null-badindex/) | `EN_setnodevalues`/`EN_setlinkvalues` failing with `badIndex` NULL (PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908), unreleased) | Null write | API |
| [CRASH-16](2-crashes/CRASH-16-gpv-without-curve/) | GPV with no curve, added through the API or from a \[VALVES\] line | Solver reads `Curve[0]` | File or API |
| [CRASH-17](2-crashes/CRASH-17-addrule-multiple-rules/) | `EN_addrule` with two or more `RULE` clauses in one string | Heap overflow (rule array grown by one) | API |
| [CRASH-18](2-crashes/CRASH-18-addrule-long-line/) | `EN_addrule` with a line of 1,024 characters or more | Stack overflow (`strcpy`) | API |
| [CRASH-19](2-crashes/CRASH-19-addrule-edits-caller-string/) | `EN_addrule` called with a string literal | Write to read-only memory (the string is edited in place) | API |
| [CRASH-20](2-crashes/CRASH-20-rule-setters-unchecked-index/) | Rule premise and action setters with an invalid object index or type | Heap read out of bounds when the rule is evaluated | API |
| [CRASH-21](2-crashes/CRASH-21-deletedemand-index-zero/) | `EN_deletedemand` with index 0 or negative | Use after free of the demand list | API |
| [CRASH-22](2-crashes/CRASH-22-setcurvevalue-out-of-range/) | `EN_setcurvevalue` with a point index past the end + 1 | Heap read out of bounds before the clamp | API |
| [CRASH-23](2-crashes/CRASH-23-setlinknodes-node-zero/) | `EN_setlinknodes` with node index 0 | Heap overflow in `disconnected()` during `EN_solveH` | API |
| [CRASH-24](2-crashes/CRASH-24-31-character-ids-unterminated/) | `EN_addnode`/`EN_addlink` with a 31-character ID | ID stored unterminated: object not found by ID; `EN_getlinkid` overruns the caller's buffer | API |
| [CRASH-25](2-crashes/CRASH-25-setflowunits-out-of-range/) | `EN_setflowunits` with an out-of-range code | Global array read out of bounds | API |
| [CRASH-26](2-crashes/CRASH-26-rule-status-on-node-variable/) | A rule premise such as `IF TANK T1 LEVEL IS OPEN` | Heap read past the link arrays at every rule check | File or API |
| [CRASH-27](2-crashes/CRASH-27-rule-labels-unterminated/) | `EN_addrule` with a rule label of 31 characters or more | Label stored unterminated; `EN_getruleID` writes 33 bytes into a 32-byte buffer | API |
| [CRASH-28](2-crashes/CRASH-28-setcurve-shrinks-gpv-curve-while-open/) | `EN_setcurve` shrinks a GPV's curve to one point while the solver is open | Heap read out of bounds in `curvecoeff` | API |
| [CRASH-29](2-crashes/CRASH-29-huge-rule-time-to-text/) | A rule time of 2^31 seconds or more | Undefined float-to-integer conversion when the rule is checked and when it is saved | File or API |
| [CRASH-30](2-crashes/CRASH-30-api-out-of-range-conversions/) | NaN or huge values in `EN_setoption`, `EN_settimeparam`, `EN_setnodevalue` or `EN_setlinkvalue`; `TRIALS` near `INT_MAX` with `UNBALANCED CONTINUE` | Undefined float-to-integer conversions; the trial counter overflows | File or API |
| [CRASH-31](2-crashes/CRASH-31-clock-time-buffer-overflow/) | A run of 1,000,000 hours or more | Clock-time text overruns a 13-byte buffer: the report file's name is overwritten, or the stack with a status report | File or API |

## Toolkit API behaviour (30)

Most of these do not crash. They leave the project in a different state from the one the call reports, or accept input the file format rejects.

| ID | Bug | What happens |
| --- | --- | --- |
| [API-01](3-api/API-01-addnode-shifts-wrong-rule-premises/) | `EN_addnode(EN_JUNCTION)` shifts rule premises after incrementing `Njuncs` | A premise on the first reservoir now points at the new junction |
| [API-02](3-api/API-02-addnode-trace-node-not-shifted/) | `EN_addnode` does not shift the trace node (fix [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) silently reverted two days later by [#831](https://github.com/OpenWaterAnalytics/EPANET/issues/831)) | A trace runs from the wrong source |
| [API-03](3-api/API-03-setlinktype-cv-keeps-controls/) | `EN_setlinktype(PIPE -> CVPIPE)` keeps the link's controls and rules | The saved file fails to reload (error 207) |
| [API-04](3-api/API-04-settankdata-diameter-zero/) | `EN_settankdata` with diameter 0 and no curve ([#647](https://github.com/OpenWaterAnalytics/EPANET/issues/647), won't fix) | The tank silently becomes a reservoir |
| [API-05](3-api/API-05-stale-hydraulics-after-edit/) | Structural edits do not invalidate the saved hydraulics | `EN_openQ` reads a misaligned scratch file; wrong quality, return code 0 |
| [API-06](3-api/API-06-addlink-self-loop/) | `EN_addlink` accepts a link from a node to itself ([#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937); fixed upstream by [#940](https://github.com/OpenWaterAnalytics/EPANET/issues/940)) | Heap overflow in `EN_openH`, or a saved file the parser refuses |
| [API-07](3-api/API-07-deletelink-cv-pipe-count/) | `EN_deletelink` does not decrement the count for check-valve pipes (incomplete fix of [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790)) | Pipe count over-reported |
| [API-08](3-api/API-08-bad-rule-deletes-previous-rule/) | A bad `EN_addrule` string deletes the previous rule | Rule count can reach -1; `"DISABLED"` alone disables the last rule |
| [API-09](3-api/API-09-malformed-rule-drops-later-rules/) | One malformed rule in a file | Every rule after it is silently dropped |
| [API-10](3-api/API-10-reindextanks-shifts-premises-repeatedly/) | `reindextanks` shifts rule premises once per tank | Premise points at the wrong node (`EN_openX` with a bad junction line) |
| [API-11](3-api/API-11-bulk-setters-read-before-write/) | Bulk setters read every node before writing (PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908), unreleased) | Source and tank properties can never be bulk-set |
| [API-12](3-api/API-12-sourcemass-cumulative/) | `EN_SOURCEMASS` returns cumulative mass x 60 | Documented as a rate; grows 216,000 per hour |
| [API-13](3-api/API-13-reportstart-validates-old-value/) | `EN_settimeparam(EN_REPORTSTART)` validates the old value | A start past the duration is accepted, then every valid value is refused |
| [API-14](3-api/API-14-initstatus-char-cast/) | `EN_INITSTATUS` cast to `char` before the range check | 256 accepted as 0, 257 as 1 |
| [API-15](3-api/API-15-pipe-setting-ignores-roughness-error/) | `EN_SETTING` on a pipe ignores the roughness error (regression in v2.3) | -5 returns 0 and changes nothing |
| [API-16](3-api/API-16-options-not-reloadable/) | `EN_CHECKFREQ`, `EN_MAXCHECK` below 1 and tiny viscosity accepted | Saved files that do not reload |
| [API-17](3-api/API-17-haltflag-always-zero/) | `EN_gettimeparam(EN_HALTFLAG)` has no body (reported as [#2](https://github.com/OpenWaterAnalytics/EPANET/issues/2) in 2015) | Always 0 |
| [API-18](3-api/API-18-tanklevel-returns-initial-level/) | `EN_TANKLEVEL` returns the initial level; documented as current | Level stays constant while a tank drains |
| [API-19](3-api/API-19-specific-gravity-call-order/) | `EN_SP_GRAVITY` does not rescale stored emitter and PDA values | Results depend on call order |
| [API-20](3-api/API-20-reads-before-run-return-scratch-values/) | Reads before a run return scratch values | Emitter flow 1 cfs; tank volume -1.7e6 on Net1 |
| [API-21](3-api/API-21-disabled-controls-cut-time-steps/) | Controls disabled with link index 0 still cut time steps | Extra steps at 1800 s |
| [API-22](3-api/API-22-timeofday-controls-not-wrapped/) | API `TIMEOFDAY` controls not reduced modulo 24 h (regression from PR [#732](https://github.com/OpenWaterAnalytics/EPANET/issues/732)) | A clock time of 24 h or more never fires |
| [API-23](3-api/API-23-nexthyd-drops-save-error/) | `nexthyd` overwrites a `savehyd` write error | Truncated hydraulics file found only later (error 307) |
| [API-24](3-api/API-24-library-changes-host-locale/) | `EN_createproject` and `EN_saveinpfile` call `setlocale` (PR [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919), unreleased) | The host program's own number formatting switches to the environment locale: `printf("%.2f", 1.25)` prints `1,25`; not thread-safe |
| [API-25](3-api/API-25-report-misaligned-after-edit/) | `EN_report` after a structural edit | Prints shifted results without an error: a "velocity" of 300.00 fps is really a flow |
| [API-26](3-api/API-26-link-status-before-run/) | `EN_STATUS`, `EN_SETTING` and `EN_DEMAND` read leftover values before a run | Every link reads closed; a 100 gpm demand reads 44,883 gpm |
| [API-27](3-api/API-27-results-on-wrong-objects-after-edit/) | Adding or deleting nodes and links leaves results attached to the wrong objects | Until the next run, nodes and links read their neighbours' heads and flows |
| [API-28](3-api/API-28-control-times-truncated/) | Control times added through the API are truncated, not rounded (regression from PR [#732](https://github.com/OpenWaterAnalytics/EPANET/issues/732)) | A control at `8.2 * 3600` s acts at 8:11:59; a time of 1e300 is undefined behaviour |
| [API-29](3-api/API-29-addlink-leaves-new-link-fields-unset/) | `EN_addlink` leaves a new pump's diameter and a new link's flow, status and setting unset | A new PRV reads a deleted pipe's 100 gpm before any run |
| [API-30](3-api/API-30-report-file-left-open-after-failed-open/) | `EN_deleteproject` after a failed `EN_open` skips `EN_close` | The report file stays empty until the program exits, and file handles leak |

## Input files, saved files and the output file (19)

Most were found by opening a file, saving it, reopening the saved file, saving again, and comparing the two saved files and about 70 properties per object.

| ID | Bug | What happens |
| --- | --- | --- |
| [FILE-01](4-files/FILE-01-saved-files-disable-quality-report/) | The writer emits `Quality NO` for the link-quality field, which has the same name as node quality | **Every saved file** turns off the node Quality column in the report when reloaded |
| [FILE-02](4-files/FILE-02-save-over-input-loses-labels/) | Saving over the input file truncates it before \[LABELS\] and \[BACKDROP\] are copied from it ([#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522), closed but still broken) | Both sections lost |
| [FILE-03](4-files/FILE-03-filltime-rules-not-reloadable/) | FILLTIME and DRAINTIME rule values written as h:mm:ss, read as numbers | Saved file fails to load (error 202) |
| [FILE-04](4-files/FILE-04-pumps-gpvs-without-curve-not-reloadable/) | Pumps with no curve or power, and GPVs with no curve, written in formats the parser rejects | Saved file fails to load (201, 206) |
| [FILE-05](4-files/FILE-05-timer-controls-lose-a-second/) | Timer controls written as `%.4f HOURS`, read back with truncation | The control loses one second per save |
| [FILE-06](4-files/FILE-06-zero-demand-categories-dropped/) | Demand categories with zero base demand are dropped on save (by design, [#702](https://github.com/OpenWaterAnalytics/EPANET/issues/702)) | Category name and pattern lost; demand count changes |
| [FILE-07](4-files/FILE-07-four-decimal-formats-truncate/) | Fixed 4-decimal formats | Small values truncated: pattern factor 5e-05 becomes 0.0001 |
| [FILE-08](4-files/FILE-08-strings-with-spaces-unquoted/) | Strings with spaces written unquoted | "Free Chlorine" reloads as name "Free", units "Chlorine" |
| [FILE-09](4-files/FILE-09-output-reader-last-period-and-units/) | Output-file reader treats the end period as exclusive but rejects it; pressure-unit codes differ from the engine's | Last period unreachable; a kPa file reads as metres |
| [FILE-10](4-files/FILE-10-nan-inf-accepted-as-numbers/) | `nan`, `inf`, hex and empty tokens accepted as numbers | NaN elevations and diameters pass every range check; "EPANET ran successfully" |
| [FILE-11](4-files/FILE-11-long-lines-split/) | Lines over 1,023 characters split in two; the "line too long" error is unreachable | Tail parsed as a new line: a phantom pattern named "0" |
| [FILE-12](4-files/FILE-12-tokens-past-40-dropped/) | More than 40 tokens on a line | Extra tokens dropped without warning: 400 factors become 39 |
| [FILE-13](4-files/FILE-13-fractional-trials-zero-iterations/) | `TRIALS 0.9` accepted | Zero solver trials; an uninitialised error value is read |
| [FILE-14](4-files/FILE-14-quoted-tokens-mis-split/) | Tokenizer length accounting after a quoted token (since 2014) | `"J1" 100 5 PAT1` gives error 205; quoted IDs with spaces read past the line |
| [FILE-15](4-files/FILE-15-valve-check-skips-previous-link/) | Illegal-valve check skips the link read just before the valve (regression from the [#561](https://github.com/OpenWaterAnalytics/EPANET/issues/561) fix) | Two consecutive PRVs sharing a downstream node load without error 220 |
| [FILE-16](4-files/FILE-16-short-link-lines-dropped/) | \[PIPES\], \[PUMPS\] and \[VALVES\] lines with too few fields (regression from PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758)) | Skipped without an error: the model runs without the link |
| [FILE-17](4-files/FILE-17-options-load-but-not-reload/) | \[OPTIONS\] values such as `CHECKFREQ 0.5` and `SPECIFIC GRAVITY 1e-7` | They load, but the saved copy fails to open or reopens with 0 |
| [FILE-18](4-files/FILE-18-rule-times-lose-a-second/) | Clock times in rule premises are truncated | `8:00:01` fires a rule step early and is saved as 8:00:00 |
| [FILE-19](4-files/FILE-19-trials-default-documented-wrong/) | The documentation gives the `TRIALS` default as 40; the code uses 200 | Users who rely on the documentation expect 40 trials |

## Water quality (4)

WQ-01, WQ-02 and WQ-03 give wrong results with no warning, and WQ-02 affects many real models.

| ID | Bug | What happens |
| --- | --- | --- |
| [WQ-01](5-water-quality/WQ-01-fractional-reaction-order-nan/) | Reaction order between 0 and 1: `pow(0, order - 1)` is infinite at zero concentration, and 0 x inf is NaN | NaN at every node, link and tank from the first hour; nothing rejects such orders |
| [WQ-02](5-water-quality/WQ-02-check-valve-pipes-have-no-volume/) | Water quality treats only `PIPE`, not `CVPIPE`, as having volume (since v2.2) | Water crosses a check-valve pipe instantly and unchanged: chlorine 1.00 instead of 0.90; age 1.00 h instead of 3.45 h |
| [WQ-03](5-water-quality/WQ-03-fifo-lifo-tanks-book-inflow-as-overflow/) | FIFO and LIFO tanks test "overflowing" against the hydraulic solver's tank volume (PR [#771](https://github.com/OpenWaterAnalytics/EPANET/issues/771)) | In a normal run that is the end-of-run volume, so every filling step of a tank that ends full is booked as overflow: final tank quality 1.00 instead of 0.00, mass balance still 1.000 |
| [WQ-04](5-water-quality/WQ-04-reaction-averages-low-with-report-start/) | Reaction totals accumulated only after REPORT START but averaged over the whole duration | Average reaction rates low by (duration - start) / duration |

## Where the bugs came from

72 of the original 91 had no related GitHub issue when the review was done, including SIM-01, SIM-02, SIM-06 to SIM-12, FILE-01, WQ-01, CRASH-07 and most of the crashes. None of the 18 found while writing them up had been reported either. The review checked all 933 issues and pull requests, and no open pull request fixed any finding.

- **Regressions from earlier changes:**
  - API-02: fix [#829](https://github.com/OpenWaterAnalytics/EPANET/issues/829) was silently reverted by [#831](https://github.com/OpenWaterAnalytics/EPANET/issues/831).
  - SIM-03, SIM-22, SIM-25 and FILE-16: PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758).
  - SIM-13: PR [#747](https://github.com/OpenWaterAnalytics/EPANET/issues/747).
  - CRASH-10: the [#789](https://github.com/OpenWaterAnalytics/EPANET/issues/789) fix.
  - API-07: the [#790](https://github.com/OpenWaterAnalytics/EPANET/issues/790) fix.
  - SIM-05: PR [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862).
  - SIM-19: PR [#903](https://github.com/OpenWaterAnalytics/EPANET/issues/903), v2.3.5.
  - FILE-15: the [#561](https://github.com/OpenWaterAnalytics/EPANET/issues/561) fix.
  - WQ-03: PR [#771](https://github.com/OpenWaterAnalytics/EPANET/issues/771).
  - API-22 and API-28: PR [#732](https://github.com/OpenWaterAnalytics/EPANET/issues/732).
  - API-15: commit 30c2077, v2.3.
  - WQ-02 and API-12: the v2.2 water-quality rewrite, PR [#297](https://github.com/OpenWaterAnalytics/EPANET/issues/297). EPANET 2.1 handled both correctly.
- **Introduced after v2.3.5, so they can be fixed before they ship:** CRASH-15 and API-11 (PR [#908](https://github.com/OpenWaterAnalytics/EPANET/issues/908)), and API-24 (PR [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919)). PR [#906](https://github.com/OpenWaterAnalytics/EPANET/issues/906) also expands the leakage documentation that SIM-04 corrects; the wording itself dates from 2024.
- **Already reported:** API-17 ([#2](https://github.com/OpenWaterAnalytics/EPANET/issues/2), 2015), API-02 ([#828](https://github.com/OpenWaterAnalytics/EPANET/issues/828)), API-04 ([#647](https://github.com/OpenWaterAnalytics/EPANET/issues/647), won't fix), API-18 ([#633](https://github.com/OpenWaterAnalytics/EPANET/issues/633)), FILE-02 ([#522](https://github.com/OpenWaterAnalytics/EPANET/issues/522)) and FILE-06 ([#174](https://github.com/OpenWaterAnalytics/EPANET/issues/174), by design).
- **Reported after the review:** SIM-01 ([#936](https://github.com/OpenWaterAnalytics/EPANET/issues/936)) and API-06 ([#937](https://github.com/OpenWaterAnalytics/EPANET/issues/937)). [#940](https://github.com/OpenWaterAnalytics/EPANET/issues/940) has since fixed both.

## Licence

The write-ups, tests, input files, patches and scripts in this repository are released by Iterating Inc. under the [MIT licence](LICENSE), the same licence as EPANET, so the fixes can be merged freely. The `epanet` submodule is EPANET itself, under its own licence.
