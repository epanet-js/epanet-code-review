# SIM-05: Emitter coefficients are re-converted with the pressure-unit factor

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. With kPa pressures in a GPM model, re-setting the emitter exponent multiplies every emitter coefficient by 2.63, and saving and reopening the file divides it by 2.63. |
| **Reached from** | Toolkit API: `EN_setoption(EN_EMITEXPON)` and `EN_saveinpfile`, in any project whose pressure units are not psi (US flow units) or metres (SI flow units) |
| **Where** | `EN_setoption()` in [`src/epanet.c:1312`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1312) and `saveinpfile()` in [`src/inpfile.c:363`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L363) |
| **Since** | v2.3 (PR [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862), July 2025) |
| **GitHub** | An incomplete part of [PR #862](https://github.com/OpenWaterAnalytics/EPANET/pull/862), which decoupled the pressure units from the flow units. The review of [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862) had flagged the emitter conversion. |
| **Fix** | Two lines changed in two files, plus comments: [`SIM-05_fix.patch`](SIM-05_fix.patch) |

## The problem

Since PR [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862), an emitter coefficient is always flow per psi^n in US flow units, or flow per metre^n in SI flow units, whatever the `PRESSURE` option says. The input file parser, `EN_getnodevalue(EN_EMITTER)` and `EN_setnodevalue(EN_EMITTER)` all follow this rule. Two places still convert with the factor of the current pressure units:

- **`EN_setoption(EN_EMITEXPON)`** reads every coefficient, then converts it back to internal units with the wrong factor. Setting the exponent to the value it already has changes every emitter. In a GPM model with pressures in kPa, a coefficient of 10 becomes 26.26, and the emitter in the test network discharges 172.71 gpm instead of 65.82 gpm.
- **`EN_saveinpfile`** writes every coefficient with the wrong factor. In the same model the saved file has 3.81 where the project has 10, so the reopened model has a different emitter.

Both are silent: the calls return 0, and the reopened file loads without a warning. Projects with psi in US units, or metres in SI units, are not affected, because there the two factors are equal. Any other combination is, including kPa in an SI model.

## Why it happens

The parser, getter and setter convert with a fixed psi or metre factor. The other two places use `Ucf[PRESSURE]`, which is the factor for the project's pressure units:

```c
// src/input1.c, convertunits(): the parser's conversion
ecf = (parser->Unitsflag == US) ? (PSIperFT * hyd->SpGrav) : (MperFT);
ucf = pow(pr->Ucf[FLOW], hyd->Qexp) / ecf;

// src/epanet.c, EN_setoption(), case EN_EMITEXPON
ucf = pow(Ucf[FLOW], n) / Ucf[PRESSURE];
for (i = 1; i <= Njuncs; i++)
{
    j = EN_getnodevalue(p, i, EN_EMITTER, &Ke);   // read with ecf
    if (j == 0 && Ke > 0.0) net->Node[i].Ke = ucf / pow(Ke, n);
}

// src/inpfile.c, saveinpfile()
ke = pr->Ucf[FLOW] / pow(pr->Ucf[PRESSURE] * node->Ke, (1.0 / hyd->Qexp));
```

With kPa, `Ucf[PRESSURE]` is 6.895 times the psi factor, so with an exponent of 0.5 the coefficient changes by a factor of 6.895^0.5 = 2.63. `EN_setoption` multiplies by it and `saveinpfile` divides by it.

Before PR [#862](https://github.com/OpenWaterAnalytics/EPANET/issues/862), every one of these places used `Ucf[PRESSURE]`. That PR changed the parser, getter and setter to the fixed factor, but not these two call sites.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-05_emitter.inp`](SIM-05_emitter.inp) | Reservoir R1 feeds junction J1, which has an emitter with coefficient 10 |
| [`SIM-05_test.c`](SIM-05_test.c) | For GPM and LPS and each of the five pressure units, re-sets the emitter exponent to its current value and reads the coefficient, then saves and reopens the project and reads it again |
| [`SIM-05_fix.patch`](SIM-05_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-05            # unpatched EPANET: FAIL
tools/run-test.sh SIM-05 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-05_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-05_test
./SIM-05_test
```

**Without the fix**, only the two default combinations keep the coefficient:

```
Flow  Pressure   coefficient after         coefficient after
units units      EN_setoption(EMITEXPON)   save and reopen   (expected 10)
GPM   PSI             10.0000                   10.0000
GPM   KPA             26.2583                    3.8083   <-- wrong
GPM   METERS           8.3871                   11.9230   <-- wrong
GPM   BAR              2.6258                   38.0837   <-- wrong
GPM   FEET            15.1917                    6.5826   <-- wrong
LPS   PSI             11.9230                    8.3871   <-- wrong
LPS   KPA             31.3079                    3.1941   <-- wrong
LPS   METERS          10.0000                   10.0000
LPS   BAR              3.1307                   31.9413   <-- wrong
LPS   FEET            18.1131                    5.5209   <-- wrong

GPM, kPa: J1 emitter flow 65.82 gpm before EN_setoption(EN_EMITEXPON, 0.5), 172.71 gpm after
FAIL: the emitter coefficient changed in 8 of 10 unit combinations
```

**With the fix:**

```
GPM   PSI             10.0000                   10.0000
GPM   KPA             10.0000                   10.0000
...
LPS   FEET            10.0000                   10.0000

GPM, kPa: J1 emitter flow 65.82 gpm before EN_setoption(EN_EMITEXPON, 0.5), 65.82 gpm after
PASS: the emitter coefficient stays 10 in every unit combination
```

## The fix

Use the parser's factor in both places:

```diff
         n = 1.0 / value;
-        ucf = pow(Ucf[FLOW], n) / Ucf[PRESSURE];
+        // Emitter coeffs. are per psi (US) or per meter (SI) of pressure
+        ucf = (p->parser.Unitsflag == US) ? (PSIperFT * hyd->SpGrav) : MperFT;
+        ucf = pow(Ucf[FLOW], n) / ucf;
```

```diff
+    // Emitter coeffs. are per psi (US) or per meter (SI) of pressure
+    ucf = (parser->Unitsflag == US) ? (PSIperFT * hyd->SpGrav) : MperFT;
     for (i = 1; i <= net->Njuncs; i++)
     {
         node = &net->Node[i];
         if (node->Ke == 0.0) continue;
-        ke = pr->Ucf[FLOW] / pow(pr->Ucf[PRESSURE] * node->Ke, (1.0 / hyd->Qexp));
+        ke = pr->Ucf[FLOW] / pow(ucf * node->Ke, (1.0 / hyd->Qexp));
```

The same expression now appears in five places (parser, getter, setter, `EN_setoption`, writer). A small helper in `project.c` would keep them from drifting apart again.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- Files saved by v2.3 to v2.3.5 from a model with non-default pressure units carry wrong emitter coefficients. The patch does not repair such files.
- [API-19](../../3-api/API-19-specific-gravity-call-order/) is the related problem for specific gravity: the psi factor includes `SpGrav`, and `EN_setoption(EN_SP_GRAVITY)` does not rescale the stored emitter coefficients. The two patches change different `case` blocks of `EN_setoption` and apply together.
