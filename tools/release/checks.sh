#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# A release's checks (docs/RELEASING.md, step 1): for every game (games/*, or GAMES="a b"), with its ROM set in
# rom/<id>/: its build, check, check-blocks, check-release over its ladder movie if it has one (40,000 frames;
# Puzzle Fighter's 66,000), check-rewrites and check-widescreen if it has them, check-rollback and check-online (what
# online play needs of every game: docs/TOOLKIT.md, Online play); then the repository's make check.
# A game's steps one after another (two builds of one game must never run together), JOBS games at once (3: a
# check-rewrites runs several of its own at once too; each game has its build/<id>/ and work/<id>/). Each step's
# output is in build/release-checks/; a line for each step as it ends, and at the end the ones that failed. About
# 40 minutes on a ten-core Mac (one game at a time, 73).
#   tools/release/checks.sh            JOBS=1 tools/release/checks.sh: one game at a time
#
# Every game's code must have been found first (work/<id>/entries.txt: make GAME=<id> explore, or
# tools/seed_entries.sh from a tree where it was): a tree without it translates from guesses, and check and
# check-blocks fail for that alone. The checks stop at once if a game has none.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
LOGS=build/release-checks
mkdir -p "$LOGS"

step() { # game, step's name, make's arguments
    game=$1 name=$2
    shift 2
    if make GAME="$game" "$@" > "$LOGS/$game.$name.log" 2>&1; then
        echo "$game $name: passed"
    else
        echo "$game $name: FAILED ($LOGS/$game.$name.log)"
        failed=1
    fi
}

# One game's steps (the main part below runs this for each game, JOBS at once): status 1 if one failed
if [ "${1:-}" = --game ]; then
    game=$2 failed=0
    if [ ! -d "rom/$game" ]; then
        echo "$game rom: FAILED (no ROM set in rom/$game)"
        exit 1
    fi
    step "$game" build
    step "$game" check check
    step "$game" blocks check-blocks
    if [ -f "games/$game/movies/ladder.txt" ]; then
        frames=40000
        [ "$game" = puzzle_fighter ] && frames=66000
        step "$game" release check-release FRAMES=$frames
    fi
    [ -f "games/$game/check_rewrites.sh" ] && step "$game" rewrites check-rewrites
    [ -f "games/$game/check_widescreen.sh" ] && step "$game" widescreen check-widescreen
    step "$game" rollback check-rollback FRAMES=6000
    step "$game" online check-online FRAMES=3000
    exit $failed
fi

GAMES="${GAMES:-$(cd games && for cfg in */game.cfg; do printf '%s ' "${cfg%/game.cfg}"; done)}"
unexplored=""
for game in $GAMES; do
    [ -d "rom/$game" ] && [ ! -s "work/$game/entries.txt" ] && unexplored="$unexplored $game"
done
if [ -n "$unexplored" ]; then
    echo "error: no code found yet for:$unexplored (work/<id>/entries.txt). For each:" >&2
    echo "  make GAME=<id> explore, or tools/seed_entries.sh <a tree where it was explored>" >&2
    exit 1
fi
# (each step's line as it ends, kept for the summary)
printf '%s\n' $GAMES | xargs -P "${JOBS:-3}" -n 1 "$0" --game | tee "$LOGS/steps.txt"
failed=$(sed -n 's/^\([^ ]*\) \([^:]*\): FAILED.*/\1:\2/p' "$LOGS/steps.txt" | tr '\n' ' ')
if make check > "$LOGS/repository.log" 2>&1; then echo "make check: passed"; else echo "make check: FAILED"; failed="$failed check"; fi
if [ -n "$failed" ]; then
    echo "failed: $failed"
    exit 1
fi
echo "every check passed"
