#!/bin/sh
# Build the EPANET library at the reviewed commit, with sanitizers.
#
#   tools/build-epanet.sh              unpatched library        -> .build/base
#   tools/build-epanet.sh SIM-01       with SIM-01_fix.patch    -> .build/SIM-01
#   tools/build-epanet.sh all          with every fix patch     -> .build/all
#   tools/build-epanet.sh SIM-01 --unit-tests
#                                      also build and run EPANET's own Boost tests
#
# The source is exported at commit 83e25bb from the epanet submodule, or from
# the EPANET clone in $EPANET_SRC (see tools/get-epanet.sh). The checkout
# itself is never modified. Set SANITIZE=0 for a plain build.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)

# Paths may contain spaces, so lists of files are split on newlines only
NL='
'
IFS=$NL
SRC=$("$HERE/tools/get-epanet.sh")
COMMIT=83e25bbe2b0b460803e71df03bc989afe9557ea3
NAME=${1:-base}
UNIT_TESTS=${2:-}
OUT=$HERE/.build/$NAME
[ "${SANITIZE:-1}" = "1" ] || OUT=$OUT-plain

# Find the patch(es) to apply, prerequisites first
PATCHES=""
if [ "$NAME" != "base" ]; then
    PATCHES=$("$HERE/tools/patch-order.sh" "$NAME")
fi

# Skip the build if nothing has changed since the last one
# (a build that already ran the unit tests also serves plain requests)
STAMP=$( (echo "$COMMIT ${SANITIZE:-1}"; for p in $PATCHES; do cat "$p"; done) | cksum)
if [ -f "$OUT/stamp" ] && [ "$(cat "$OUT/stamp")" = "$STAMP" ]; then
    if [ "$UNIT_TESTS" != "--unit-tests" ] || [ -f "$OUT/unit-tests.ok" ]; then
        exit 0
    fi
fi

echo "Building EPANET ($NAME) in $OUT ..." >&2
rm -rf "$OUT"
mkdir -p "$OUT/src"
git -C "$SRC" archive "$COMMIT" | tar -x -C "$OUT/src"

for p in $PATCHES; do
    # The ceiling stops git from finding the enclosing repository, which would
    # make "git apply" silently skip every file
    if ! (cd "$OUT/src" && GIT_CEILING_DIRECTORIES="$OUT" git apply --whitespace=nowarn "$p"); then
        echo "Patch does not apply: $p" >&2
        exit 3
    fi
done

if [ "${SANITIZE:-1}" = "1" ]; then
    SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
else
    SAN=""
fi

TESTS=OFF
[ "$UNIT_TESTS" = "--unit-tests" ] && TESTS=ON

cmake -S "$OUT/src" -B "$OUT/build" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=$TESTS \
    -DCMAKE_C_FLAGS="$SAN -g -O1" -DCMAKE_CXX_FLAGS="$SAN -g -O1" \
    -DCMAKE_SHARED_LINKER_FLAGS="$SAN" -DCMAKE_EXE_LINKER_FLAGS="$SAN" \
    > "$OUT/cmake.log" 2>&1 || { cat "$OUT/cmake.log" >&2; exit 4; }

cmake --build "$OUT/build" -j 8 > "$OUT/build.log" 2>&1 || {
    grep -E "error|Error" "$OUT/build.log" | head -20 >&2
    exit 5
}

if [ "$TESTS" = "ON" ]; then
    (cd "$OUT/build" && ASAN_OPTIONS=detect_leaks=0 ctest --output-on-failure -j 8) \
        > "$OUT/unit-tests.log" 2>&1 || {
        tail -30 "$OUT/unit-tests.log" >&2
        echo "Unit tests FAILED for $NAME (see $OUT/unit-tests.log)" >&2
        exit 6
    }
    grep "tests passed" "$OUT/unit-tests.log" >&2
    touch "$OUT/unit-tests.ok"
fi

echo "$STAMP" > "$OUT/stamp"
