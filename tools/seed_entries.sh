#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Gives a fresh tree the code another tree found (its work/<id>/entries.txt and entries.txt.z80), added to what
# this tree has, so that the next 'make' translates from those without a make explore. The places are in the ROM
# set's program, which no change here moves; a later explore only adds to them. (Never committed: an entries
# file is made from the game, tools/audit.sh.)
#
#   tools/seed_entries.sh OTHER_TREE [game ...]   (every game the other tree has found code for, if none named)
set -eu
from=${1:?usage: seed_entries.sh OTHER_TREE [game ...]}
shift
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
[ $# -gt 0 ] || set -- $(cd "$from/work" 2>/dev/null && for f in */entries.txt; do [ -f "$f" ] && printf '%s ' "${f%/entries.txt}"; done)
merge() { # this tree's entries file, the other's (as tools/explore.sh merges)
    [ -f "$2" ] || return 0
    before=$(grep -vc '^#' "$1" 2>/dev/null || echo 0)
    cat "$1" "$2" 2>/dev/null | grep -v '^#' | sort -u > "$1.new"
    { echo "# Places the game's code was seen to reach in ways the translator cannot follow"; cat "$1.new"; } > "$1"
    rm -f "$1.new"
    echo "$1: $before -> $(grep -vc '^#' "$1")"
}
for game in "$@"; do
    mkdir -p "work/$game"
    merge "work/$game/entries.txt" "$from/work/$game/entries.txt"
    merge "work/$game/entries.txt.z80" "$from/work/$game/entries.txt.z80"
done
