#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# What online play's rollback needs of a game (issue #70): that a frame put back and run again comes out exactly as
# the first time. Made-up play for as many players as the game has (game.cfg's players; two unless said) is run straight, and again with
# --rollback-test (port/port.h, RollbackTest: player 2's controls late and guessed, and the frames guessed wrong
# run again); the per-frame state hashes must be the same. Three ways:
#   1. with the game's own features;
#   2. with every feature that changes play switched on (the cheats, free play, widescreen, the arcade timing...);
#   3. as 2, but the frames run again not drawn (--hash-no-picture: all but the picture compared), as online play
#      runs them.
# A difference is something the game keeps outside its save state, or that a frame run twice does twice.
#
#   GAME=<id> check_rollback.sh [FRAMES]   (make GAME=<id> check-rollback: from the repository's top, after `make`; ROM=your set)
set -eu
GAME=${GAME:?which game: GAME=<id>}
ROM=${ROM:-rom/$GAME}
frames=${1:-12000}
work=work/$GAME/check
mkdir -p "$work"
: > "$work/rollback-settings.ini"
players=$(python3 tools/gamecfg.py players "$GAME")
players=${players:-2}
# (the on/off features of the Game, Cheats and Display pages, but for those that only show the game another way)
play=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '($2=="Game"||$2=="Cheats"||$2=="Display") && NF==5 &&
    $1 !~ /^(display_|window_size|run_ahead|rapid_fire|custom_soundtrack|widescreen_walls)/ {print $1}' | paste -sd, -)
play="${play:+$play,}widescreen"
run() { # name, then the program's arguments
    name=$1; shift
    rm -f "$work/rollback-eeprom.bin"
    "build/$GAME/$GAME" --rom "$ROM" --headless --frames "$frames" --auto "11:0:$players" --settings "$work/rollback-settings.ini" \
        --eeprom "$work/rollback-eeprom.bin" --hash-log "$work/$name.txt" "$@" 2> "$work/$name.err" || {
        echo "FAILED: the $name run stopped"; cat "$work/$name.err"; exit 1; }
}
status=0
compare() { # what, straight, rolled back
    if cmp -s "$work/$2.txt" "$work/$3.txt"; then
        echo "ok: $1: the same over $frames frames ($(grep -o '[0-9]* rollbacks' "$work/$3.err"))"
    else
        echo "FAILED: $1: first difference at frame $(diff "$work/$2.txt" "$work/$3.txt" | grep '^<' | head -1 | cut -d' ' -f2)"
        status=1
    fi
}
run rollback-straight
run rollback-rolled --rollback-test 9
compare "the game's own features" rollback-straight rollback-rolled
run rollback-play-straight --features "$play"
run rollback-play-rolled --features "$play" --rollback-test 9
compare "with $play" rollback-play-straight rollback-play-rolled
run rollback-undrawn-straight --features "$play" --hash-no-picture
run rollback-undrawn-rolled --features "$play" --hash-no-picture --rollback-test 9
compare "the frames run again not drawn" rollback-undrawn-straight rollback-undrawn-rolled
grep "rollback test: a" "$work/rollback-undrawn-rolled.err" | sed 's/^rollback test: /cost: /'
exit $status
