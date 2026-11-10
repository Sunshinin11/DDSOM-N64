#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare all N64 cartridge assets and recompiled 68000 sources from rom/ddsom/."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd, cwd=None):
    print("+", " ".join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], cwd=cwd, check=True)


def verify_sound_table(snd_dir: Path) -> tuple[int, int, int]:
    table_path = snd_dir / "table.bin"
    raw = table_path.read_bytes()
    if len(raw) != 0x490 * 3:
        raise RuntimeError(f"Invalid table.bin size: {len(raw)} (expected {0x490 * 3})")
    refs = set()
    music_count = 0
    sfx_count = 0
    for code in range(0x490):
        fid = (raw[code * 3] << 8) | raw[code * 3 + 1]
        music = raw[code * 3 + 2]
        if fid == 0xFFFF:
            continue
        wav64 = snd_dir / f"s{fid:04X}.wav64"
        if not wav64.is_file() or wav64.stat().st_size == 0:
            raise RuntimeError(f"table.bin code {code:04X} references missing/empty {wav64}")
        refs.add(wav64.name)
        if music:
            music_count += 1
        else:
            sfx_count += 1
    return len(refs), music_count, sfx_count


def convert_wav_dir(audioconv64: str, src_dir: Path, dst_dir: Path, compress_level: int):
    wavs = sorted(src_dir.glob("*.wav"))
    if not wavs:
        return
    # Batch in chunks of 100 to stay well below command-line length limits.
    chunk = 100
    for i in range(0, len(wavs), chunk):
        batch = wavs[i : i + chunk]
        run([
            audioconv64,
            "--wav-mono",
            "--wav-resample",
            "12019",
            "--wav-compress",
            str(compress_level),
            "-o",
            str(dst_dir),
            *[str(w) for w in batch],
        ])


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", default=os.environ.get("ROM", str(root / "rom" / "ddsom")),
                        help="Path to the user's DDSOM ROM folder or zip (default: rom/ddsom)")
    parser.add_argument("--n64-inst", default=os.environ.get("N64_INST", "/opt/libdragon"),
                        help="Path to libdragon installation (default: $N64_INST or /opt/libdragon)")
    parser.add_argument("--skip-existing", action="store_true",
                        help="Skip regeneration if n64fs_native and n64nt already exist and validate")
    args = parser.parse_args()

    n64_inst = Path(args.n64_inst)
    audioconv64 = n64_inst / "bin" / "audioconv64"
    if not audioconv64.is_file():
        sys.exit(f"ERROR: audioconv64 not found at {audioconv64}. Set N64_INST to your libdragon prefix.")

    work = root / "work" / "ddsom"
    gen = work / "gen"
    n64fs = work / "n64fs"
    n64fs_native = work / "n64fs_native"
    snd_dir = n64fs_native / "snd"
    n64nt = work / "n64nt"
    host_build = root / "port_n64" / "build" / "host_tools"

    if args.skip_existing and (snd_dir / "table.bin").is_file() and (n64nt / "native_table.cpp").is_file():
        needed = [n64fs_native / f for f in ("program.bin", "opcodes.bin", "gfx.bin", "empty.bin")]
        if all(f.is_file() for f in needed):
            unique, mus, sfx = verify_sound_table(snd_dir)
            print(f"Assets already prepared ({unique} unique wav64, {mus} music, {sfx} sfx); skipping.")
            return

    rom_path = Path(args.rom)
    if not rom_path.exists():
        sys.exit(f"ERROR: ROM folder not found: {rom_path}\nPlace your DDSOM ROM files in rom/ddsom/ or pass --rom.")

    gen.mkdir(parents=True, exist_ok=True)
    n64fs.mkdir(parents=True, exist_ok=True)
    n64fs_native.mkdir(parents=True, exist_ok=True)
    snd_dir.mkdir(parents=True, exist_ok=True)
    host_build.mkdir(parents=True, exist_ok=True)

    cxx = os.environ.get("CXX", "g++")

    # 1. Generate game_info.h from games/ddsom/game.cfg
    run([sys.executable, str(root / "tools" / "gamecfg.py"), "--header", str(gen / "game_info.h"), "ddsom"], cwd=root)

    # 2. Build and run dump.cpp -> work/ddsom/n64fs/{program,opcodes,gfx,z80,samples}.bin
    dump_bin = host_build / "dump"
    run([
        cxx, "-std=c++20", "-O2",
        f"-I{gen}", f"-I{root / 'machine'}",
        str(root / "port_n64" / "dump.cpp"),
        str(root / "machine" / "romset.cpp"),
        str(root / "machine" / "cps2crypt.cpp"),
        str(root / "machine" / "kabuki.cpp"),
        "-lz", "-o", str(dump_bin),
    ], cwd=root)
    run([str(dump_bin), str(rom_path), str(n64fs)], cwd=root)

    # 3. Build and run ci4.cpp -> work/ddsom/n64fs_native/{gfx.bin, empty.bin}
    ci4_bin = host_build / "ci4"
    run([cxx, "-std=c++20", "-O2", str(root / "port_n64" / "ci4.cpp"), "-o", str(ci4_bin)], cwd=root)
    run([str(ci4_bin), str(n64fs / "gfx.bin"), str(n64fs_native / "gfx.bin"), str(n64fs_native / "empty.bin")], cwd=root)

    # 4. Copy program/opcode ROM images for cartridge and host tools
    shutil.copy2(n64fs / "program.bin", n64fs_native / "program.bin")
    shutil.copy2(n64fs / "opcodes.bin", n64fs_native / "opcodes.bin")
    shutil.copy2(n64fs / "opcodes.bin", work / "program.bin")
    shutil.copy2(n64fs / "program.bin", work / "program.bin.data")
    (work / "program.bin.z80").write_bytes((n64fs / "z80.bin").read_bytes()[:0x8000])

    # 5. Build and run offline Z80 + QSound WAV/table.bin generator
    gen_snd_bin = host_build / "gen_sound_bank"
    run([
        cxx, "-std=c++20", "-O2", "-pthread",
        f"-I{root}", f"-I{root / 'sound'}", f"-I{root / 'runtime'}",
        str(root / "port_n64" / "gen_sound_bank.cpp"),
        str(root / "sound" / "z80.cpp"),
        str(root / "sound" / "qsound.cpp"),
        str(root / "runtime" / "recomp_z80.cpp"),
        str(root / "runtime" / "no_z80_blocks.cpp"),
        "-o", str(gen_snd_bin),
    ], cwd=root)

    wav_tmp = work / "snd_wav"
    if wav_tmp.exists():
        shutil.rmtree(wav_tmp)
    for old_wav64 in snd_dir.glob("*.wav64"):
        old_wav64.unlink()

    run([
        str(gen_snd_bin),
        str(n64fs / "z80.bin"),
        str(work / "program.bin.z80"),
        str(n64fs / "samples.bin"),
        str(wav_tmp),
        str(snd_dir / "table.bin"),
    ], cwd=root)

    # 6. Convert rendered WAVs to WAV64 (SFX: VADPCM=1, Music: ULC=2)
    convert_wav_dir(str(audioconv64), wav_tmp / "sfx", snd_dir, 1)
    convert_wav_dir(str(audioconv64), wav_tmp / "music", snd_dir, 2)
    shutil.rmtree(wav_tmp)

    unique, mus, sfx = verify_sound_table(snd_dir)
    print(f"Verified sound bank: {unique} unique wav64 files ({mus} music codes, {sfx} sfx codes)")

    # 7. Generate native-time recompiled 68000 C++ files for the N64 ROM
    run([
        sys.executable,
        str(root / "tools" / "recomp.py"),
        "--program", str(work / "program.bin"),
        "--entries", str(root / "port_n64" / "native_entries.txt"),
        "--out", str(n64nt),
        "--limit", "400000",
        "--hook-sources", "port_n64",
        "--native-time",
    ], cwd=root)

    print("Asset preparation complete.")


if __name__ == "__main__":
    main()

