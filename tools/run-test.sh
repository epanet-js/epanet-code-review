#!/bin/sh
# Build and run one issue's test program.
#
#   tools/run-test.sh SIM-01             against unpatched EPANET (shows the bug)
#   tools/run-test.sh SIM-01 --patched   against EPANET with SIM-01_fix.patch
#   tools/run-test.sh SIM-01 --all       against EPANET with every fix patch
#
# The test runs in a scratch copy of the issue folder, so files it writes do
# not end up in the repository. The last line printed is the verdict:
#   PASS   the behaviour is correct (exit 0)
#   FAIL   the test detected the bug (exit 1)
#   CRASH  a sanitizer report, signal or unexpected exit code
#   HANG   still running after $TEST_TIMEOUT seconds (default 60)
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
ID=${1:?usage: run-test.sh ID [--patched|--all]}
MODE=${2:-}

case "$MODE" in
    "")        LIB=base;  LABEL=base ;;
    --patched) LIB=$ID;   LABEL=patched ;;
    --all)     LIB=all;   LABEL=all-patches ;;
    *) echo "unknown option $MODE" >&2; exit 2 ;;
esac

ISSUE=$(ls -d "$HERE"/*/"$ID"-*/ 2>/dev/null | head -1)
if [ -z "$ISSUE" ]; then
    echo "No folder found for $ID" >&2
    exit 2
fi

"$HERE/tools/build-epanet.sh" "$LIB"
B=$HERE/.build/$LIB
[ "${SANITIZE:-1}" = "1" ] || B=$B-plain

# Fresh scratch copy of the issue folder
[ "${SANITIZE:-1}" = "1" ] || LABEL=$LABEL-plain
RUN=$HERE/.build/run/$ID-$LABEL
rm -rf "$RUN"
mkdir -p "$RUN"
cp -R "$ISSUE". "$RUN/"
cd "$RUN"

if [ "${SANITIZE:-1}" = "1" ]; then
    SAN="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
else
    SAN=""
fi

cc $SAN -g -std=c99 -Wall -Wno-unused-variable \
    -I"$B/src/include" -I"$B/src/src/outfile/include" -I"$B/build/src/outfile" \
    "${ID}_test.c" -o test \
    -L"$B/build/lib" -lepanet2 -lepanet-output -lm -Wl,-rpath,"$B/build/lib" \
    > compile.log 2>&1 || { cat compile.log; echo "$ID [$LABEL]: COMPILE ERROR"; exit 3; }

# Run with a time limit; perl's alarm is the most portable timeout on macOS and Linux
set +e
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
    perl -e 'alarm shift; exec @ARGV' "${TEST_TIMEOUT:-60}" ./test > output.txt 2>&1
CODE=$?
set -e
cat output.txt

if [ $CODE -eq 0 ] && grep -q "^PASS" output.txt; then
    VERDICT=PASS
elif [ $CODE -eq 1 ] && grep -q "^FAIL" output.txt; then
    VERDICT=FAIL
elif [ $CODE -eq 142 ] || [ $CODE -eq 14 ]; then
    VERDICT=HANG
else
    VERDICT="CRASH"
fi
echo "$ID [$LABEL]: $VERDICT (exit code $CODE)"
