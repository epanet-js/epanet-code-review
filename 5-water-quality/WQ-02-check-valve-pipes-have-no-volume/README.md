# WQ-02: Check-valve pipes have no volume and no reactions in water quality

| | |
|---|---|
| **Category** | [Water quality](../README.md) |
| **Impact** | Wrong results with no warning. Water crosses a pipe with a check valve instantly and unchanged: no travel time, no water age and no decay or growth, however long the pipe. |
| **Reached from** | Any water quality run (chemical, age or trace) with a pipe whose status is `CV`, from an input file or the toolkit |
| **Where** | `initsegs()` in [`src/qualroute.c:586`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualroute.c#L586) and `reactpipes()` in [`src/qualreact.c:137`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/qualreact.c#L137) |
| **Since** | v2.2: the water quality rewrite in PR [#297](https://github.com/OpenWaterAnalytics/EPANET/pull/297) (commit [`7c021cf`](https://github.com/OpenWaterAnalytics/EPANET/commit/7c021cf533c247b66eb1b5c562856e88e8dc7a4b), 2018). EPANET 2.1 gave every link a volume segment. |
| **GitHub** | No related issue |
| **Fix** | Two lines: [`WQ-02_fix.patch`](WQ-02_fix.patch) |

## The problem

A pipe with a check valve is still a pipe: it has a length, a diameter and a volume, and the water in it ages and reacts. EPANET's hydraulics treat it that way. Its water quality solver does not: it gives the pipe no volume, so water arrives at the far end at the moment it enters, with the quality it had at the near end.

Pipes on the discharge side of pumps are often modelled as check-valve pipes, so many real models have long pipes affected by this.

In the test network, R1 feeds two identical 10,000 ft, 6 in pipes, each supplying 100 gpm. The water takes about 2.4 h to cross either pipe, and loses about 10 % of its chlorine on the way. P1 is an ordinary pipe; P2 has a check valve that never closes:

- At J1, at the end of the open pipe, chlorine is 0 for the first two hours (the water is still in the pipe), then 0.90 mg/L.
- At J2, at the end of the check-valve pipe, chlorine is 1.00 mg/L from the first hour on.

Water age shows the same thing: the check-valve pipe adds no age.

## Why it happens

The link type for a pipe with a check valve is `CVPIPE`, which is 0; an ordinary pipe is `PIPE`, which is 1. The two quality functions test for `PIPE` only:

```c
// src/qualroute.c, initsegs()
    // Add one segment with assigned downstream node quality to each pipe
    for (k = 1; k <= net->Nlinks; k++)
    {
        qual->FirstSeg[k] = NULL;
        qual->LastSeg[k] = NULL;
        if (net->Link[k].Type == PIPE)
        {
            v = LINKVOL(k);
            ...

// src/qualreact.c, reactpipes()
        // Skip non-pipe links (pumps & valves)
        if (net->Link[k].Type != PIPE) continue;
```

A link with no initial segment has no volume, so it behaves like a pump or valve: `evalnodeoutflow()` adds a segment for the flow entering it, and `evalnodeinflow()` passes that flow on to the downstream node in the same step. `setreactflag()` in the same file uses `Type <= PIPE`, which is how the rest of the code base writes "any pipe".

The test arrived with the rewrite of the quality solver in PR [#297](https://github.com/OpenWaterAnalytics/EPANET/issues/297) as `Type == EN_PIPE`. Commit [`7443cea`](https://github.com/OpenWaterAnalytics/EPANET/commit/7443cea9d4fb3f42b999e85dac35d15bd58efdf1) later renamed `EN_PIPE` to `PIPE`. Both commits are in v2.2.

## How to reproduce

| File | What it is |
|---|---|
| [`WQ-02_twin-pipes.inp`](WQ-02_twin-pipes.inp) | R1 (chlorine 1.0 mg/L, bulk decay -1/day) feeds J0, which supplies J1 through the open pipe P1 and J2 through the check-valve pipe P2. P1 and P2 are identical. |
| [`WQ-02_test.c`](WQ-02_test.c) | Runs the network and compares the chlorine at J1 and J2 every hour |
| [`WQ-02_fix.patch`](WQ-02_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh WQ-02            # unpatched EPANET: FAIL
tools/run-test.sh WQ-02 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc WQ-02_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -lm -o WQ-02_test
./WQ-02_test
```

The report from `runepanet WQ-02_twin-pipes.inp WQ-02.rpt` lists J1 and J2 every hour and shows the same difference.

**Without the fix**, the check-valve pipe delivers the reservoir's water at once:

```
P2 link type: 0 (EN_CVPIPE = 0)

Hour   J1, open pipe   J2, check-valve pipe   (mg/L)
   0          0.0000                 0.0000   
   1          0.0000                 0.9998   <-- differs
   2          0.0000                 0.9998   <-- differs
   3          0.9043                 0.9998   <-- differs
   4          0.9039                 0.9998   <-- differs
   5          0.8976                 0.9998   <-- differs
   6          0.9070                 0.9998   <-- differs
FAIL: the check-valve pipe gives different quality in 6 of 7 hours
```

The 0.0002 lost at J2 is the decay in the 10 ft pipe P0 from R1 to J0.

**With the fix**, the two pipes give the same result:

```
Hour   J1, open pipe   J2, check-valve pipe   (mg/L)
   0          0.0000                 0.0000   
   1          0.0000                 0.0000   
   2          0.0000                 0.0000   
   3          0.9043                 0.9043   
   4          0.9039                 0.9039   
   5          0.8976                 0.8976   
   6          0.9070                 0.9070   
PASS: the check-valve pipe gives the same quality as the open pipe
```

## The fix

Treat `CVPIPE` as a pipe in both places:

```diff
 // reactpipes()
-        if (net->Link[k].Type != PIPE) continue;
+        if (net->Link[k].Type > PIPE) continue;

 // initsegs()
-        if (net->Link[k].Type == PIPE)
+        if (net->Link[k].Type <= PIPE)
```

This patch was first written as `fix_cvpipe_quality.diff` in the performance work of this review; it is unchanged here.

EPANET's own unit tests (6 suites) pass with the patch. Results change for every model with check-valve pipes, so regression results for such models will need to be regenerated.
