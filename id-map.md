# Issue IDs and earlier IDs

Every issue in this review has one ID. The ID starts with its category:

| Prefix | Category |
|---|---|
| `SIM` | [Wrong simulation results](1-wrong-results/README.md) |
| `CRASH` | [Crashes and invalid memory access](2-crashes/README.md) |
| `API` | [Toolkit API behaviour](3-api/README.md) |
| `FILE` | [Input files, saved files and the output file](4-files/README.md) |
| `WQ` | [Water quality](5-water-quality/README.md) |

Earlier drafts of this review used other IDs, and some bugs appeared under two of them. For example, "A1" meant one bug in the crash table and a different bug in the API table. This page maps the earlier IDs to the current ones:

- **Earlier ID** is the ID in the September 2026 summary. The crash table there used the review-log IDs, so this column also lists those.
- **Review log** lists the entries in the verification log that was written while each item was confirmed.

| ID | Issue | Earlier ID | Review log |
|---|---|---|---|
| [SIM-01](1-wrong-results/SIM-01-tank-keeps-supplying-after-empty/) | A draining tank keeps supplying water after it is empty | N1 | C3 |
| [SIM-02](1-wrong-results/SIM-02-gpv-pbv-flow-through-empty-tank/) | GPV and PBV ignore TEMPCLOSED and pass flow through an empty or full tank | N2 | D1 |
| [SIM-03](1-wrong-results/SIM-03-si-power-pumps-converted-twice/) | Constant-power pumps in SI input files are converted from kW twice | N3 | D3, B1, F-1 |
| [SIM-04](1-wrong-results/SIM-04-leak-expansion-units/) | Leak expansion is per metre of head in both unit systems, but the docs say per ft or m | N4 | D4 |
| [SIM-05](1-wrong-results/SIM-05-emitter-coefficient-pressure-units/) | Emitter coefficients are re-converted with the pressure-unit factor | N5 | B7, F-2 |
| [SIM-06](1-wrong-results/SIM-06-demand-charge-squared/) | Demand charge is multiplied by the price twice and compounds across runs | N6 | C4, F-4 |
| [SIM-07](1-wrong-results/SIM-07-energy-averages-computed-in-place/) | Energy averages are computed in place, so a second EN_solveQ corrupts them | N7 | C5, F-6 |
| [SIM-08](1-wrong-results/SIM-08-reservoir-level-controls-always-fire/) | Level controls on a reservoir always fire | N8 | C6 |
| [SIM-09](1-wrong-results/SIM-09-report-start-mislabels-rows/) | REPORT START that is not a multiple of REPORT STEP mislabels every row | N9 | C8 |
| [SIM-10](1-wrong-results/SIM-10-pattern-start-boundary-missed/) | PATTERN START that is not a multiple of PATTERN STEP misses pattern changes | N10 | C9 |
| [SIM-11](1-wrong-results/SIM-11-duration-overshoot/) | A duration that is not a multiple of the hydraulic step is overshot | N11 | C10 |
| [SIM-12](1-wrong-results/SIM-12-hydraulic-step-under-10s/) | A hydraulic step under 10 s hangs water quality and divides by zero in rule timing | N12 | C7, G3 |
| [SIM-13](1-wrong-results/SIM-13-pcv-curve-extrapolation/) | PCV curve extrapolation uses 1 instead of 100 % | N13 | D7 |
| [SIM-14](1-wrong-results/SIM-14-pda-kpa-minimum-pressure-difference/) | PDA with kPa units uses two different demand curves when the pressure range is narrow | N14 | D8 |
| [SIM-15](1-wrong-results/SIM-15-pressure-exponent-zero/) | PRESSURE EXPONENT 0 is accepted and gives NaN results | N15, P16 (part) | D5 |
| [SIM-16](1-wrong-results/SIM-16-negative-pump-speed-nan/) | A negative pump-speed pattern factor gives NaN for the rest of the run | N16 | D6 |
| [SIM-17](1-wrong-results/SIM-17-tank-volumes-stale-after-edit/) | EN_MINLEVEL and EN_setcurve leave tank volumes stale | N17 | B8 |
| [SIM-18](1-wrong-results/SIM-18-setqualtype-scales-initial-quality/) | EN_setqualtype scales initial quality by 1/28.3 when switching to CHEM | N18, Q6 | B9, G4 |
| [SIM-19](1-wrong-results/SIM-19-gpv-curve-type-not-set/) | GPV curves set through the API are never typed as head loss curves | N19 | B10 |
| [SIM-20](1-wrong-results/SIM-20-volcurve-area-user-units/) | EN_VOLCURVE stores the tank area in user units | N20 | B11 |
| [SIM-21](1-wrong-results/SIM-21-gpv-curve-index-three-copies/) | A GPV's curve index is stored in three places and only one is kept current | N21 | A4 (stage 1) |
| [SIM-22](1-wrong-results/SIM-22-curve-change-while-solver-open/) | Changing a GPV or pump curve while the solver is open has no effect | N22 | B12, B17 |
| [SIM-23](1-wrong-results/SIM-23-output-headloss-float-precision/) | Output-file head loss is computed from heads rounded to 4-byte floats | R7 | R2-N3 |
| [SIM-24](1-wrong-results/SIM-24-deletecurve-leaves-tank-volumes/) | EN_deletecurve leaves a tank's volumes computed from the deleted curve | New in this write-up | - |
| [SIM-25](1-wrong-results/SIM-25-pump-power-change-while-solver-open/) | Changing a pump's power while the solver is open has no effect | New in this write-up | - |
| [CRASH-01](2-crashes/CRASH-01-long-token-error-message/) | An invalid token over 255 characters overruns the error-message buffers | P17 (part); crash table F4 | F4, E1 |
| [CRASH-02](2-crashes/CRASH-02-clock-time-four-fields/) | A clock time with four or more fields writes past a 3-element array | P17 (part); crash table E2 | E2 |
| [CRASH-03](2-crashes/CRASH-03-bare-priority/) | A bare PRIORITY with no value dereferences a null token | P17 (part); crash table H3 | H3 |
| [CRASH-04](2-crashes/CRASH-04-inf-nan-time-values/) | inf, nan or huge values in [TIMES] and [OPTIONS] overflow integer conversions | crash table F5 | F5 |
| [CRASH-05](2-crashes/CRASH-05-report-precision-overflow/) | PRECISION 7 or more overflows a 16-byte report buffer | P10; crash table F-3 | F-3 |
| [CRASH-06](2-crashes/CRASH-06-gpv-one-point-curve/) | A one-point GPV head loss curve reads out of bounds | crash table D2 | D2 |
| [CRASH-07](2-crashes/CRASH-07-quality-isolated-component/) | A network with an isolated component overflows SortedNodes in a quality run | Q2; crash table G2 | G2 |
| [CRASH-08](2-crashes/CRASH-08-long-names-unterminated/) | xstrcpy leaves over-long names unterminated | R3 | R2-B2 |
| [CRASH-09](2-crashes/CRASH-09-saveinp-rule-value-overflow/) | EN_saveinpfile overflows a 20-byte buffer for rule values of 1e15 or more | P18; crash table P18 | P18 |
| [CRASH-10](2-crashes/CRASH-10-adjacency-lists-freed-by-both-solvers/) | Both solvers free the shared adjacency lists | Q4; crash table C2 and G6 | C2, G6 |
| [CRASH-11](2-crashes/CRASH-11-closeq-not-idempotent/) | EN_closeQ after EN_solveQ frees the segment pool twice | Q3; crash table G5 | G5 |
| [CRASH-12](2-crashes/CRASH-12-inith-initq-shared-save-flag/) | EN_initH and EN_initQ share one save flag | crash table C1 | C1 |
| [CRASH-13](2-crashes/CRASH-13-report-closes-output-file/) | EN_report before EN_closeQ closes the live output file | P11; crash table F-5 | F-5 |
| [CRASH-14](2-crashes/CRASH-14-calls-after-close/) | Rule, control, ID and link-node functions work on freed memory after EN_close | crash table A12 | A12 (stage 1) |
| [CRASH-15](2-crashes/CRASH-15-bulk-setters-null-badindex/) | EN_setnodevalues and EN_setlinkvalues write through a NULL badIndex | crash table F2 | F2, B6 |
| [CRASH-16](2-crashes/CRASH-16-gpv-without-curve/) | A GPV with no head loss curve crashes the solver | crash table F3 | F3, A5 (stage 1) |
| [CRASH-17](2-crashes/CRASH-17-addrule-multiple-rules/) | EN_addrule with two RULE clauses overflows the rule array | crash table A1 | A1 (stage 1), H2 |
| [CRASH-18](2-crashes/CRASH-18-addrule-long-line/) | EN_addrule with a line of 1,024 characters or more overflows the stack | crash table A3 | A3 (stage 1) |
| [CRASH-19](2-crashes/CRASH-19-addrule-edits-caller-string/) | EN_addrule edits the caller's string, so a string literal faults | A13 (part); crash table H1 | H1 |
| [CRASH-20](2-crashes/CRASH-20-rule-setters-unchecked-index/) | Rule premise and action setters accept any object index | crash table A11 | A11 (stage 1), H7 |
| [CRASH-21](2-crashes/CRASH-21-deletedemand-index-zero/) | EN_deletedemand with index 0 leaves a dangling demand list | crash table A8 | A8 (stage 1), B4 |
| [CRASH-22](2-crashes/CRASH-22-setcurvevalue-out-of-range/) | EN_setcurvevalue reads past the curve before range-checking the point index | crash table A9 | A9 (stage 1), B5 |
| [CRASH-23](2-crashes/CRASH-23-setlinknodes-node-zero/) | EN_setlinknodes accepts node index 0 | crash table A10 | A10 (stage 1), B3 |
| [CRASH-24](2-crashes/CRASH-24-31-character-ids-unterminated/) | EN_addnode and EN_addlink leave 31-character IDs unterminated | crash table A13 | A13 (stage 1) |
| [CRASH-25](2-crashes/CRASH-25-setflowunits-out-of-range/) | EN_setflowunits does not check its argument | A13 (part); crash table B2 | B2 |
| [CRASH-26](2-crashes/CRASH-26-rule-status-on-node-variable/) | A rule premise that compares a node variable with a status reads past the link arrays | New in this write-up | - |
| [CRASH-27](2-crashes/CRASH-27-rule-labels-unterminated/) | EN_addrule leaves rule labels of 31 characters or more unterminated | New in this write-up | - |
| [CRASH-28](2-crashes/CRASH-28-setcurve-shrinks-gpv-curve-while-open/) | EN_setcurve can shrink a GPV's curve to one point while the solver is open | New in this write-up | - |
| [CRASH-29](2-crashes/CRASH-29-huge-rule-time-to-text/) | Rule time values of 2^31 seconds or more are converted with undefined behaviour | New in this write-up | - |
| [CRASH-30](2-crashes/CRASH-30-api-out-of-range-conversions/) | Time and option setters convert out-of-range values to integers with undefined behaviour | not in the summary | F5 (API addendum) |
| [CRASH-31](2-crashes/CRASH-31-clock-time-buffer-overflow/) | A run of 1,000,000 hours or more overflows the 13-byte clock-time buffers | New in this write-up | - |
| [API-01](3-api/API-01-addnode-shifts-wrong-rule-premises/) | EN_addnode shifts rule premises after incrementing the junction count | A1 | A6 (stage 1) |
| [API-02](3-api/API-02-addnode-trace-node-not-shifted/) | EN_addnode does not shift the trace node (a reverted fix) | A2 | A7 (stage 1) |
| [API-03](3-api/API-03-setlinktype-cv-keeps-controls/) | EN_setlinktype to CVPIPE keeps the link's controls and rules | A3 | A15 |
| [API-04](3-api/API-04-settankdata-diameter-zero/) | EN_settankdata with diameter 0 turns a tank into a reservoir | A4 | A16 |
| [API-05](3-api/API-05-stale-hydraulics-after-edit/) | Structural edits do not invalidate the saved hydraulics | A5 | A14 |
| [API-06](3-api/API-06-addlink-self-loop/) | EN_addlink accepts a link from a node to itself | A6 | A18 |
| [API-07](3-api/API-07-deletelink-cv-pipe-count/) | EN_deletelink does not decrement the pipe count for check-valve pipes | A7 | A19 |
| [API-08](3-api/API-08-bad-rule-deletes-previous-rule/) | A bad EN_addrule string deletes the previous rule | A8 | A2 (stage 1), H4 |
| [API-09](3-api/API-09-malformed-rule-drops-later-rules/) | One malformed rule in a file drops every rule after it | A9 | H5 |
| [API-10](3-api/API-10-reindextanks-shifts-premises-repeatedly/) | reindextanks shifts rule premises once per tank | A10 | H6 |
| [API-11](3-api/API-11-bulk-setters-read-before-write/) | Bulk setters read every node before writing, so source and tank properties cannot be set | A11 | B15 |
| [API-12](3-api/API-12-sourcemass-cumulative/) | EN_SOURCEMASS returns cumulative mass x 60, not a rate | A12 | B16 |
| [API-13](3-api/API-13-reportstart-validates-old-value/) | EN_settimeparam(EN_REPORTSTART) validates the old value | A13 (part) | C11, B14, F-13 |
| [API-14](3-api/API-14-initstatus-char-cast/) | EN_INITSTATUS is cast to char before the range check | A13 (part) | B19 |
| [API-15](3-api/API-15-pipe-setting-ignores-roughness-error/) | EN_SETTING on a pipe ignores the roughness error | A13 (part) | B20 |
| [API-16](3-api/API-16-options-not-reloadable/) | EN_CHECKFREQ, EN_MAXCHECK below 1 and tiny viscosity are accepted but cannot be reloaded | A13 (part) | F6 |
| [API-17](3-api/API-17-haltflag-always-zero/) | EN_gettimeparam(EN_HALTFLAG) always returns 0 | A13 (part) | F1, B13 |
| [API-18](3-api/API-18-tanklevel-returns-initial-level/) | EN_TANKLEVEL returns the initial level but is documented as current | A13 (part) | C13, B21 |
| [API-19](3-api/API-19-specific-gravity-call-order/) | EN_SP_GRAVITY does not rescale stored emitter and PDA values | A13 (part) | B18 |
| [API-20](3-api/API-20-reads-before-run-return-scratch-values/) | Reads before a run return solver scratch values | A13 (part) | B22 |
| [API-21](3-api/API-21-disabled-controls-cut-time-steps/) | Controls disabled with link index 0 still cut time steps | A13 (part) | C12 |
| [API-22](3-api/API-22-timeofday-controls-not-wrapped/) | API TIMEOFDAY controls are not reduced modulo 24 h | A13 (part) | C15 |
| [API-23](3-api/API-23-nexthyd-drops-save-error/) | nexthyd overwrites a savehyd write error | A13 (part) | C14 |
| [API-24](3-api/API-24-library-changes-host-locale/) | The library changes the host program's numeric locale | R1, P9 | R2-L1, F-12 |
| [API-25](3-api/API-25-report-misaligned-after-edit/) | EN_report after a structural edit prints misaligned results | New in this write-up | - |
| [API-26](3-api/API-26-link-status-before-run/) | EN_STATUS, EN_SETTING and EN_DEMAND read leftover values before a run | New in this write-up | - |
| [API-27](3-api/API-27-results-on-wrong-objects-after-edit/) | Adding or deleting nodes and links leaves results attached to the wrong objects | New in this write-up | - |
| [API-28](3-api/API-28-control-times-truncated/) | Control times added through the API are truncated, not rounded | New in this write-up | - |
| [API-29](3-api/API-29-addlink-leaves-new-link-fields-unset/) | EN_addlink leaves a new pump's diameter and a new link's flow, status and setting unset | not in the summary | A17 (stage 1) |
| [API-30](3-api/API-30-report-file-left-open-after-failed-open/) | EN_deleteproject leaves the report file open after a failed EN_open | New in this write-up | - |
| [FILE-01](4-files/FILE-01-saved-files-disable-quality-report/) | Every saved file turns off the node Quality column when reloaded | P1 | F-10 |
| [FILE-02](4-files/FILE-02-save-over-input-loses-labels/) | Saving over the input file loses [LABELS] and [BACKDROP] | P2 | F-11 |
| [FILE-03](4-files/FILE-03-filltime-rules-not-reloadable/) | FILLTIME and DRAINTIME rule values are written in a form the parser rejects | P3 | F-7 |
| [FILE-04](4-files/FILE-04-pumps-gpvs-without-curve-not-reloadable/) | Pumps and GPVs without a curve are written in a form the parser rejects | P4 | F-8 |
| [FILE-05](4-files/FILE-05-timer-controls-lose-a-second/) | Timer controls lose one second per save | P5 | F-9 |
| [FILE-06](4-files/FILE-06-zero-demand-categories-dropped/) | Demand categories with zero base demand are dropped on save | P6 | F-15 |
| [FILE-07](4-files/FILE-07-four-decimal-formats-truncate/) | Fixed four-decimal formats truncate small values | P7 | F-16 |
| [FILE-08](4-files/FILE-08-strings-with-spaces-unquoted/) | Strings with spaces are written unquoted | P8 | F-17 |
| [FILE-09](4-files/FILE-09-output-reader-last-period-and-units/) | The output-file reader cannot reach the last period and misreads pressure units | P12 | F-14 |
| [FILE-10](4-files/FILE-10-nan-inf-accepted-as-numbers/) | nan, inf, hex and empty tokens are accepted as numbers | P13 | E3 |
| [FILE-11](4-files/FILE-11-long-lines-split/) | Lines over 1,023 characters are split in two | P14 | E4 |
| [FILE-12](4-files/FILE-12-tokens-past-40-dropped/) | Tokens past the 40th on a line are dropped | P15 | E5 |
| [FILE-13](4-files/FILE-13-fractional-trials-zero-iterations/) | TRIALS below 1 gives zero solver iterations | P16 (part) | D10 |
| [FILE-14](4-files/FILE-14-quoted-tokens-mis-split/) | A quoted token corrupts the rest of the line | R2 | R2-B1 |
| [FILE-15](4-files/FILE-15-valve-check-skips-previous-link/) | The illegal-valve check skips the link read just before the valve | R4 | R2-B3 |
| [FILE-16](4-files/FILE-16-short-link-lines-dropped/) | [PIPES], [PUMPS] and [VALVES] lines with too few fields are silently dropped | New in this write-up | - |
| [FILE-17](4-files/FILE-17-options-load-but-not-reload/) | [OPTIONS] values that load but cannot be saved and reloaded | New in this write-up | - |
| [FILE-18](4-files/FILE-18-rule-times-lose-a-second/) | Clock times in rule premises are truncated and lose a second | New in this write-up | - |
| [FILE-19](4-files/FILE-19-trials-default-documented-wrong/) | The documented default for TRIALS is 40; the code uses 200 | New in this write-up | - |
| [WQ-01](5-water-quality/WQ-01-fractional-reaction-order-nan/) | A reaction order between 0 and 1 gives NaN across the network | Q1 | G1 |
| [WQ-02](5-water-quality/WQ-02-check-valve-pipes-have-no-volume/) | Check-valve pipes have no volume and no reactions in water quality | R5 | R2-N1 |
| [WQ-03](5-water-quality/WQ-03-fifo-lifo-tanks-book-inflow-as-overflow/) | FIFO and LIFO tanks treat filling as overflow | R6 | R2-N2 |
| [WQ-04](5-water-quality/WQ-04-reaction-averages-low-with-report-start/) | Average reaction rates are low when REPORT START is not 0 | Q5 | G7 |

Issues marked "New in this write-up" were found while the tests and patches for the original findings were being written. Two items marked "not in the summary" were confirmed in the review log but left out of the September summary.

**Merged items.** These earlier items described a bug that is listed elsewhere, so they no longer have their own ID:

| Earlier ID | Now part of |
|---|---|
| P9 (`saveinpfile` changes the locale) | [API-24](3-api/API-24-library-changes-host-locale/), with R1. One fix covers both. |
| P16, `PRESSURE EXPONENT 0` | [SIM-15](1-wrong-results/SIM-15-pressure-exponent-zero/). The `TRIALS` half of P16 is [FILE-13](4-files/FILE-13-fractional-trials-zero-iterations/). |
| Q6 | [SIM-18](1-wrong-results/SIM-18-setqualtype-scales-initial-quality/). Q6 was the same bug, confirmed in a quality run. |
| P10, P11, P18, Q2, Q3, Q4 | The crash items [CRASH-05](2-crashes/CRASH-05-report-precision-overflow/), [CRASH-13](2-crashes/CRASH-13-report-closes-output-file/), [CRASH-09](2-crashes/CRASH-09-saveinp-rule-value-overflow/), [CRASH-07](2-crashes/CRASH-07-quality-isolated-component/), [CRASH-11](2-crashes/CRASH-11-closeq-not-idempotent/) and [CRASH-10](2-crashes/CRASH-10-adjacency-lists-freed-by-both-solvers/) |
| P17 | Three separate crashes: [CRASH-01](2-crashes/CRASH-01-long-token-error-message/), [CRASH-02](2-crashes/CRASH-02-clock-time-four-fields/) and [CRASH-03](2-crashes/CRASH-03-bare-priority/) |
| A13 | Split into one issue per defect: API-13 to API-23, [CRASH-19](2-crashes/CRASH-19-addrule-edits-caller-string/) and [CRASH-25](2-crashes/CRASH-25-setflowunits-out-of-range/) |
| Crash-table C2 and G6 | One bug with two symptoms: [CRASH-10](2-crashes/CRASH-10-adjacency-lists-freed-by-both-solvers/) |
