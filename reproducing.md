# Reproducing the issues

Every issue folder contains a test program (`<ID>_test.c`), the input files it needs (`<ID>_*.inp`) and a fix (`<ID>_fix.patch`). The tests and patches target EPANET at commit [`83e25bb`](https://github.com/OpenWaterAnalytics/EPANET/commit/83e25bbe2b0b460803e71df03bc989afe9557ea3), the head of `dev` on 27 August 2026.

Each test prints what it observed and ends with one line:

| Last line | Exit code | Meaning |
|---|---|---|
| `PASS: ...` | 0 | EPANET behaves correctly |
| `FAIL: ...` | 1 | The test found the bug |

Tests for crash bugs make the call that crashes. In a build with AddressSanitizer they stop there with a sanitizer report. With the fix applied, the call returns an error code and the test prints `PASS`.

## With the scripts (macOS or Linux)

You need a C compiler, CMake and git. EPANET at 83e25bb is the `epanet` submodule of this repository. Clone with `git clone --recurse-submodules`, or let the scripts fetch the submodule the first time they run. To use an EPANET clone you already have, set `EPANET_SRC` to its path. The scripts export commit 83e25bb from it, so the checkout itself is never changed.

```sh
tools/run-test.sh SIM-01             # build EPANET with sanitizers, then run SIM-01's test: FAIL
tools/run-test.sh SIM-01 --patched   # the same with SIM-01_fix.patch applied: PASS
tools/run-test.sh SIM-01 --all       # with every fix patch applied
tools/run-all.sh                     # every test, unpatched and patched; writes verification.md
```

| Script | What it does |
|---|---|
| [`tools/build-epanet.sh`](tools/build-epanet.sh) | Builds EPANET 83e25bb with AddressSanitizer and UndefinedBehaviorSanitizer into `.build/base`, or with one issue's patch into `.build/<ID>`. With `--unit-tests` it also runs EPANET's own Boost tests. Set `SANITIZE=0` for a plain build, in `.build/<name>-plain`. |
| [`tools/run-test.sh`](tools/run-test.sh) | Compiles one issue's test against a build, runs it in a scratch copy of the issue folder, and prints the verdict: `PASS`, `FAIL`, `CRASH` (sanitizer report or signal) or `HANG` (over 60 seconds) |
| [`tools/run-all.sh`](tools/run-all.sh) | Runs every test against the unpatched build, its own patch and all patches together, and records the results in [verification.md](verification.md) |
| [`tools/patch-workspace.sh`](tools/patch-workspace.sh) | Gives you an editable copy of the source with an issue's patch applied, and writes your changes back to the patch |
| [`tools/patch-order.sh`](tools/patch-order.sh) | Lists the patches to apply for an issue, prerequisites first (a few patches name another one in a `Requires:` line) |
| [`tools/get-epanet.sh`](tools/get-epanet.sh) | Finds the EPANET source the other scripts use: `EPANET_SRC` if it is set, otherwise the `epanet` submodule |

Everything the scripts build or write goes into `.build/`, which git ignores.

## By hand

From an issue folder, against any EPANET build:

```sh
cc SIM-01_test.c -I<epanet>/include -L<epanet>/build/lib -lepanet2 -o SIM-01_test
./SIM-01_test
```

Tests that read the binary output file also need the output library:

```sh
cc FILE-09_test.c -I<epanet>/include -I<epanet>/src/outfile/include -I<epanet>/build/src/outfile \
   -L<epanet>/build/lib -lepanet2 -lepanet-output -o FILE-09_test
```

On Windows, compile the test with MSVC against `epanet2.lib` and copy `epanet2.dll` next to the program. A few tests use POSIX calls (for example `setenv`, `alarm` or `setrlimit`), which their READMEs point out.

To apply a fix, run this from the root of an EPANET checkout at 83e25bb, such as the `epanet` submodule:

```sh
git apply "<path>/SIM-01_fix.patch"
```

The text above the first `diff --git` line in each patch says what the patch changes and why. `git apply` ignores that text.

## Sanitizers

Most crash bugs are memory errors: reads or writes past the end of an array, or use of freed memory. Without AddressSanitizer they often do no visible harm, or crash somewhere unrelated later. To see them reliably, build EPANET as `tools/build-epanet.sh` does:

```sh
cmake -S <epanet> -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
```

Compile the test with `-fsanitize=address,undefined` too, and run it with `ASAN_OPTIONS=detect_leaks=0`. Upstream pull request [#916](https://github.com/OpenWaterAnalytics/EPANET/pull/916) adds a build option to run EPANET's own tests under UndefinedBehaviorSanitizer. It would have caught the division by zero in [SIM-12](1-wrong-results/SIM-12-hydraulic-step-under-10s/) and the float-to-integer conversions in [CRASH-04](2-crashes/CRASH-04-inf-nan-time-values/).
