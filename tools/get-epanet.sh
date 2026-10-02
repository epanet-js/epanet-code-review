#!/bin/sh
# Print the path of a git repository that holds the reviewed EPANET commit.
#
#   tools/get-epanet.sh
#
# Uses the EPANET clone in $EPANET_SRC if it is set, otherwise the epanet
# submodule, which is fetched the first time if the repository was cloned
# without --recurse-submodules.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
COMMIT=83e25bbe2b0b460803e71df03bc989afe9557ea3

has_commit() {
    git -C "$1" cat-file -e "$COMMIT^{commit}" 2>/dev/null
}

if [ -n "${EPANET_SRC:-}" ]; then
    SRC=$EPANET_SRC
else
    SRC=$HERE/epanet
    if ! has_commit "$SRC"; then
        echo "Fetching the epanet submodule ..." >&2
        git -C "$HERE" submodule update --init epanet >&2 || true
    fi
fi

if ! has_commit "$SRC"; then
    echo "EPANET commit $COMMIT was not found in $SRC." >&2
    echo "Run 'git submodule update --init', or set EPANET_SRC to an EPANET clone that has it." >&2
    exit 2
fi
echo "$SRC"
