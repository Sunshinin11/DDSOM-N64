// Gera os dados do cartucho N64 a partir do romset (usa o carregador do proprio cps2recomp).
#include "romset.h"
#include "game_info.h"
#include <cstdio>
#include <string>
using namespace cps2recomp;
static void W(const std::string &p, const std::vector<uint8_t> &v) {
    FILE *f = fopen(p.c_str(), "wb"); fwrite(v.data(), 1, v.size(), f); fclose(f);
    printf("%s %zu\n", p.c_str(), v.size());
}
int main(int, char **argv) {
    RomList list = ParseRomList(GAME_PROGRAM, GAME_GRAPHICS, GAME_SOUND_PROGRAM, GAME_SAMPLES, GAME_KEY, GAME_ROM_SET,
                                GAME_ROM_PARENT, "", "", "");
    RomSet set; std::string err;
    if (!LoadRomSet(argv[1], list, set, err)) { printf("erro: %s\n", err.c_str()); return 1; }
    std::string o = argv[2];
    W(o + "/program.bin", set.program); W(o + "/opcodes.bin", set.opcodes); W(o + "/gfx.bin", set.graphics);
    W(o + "/z80.bin", set.soundProgram); W(o + "/samples.bin", set.samples);
    printf("soundOpcodes %zu encryptedEnd %X cps1 %d\n", set.soundOpcodes.size(), set.encryptedEnd, set.board.cps1);
}
