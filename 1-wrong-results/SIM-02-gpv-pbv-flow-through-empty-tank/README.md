# SIM-02: GPV and PBV ignore TEMPCLOSED and pass flow through an empty or full tank

| | |
|---|---|
| **Category** | [Wrong simulation results](../README.md) |
| **Impact** | Wrong results with no warning. Water flows out of an empty tank (or into a full one) through the valve, the tank level does not change, and the reported valve flow is 0. |
| **Reached from** | Any run with a GPV or PBV connected to a tank that reaches its minimum or maximum level |
| **Where** | `gpvcoeff()` in [`src/hydcoeffs.c:969`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L969) and `pbvcoeff()` in [`src/hydcoeffs.c:1006-1026`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/hydcoeffs.c#L1006-L1026) |
| **Since** | EPANET 2.0: the same tests are in the first commit of this repository (2014) |
| **GitHub** | No related issue. The upstream branch `dev_2.3_hyd` has a partial fix (see [Notes](#notes)). |
| **Fix** | Three lines: [`SIM-02_fix.patch`](SIM-02_fix.patch) |

## The problem

When a tank reaches its minimum level, `tankstatus()` marks every link that would drain it as TEMPCLOSED, and the solver should then treat those links as closed. Pipes, pumps and the other valves do. A GPV or PBV does not: it keeps passing the flow its curve or setting allows.

The rest of the solver does treat the valve as closed. Its flow is not charged to the tank, so the tank level never changes, and `EN_getlinkvalue(EN_FLOW)` reports 0. The water appears from nowhere at the downstream node.

In the test network, an empty tank feeds a 200 gpm junction through a valve, and a reservoir with a lower head feeds the same junction through a pipe. The reservoir should supply the 200 gpm. Instead:

- **GPV:** 1,465 gpm flows back into the reservoir.
- **PBV:** 3,340 gpm flows back into the reservoir.

The report's flow balance shows a flow ratio of 27.0 for the three-valve test network (inflow 200 gpm, outflow 5,406 gpm), while the status log says "GPV V1 temporarily closed" and "PBV V2 temporarily closed". The same happens at a full tank, where the valve should stop filling it.

## Why it happens

`StatusType` is ordered `XHEAD`, `TEMPCLOSED`, `CLOSED`, `OPEN`, `ACTIVE`, so "closed for any reason" is `<= CLOSED`. That is the test in `pipecoeff()` (line 643), `pumpcoeff()` (808) and `valvecoeff()` (1268), and in `newlinkflows()`, which decides whether a link's flow counts towards a tank. `gpvcoeff()` tests `== CLOSED`, and `pbvcoeff()` does not look at the status at all:

```c
// src/hydcoeffs.c, gpvcoeff()
    // Treat as a pipe if valve closed
    if (hyd->LinkStatus[k] == CLOSED) valvecoeff(pr, k);
    // Otherwise utilize segment of head loss curve ...
    else
    ...

// src/hydcoeffs.c, pbvcoeff()
    // If valve fixed OPEN or CLOSED then treat as a pipe
    if (hyd->LinkSetting[k] == MISSING || hyd->LinkSetting[k] == 0.0)
    {
        valvecoeff(pr, k);
    }
    // If valve is active
    else
    ...
            hyd->P[k] = CBIG;
            hyd->Y[k] = hyd->LinkSetting[k] * CBIG;
```

A TEMPCLOSED GPV therefore follows its head-loss curve, and a TEMPCLOSED PBV forces its 5 ft head drop, while the tank is held at its level.

## How to reproduce

| File | What it is |
|---|---|
| [`SIM-02_empty-tanks.inp`](SIM-02_empty-tanks.inp) | Three copies of one layout: empty tank T feeds junction J (200 gpm) through valve V; reservoir R (head 50 ft, below the tank) feeds J through pipe P. V1 is a GPV, V2 a PBV, V3 a TCV for comparison. |
| [`SIM-02_test.c`](SIM-02_test.c) | Solves the first period and checks that each reservoir pipe carries the 200 gpm demand |
| [`SIM-02_fix.patch`](SIM-02_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh SIM-02            # unpatched EPANET: FAIL
tools/run-test.sh SIM-02 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc SIM-02_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-02_test
./SIM-02_test
```

**Without the fix**, the GPV and PBV push water from the empty tank back into their reservoirs, and report 0 flow while doing it:

```
Valve  Type   Valve flow   Pipe flow from reservoir   Expected pipe flow
                 (gpm)              (gpm)                    (gpm)
V1     GPV          0.0                -1465.5                    200.0   <-- wrong
V2     PBV          0.0                -3340.5                    200.0   <-- wrong
V3     TCV          0.0                  200.0                    200.0   
FAIL: 2 valve(s) passed flow out of an empty tank; the reservoir pipe does not carry the demand
```

**With the fix**, all three valves behave like the TCV:

```
V1     GPV          0.0                  200.0                    200.0   
V2     PBV          0.0                  200.0                    200.0   
V3     TCV          0.0                  200.0                    200.0   
PASS: no valve passes flow out of an empty tank; each reservoir supplies the 200 gpm demand
```

`runepanet SIM-02_empty-tanks.inp` shows the same in the flow balance: a flow ratio of 27.030 without the fix and 1.000 with it.

## The fix

Test `<= CLOSED` in `gpvcoeff()`, and add the same test to the "treat as a pipe" branch of `pbvcoeff()`. `valvecoeff()` already gives a closed link its near-zero conductance.

```diff
     // Treat as a pipe if valve closed
-    if (hyd->LinkStatus[k] == CLOSED) valvecoeff(pr, k);
+    if (hyd->LinkStatus[k] <= CLOSED) valvecoeff(pr, k);
...
-    // If valve fixed OPEN or CLOSED then treat as a pipe
-    if (hyd->LinkSetting[k] == MISSING || hyd->LinkSetting[k] == 0.0)
+    // If valve closed, or fixed OPEN or CLOSED, then treat as a pipe
+    if (hyd->LinkStatus[k] <= CLOSED ||
+        hyd->LinkSetting[k] == MISSING || hyd->LinkSetting[k] == 0.0)
```

`linkstatus()` reopens TEMPCLOSED links at every status check and `tankstatus()` closes them again only while the tank is still empty or full, so the valves resume normal operation once the tank level allows it.

EPANET's own unit tests (6 suites) pass with the patch.

## Notes

- The upstream branch `dev_2.3_hyd` (commit [24e77cf](https://github.com/OpenWaterAnalytics/EPANET/commit/24e77cf01cbcbd10845398544a1ef8394e242521), February 2020, never merged) adds `hyd->LinkStatus[k] == CLOSED` to `pbvcoeff()`. That catches a PBV closed by a control, but not TEMPCLOSED, so it would not fix this issue. The same branch keeps `== CLOSED` in `gpvcoeff()`.
- [SIM-21](../SIM-21-gpv-curve-index-three-copies/) also changes `gpvcoeff()`, to read the curve index from `Link.Kc`. The two patches change different lines and apply together.
