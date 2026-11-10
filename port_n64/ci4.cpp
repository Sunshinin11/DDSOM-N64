// Converte os graficos do CPS-2 (4 bitplanes por grupo de 8 pixels) para CI4 do N64 (nibbles, pixel da esquerda no
// nibble alto), no mesmo lugar e com o mesmo tamanho: os enderecos dos tiles nao mudam. Gera tambem o mapa de vazios:
// 1 bit por meio bloco de 64 bytes (os grupos de 4 bytes nas posicoes 0-3 ou 4-7 de cada linha de 8), ligado quando
// todos os pixels sao a pen 15 (transparente).
#include <cstdio>
#include <cstdint>
#include <vector>
int main(int, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    std::vector<uint8_t> g;
    fseek(f, 0, SEEK_END); g.resize(ftell(f)); fseek(f, 0, SEEK_SET); fread(g.data(), 1, g.size(), f); fclose(f);
    const size_t blocks = g.size() / 64;
    std::vector<uint8_t> empty((blocks * 2 + 7) / 8, 0);
    for (size_t b = 0; b < blocks; b++) {
        bool e[2] = {true, true};
        for (int o = 0; o < 64; o++) {
            if (g[b * 64 + o] != 0xFF) e[(o & 7) >= 4] = false;
        }
        for (int h = 0; h < 2; h++) if (e[h]) empty[(b * 2 + h) / 8] |= 1 << ((b * 2 + h) & 7);
    }
    for (size_t at = 0; at + 4 <= g.size(); at += 4) {
        const uint8_t b0 = g[at], b1 = g[at + 1], b2 = g[at + 2], b3 = g[at + 3];
        uint8_t px[8];
        for (int k = 0; k < 8; k++) {
            const int s = 7 - k;
            px[k] = ((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) | (((b2 >> s) & 1) << 2) | (((b3 >> s) & 1) << 3);
        }
        for (int j = 0; j < 4; j++) g[at + j] = (px[2 * j] << 4) | px[2 * j + 1];
    }
    f = fopen(argv[2], "wb"); fwrite(g.data(), 1, g.size(), f); fclose(f);
    f = fopen(argv[3], "wb"); fwrite(empty.data(), 1, empty.size(), f); fclose(f);
    size_t n = 0; for (auto c : empty) n += __builtin_popcount(c);
    printf("ci4 %zu bytes, vazios %zu de %zu meios-blocos, mapa %zu bytes\n", g.size(), n, blocks * 2, empty.size());
}
