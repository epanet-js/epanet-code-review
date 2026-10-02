#!/bin/sh
# Print the fix patches needed for the given issues, one path per line,
# with any prerequisites first.
#
#   tools/patch-order.sh SIM-01 [ID ...]
#   tools/patch-order.sh all              every patch, in folder order
#
# A patch that has to be applied on top of another one names it in its
# description header, before the first "diff --git" line:
#   Requires: CRASH-18
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)

find_patch() {
    ls "$HERE"/*/"$1"-*/"$1"_fix.patch 2>/dev/null | head -1
}

requires() {
    awk '/^diff --git /{exit}
         /^Requires:/ { for (i = 2; i <= NF; i++) { gsub(",", "", $i); print $i } }' "$1"
}

SEEN=""
visit() {
    case " $SEEN " in
        *" $1 "*) return ;;
    esac
    SEEN="$SEEN $1"
    # sh has no local variables and visit() recurses, so look the path up
    # again after the prerequisites instead of keeping it in a variable
    if [ -z "$(find_patch "$1")" ]; then
        echo "No patch found for $1" >&2
        exit 2
    fi
    for r in $(requires "$(find_patch "$1")"); do
        visit "$r"
    done
    find_patch "$1"
}

if [ "${1:-}" = "all" ]; then
    set -- $(ls "$HERE"/*/*/*_fix.patch | sed 's|.*/\([A-Z]*-[0-9]*\)_fix\.patch$|\1|')
fi
for id in "$@"; do
    visit "$id"
done
