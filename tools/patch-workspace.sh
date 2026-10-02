#!/bin/sh
# Edit an issue's fix in a clean copy of the EPANET source.
#
#   tools/patch-workspace.sh SIM-01        create .build/edit/SIM-01 (a git repo at
#                                          83e25bb, with SIM-01_fix.patch applied
#                                          if it exists) and print its path
#   tools/patch-workspace.sh SIM-01 save   write the workspace's diff to
#                                          SIM-01_fix.patch, keeping the patch's
#                                          description header
#
# If the fix has to sit on top of another issue's fix, put "Requires: <ID>" in
# the patch header before creating the workspace (delete .build/edit/<ID> to
# start again). The prerequisite is committed in the workspace, so the saved
# patch contains only this issue's change.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
SRC=$("$HERE/tools/get-epanet.sh")
COMMIT=83e25bbe2b0b460803e71df03bc989afe9557ea3
ID=${1:?usage: patch-workspace.sh ID [save]}
WS=$HERE/.build/edit/$ID

ISSUE=$(ls -d "$HERE"/*/"$ID"-*/ 2>/dev/null | head -1)
if [ -z "$ISSUE" ]; then
    echo "No folder found for $ID" >&2
    exit 2
fi
PATCH=${ISSUE}${ID}_fix.patch

if [ "${2:-}" = "save" ]; then
    HEADER=$(mktemp)
    if [ -f "$PATCH" ]; then
        # Everything before the first "diff --git" line is the description
        awk '/^diff --git /{exit} {print}' "$PATCH" > "$HEADER"
    fi
    {
        cat "$HEADER"
        # Against HEAD, so staged changes (e.g. from "git apply -3") are included
        git -C "$WS" diff HEAD --no-color --no-ext-diff
    } > "$PATCH.tmp"
    mv "$PATCH.tmp" "$PATCH"
    rm -f "$HEADER"
    echo "$PATCH"
    exit 0
fi

if [ ! -d "$WS/.git" ]; then
    mkdir -p "$WS"
    git -C "$SRC" archive "$COMMIT" | tar -x -C "$WS"
    (
        cd "$WS"
        git init -q
        git add -A
        git -c user.name=review -c user.email=review@localhost commit -q -m "EPANET 83e25bb"

        # Prerequisite patches (a "Requires:" line in the header) are committed,
        # so the saved diff holds only this issue's change
        if [ -f "$PATCH" ]; then
            "$HERE/tools/patch-order.sh" "$ID" | sed '$d' | while IFS= read -r p; do
                git apply --whitespace=nowarn "$p"
            done
            if [ -n "$(git status --porcelain)" ]; then
                git add -A
                git -c user.name=review -c user.email=review@localhost commit -q -m "prerequisites"
            fi
            # The patch may still be just a description header
            if grep -q "^diff --git " "$PATCH"; then
                git apply --whitespace=nowarn "$PATCH"
            fi
        fi
    )
fi
echo "$WS"
