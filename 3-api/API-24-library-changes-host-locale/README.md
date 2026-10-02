# API-24: The library changes the host program's numeric locale

| | |
|---|---|
| **Category** | [Toolkit API behaviour](../README.md) |
| **Impact** | After the first `EN_createproject`, a host program running under a German, French or similar locale formats and parses its own numbers with a decimal comma: `printf("%.2f", 1.25)` gives `1,25`, and `strtod("2.5")` gives 2. The switch is process-wide, so it also affects other threads, and `EN_saveinpfile` switches the locale again while other threads run. |
| **Reached from** | Toolkit: `EN_createproject` (and the legacy API's first `ENopen`), and `EN_saveinpfile`, whenever the environment's locale uses a decimal comma |
| **Where** | `EN_createproject` in [`src/epanet.c:55`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet.c#L55), `createtmpfiles` in [`src/epanet2.c:39`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/epanet2.c#L39), `saveinpfile` in [`src/inpfile.c:138`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L138) and [`890`](https://github.com/OpenWaterAnalytics/EPANET/blob/83e25bbe2b0b460803e71df03bc989afe9557ea3/src/inpfile.c#L890) |
| **Since** | Pull request [#919](https://github.com/OpenWaterAnalytics/EPANET/pull/919) (May 2026, commit [25d7229](https://github.com/OpenWaterAnalytics/EPANET/commit/25d72291b4f000430a372cde26f862f5b48cdd86)). Not yet in a release. |
| **GitHub** | [#919](https://github.com/OpenWaterAnalytics/EPANET/pull/919) was the fix for [#875](https://github.com/OpenWaterAnalytics/EPANET/issues/875) and [#910](https://github.com/OpenWaterAnalytics/EPANET/issues/910) (input files misread under a decimal-comma locale) |
| **Fix** | Two helper functions in `project.c` and calls around the report and the save: [`API-24_fix.patch`](API-24_fix.patch). Apply [FILE-02](../../4-files/FILE-02-save-over-input-loses-labels/)'s patch first. |

## The problem

Pull request [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919) fixed reading and writing input files under locales that use a decimal comma. It also set out that the report should use the system's decimal separator ("Numerical output written to EPANET's Report file will use the system locale's setting"). To get that, it made `EN_createproject` call `setlocale(LC_NUMERIC, "")`.

`setlocale` changes the locale of the whole process, not only EPANET's. A C program starts in the "C" locale, with a decimal point. Once it creates an EPANET project under a German or French environment, its own `printf` writes `1,25` and `strtod`, `atof` and `sscanf` stop at the `.` in `2.5`. This affects any host that formats or reads numbers with the C library: C and C++ programs, and C extensions in Python or R. Issue [#875](https://github.com/OpenWaterAnalytics/EPANET/issues/875) came from a developer embedding the toolkit in applications that set their own locale, and those are the hosts whose choice is overwritten.

`saveinpfile` has the same problem in a second form. It sets the process to "C" for the save, then to the environment's locale (not back to what the host had). Other threads see both switches. [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919) added a note to `EN_saveinpfile` that it is not thread safe.

Because the process locale now has a decimal comma, `getfloat()` also takes its slow path, which builds a new locale object for every number (performance item X10). On a synthetic 62,500-junction grid, `EN_open` takes 163 to 164 ms under `de_DE.UTF-8` against 98 ms under "C" (plain Release build, Apple arm64, best of 7).

## Why it happens

```c
// src/epanet.c, EN_createproject() (the same in createtmpfiles() in epanet2.c)
// Use system's decimal point (will be changed locally to dot
// when reading/writing EPANET input files)
setlocale(LC_NUMERIC, "");

// src/inpfile.c, saveinpfile()
// Set global decimal point character to dot (not thread safe)
setlocale(LC_NUMERIC, "C");
...
// Reset global decimal point character to system value
setlocale(LC_NUMERIC, "");
```

The comment says the locale "will be changed locally", but `setlocale` has no local form: it sets the locale for every thread in the process.

## How to reproduce

| File | What it is |
|---|---|
| [`API-24_network.inp`](API-24_network.inp) | Reservoir R1 feeds junction J1. J1's elevation is 12.5, with a dot. |
| [`API-24_test.c`](API-24_test.c) | Sets `LC_ALL=de_DE.UTF-8` (or `fr_FR.UTF-8`) in its own environment, then creates a project, opens, solves, reports and saves. After each step it prints its own `LC_NUMERIC` and `printf("%.2f", 1.25)`. It also checks the numbers EPANET read and wrote. |
| [`API-24_fix.patch`](API-24_fix.patch) | The fix |

With the tools in this folder (see [Reproducing the issues](../../reproducing.md)):

```sh
tools/run-test.sh API-24            # unpatched EPANET: FAIL
tools/run-test.sh API-24 --patched  # with the fix: PASS
```

Or by hand against your own EPANET build, from this folder:

```sh
cc API-24_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o API-24_test
./API-24_test
```

The test uses `setenv`, so it runs on POSIX systems only, and it needs a `de_DE` or `fr_FR` locale. On Linux, `locale -a` lists them; Debian and Ubuntu add one with `sudo locale-gen de_DE.UTF-8`.

**Without the fix**, the host's numbers switch to a decimal comma as soon as the project is created. Even the test's own report of J1's elevation comes out as `12,5`:

```
Environment: LC_ALL=de_DE.UTF-8
  Host program                 LC_NUMERIC     printf("%.2f", 1.25)
  before EN_createproject      C              1.25
  after EN_createproject       de_DE.UTF-8    1,25   <-- changed
  after EN_open                de_DE.UTF-8    1,25   <-- changed
  after EN_solveH, EN_report   de_DE.UTF-8    1,25   <-- changed
  after EN_saveinpfile         de_DE.UTF-8    1,25   <-- changed
EPANET read J1's elevation 12.5 as 12,5 (EN_open returned 0)
Report uses the decimal comma (Hydraulic Accuracy 0,001000): yes
Saved input file uses the decimal point (J1 elevation 12.5): yes
FAIL: EPANET changed the host program's numeric locale
```

**With the fix**, the host keeps "C", while EPANET's report still uses the decimal comma and the saved file the decimal point:

```
Environment: LC_ALL=de_DE.UTF-8
  Host program                 LC_NUMERIC     printf("%.2f", 1.25)
  before EN_createproject      C              1.25
  after EN_createproject       C              1.25
  after EN_open                C              1.25
  after EN_solveH, EN_report   C              1.25
  after EN_saveinpfile         C              1.25
EPANET read J1's elevation 12.5 as 12.5 (EN_open returned 0)
Report uses the decimal comma (Hydraulic Accuracy 0,001000): yes
Saved input file uses the decimal point (J1 elevation 12.5): yes
PASS: the host's locale is unchanged and EPANET's files are correct
```

## The fix

The patch keeps what [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919) set out to do, but changes the locale of the calling thread only, and only while EPANET is writing. Two new functions in `project.c` do this:

- `setnumericlocale(name)` installs a locale for the calling thread whose numeric part is `name` ("C" for the input file, "" for the system's locale in the report) and returns the thread's previous locale.
- `restorenumericlocale(saved)` puts the previous locale back.

On POSIX they use `newlocale` on a copy of the thread's current locale, and `uselocale`, which affects only the calling thread:

```c
// Copy the thread's current locale with its numeric part replaced
locale_t base = duplocale(uselocale((locale_t)0));
if (base != (locale_t)0)
{
    saved->loc = newlocale(LC_NUMERIC_MASK, name, base);
    if (saved->loc != (locale_t)0)
    {
        saved->prev = uselocale(saved->loc);
        return saved;
    }
    freelocale(base);
}
```

On Windows, where MSVC has no `uselocale`, they call `_configthreadlocale(_ENABLE_PER_THREAD_LOCALE)` so that `setlocale` affects only the calling thread, and afterwards restore both the thread's locale name and its previous per-thread setting. **The Windows code has not been compiled or run.** It needs a build and a test on MSVC before it is merged.

The calls are placed as follows:

| Where | Locale | Why |
|---|---|---|
| `EN_createproject`, `createtmpfiles` | none | The `setlocale` calls are removed |
| `saveinpfile` | "C" | Replaces the two `setlocale` calls |
| `openproject`, around `writesummary` | system | The input summary written by `EN_open` |
| `EN_runH`, `EN_nextH` | system | Status report lines, warnings and the flow balance |
| `writemassbalance()` in `report.c` | system | The mass balance at the end of a quality run, the only report output of `EN_nextQ` and `EN_stepQ` |
| `EN_report` | system | The results tables and energy report |

`EN_solveH`, `EN_solveQ`, `EN_runproject` and the legacy functions go through these. The mass balance is switched inside `writemassbalance()` rather than in `EN_nextQ` and `EN_stepQ`, which keeps this patch clear of [CRASH-12](../../2-crashes/CRASH-12-inith-initq-shared-save-flag/), which edits those two functions. The patch also removes the "not thread safe" note that [#919](https://github.com/OpenWaterAnalytics/EPANET/issues/919) added to `EN_saveinpfile` in `epanet2_2.h`.

Checks beyond the test, all under `LC_ALL=de_DE.UTF-8`:

- A Net1 run with a full status report and a chlorine mass balance (310 report lines, 140 of them with decimal commas) gives a report identical to the unpatched library's, apart from the dates. So does `runepanet` on the same file, and the binary output files are identical.
- `EN_open` on the 62,500-junction grid takes 99 ms instead of 163 to 164 ms, because `getfloat()` now sees the host's "C" locale and uses plain `strtod`.
- Each switch costs about 0.4 µs on macOS (two per `EN_runH`/`EN_nextH` pair).

EPANET's own unit tests (6 suites) pass with the patch.

**A simpler alternative.** Leave the report in whatever locale the host has chosen, and switch to "C" only in `saveinpfile`. The command-line runner would then call `setlocale(LC_NUMERIC, "")` in its own `main()`, as a host program may. That respects a host that chose its locale deliberately, and needs no switching around the report. The cost is that a host that never calls `setlocale` (most C programs, and Python) gets a report with a decimal point, as in v2.3.5 and earlier.

## Notes

- A host that sets a decimal-comma locale itself still sends `getfloat()` down its slow path. Creating the "C" locale once per process, as `2-performance/load-build/patches/loc.diff` does, removes that cost.
- A report callback set with `EN_setreportcallback` is called while the thread uses the system's locale. A callback that formats numbers of its own will see a decimal comma.
- [FILE-10](../../4-files/FILE-10-nan-inf-accepted-as-numbers/) changes the end of `getfloat()` but keeps its locale handling. The two patches apply together.
- [FILE-02](../../4-files/FILE-02-save-over-input-loses-labels/) also changes the start of `saveinpfile()` (its declarations and the `fopen` call next to the first `setlocale`), so this patch is made on top of FILE-02's: apply `FILE-02_fix.patch` first. `tools/run-test.sh API-24 --patched` does this for you.
