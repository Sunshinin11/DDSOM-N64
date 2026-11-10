"""Validate test artifacts and verify sound table references for ddsom_native_playtest.z64."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

p = Path(__file__).resolve().parent
b = p / "build"
root = p.parent


def sha(f):
    return hashlib.sha256(f.read_bytes()).hexdigest()


rom = p / "ddsom_native_playtest.z64"
elf = b / "test/ddsom_native_playtest.elf"
assert rom.is_file(), f"Missing ROM: {rom}"
assert rom.read_bytes()[:4] == bytes.fromhex("80371240"), "ROM header invalid"
assert not (p / "ddsom_native_test.z64").exists(), "Duplicate ROM ddsom_native_test.z64 must not exist"
if (p / "ddsom.z64").exists() and (b / "audit-baseline.z64").exists():
    assert sha(p / "ddsom.z64") == sha(b / "audit-baseline.z64"), "Original ROM changed since audit baseline"

symbols = subprocess.check_output(
    [os.environ.get("N64_INST", "/opt/libdragon") + "/bin/mips64-elf-nm", "-C", str(elf)],
    text=True,
)
assert not re.search(r"::(?:Interpret\(|ExecuteOne\(|Execute\(unsigned char)", symbols), "Interpreter linked into test ROM"
assert "cps2recomp::Z80::" not in symbols and "ddsom::NativeAudio" not in symbols, "Experimental audio linked into playtest"

checks = {}
for name, marker in [
    ("board-test", "BOARD PASS 3000"),
    ("native-time-test", "NATIVE TIME PASS: 6000 frames identical"),
    ("video-test", "RDP PASS 80"),
    ("audio-protocol-test", "AUDIO PROTOCOL PASS"),
    ("save_format-test", "SAVE PASS"),
    ("pcm_voice-test", "PCM PASS"),
    ("sound-test", "SOUND SEQUENCE PASS"),
]:
    log = b / (name + ".log")
    text = log.read_text()
    assert marker in text, (name, text)
    checks[name] = text.strip()
assert "instructions=0 entries=0" in checks["board-test"]
assert re.search(r"POLL SKIPPED [1-9]\d*", checks["board-test"]), "Polling optimization was not exercised"

fs = root / "work/ddsom/n64fs_native"
snd_dir = fs / "snd"
table_bytes = (snd_dir / "table.bin").read_bytes()
assert len(table_bytes) == 0x490 * 3, "Invalid snd/table.bin size"
for code in range(0x490):
    fid = (table_bytes[code * 3] << 8) | table_bytes[code * 3 + 1]
    if fid != 0xFFFF:
        wav64 = snd_dir / f"s{fid:04X}.wav64"
        assert wav64.is_file() and wav64.stat().st_size > 0, f"Missing sound file {wav64} for code {code:04X}"

assets = {f.relative_to(fs).as_posix(): {"bytes": f.stat().st_size, "sha256": sha(f)} for f in sorted(fs.rglob("*")) if f.is_file()}
manifest = {
    "status": "interactive playtest; host equivalence and pixel tests verified; Ares/hardware playtest logged separately",
    "rom": rom.name,
    "sha256": sha(rom),
    "bytes": rom.stat().st_size,
    "elf_sha256": sha(elf),
    "interpreter_symbols": False,
    "experimental_audio_linked": False,
    "live_input": True,
    "audio": "8-channel recorded WAV64 (VADPCM/ULC) on native RSP mixer",
    "tests": checks,
    "filesystem": "work/ddsom/n64fs_native",
    "assets": assets,
}
(b / "native-test-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"INTERACTIVE ARTIFACT VERIFIED: {rom.name} {rom.stat().st_size} bytes SHA256={manifest['sha256']}")
