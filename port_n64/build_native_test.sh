#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p port_n64/build
python3 port_n64/prepare_assets.py --skip-existing
python3 port_n64/prepare_test_assets.py
python3 tools/recomp.py --program work/ddsom/program.bin --entries port_n64/native_entries.txt --out work/ddsom/n64test --limit 400000 --hook-sources port_n64 > port_n64/build/generate-test.log
# The N64 code: no 68000 time at all (c.cycles is gone from it); n64test above is the timed reference of the host tests
python3 tools/recomp.py --program work/ddsom/program.bin --entries port_n64/native_entries.txt --out work/ddsom/n64nt --limit 400000 --hook-sources port_n64 --native-time > port_n64/build/generate-nt.log
python3 tools/recompz80.py --program work/ddsom/n64fs/z80.bin --opcodes work/ddsom/program.bin.z80 --entries port_n64/sound_entries.txt --out work/ddsom/n64soundtest > port_n64/build/sound-generation.log
make -f port_n64/tests/host.mk -j4 > port_n64/build/host-build.log 2>&1
port_n64/build/host/board-test 3000 > port_n64/build/board-test.log 2> port_n64/build/board-state.log
# The code without time against the timed code run until idle: RAM, video, palette and sound must be identical per frame (6000 frames)
frames_of() { # name generated-dir flags...
    n=$1; g=$2; shift 2
    d=port_n64/build/fh_$n; mkdir -p "$d"
    g++ -DDDSOM_NATIVE_BOARD=1 "$@" -Iruntime -Imachine -I"$g" -std=gnu++20 -O2 -g0 -fno-strict-aliasing -ffunction-sections -fdata-sections -Wl,--gc-sections $g/native_*.cpp machine/m68k.cpp machine/eeprom.cpp runtime/recomp.cpp runtime/game.cpp port_n64/board.cpp port_n64/tests/frame_hash.cpp -o "$d/fh"
    "$d/fh" 6000 > "$d/frames.txt" 2>/dev/null
}
frames_of old work/ddsom/n64test -DDDSOM_IDLE_FRAMES
frames_of nt work/ddsom/n64nt -DDDSOM_NATIVE_TIME -DDDSOM_NT_IDLE_ONLY
cmp port_n64/build/fh_old/frames.txt port_n64/build/fh_nt/frames.txt > port_n64/build/native-time-test.log && echo "NATIVE TIME PASS: 6000 frames identical" >> port_n64/build/native-time-test.log
make -f port_n64/tests/host.mk port_n64/build/host/video-test -j4 > port_n64/build/video-build.log 2>&1
port_n64/build/host/video-test > port_n64/build/video-test.log
make -f port_n64/tests/sound.mk -j4 > port_n64/build/sound-build.log 2>&1
port_n64/build/sound-host/sequence-test > port_n64/build/sound-test.log
for name in save_format pcm_voice audio_protocol; do
    g++ -std=c++20 -O2 -fsanitize=address,undefined "port_n64/tests/$name.cpp" -o "port_n64/build/$name-test"
    "port_n64/build/$name-test" > "port_n64/build/$name-test.log"
done
cp port_n64/build/audio_protocol-test.log port_n64/build/audio-protocol-test.log
make -C port_n64 -f Makefile.test N64_INST="${N64_INST:-/opt/libdragon}" -j4 > port_n64/build/native-build.log 2>&1
python3 port_n64/verify_native_test.py
