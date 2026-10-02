# API-16: EN_CHECKFREQ, EN_MAXCHECK below 1 and tiny viscosity are accepted but cannot be reloaded

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | `EN_setoption` accepts values that `EN_saveinpfile` cannot write in a form the parser reads back. The saved file either fails to open (error 200) or reopens with a viscosity or diffusivity thousands of times too large. |
| **Reached from** | Toolkit API: `EN_setoption` with `EN_CHECKFREQ` or `EN_MAXCHECK` below 1, `EN_SP_VISCOS` of 0.001 or less, or `EN_SP_DIFFUS` above 0 and up to 0.0001, followed by `EN_saveinpfile` |
| **Where** | `EN_setoption()` in [`src/epanet.c:1373-1384`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1373-L1384) and [`:1390-1391`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1390-L1391); the parser's rules are in `adjustdata()`, [`src/input1.c:282-299`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input1.c#L282-L299), and `optionvalue()`, [`src/input3.c:2162`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2162) |
| **Since** | EPANET 2.2 (2019), when these options became settable through `EN_setoption` |
| **GitHub** | No related issue |
| **Fix** | Four range checks in `EN_setoption`: [`API-16_fix.patch`](API-16_fix.patch) |

## The problem

A project built or edited through the toolkit should survive `EN_saveinpfile` and a reload. These option values do not:

- **`EN_CHECKFREQ` and `EN_MAXCHECK` below 1.** `EN_setoption` stores `(int)value`, so 0.25 becomes 0. The writer saves `CHECKFREQ 0`, and the parser refuses any value of 0 or less with error 213, so `EN_open` of the saved file returns 200. `EN_TRIALS` already refuses values below 1.
- **`EN_SP_VISCOS` of 0.001 or less.** The API value is relative to water. The parser reads a `VISCOSITY` above 0.001 as relative, and 0.001 or less as the actual kinematic viscosity in ft²/s or m²/s. A relative 0.0005 is saved as `0.000500` and reloaded as an actual viscosity 45 times that of water (`EN_getoption` returns 45.45). A relative 1e-10 is saved as `0.000000` and the file fails to open.
- **`EN_SP_DIFFUS` above 0 and up to 0.0001.** The same rule with a threshold of 0.0001: a relative 5e-5 comes back as 3,846.

The values themselves are not meaningful (water's kinematic viscosity changes by a factor of about 6 between 0 °C and 100 °C), but `EN_setoption` returns 0 for them, and the damage only shows when the saved file is used.

With `CHECKFREQ` or `MAXCHECK` at 0 the run also behaves differently: by reading `hydsolve()`, `nextcheck` starts at 0 and `*iter == nextcheck` never holds, so the periodic status checks before convergence never run. The review log described this as checks running every iteration; reading the loop again, they never run.

## Why it happens

```c
// src/epanet.c, EN_setoption()
case EN_SP_VISCOS:
    if (value <= 0.0) return 213;
    hyd->Viscos = value * VISCOS;
    break;

case EN_CHECKFREQ:
    hyd->CheckFreq = (int)value;
    break;
```

```c
// src/input1.c, adjustdata(): how a file's VISCOSITY is read
else if (hyd->Viscos > 1.e-3)
{
    hyd->Viscos = hyd->Viscos * VISCOS; // Multiplier supplied
}
else hyd->Viscos = hyd->Viscos / ucf;   // Actual value supplied
```

`saveinpfile()` always writes the relative value with `%-.6f` ([`src/inpfile.c:732-733`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L732-L733)), so a relative value at or below the threshold is read back as an absolute one.

## How to reproduce

| File | What it is |
|---|---|
| [`API-16_network.inp`](API-16_network.inp) | Reservoir R1 feeds junction J1 through pipe P1, with chlorine decay |
| [`API-16_test.c`](API-16_test.c) | For each option value: `EN_setoption`, `EN_saveinpfile`, reopen, `EN_getoption`. The first two values are valid controls. |
| [`API-16_fix.patch`](API-16_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-16            # unpatched EPANET: FAIL
tools/run-test.sh API-16 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-16_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o API-16_test
./API-16_test
```

**Without the fix:**

```
Option       value     EN_setoption  reopen  value after reload
CHECKFREQ          3         0         0   3
VISCOSITY        0.5         0         0   0.5
CHECKFREQ       0.25         0       200   0          <-- accepted, but lost in the saved file
MAXCHECK         0.5         0       200   0          <-- accepted, but lost in the saved file
VISCOSITY      1e-10         0       200   0          <-- accepted, but lost in the saved file
VISCOSITY     0.0005         0         0   45.4545    <-- accepted, but lost in the saved file
DIFFUSIVITY    5e-05         0         0   3846.15    <-- accepted, but lost in the saved file
FAIL: 5 option values were accepted but do not survive a save and reload
```

**With the fix:**

```
Option       value     EN_setoption  reopen  value after reload
CHECKFREQ          3         0         0   3
VISCOSITY        0.5         0         0   0.5
CHECKFREQ       0.25       213     (not saved)
MAXCHECK         0.5       213     (not saved)
VISCOSITY      1e-10       213     (not saved)
VISCOSITY     0.0005       213     (not saved)
DIFFUSIVITY    5e-05       213     (not saved)
PASS: every accepted value survives a save and reload; the others are refused
```

## The fix

Refuse in `EN_setoption` what the file format cannot hold:

```diff
     case EN_SP_VISCOS:
-        if (value <= 0.0) return 213;
+        // An input file reads a VISCOSITY of 0.001 or less as an actual
+        // viscosity, so such relative values could not be saved
+        if (value <= 1.e-3) return 213;
 ...
     case EN_CHECKFREQ:
+        if (value < 1.0) return 213;
 ...
     case EN_MAXCHECK:
+        if (value < 1.0) return 213;
 ...
     case EN_SP_DIFFUS:
+        // An input file reads a DIFFUSIVITY of 0.0001 or less (other than 0)
+        // as an actual diffusivity, so such relative values could not be saved
+        if (value > 0.0 && value <= 1.e-4) return 213;
```

A diffusivity of 0 stays allowed: it is written and read back as 0.

The alternative for viscosity and diffusivity is to change the writer: when the relative value is at or below the threshold, write the actual value (in ft²/s or m²/s) in a format that keeps small numbers, such as `%g`. That keeps every value the API accepts, but changes `inpfile.c`, which other fixes also edit, for values no real network uses.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The input file has the matching hole for `CHECKFREQ` and `MAXCHECK`: the parser accepts 0.5 (it only refuses values of 0 or less) and truncates it to 0. In a separate run, a file with `CHECKFREQ 0.5` opened, and its saved copy failed to open with error 200. [FILE-13](../../4-files/FILE-13-fractional-trials-zero-iterations/) adds an `if (y < 1.0)` check for `TRIALS` in `optionvalue()`, and [CRASH-04](../../2-crashes/CRASH-04-inf-nan-time-values/) rejects huge values for all three; the same `y < 1.0` check on the `CHECKFREQ` and `MAXCHECK` lines ([`src/input3.c:2181-2182`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/input3.c#L2181-L2182)) would close it. [FILE-17](../../4-files/FILE-17-options-load-but-not-reload/) covers this.
- `SPECIFIC GRAVITY` is written with the same `%-.6f` format. In a separate run, `EN_setoption(EN_SP_GRAVITY, 1e-7)` was accepted and the saved file failed to open with error 200. It is not changed here because the [API-19](../API-19-specific-gravity-call-order/) patch rewrites the same `case`; [FILE-17](../../4-files/FILE-17-options-load-but-not-reload/) covers it.
