from pathlib import Path
import shutil,hashlib,json
p=Path(__file__).resolve().parents[1];dst=p/'work/ddsom/n64fs_test';dst.mkdir(exist_ok=True)
for name in ('program.bin','opcodes.bin','gfx.bin','empty.bin'):
 src=p/'work/ddsom/n64fs_native'/name;out=dst/name
 shutil.copy2(src,out)
shutil.copy2(p/'work/ddsom/n64fs/z80.bin',dst/'sound_program.bin')
shutil.copy2(p/'work/ddsom/program.bin.z80',dst/'sound_opcodes.bin')
samples=(p/'work/ddsom/n64fs/samples.bin').read_bytes()
(dst/'sample8.bin').write_bytes(samples)
(dst/'sample8_shifted.bin').write_bytes(b'\0'+samples)
(dst/'sample16.bin').unlink(missing_ok=True)
shutil.copytree(p/'work/ddsom/n64fs_native/snd',dst/'snd',dirs_exist_ok=True)
program=(dst/'sound_program.bin').read_bytes();ops=(dst/'sound_opcodes.bin').read_bytes()
assert program[0x16:0x1a]==b'1.15'
assert ops[0x89]==ops[0x8b]==ops[0x175]==0xfb
manifest={f.relative_to(dst).as_posix():{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in sorted(dst.rglob('*')) if f.is_file()}
(p/'port_n64/build/native-assets.json').write_text(json.dumps(manifest,indent=2))
print('Native sound assets: 1.15 driver, opcode data and signed 8-bit PCM with both DMA byte phases; recorded tracks not packaged')
