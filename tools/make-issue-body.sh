#!/bin/sh
# Print a Markdown file with its relative links made absolute, for pasting
# into a GitHub issue (relative links do not work in issue text).
#
#   tools/make-issue-body.sh <base-url> [file]
#
# <base-url> is the URL of this repository's files on GitHub, for example
#   https://github.com/<owner>/epanet-code-review/blob/main
# [file] defaults to README.md. Links to other files are resolved relative
# to the file's own folder.
set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
BASE=${1:?usage: make-issue-body.sh <base-url> [file]}
FILE=${2:-README.md}

# Folder of the file, relative to the repository root, for resolving its links
DIR=$(dirname "$FILE")
[ "$DIR" = "." ] && PREFIX="$BASE" || PREFIX="$BASE/$DIR"

perl -pe '
    BEGIN { $p = shift @ARGV }
    s{\]\((?!https?://|#|mailto:)([^)\s]+)\)}{]($p/$1)}g;
' "$PREFIX" "$HERE/$FILE"
