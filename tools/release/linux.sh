#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Release build for Linux, for the processor it runs on: x86_64 (Steam Deck and Steam Machine included) or ARM64
# (aarch64: ARM handhelds, single-board computers): build/release/CPS2Recomp-linux-x86_64.tar.gz or
# CPS2Recomp-linux-arm64.tar.gz (the launcher, every game's program, licences). Run on Linux with the packages of
# tools/release/Dockerfile (docs/BUILDING.md), and the translated code of a build of each game with its ROM set
# (lib.sh), and Rust for librashader (tools/librashader.sh: the Display page's shaders). SDL 3 is built in, so the programs need only the system's usual X11 or Wayland,
# sound and zlib libraries.
set -eu
. "$(dirname "$0")/lib.sh"
case "$(uname -m)" in
    x86_64) SYSTEM=linux-x86_64 ;;
    aarch64|arm64) SYSTEM=linux-arm64 ;;
    *) echo "error: no Linux release for $(uname -m)" >&2; exit 1 ;;
esac
BUILD="$ROOT/build/release-$SYSTEM"
sdl_static "$SYSTEM"
build_programs "$BUILD" -DCMAKE_PREFIX_PATH="$DEPS/sdl-$SYSTEM" -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"

STAGE="$BUILD/stage/$ARCHIVE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
copy_programs "$BUILD" "$STAGE" ""
strip "$STAGE/$EXE-launcher" $(for game in $GAMES; do echo "$STAGE/$game"; done)
add_notices "$STAGE" "$BUILD/launcher"
# (the Display page's shaders, port/shading.h: librashader beside the games and the presets, tools/librashader.sh)
LIBRA=$("$ROOT/tools/librashader.sh")
cp "$LIBRA/librashader.so" "$STAGE/"
cp -R "$LIBRA/shaders" "$STAGE/"
cp "$LIBRA/librashader-LICENSE.md" "$STAGE/licenses/librashader-MPL-2.0.txt"
cp "$LIBRA/librashader-LICENSE-GPL.md" "$STAGE/licenses/librashader-GPL-3.0.txt"
rm -f "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
tar -czf "$OUT/$ARCHIVE-$SYSTEM.tar.gz" -C "$BUILD/stage" "$ARCHIVE"
check_archive "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
update_files "$SYSTEM" "$STAGE" "$OUT/$ARCHIVE-$SYSTEM.tar.gz"
echo "wrote $OUT/$ARCHIVE-$SYSTEM.tar.gz"
