# Water quality

[All issues](../README.md)

WQ-01, WQ-02 and WQ-03 give wrong results with no warning, and WQ-02 affects many real models: pipes downstream of pumps are often modelled as check-valve pipes. The water-quality crashes are in the [crash list](../2-crashes/README.md) (CRASH-07, CRASH-10 and CRASH-11), and the `EN_setqualtype` conversion bug is [SIM-18](../1-wrong-results/SIM-18-setqualtype-scales-initial-quality/).

| ID | Issue | What happens | Fix |
|---|---|---|---|
| [WQ-01](WQ-01-fractional-reaction-order-nan/) | A reaction order between 0 and 1 gives NaN across the network | With a bulk or tank order between 0 and 1, water at 0 mg/L gets a NaN reaction rate that spreads and stays in tanks. | Return a zero potential in `bulkrate()` when `c <= 0` |
| [WQ-02](WQ-02-check-valve-pipes-have-no-volume/) | Check-valve pipes have no volume and no reactions in water quality | Water crosses a check-valve pipe instantly and unchanged: J2 gets 1.00 mg/L from hour 1 instead of 0.90 mg/L after 2.4 h. | Test `Type <= PIPE` in `initsegs()` and `reactpipes()` |
| [WQ-03](WQ-03-fifo-lifo-tanks-book-inflow-as-overflow/) | FIFO and LIFO tanks treat filling as overflow | FIFO and LIFO tanks discard water they should store, even when they cannot overflow: 77 % of the inflow in the test's normal run. | Track tank volume in the quality solver; overflow only above `Vmax`, if allowed |
| [WQ-04](WQ-04-reaction-averages-low-with-report-start/) | Average reaction rates are low when REPORT START is not 0 | The output file's average reaction and source rates are divided by the whole duration: 54 % of the true value with REPORT START at mid-run. | Count steps with `Qtime >= Rstart`; divide by `Dur - Rstart` |

Each issue's folder has the full write-up, a test program, its input files and a fix patch. [Reproducing the issues](../reproducing.md) explains how to run them.

## WQ-01: A reaction order between 0 and 1 gives NaN across the network

EPANET accepts any reaction order, but with a bulk or tank order between 0 and 1 any water at exactly 0 mg/L gets a reaction rate of NaN. Pipes and tanks often start at 0, so this is common. The NaN travels with the water, stays in any completely mixed tank it enters, and returns to the network when the tank drains. The mass balance ratio is NaN, and the run reports no error.

`bulkrate()` computes the potential as `c1 * pow(c, order - 1)`. For `c = 0` and an order below 1 this is `0 * inf`, and neither the `c < 0` guard nor the later `MAX(0, c)` catches NaN.

**Fix:** return a zero reaction potential when the concentration is not positive.

[Write-up, test and patch](WQ-01-fractional-reaction-order-nan/)

## WQ-02: Check-valve pipes have no volume and no reactions in water quality

A pipe with a check valve gets no volume in the water quality solver, so water crosses it instantly and unchanged: no travel time, no ageing and no reactions, however long the pipe. In the test, two identical 10,000 ft pipes take 2.4 h to deliver chlorine; the open pipe delivers 0.90 mg/L after 2.4 h, the check-valve pipe 1.00 mg/L from the first hour. Pump discharge pipes are often modelled as check-valve pipes.

`initsegs()` gives an initial segment only to links of type `PIPE`, and `reactpipes()` skips links that are not `PIPE`. A check-valve pipe is `CVPIPE` (0), so it is treated like a pump or valve.

**Fix:** test `Type <= PIPE` in both functions, as the rest of the code does.

[Write-up, test and patch](WQ-02-check-valve-pipes-have-no-volume/)

## WQ-03: FIFO and LIFO tanks treat filling as overflow

Since v2.3, FIFO and LIFO tanks decide whether they are overflowing from the hydraulic solver's tank volume, which during a quality step is the volume at the end of the run (normal `EN_solveH` then `EN_solveQ` path) or at the end of the current step (hydraulics and quality stepped together). A tank that ends the run full books every filling step as overflow, even if it is not allowed to overflow. In the test, 77 % of the chlorine that enters the network is discarded in the normal path and 3 % when stepped together. The mass balance ratio stays at 1.000 because the lost mass is booked as outflow.

`tankmix3()` and `tankmix4()` test `tank->V >= Vmax`, and treat the whole net inflow of the step as overflow when it is true, without checking `CanOverflow`.

**Fix:** have the quality solver track each tank's volume itself, and treat only the inflow above `Vmax` as overflow, for tanks that can overflow.

[Write-up, test and patch](WQ-03-fifo-lifo-tanks-book-inflow-as-overflow/)

## WQ-04: Average reaction rates are low when REPORT START is not 0

The binary output file ends with network-wide average reaction rates (bulk, wall, tank) and the average source inflow rate. The solver counts reactions only from REPORT START on, but the averages divide by the whole duration, so any REPORT START above 0 makes them too low. In a steady 24 h test run with REPORT START 12:00, both the bulk reaction rate and the source rate come out at 54 % of their true values.

The totals are also counted from one hydraulic step too early: the tests use `Htime`, which already holds the end of the hydraulic period during a quality step. `savenetreacts()` divides by `Dur`.

**Fix:** count a quality step when `Qtime >= Rstart`, and divide the totals by `Dur - Rstart`.

[Write-up, test and patch](WQ-04-reaction-averages-low-with-report-start/)
