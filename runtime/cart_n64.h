// N64: o barramento do cartucho so devolve leituras de 32 bits alinhadas corretamente; leituras de 8 e 16 bits
// voltam com a parte errada da palavra. Le um byte por uma leitura de 32 bits alinhada (vale tambem para a RAM).
// Para o programa do 68000 (dados e opcodes), um cache de paginas na RAM, preenchido por DMA (port_n64/rom_cache.cpp):
// cada leitura sem cache do cartucho custa cerca de 1 us, e o jogo le tabelas da ROM o tempo todo.
#pragma once
#include <cstdint>

namespace cps2recomp {
inline uint8_t CartByte(const uint8_t *p) {
#ifdef CPS2RECOMP_N64
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    const uint32_t w = *reinterpret_cast<const volatile uint32_t *>(a & ~static_cast<uintptr_t>(3));
    return static_cast<uint8_t>(w >> ((3 - (a & 3)) * 8)); // (big-endian)
#else
    return *p;
#endif
}

#ifdef CPS2RECOMP_N64
struct RomCache {
    static constexpr uint32_t kPageBits = 9, kPage = 1u << kPageBits, kSlots = 128; // 128 paginas de 512 bytes (64 KB)
    uint32_t pi = 0;              // endereco PI do arquivo no cartucho
    uint32_t size = 0;
    uint8_t *data = nullptr;      // kSlots * kPage bytes
    uint32_t tag[kSlots] = {};    // pagina + 1 (0: vazio)
    uint32_t reads = 0, misses = 0;
};
extern RomCache g_romData, g_romOpcodes, g_romSamples;
const uint8_t *RomCacheMiss(RomCache &c, uint32_t address); // (port_n64/rom_cache.cpp)

inline const uint8_t *RomPtr(RomCache &c, uint32_t address) {
    const uint32_t page = address >> RomCache::kPageBits;
    const uint32_t slot = page & (RomCache::kSlots - 1);
    c.reads++;
    if (c.tag[slot] != page + 1) {
        return RomCacheMiss(c, address);
    }
    return c.data + slot * RomCache::kPage + (address & (RomCache::kPage - 1));
}
// Uma palavra (endereco par: nunca atravessa uma pagina)
inline uint32_t RomWord(RomCache &c, uint32_t address) {
    const uint8_t *p = RomPtr(c, address);
    return static_cast<uint32_t>((p[0] << 8) | p[1]);
}
#endif
} // namespace cps2recomp

