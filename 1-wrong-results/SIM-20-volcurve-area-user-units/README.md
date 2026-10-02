# SIM-20: EN_VOLCURVE stores the tank area in user units

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | In SI projects the tank's nominal area is 10.76 times too small. `EN_TANKDIAM` and the saved diameter are wrong, and if the curve is later deleted the tank runs with the wrong area. |
| **Reached from** | Toolkit API: `EN_setnodevalue(EN_VOLCURVE)` in a project with SI flow units. US projects are not affected. |
| **Where** | `EN_setnodevalue()`, case `EN_VOLCURVE`, in [`src/epanet.c:2698-2699`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2698-L2699) |
| **Since** | EPANET 2.2 (2019), when `EN_VOLCURVE` became settable |
| **GitHub** | No related issue |
| **Fix** | One line: [`SIM-20_fix.patch`](SIM-20_fix.patch) |

## The problem

A tank with a volume curve also keeps a nominal area, the slope of the curve from its first point to its last. EPANET stores areas in ft². The input file parser and `EN_settankdata` convert the curve's slope from user units; `EN_setnodevalue(EN_VOLCURVE)` does not.

In an SI project the curve is in metres and cubic metres, so the slope is in m², and the tank gets an area of 100 "ft²" (9.29 m²) where it should have 100 m². In the test, a tank that is a 100 m² cylinder reports a diameter of 3.439 m after its matching volume curve is assigned, instead of 11.284 m.

While the curve stays assigned, the hydraulics are right, because `tankvolume()` and `tankgrade()` use the curve and not the area. The wrong area shows up in:

- `EN_getnodevalue(EN_TANKDIAM)`;
- `EN_saveinpfile`, which writes the wrong diameter in `[TANKS]` (3.4393 for this tank). The parser recomputes the area from the curve when the file is read back, so this is only wrong text in the file;
- the tank areas in the prolog of the binary output file;
- the simulation, once the curve is removed. `EN_deletecurve` turns the tank back into a cylinder with its nominal area. In the test, the tank then rises 46.9 m in one hour while draining 5 L/s, instead of falling 0.18 m.

## Why it happens

```c
// src/epanet.c, EN_setnodevalue(), case EN_VOLCURVE
Tank[j].A = (curve->Y[n] - curve->Y[0]) /      // nominal area
    (curve->X[n] - curve->X[0]);
```

Curves are stored in user units. The other two places that compute the nominal area convert it:

```c
// src/epanet.c, EN_settankdata()
Tank[j].A = area / Ucf[ELEV] / Ucf[ELEV];

// src/input1.c, inittanks() and convertunits(): stored as a diameter, then converted
tank->A = sqrt(4.0 * a / PI);
...
tank->A = PI * SQR(tank->A / pr->Ucf[ELEV]) / 4.0;
```

In US units `Ucf[ELEV]` is 1, so only SI projects are affected. The factor is 1 / 0.3048² = 10.76.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-20_si-tank.inp`](SIM-20_si-tank.inp) | SI (L/s) network: tank T1, a 100 m² cylinder, drains 5 L/s into J1. Volume curve VC describes the same cylinder. |
| [`SIM-20_test.c`](SIM-20_test.c) | Assigns VC to T1 with `EN_VOLCURVE` and reads the diameter, then deletes the curve and runs one hour |
| [`SIM-20_fix.patch`](SIM-20_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-20            # unpatched EPANET: FAIL
tools/run-test.sh SIM-20 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-20_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o SIM-20_test
./SIM-20_test
```

**Without the fix:**

```
T1 diameter: 11.284 m as a cylinder, 3.439 m after EN_VOLCURVE (expected 11.284)
After EN_deletecurve, T1's level changes by +46.882 m in one hour (expected -0.180)
FAIL: EN_VOLCURVE gave T1 an area of 9.29 m2 instead of 100 m2
```

The 46.9 m rise comes from the area mismatch: the tank keeps the volumes computed from the curve (500 m³ at 5 m), but converts volume to level with an area of 9.29 m².

**With the fix:**

```
T1 diameter: 11.284 m as a cylinder, 11.284 m after EN_VOLCURVE (expected 11.284)
After EN_deletecurve, T1's level changes by -0.180 m in one hour (expected -0.180)
PASS: EN_VOLCURVE gives T1 the curve's area of 100 m2
```

## The fix

Convert the area to ft², as `EN_settankdata` does:

```diff
         Tank[j].A = (curve->Y[n] - curve->Y[0]) /      // nominal area
             (curve->X[n] - curve->X[0]);
+        Tank[j].A /= SQR(Ucf[ELEV]);                   // area in ft2
         break;
```

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- `EN_deletecurve` keeps the tank's volumes from the curve and only drops the curve index. That is consistent after this fix for a straight-line curve like the one in the test, but not for a curved one: see [SIM-24](../SIM-24-deletecurve-leaves-tank-volumes/). With the SIM-24 patch but not this one, the deletion recomputes the volumes from the wrong area and the test tank falls 1.94 m instead of 0.18 m, so the test still fails.
- The SIM-17 patch changes the neighbouring `EN_MINLEVEL` case; the two patches apply together.
