# API-19: EN_SP_GRAVITY does not rescale stored emitter and PDA values

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | The model depends on the order of the calls. Emitter coefficients, PDA pressures, PRV/PSV/PBV settings and pressure controls set before `EN_setoption(EN_SP_GRAVITY)` silently change their psi values: with gravity 2 they double, and emitter coefficients drop by a factor of 1.41. |
| **Reached from** | Toolkit API: `EN_setoption(EN_SP_GRAVITY)` after any pressure value has been set, including the values read from the input file |
| **Where** | `EN_setoption()` in [`src/epanet.c:1362-1371`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1362-L1371) |
| **Since** | EPANET 2.2 (2019), when `EN_SP_GRAVITY` became settable |
| **GitHub** | No related issue |
| **Fix** | About 35 lines in one `case` block: [`API-19_fix.patch`](API-19_fix.patch) |

## The problem

EPANET stores pressures internally as heads in feet. Changing the specific gravity changes how many feet one psi is. `EN_setoption(EN_SP_GRAVITY)` updates the psi-to-feet factor `Ucf[PRESSURE]`, but leaves the heads that were stored for values entered earlier in psi, kPa or bar. Those values now read back, and act, as different pressures.

The test network has a PRV set to 40 psi, an emitter of 10 gpm/psi^0.5, pressure-driven demand with a required pressure of 20 psi, and a control at 30 psi. Read from a file with `SPECIFIC GRAVITY 2`, the model is as written. Read from a file with gravity 1 and then given `EN_setoption(EN_SP_GRAVITY, 2)`, the PRV holds 80 psi, the required pressure is 40 psi, the control level is 60 psi, and the emitter coefficient is 7.07.

In an input file this cannot happen: every pressure value is converted with the final specific gravity, whatever the order of the sections. Through the API the same calls in a different order give a different model.

## Why it happens

```c
// src/epanet.c, EN_setoption()
case EN_SP_GRAVITY:
    if (value <= 0.0) return 213;
    if (p->parser.Pressflag == PSI || 
        p->parser.Pressflag == KPA || 
        p->parser.Pressflag == BAR)
    {
        Ucf[PRESSURE] *= (value / hyd->SpGrav);
    }
    hyd->SpGrav = value;
    break;
```

These values were converted from psi, kPa or bar to feet with the old gravity, and keep that conversion:

- `Node.Ke`, the emitter coefficient (always per psi in US units, converted with `PSIperFT * SpGrav`);
- `hyd->Pmin` and `hyd->Preq`, the PDA pressures;
- `Link.Kc` and `Link.InitSetting` of PRVs, PSVs and PBVs;
- `Control.Grade` for controls on junction pressure, and `Control.Setting` for controls that set a PRV, PSV or PBV.

Rule premises and actions are not affected: they are stored in user units and converted when they are evaluated.

## How to reproduce

| File | What it is |
|---|---|
| [`API-19_sg1.inp`](API-19_sg1.inp) | Reservoir R1 feeds J2 through PRV V1 (40 psi). J2 has a 100 gpm pressure-driven demand (required pressure 20 psi) and an emitter (10 gpm/psi^0.5); a control closes P2 if J2 is below 30 psi. Specific gravity 1. |
| [`API-19_sg2.inp`](API-19_sg2.inp) | The same network with specific gravity 2 |
| [`API-19_test.c`](API-19_test.c) | Opens `API-19_sg1.inp`, sets the specific gravity to 2, and compares five values with `API-19_sg2.inp` |
| [`API-19_fix.patch`](API-19_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-19            # unpatched EPANET: FAIL
tools/run-test.sh API-19 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-19_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-19_test
./API-19_test
```

**Without the fix:**

```
                               SG 2 in    SG 1 file
                               the file   + EN_SP_GRAVITY 2
J2 emitter coeff. (gpm/psi^n)      10.000      7.071   <-- differs
PDA required pressure (psi)        20.000     40.000   <-- differs
V1 PRV setting (psi)               40.000     80.000   <-- differs
Control level on J2 (psi)          30.000     60.000   <-- differs
J2 pressure after run (psi)        40.000     80.000   <-- differs
FAIL: 5 of 5 values differ from the same network read with specific gravity 2
```

**With the fix:**

```
J2 emitter coeff. (gpm/psi^n)      10.000     10.000
PDA required pressure (psi)        20.000     20.000
V1 PRV setting (psi)               40.000     40.000
Control level on J2 (psi)          30.000     30.000
J2 pressure after run (psi)        40.000     40.000
PASS: changing the specific gravity keeps the pressure values entered in psi
```

## The fix

The patch scales every stored head that came from a pressure by the ratio of the old to the new gravity, so the values keep their psi (or kPa, or bar) values:

```diff
+        ucf = hyd->SpGrav / value;
+        if (p->parser.Unitsflag == US)
+        {
+            // Emitter coeffs. are per psi in US units
+            for (i = 1; i <= Njuncs; i++) net->Node[i].Ke *= ucf;
+        }
         if (p->parser.Pressflag == PSI || 
             p->parser.Pressflag == KPA || 
             p->parser.Pressflag == BAR)
         {
             Ucf[PRESSURE] *= (value / hyd->SpGrav);
+            hyd->Pmin *= ucf;
+            hyd->Preq *= ucf;
+            ... PRV/PSV/PBV Kc and InitSetting, control Grade and Setting
```

With pressures in metres or feet, `Ucf[PRESSURE]` does not depend on the gravity, so only the US emitter coefficients change.

This is a choice between two consistent rules, and the maintainers may prefer the other one:

- **Keep the entered values (this patch).** The API then behaves like the input file, where the order of `[OPTIONS]` lines does not matter.
- **Keep the stored heads (current code).** Pressure values are re-expressed in the new gravity, as `EN_setoption(EN_PRESS_UNITS)` re-expresses them in new units. A fixed orifice does pass the same flow at the same head whatever the fluid, so there is a physical argument for it. This option needs only a documentation change: set `EN_SP_GRAVITY` before any pressure value, and expect values set earlier to change.

The patch does not touch `hyd->LinkSetting`, so a gravity change while the hydraulic solver is open only reaches the valves at the next `EN_initH`. Refusing the change with error 262 while the solver is open, as `EN_HEADLOSSFORM` does, would make that explicit.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- [SIM-05](../../1-wrong-results/SIM-05-emitter-coefficient-pressure-units/) is the related emitter-unit bug in the `EN_EMITEXPON` case of the same function. The two patches change different `case` blocks and apply together.
