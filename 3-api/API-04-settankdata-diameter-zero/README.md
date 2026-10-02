# API-04: EN_settankdata with diameter 0 turns a tank into a reservoir

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | The tank becomes a reservoir with a fixed head and unlimited supply, and the change cannot be undone through the toolkit. Neither the call nor the documentation says so. |
| **Reached from** | Toolkit API: `EN_settankdata` with `diam` = 0 and no volume curve |
| **Where** | `EN_settankdata()` in [`src/epanet.c:2911`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2911) (accepts 0) and [`:2932-2936`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L2932-L2936) (sets the area to 0) |
| **Since** | `EN_settankdata` was added in v2.2 |
| **GitHub** | [#647](https://github.com/OpenWaterAnalytics/EPANET/issues/647) (2021, closed): the maintainer called it a feature and kept it for backward compatibility |
| **Fix** | Documentation only, 8 lines: [`API-04_fix.patch`](API-04_fix.patch). An error-209 alternative is below. |

## The problem

EPANET decides whether a storage node is a tank or a reservoir from its cross-sectional area: `EN_getnodetype`, the hydraulic solver and the tank setters all treat `Tank.A == 0` as a reservoir. `EN_settankdata` accepts a diameter of 0 when no volume curve is given and stores an area of 0. From then on:

- `EN_getnodetype` returns `EN_RESERVOIR`.
- The node holds a fixed head of `elev` + `initlvl` and supplies any flow. In the test, T1 stays at 110 ft for 4 hours while an identical tank, T2, drains to 104.59 ft.
- `EN_settankdata` and the tank setters of `EN_setnodevalue` return 263 ("node is not a tank"), so the diameter cannot be set back.

`EN_setnodevalue(EN_TANKDIAM, 0)` refuses the same value with error 209. The `EN_settankdata` documentation says only "the tank's diameter (0 if a volume curve is supplied)".

This was reported as [#647](https://github.com/OpenWaterAnalytics/EPANET/issues/647) in 2021. The maintainer replied: "I'd call it a feature. I'd vote to leave it as is and not break backward compatibility for such an uncommon action." The input file behaves the same way: a `[TANKS]` line with diameter 0 and no volume curve is loaded as a reservoir (checked with `runepanet` at 83e25bb: the summary lists it under reservoirs, with head `elev` + `initlvl`).

## Why it happens

The only check on the diameter is that it is not negative. The area then follows from it, and the node type is derived from the area:

```c
// src/epanet.c, EN_settankdata()
    if (diam < 0.0 || minvol < 0.0) return 209;
    ...
    // Tank diameter supplied
    else area = PI * diam * diam / 4.0;

    // Assign parameters to tank object
    net->Node[Tank[j].Node].El = elev / Ucf[ELEV];
    Tank[j].A = area / Ucf[ELEV] / Ucf[ELEV];

// src/epanet.c, EN_getnodetype()
        if (p->network.Tank[index - p->network.Njuncs].A == 0.0)
        {
            *nodeType = EN_RESERVOIR;
```

## How to reproduce

| File | What it is |
|---|---|
| [`API-04_two-tanks.inp`](API-04_two-tanks.inp) | Two identical tanks, T1 and T2 (50 ft diameter, bottom 100 ft, initial level 10 ft), both draining to J1 |
| [`API-04_test.c`](API-04_test.c) | Sets T2's diameter to 0 with `EN_setnodevalue` and T1's with `EN_settankdata`, tries to restore T1, runs 4 hours and prints both heads |
| [`API-04_fix.patch`](API-04_fix.patch) | The fix (documentation) |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-04            # unpatched EPANET: PASS
tools/run-test.sh API-04 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-04_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-04_test
./API-04_test
```

The patch changes only the documentation, so the test checks the behaviour that the patch documents, and it passes with and without the patch. The output is the same for both:

```
EN_setnodevalue(T2, EN_TANKDIAM, 0) returned 209
EN_settankdata(T1, diameter 0) returned 0; T1 is now type 1 (EN_TANK = 2, EN_RESERVOIR = 1)
EN_settankdata(T1, diameter 50) returned 263
Head after 4 hours: T1 110.00 ft, T2 104.59 ft (both started at 110 ft)
PASS: as documented, diameter 0 turned T1 into a reservoir for good, and EN_setnodevalue refused it with 209
```

## The fix

The patch follows the ruling in [#647](https://github.com/OpenWaterAnalytics/EPANET/issues/647) and documents the behaviour in `include/epanet2_2.h`, which is where the Doxygen reference for `EN_settankdata` comes from:

```diff
   These properties have units that depend on the units used for flow rate (see @ref Units).
+
+  If `diam` is 0 and no volume curve is supplied, the tank's area becomes 0 and the node
+  is converted to a reservoir with a fixed head of `elev` + `initlvl`, as for a tank with
+  zero diameter in an input file. This is kept for backward compatibility. It cannot be
+  undone: ::EN_getnodetype then returns `EN_RESERVOIR`, and this function and
+  ::EN_setnodevalue for the tank's diameter, minimum and maximum levels, minimum volume
+  and volume curve return error 263 for the node. To change only a tank's diameter, use
+  ::EN_setnodevalue with `EN_TANKDIAM`, which rejects 0 with error 209.
```

Why documentation rather than code: the maintainer ruled on this explicitly, the toolkit matches the input file format, and the toolkit has no other way to turn a tank into a reservoir (there is no `EN_setnodetype`), so some callers may rely on it.

**Alternative: reject it.** If the maintainers would rather make the API consistent with `EN_setnodevalue`, one line does it:

```diff
     if (diam < 0.0 || minvol < 0.0) return 209;
+    if (diam == 0.0 && strlen(volcurve) == 0) return 209;
```

We built this variant at 83e25bb: EPANET's unit tests (6 suites) still pass, `EN_settankdata` returns 209 and T1 stays a tank. The test in this folder then prints `FAIL`, because it checks the documented behaviour; its expectations would change with the code. The input file parser would still turn a zero-diameter tank into a reservoir, so the two paths would differ.

## Notes

- The node keeps `Node.Type == TANK` internally; only `Tank.A` changes. Code that tests `Node.Type` rather than `Tank.A` still sees a tank. By reading the code, one example is `EN_setnodevalue(EN_CANOVERFLOW)`, which checks `Node.Type` and so is still accepted after the conversion.
