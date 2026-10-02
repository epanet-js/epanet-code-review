# Wrong simulation results

[All issues](../README.md)

These bugs change the numbers in a report or read through the toolkit, and nothing warns the user. Most are in the hydraulic solver, unit conversions, energy accounting and time stepping. SIM-17 to SIM-22, SIM-24 and SIM-25 are reached through toolkit setters, but their effect is on results.

| ID | Issue | What happens | Fix |
|---|---|---|---|
| [SIM-01](SIM-01-tank-keeps-supplying-after-empty/) | A draining tank keeps supplying water after it is empty | The tank keeps supplying for up to a full step after it is empty: 526 ft³ more than it held in the test. Or it ends below its minimum level. | Change one sign: `V + NodeDemand <= Vmin` |
| [SIM-02](SIM-02-gpv-pbv-flow-through-empty-tank/) | GPV and PBV ignore TEMPCLOSED and pass flow through an empty or full tank | A GPV or PBV at an empty tank keeps passing flow (1,465 and 3,340 gpm in the test) while reporting 0 flow. | Test `LinkStatus <= CLOSED` in `gpvcoeff()` and `pbvcoeff()` |
| [SIM-03](SIM-03-si-power-pumps-converted-twice/) | Constant-power pumps in SI input files are converted from kW twice | An SI file's constant-power pump runs at 1.341 times its power (136.8 m instead of 101.9 m); each save and reload multiplies it again. | Keep pump power in hp; convert once in getter, setter and writer |
| [SIM-04](SIM-04-leak-expansion-units/) | Leak expansion is per metre of head in both unit systems, but the docs say per ft or m | US users who follow the docs enter sq mm per ft of head and get 0.3048 times the intended variable-area leakage. | Correct the two doc strings to "sq mm per m of head" |
| [SIM-05](SIM-05-emitter-coefficient-pressure-units/) | Emitter coefficients are re-converted with the pressure-unit factor | With non-default pressure units, re-setting the emitter exponent or saving the file changes every emitter coefficient (10 becomes 26.26, or 3.81). | Use the parser's psi or metre factor in `EN_setoption` and `saveinpfile` |
| [SIM-06](SIM-06-demand-charge-squared/) | Demand charge is multiplied by the price twice and compounds across runs | The report shows peak kW x Dcost² as the demand charge (386.83 instead of 193.42), and each further EN_solveQ multiplies it again. | Write `Emax * Dcost` to a local; reset `Emax` in `inithyd()` |
| [SIM-07](SIM-07-energy-averages-computed-in-place/) | Energy averages are computed in place, so a second EN_solveQ corrupts them | A second EN_solveQ or EN_saveH on the same hydraulics reports a pump usage factor of 240 % and efficiency of 130 %. | New `pumpenergystats()` converts a copy; used by `saveenergy()` and `writeenergy()` |
| [SIM-08](SIM-08-reservoir-level-controls-always-fire/) | Level controls on a reservoir always fire | A BELOW or ABOVE control on a reservoir fires at every time step, whatever the head, so the link is forced shut or open. | For a reservoir, compare its head with the control grade |
| [SIM-09](SIM-09-report-start-mislabels-rows/) | REPORT START that is not a multiple of REPORT STEP mislabels every row | No solution is computed at the reporting times; the row labelled 0:30 holds the 1:00 results, and so on for every row. | Start `Rtime` in `inithyd()` at `Rstart % Rstep` (or `Rstep` if 0) |
| [SIM-10](SIM-10-pattern-start-boundary-missed/) | PATTERN START that is not a multiple of PATTERN STEP misses pattern changes | Every pattern change takes effect up to one hydraulic step late; in Net1 with Pattern Start 0:30 the tank is 1.26 ft off by 6:00. | `t = n * Pstep - (Htime + Pstart)` in `timestep()` |
| [SIM-11](SIM-11-duration-overshoot/) | A duration that is not a multiple of the hydraulic step is overshot | The last hydraulic step runs past the duration: pump usage 133 %, cost a third too high, and water quality stops a step early. | Limit the step to `Dur - Htime` in `timestep()` |
| [SIM-12](SIM-12-hydraulic-step-under-10s/) | A hydraulic step under 10 s hangs water quality and divides by zero in rule timing | With a 1 to 9 s hydraulic step, a water quality run never finishes, and a run with rules divides by zero. | Default steps `MAX(1, Hstep/10)`; guard `Rulestep <= 0` in `ruletimestep()` |
| [SIM-13](SIM-13-pcv-curve-extrapolation/) | PCV curve extrapolation uses 1 instead of 100 % | A PCV set above its curve's last point loses 3.96 ft instead of 2.22 ft, or acts fully open. | Extrapolate to `(100, 100)` instead of `(1, 1)` in `pcvlosscoeff()` |
| [SIM-14](SIM-14-pda-kpa-minimum-pressure-difference/) | PDA with kPa units uses two different demand curves when the pressure range is narrow | With kPa units and a 0.1 to 0.3 kPa PDA range the solver does not converge (211 trials) and demands are off the curve. | Drop the `MINPDIFF` floor in `newdemandflows()` |
| [SIM-15](SIM-15-pressure-exponent-zero/) | PRESSURE EXPONENT 0 is accepted and gives NaN results | An input file with `PRESSURE EXPONENT 0` runs with return code 0 and NaN for every junction head and demand. | Reject `PRESSURE EXPONENT <= 0` with error 213 |
| [SIM-16](SIM-16-negative-pump-speed-nan/) | A negative pump-speed pattern factor gives NaN for the rest of the run | A pump speed pattern factor of -1 makes the pump flow and nearby heads NaN for that period and every later one. | Treat a negative pump speed as 0 (closed) in `setlinksetting()` |
| [SIM-17](SIM-17-tank-volumes-stale-after-edit/) | EN_MINLEVEL and EN_setcurve leave tank volumes stale | After the edit the tank starts from the wrong volume, so its level jumps: a tank draining 200 gpm rises 4.2 ft. | Recompute `V0`, `Vmax` (and `Vmin` for curve tanks) in the three setters |
| [SIM-18](SIM-18-setqualtype-scales-initial-quality/) | EN_setqualtype scales initial quality by 1/28.3 when switching to CHEM | After a switch to CHEM, initial quality is 28.3 times too small (0.5 becomes 0.0177 mg/L) and the run starts from it. | Rescale `C0` by old/new factor on every switch; convert `Climit` like `Ctol` |
| [SIM-19](SIM-19-gpv-curve-type-not-set/) | GPV curves set through the API are never typed as head loss curves | After `EN_setflowunits`, a GPV curve assigned with `EN_GPV_CURVE` keeps its old numbers: 0.126 m of head loss instead of 0.610 m. | Read `Kc` in `assigncurvetypes()`; type the curve in the setter |
| [SIM-20](SIM-20-volcurve-area-user-units/) | EN_VOLCURVE stores the tank area in user units | In SI projects the tank's nominal area is 10.76 times too small: wrong diameter, and wrong levels if the curve is deleted. | Divide the nominal area by `SQR(Ucf[ELEV])` |
| [SIM-21](SIM-21-gpv-curve-index-three-copies/) | A GPV's curve index is stored in three places and only one is kept current | After `EN_deletecurve`, or a new curve on a GPV with a control, the GPV runs on a different curve from the one reported. | Solver reads `Link.Kc`; copies kept in step or no longer used |
| [SIM-22](SIM-22-curve-change-while-solver-open/) | Changing a GPV or pump curve while the solver is open has no effect | Curve changes made during a run (`EN_GPV_CURVE`, `EN_setheadcurveindex`, `EN_setcurve`) are reported by getters but ignored by the solver, even after `EN_initH`. | Recompute pump coefficients when the solver is open; GPV part via SIM-21 |
| [SIM-24](SIM-24-deletecurve-leaves-tank-volumes/) | EN_deletecurve leaves a tank's volumes computed from the deleted curve | A tank whose volume curve is deleted keeps the curve's volumes; draining 20 gpm, it falls 7.90 ft in an hour instead of 0.40 ft. | Recompute the tank's volumes in `EN_deletecurve`, as `EN_TANKDIAM` does |
| [SIM-25](SIM-25-pump-power-change-while-solver-open/) | Changing a pump's power while the solver is open has no effect | A pump power set with `EN_PUMP_POWER` during a run is ignored; a curve pump switched to constant power delivers no head. | Call `updatepumpparams()` in the `EN_PUMP_POWER` setter when the solver is open |
| [SIM-23](SIM-23-output-headloss-float-precision/) | Output-file head loss is computed from heads rounded to 4-byte floats | Output-file head loss and friction factor come from float-rounded heads; in Net2, 25 % of pipe values are off by over 1 %. | Keep double-precision heads in a second scratch file for `EN_solveQ` |

Each issue's folder has the full write-up, a test program, its input files and a fix patch. [Reproducing the issues](../reproducing.md) explains how to run them.

## Hydraulics and units

### SIM-01: A draining tank keeps supplying water after it is empty

When a tank empties partway through a time step, EPANET lands it within half a second of its minimum level. A clamp is meant to snap it to exactly empty, but the clamp has the wrong sign. If the tank lands just above its minimum, its outlet stays open and it keeps supplying the full demand for the whole next step. In the test that is 526 ft³ (3,900 gal) more than the tank held. If it lands just below, it stays there. The flow balance still reports a ratio of 1.000.

The "empty within the next second" test in `tanklevels()` subtracts the tank's net inflow, which is negative while draining. The matching "full" test adds it.

**Fix:** use `tank->V + hyd->NodeDemand[n] <= tank->Vmin`, as the "full" test does. Reported independently as [#936](https://github.com/OpenWaterAnalytics/EPANET/issues/936) after the review, and fixed on `dev` by [#940](https://github.com/OpenWaterAnalytics/EPANET/issues/940) (1 October 2026) with the same change.

[Write-up, test and patch](SIM-01-tank-keeps-supplying-after-empty/)

### SIM-02: GPV and PBV ignore TEMPCLOSED and pass flow through an empty or full tank

When a tank reaches its minimum or maximum level, the solver marks the links that would drain or overfill it as temporarily closed. A GPV or PBV ignores this and keeps passing flow, while the rest of the solver treats it as closed: the flow is not charged to the tank, the tank level stays fixed, and the valve reports 0 flow. In the test, an empty tank pushes 1,465 gpm through a GPV and 3,340 gpm through a PBV back into a reservoir that should be supplying a 200 gpm demand.

`gpvcoeff()` tests `LinkStatus == CLOSED`, and `pbvcoeff()` does not look at the status, while every other coefficient routine treats `<= CLOSED` (which includes TEMPCLOSED) as closed.

**Fix:** test `LinkStatus <= CLOSED` in `gpvcoeff()` and at the top of `pbvcoeff()`.

[Write-up, test and patch](SIM-02-gpv-pbv-flow-through-empty-tank/)

### SIM-03: Constant-power pumps in SI input files are converted from kW twice

A constant-power pump in an SI input file runs at 1/0.7457 = 1.341 times its stated power. A 10 kW pump lifting 10 L/s adds 136.8 m of head instead of 101.9 m. `EN_PUMP_POWER` reads 13.41, each save and reload multiplies the power by 1.341 again, setting the same power through the API gives a different (correct) result, and `EN_setflowunits` on a US project changes the pump's physical power.

The parser converts the power from kW to hp in `convertunits()`, and `constpowerpump()` converts it again at every `EN_openH`. The setter, getter and INP writer treat the stored value as kW. This is a regression from PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758).

**Fix:** keep the power in hp internally: drop the second conversion and convert once in the getter, setter and INP writer.

[Write-up, test and patch](SIM-03-si-power-pumps-converted-twice/)

### SIM-04: Leak expansion is per metre of head in both unit systems, but the docs say per ft or m

The leak expansion rate in `[LEAKAGE]` (and `EN_LEAK_EXPAN`) says how much a pipe's leak area grows with pressure head. The documentation says it is in sq mm per unit of head, ft or m. The code reads it as sq mm per metre of head in both unit systems, so a US user who follows the documentation gets 0.3048 times the intended variable-area leakage: 7.09 gpm instead of 23.26 gpm in the test.

`leakage.c` divides the rate by `MperFT` whatever the unit system. That is the design the maintainers stated in [#899](https://github.com/OpenWaterAnalytics/EPANET/issues/899) and implemented in PR [#901](https://github.com/OpenWaterAnalytics/EPANET/issues/901); the documentation was never brought into line.

**Fix:** correct the `[LEAKAGE]` page and the `EN_LEAK_EXPAN` comment to say sq mm per metre of head.

[Write-up, test and patch](SIM-04-leak-expansion-units/)

### SIM-05: Emitter coefficients are re-converted with the pressure-unit factor

Since v2.3, an emitter coefficient is always per psi (US flow units) or per metre (SI flow units), whatever the pressure units. Two places still convert with the factor of the current pressure units. In a GPM model with pressures in kPa, calling `EN_setoption(EN_EMITEXPON)` with the exponent it already has turns a coefficient of 10 into 26.26 (emitter flow 65.8 to 172.7 gpm), and `EN_saveinpfile` writes 3.81, so the reopened model is different. Only psi in US models and metres in SI models are unaffected.

PR [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862) changed the parser, `EN_getnodevalue` and `EN_setnodevalue` to the fixed factor (`PSIperFT * SpGrav` or `MperFT`), but `EN_setoption(EN_EMITEXPON)` and the `[EMITTERS]` writer in `saveinpfile` still use `Ucf[PRESSURE]`.

**Fix:** use the parser's factor in both places.

[Write-up, test and patch](SIM-05-emitter-coefficient-pressure-units/)

## Energy

### SIM-06: Demand charge is multiplied by the price twice and compounds across runs

With a demand charge in `[ENERGY]`, the energy report shows peak kW × Dcost² instead of peak kW × Dcost. For Net1 with a demand charge of 2.0, that is 386.83 instead of 193.42, from a single `runepanet` run. In a toolkit program, each further `EN_solveQ` or `EN_saveH` multiplies the charge by Dcost again, in the report and in the binary output file, and a new `EN_solveH` does not reset it (1,547.31 by the third run).

`saveenergy()` stores `Emax * Dcost` back into `hyd->Emax`, and `writeenergy()` multiplies by `Dcost` again. `inithyd()` never resets `Emax`, and `addenergy()` keeps the larger of the old and new values.

**Fix:** write the charge from a local value in `saveenergy()` and reset `hyd->Emax` in `inithyd()`.

[Write-up, test and patch](SIM-06-demand-charge-squared/)

### SIM-07: Energy averages are computed in place, so a second EN_solveQ corrupts them

A toolkit program that solves the hydraulics once and then runs `EN_solveQ` (or `EN_saveH`) more than once gets a different energy table each time. For Net1, the second run reports a usage factor of 240.47 % instead of 57.71 %, an efficiency of 129.95 % instead of 75.00 %, and an average of 1.67 kW instead of 96.25 kW, in the report and in the binary output file.

`saveenergy()` runs on every `EN_initQ(EN_SAVE)` and converts the pump energy totals into averages in place, in `pump->Energy`. Only `inithyd()` resets the totals, so the second call divides the averages again.

**Fix:** move the conversion into a new `pumpenergystats()` that works on a copy, and call it from both `saveenergy()` and `writeenergy()`.

[Write-up, test and patch](SIM-07-energy-averages-computed-in-place/)

## Controls and time stepping

### SIM-08: Level controls on a reservoir always fire

A simple level control whose node is a reservoir, such as `LINK P1 CLOSED IF NODE RIVER ABOVE 10`, fires at every time step whatever the reservoir's head. In the test, two intakes on reservoirs with head patterns should close at 3:00; both close at 0:00 and stay closed, and the status report blames the reservoir controls.

`controls()` compares the volume at the current head with the volume at the control grade. `tankvolume()` returns `Vmin` for a reservoir (area 0) at any head, so the two volumes are equal and both the BELOW and the ABOVE test are true.

**Fix:** when the control node is a reservoir, compare its head with the control grade directly.

[Write-up, test and patch](SIM-08-reservoir-level-controls-always-fire/)

### SIM-09: REPORT START that is not a multiple of REPORT STEP mislabels every row

With a report start that is not a multiple of the report step, such as `Report Start 0:30` with `Report Timestep 1:00`, the hydraulic solver never stops at the reporting times. Each row in the report and the binary output file holds the next solution instead: in Net1 the row labelled 1:30 shows junction 11 at 121.20 psi with the 2:00 demand of 180 gpm, where the 1:30 state is 121.03 psi and 150 gpm.

`inithyd()` starts the hydraulic reporting clock `Rtime` at `Rstep`, ignoring `Rstart`, so time steps land on multiples of `Rstep`. The quality pass starts its clock at `Rstart` and saves the first solution at or after each label.

**Fix:** start `Rtime` at the first time after 0 on the grid `Rstart + k*Rstep`; runs with an aligned report start are unchanged.

[Write-up, test and patch](SIM-09-report-start-mislabels-rows/)

### SIM-10: PATTERN START that is not a multiple of PATTERN STEP misses pattern changes

With a pattern start that is not a multiple of the pattern step, such as `Pattern Start 0:30` with `Pattern Timestep 2:00`, the hydraulic solver never takes a step that ends at a pattern boundary. In Net1, the new demands that should start at 1:30, 3:30, ... only apply at 2:00, 4:00, ..., and the tank level is 1.26 ft off by 6:00. All time patterns are affected: demands, reservoir heads, pump speeds and energy prices.

`timestep()` finds the next pattern period from `Htime + Pstart`, but computes the time to it as `n * Pstep - Htime`, leaving out `Pstart`.

**Fix:** subtract `Htime + Pstart`, as `demands()` does.

[Write-up, test and patch](SIM-10-pattern-start-boundary-missed/)

### SIM-11: A duration that is not a multiple of the hydraulic step is overshot

When the hydraulic steps do not land on the end of the run, the last step goes past it. Net1 run for 1:30 with a 1-hour step is solved at 0:00, 1:00 and 2:00. The energy report divides 2 hours of pumping by the 1.5-hour duration, giving a usage factor of 133.33 % and a cost per day of 307.06 instead of 230.20, and the water quality solver stops at 1:00, so the last 30 minutes are never simulated.

`timestep()` shortens the step for pattern periods, reporting times, tanks and controls, but not for the end of the simulation, so `nexthyd()` moves `Htime` past `Dur`.

**Fix:** limit the step to `Dur - Htime` in `timestep()`, next to the other limits.

[Write-up, test and patch](SIM-11-duration-overshoot/)

### SIM-12: A hydraulic step under 10 s hangs water quality and divides by zero in rule timing

A hydraulic step under 10 seconds, with no quality or rule step given, breaks two things. A water quality run never finishes: `EN_solveQ` (and `runepanet`) loops forever. A run with rules divides by zero; on arm64 the run then stops at time 0 and still reports success.

`adjustdata()` sets the missing quality and rule steps to `Hstep / 10` with integer division, which is 0. `nextqual()` then never advances, and `ruletimestep()` takes `tnow % 0`. `EN_settimeparam` rejects 0 for both, but here EPANET sets it itself.

**Fix:** use `MAX(1, Hstep / 10)` for both defaults, and treat a rule step that is not positive as no rule step in `ruletimestep()`.

[Write-up, test and patch](SIM-12-hydraulic-step-under-10s/)

## Numerics of valves, demands and pumps

### SIM-13: PCV curve extrapolation uses 1 instead of 100 %

A positional control valve's curve is extrapolated linearly from its last point to fully open. A valve set 90 % open with a curve ending at (80 %, 50 %) should have Kv/Kvo = 75 % and 2.22 ft of head loss in the test; EPANET gives 3.96 ft. If the curve ends at (1, 1) the extrapolation is 0/0, the NaN is clamped to 1, and the valve is treated as fully open.

PR [#747](https://github.com/OpenWaterAnalytics/EPANET/issues/747) changed PCV curves from fractions to percentages, but the extrapolation in `pcvlosscoeff()` still uses 1 as the end point instead of 100.

**Fix:** use `(100. - x[k2])` and `(100. - y[k2])` in the extrapolation.

[Write-up, test and patch](SIM-13-pcv-curve-extrapolation/)

### SIM-14: PDA with kPa units uses two different demand curves when the pressure range is narrow

In a pressure-driven analysis with kPa units, a required pressure between 0.1 and 0.3 kPa above the minimum is accepted, but the solver then does not converge. In the test (20 to 20.2 kPa) `EN_runH` returns "System may be unstable" after 211 trials, and a junction at 20.04 kPa gets 18.26 L/s instead of the 22.32 L/s the demand curve gives.

The demand update in `newdemandflows()` floors the pressure range at `MINPDIFF` = 0.1, which is meant in user units but is applied in feet (0.30 kPa). The matrix and the convergence test use the true range, so they follow a different curve.

**Fix:** use `dp = Preq - Pmin` in `newdemandflows()`, as the other two routines do.

[Write-up, test and patch](SIM-14-pda-kpa-minimum-pressure-difference/)

### SIM-15: PRESSURE EXPONENT 0 is accepted and gives NaN results

`EN_setdemandmodel` rejects a pressure exponent of 0, but the input file parser accepts `PRESSURE EXPONENT 0`. The run then returns 0 while every junction head and demand is NaN.

`optionvalue()` rejects only negative exponents. The solver uses `1 / Pexp`, which is infinite, and the NaN that follows passes the `diagj <= 0` ill-conditioning check in `linsolve()`.

**Fix:** reject values `<= 0` in the parser, as `EN_setdemandmodel` does.

[Write-up, test and patch](SIM-15-pressure-exponent-zero/)

### SIM-16: A negative pump-speed pattern factor gives NaN for the rest of the run

A pump's speed can follow a time pattern, and patterns are not range-checked. In the test, the speed pattern 1, −1, 1 turns the pump flow and the head at the downstream junction into NaN in the second hour, and they stay NaN in the third hour after the speed is back to 1. No error or warning is raised.

`setlinksetting()` leaves a pump OPEN for a negative speed, and `pumpcoeff()` takes `pow(setting, 2 - n)`. Because the pump never closes, `resetpumpflow()` is not called when the factor turns positive, so the NaN flow persists.

**Fix:** treat a negative speed as 0 in `setlinksetting()`: the pump closes and reopens at the next positive factor.

[Write-up, test and patch](SIM-16-negative-pump-speed-nan/)

## Setters that leave the model inconsistent

### SIM-17: EN_MINLEVEL and EN_setcurve leave tank volumes stale

A tank stores its minimum, initial and maximum volumes alongside its levels, and a run starts from the stored initial volume. Three setters change the level-volume relation without updating those volumes. After raising a cylindrical tank's minimum level from 0 to 5 ft with `EN_setnodevalue(EN_MINLEVEL)`, a tank draining 200 gpm rises 4.18 ft in the first hour instead of falling 0.82 ft. After `EN_setcurve` doubles a tank's volume curve, it falls 5.40 ft instead of 0.40 ft. `EN_setcurvevalue` behaves the same way.

`EN_MINLEVEL` moves `Hmin` but keeps `Vmin`, so every volume above it changes, but `V0` and `Vmax` are not recomputed. `EN_setcurve` and `EN_setcurvevalue` load the new points without touching the tanks that use the curve.

**Fix:** recompute the stored volumes with `tankvolume()` in all three setters, as the other tank setters already do.

[Write-up, test and patch](SIM-17-tank-volumes-stale-after-edit/)

### SIM-18: EN_setqualtype scales initial quality by 1/28.3 when switching to CHEM

`EN_setqualtype` changes the factor between the user's concentration units and EPANET's internal per-cubic-foot units, but rescales node initial quality only when switching from a chemical to water age or trace. Switching to a chemical from any other type divides every initial quality by 28.317: Net1's 0.5 mg/L at node 10 becomes 0.0177 mg/L after a switch to age and back, and the chlorine run starts from that value. A project built with `EN_init` that then turns on a chemical gets the same error. The limiting concentration is never rescaled and reads 56.6 instead of 2 after leaving a chemical analysis.

The function rescales `Ctol` around the change of `Ucf[QUALITY]`, but `Node.C0` only in one direction and `Climit` not at all.

**Fix:** rescale `C0` by the ratio of old to new factor on every switch, and convert `Climit` the same way as `Ctol`.

[Write-up, test and patch](SIM-18-setqualtype-scales-initial-quality/)

### SIM-19: GPV curves set through the API are never typed as head loss curves

`EN_setflowunits` converts curves according to their type. A GPV curve assigned through the toolkit stays a generic curve, so switching units leaves its numbers unchanged and they are read in the new units. In the test the GPV loses 0.126 m instead of 0.610 m (2 ft) after switching from GPM to LPS, and the saved file records the curve as `GENERIC`.

PR [#903](https://github.com/OpenWaterAnalytics/EPANET/issues/903) added `assigncurvetypes()` to repair types before conversion, but it looks for a GPV's curve in `Valve[].Curve`, which only PCVs use; the index is in `Link.Kc`. `EN_setlinkvalue(EN_GPV_CURVE)` does not set the type either.

**Fix:** read `Link.Kc` in `assigncurvetypes()`, and set `HLOSS_CURVE` when the curve is assigned.

[Write-up, test and patch](SIM-19-gpv-curve-type-not-set/)

### SIM-20: EN_VOLCURVE stores the tank area in user units

A tank with a volume curve also keeps a nominal area, the slope of the curve. `EN_setnodevalue(EN_VOLCURVE)` stores that slope in user units, while EPANET works in ft². In an SI project a 100 m² tank gets 100 ft², so `EN_TANKDIAM` reports 3.44 m instead of 11.28 m and the saved file carries the same wrong diameter. The hydraulics use the curve, so they are only affected if the curve is later deleted: the tank then rises 46.9 m in an hour while draining, instead of falling 0.18 m.

The `EN_VOLCURVE` case computes `(Y[n] - Y[0]) / (X[n] - X[0])` from the curve, which is in user units, and does not divide by `Ucf[ELEV]²` as `EN_settankdata` and the parser do.

**Fix:** convert the nominal area to ft² with `/ SQR(Ucf[ELEV])`.

[Write-up, test and patch](SIM-20-volcurve-area-user-units/)

### SIM-21: A GPV's curve index is stored in three places and only one is kept current

A GPV's curve index is kept in `Link.Kc` (what `EN_GPV_CURVE` reads and `EN_deletecurve` updates), in `Link.InitSetting` (which the solver and the INP writer use) and in each control on the valve. They drift apart. In the test, deleting an unrelated curve makes the GPV lose 80 ft instead of 20 ft (and saves the wrong curve), and a control resets a newly assigned curve back to the old one. Deleting the GPV's own curve leaves it running on another curve.

`gpvcoeff()` reads the run-time setting, which comes from `InitSetting` or from the control, while the toolkit updates `Kc`.

**Fix:** make `gpvcoeff()` read `Kc`, keep `InitSetting` equal to it, and stop GPV controls from writing a curve index.

[Write-up, test and patch](SIM-21-gpv-curve-index-three-copies/)

### SIM-22: Changing a GPV or pump curve while the solver is open has no effect

A toolkit program that changes a GPV's or a pump's curve while the hydraulic solver is open gets results for the old curve, while the getters report the new one. In the test, a GPV given a 30 ft curve still loses 10 ft and a pump given an 80 ft (then 120 ft) curve still adds 50 ft, also after `EN_initH`; the saved file keeps the old GPV curve.

The GPV setter updates only one of the three copies of the curve index while the solver is open, and pump coefficients are derived only at `EN_openH`. EPANET 2.2 recomputed them in the setters (the fix for [#550](https://github.com/OpenWaterAnalytics/EPANET/issues/550)); PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) removed those calls.

**Fix:** recompute the coefficients of affected pumps in `EN_setheadcurveindex` and `EN_setcurve` while the solver is open; the GPV part is fixed by [SIM-21](SIM-21-gpv-curve-index-three-copies/).

[Write-up, test and patch](SIM-22-curve-change-while-solver-open/)

### SIM-24: EN_deletecurve leaves a tank's volumes computed from the deleted curve

`EN_deletecurve` accepts a curve that a tank uses for its volumes. It drops the tank's curve index, so the tank becomes a cylinder with the curve's nominal area, but the tank keeps the minimum, initial and maximum volumes computed from the curve. Unless the curve is a straight line, the run starts from a volume that does not match the initial level. An elevated tank with a narrow riser and a wide bowl, draining 20 gpm, falls 7.90 ft in its first hour instead of 0.40 ft.

`adjustcurves()` only resets indices. `EN_TANKDIAM`, the other way to remove a volume curve, recomputes the volumes; `EN_deletecurve` does not.

**Fix:** recompute the volumes of tanks that use the deleted curve, as `EN_TANKDIAM` does.

[Write-up, test and patch](SIM-24-deletecurve-leaves-tank-volumes/)

### SIM-25: Changing a pump's power while the solver is open has no effect

A toolkit program that sets a pump's power with `EN_PUMP_POWER` while the hydraulic solver is open gets results for the old pump, also after `EN_initH`. In the test a 10 hp pump set to 20 hp still adds 88.14 ft instead of 176.28 ft, and a curve pump switched to 10 hp adds no head at all instead of 88.14 ft.

The setter stores the power, but the pump's coefficients are derived only at `EN_openH`. EPANET 2.2 recomputed them in the setter; PR [#758](https://github.com/OpenWaterAnalytics/EPANET/issues/758) removed that call.

**Fix:** call `updatepumpparams()` (added by [SIM-22](SIM-22-curve-change-while-solver-open/)) from the setter while the solver is open.

[Write-up, test and patch](SIM-25-pump-power-change-while-solver-open/)

## Output file

### SIM-23: Output-file head loss is computed from heads rounded to 4-byte floats

In the usual path (`runepanet`, `EN_solveH` then `EN_solveQ`), the output file is written from a scratch hydraulics file that stores heads as 4-byte floats. Head loss and friction factor are differences of those heads, so in lightly loaded pipes they lose most of their digits: in Net2, 25.4 % of pipe values are off by more than 1 %, some by a factor of 40, and 1.6 % are stored as 0 although the pipe carries flow. The text report shows the same values. `EN_getlinkvalue` is correct.

`readhyd()` loads the rounded heads into `NodeHead`, and `linkoutput()` computes `1000 * |H1 - H2| / L` from them.

**Fix:** while the hydraulics run, also save the heads as doubles to a second scratch file and read them back in `readhyd()`, leaving the hydraulics file format, which EPANET-MSX reads directly, unchanged.

[Write-up, test and patch](SIM-23-output-headloss-float-precision/)
