# API-17: EN_gettimeparam(EN_HALTFLAG) always returns 0

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | A program cannot tell that a run was halted by `UNBALANCED STOP`: the flag documented for that reads 0. |
| **Reached from** | Toolkit: `EN_gettimeparam(ph, EN_HALTFLAG, &value)` |
| **Where** | `EN_gettimeparam()` in [`src/epanet.c:1622-1625`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L1622-L1625) |
| **Since** | `EN_HALTFLAG` has been in the header since v2.1 (2015) and has never returned the flag. Commit [9f1b46b](https://github.com/OpenWaterAnalytics/EPANET/commit/9f1b46b66a960bc50c39ae3ae18c8e376d134b3b) (2019) added the empty case it has today. |
| **GitHub** | Reported in [#2](https://github.com/OpenWaterAnalytics/EPANET/issues/2) (2015), closed without a fix |
| **Fix** | 2 lines: [`API-17_fix.patch`](API-17_fix.patch) |

## The problem

With `UNBALANCED STOP` (`EN_setoption(ph, EN_UNBALANCED, -1)`), EPANET halts the run at the first hydraulic solution that does not converge. `EN_HALTFLAG` is documented as the "flag indicating if the simulation was halted", so a program stepping through the hydraulics can check it to see whether the run ended early. It always reads 0. In the test, Net1 with one trial allowed halts at t = 0 of a 24-hour run, and `EN_HALTFLAG` is 0.

## Why it happens

The `EN_QTIME` case has no `break` and falls through into an `EN_HALTFLAG` case with no body. `*value` was set to 0 at the top of the function, so `EN_HALTFLAG` returns 0, and `EN_QTIME` only works because the empty case ends with a `break`:

```c
// src/epanet.c, EN_gettimeparam()
    *value = 0;
    ...
    case EN_QTIME:
        *value = time->Qtime;
    case EN_HALTFLAG:
        break;
```

The flag itself exists: `runhyd()` sets `hyd->Haltflag` when the solution is unbalanced and no extra trials are allowed, and `nexthyd()` then ends the run.

## How to reproduce

| File | What it is |
|---|---|
| [`API-17_Net1.inp`](API-17_Net1.inp) | EPANET's example network Net1, unchanged |
| [`API-17_test.c`](API-17_test.c) | Sets `UNBALANCED STOP` and 1 trial, steps through the hydraulics and reads `EN_HALTFLAG` after each step |
| [`API-17_fix.patch`](API-17_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-17            # unpatched EPANET: FAIL
tools/run-test.sh API-17 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-17_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-17_test
./API-17_test
```

**Without the fix:**

```
t = 0 s: EN_runH returns 1, next time step 0 s, EN_HALTFLAG = 0
The run stopped at t = 0 s of 86400 s
FAIL: the run was halted at t = 0 s but EN_HALTFLAG is 0
```

(Return code 1 is the warning "system unbalanced".)

**With the fix:**

```
t = 0 s: EN_runH returns 1, next time step 0 s, EN_HALTFLAG = 1
The run stopped at t = 0 s of 86400 s
PASS: EN_HALTFLAG is 1 after the run was halted
```

## The fix

```diff
     case EN_QTIME:
         *value = time->Qtime;
+        break;
     case EN_HALTFLAG:
+        *value = p->hydraul.Haltflag;
         break;
```

`inithyd()` resets the flag at each `EN_initH`, and nothing else clears it, so after `EN_solveH` it still says whether that run was halted. EPANET's own unit tests (6 suites) pass with the patch.
