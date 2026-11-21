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
    static constexpr uint32_t kWays = 4, kMaxSets = kSlots / kWays;
    uint32_t pi = 0;              // endereco PI do arquivo no cartucho
    uint32_t size = 0, setMask = kMaxSets - 1;
    uint8_t *data = nullptr;      // (setMask + 1) * kWays * kPage bytes
    uint32_t tag[kMaxSets][kWays] = {}; // pagina + 1 (0: vazio)
    uint8_t mru[kMaxSets] = {};
    uint8_t hand[kMaxSets] = {};
    uint32_t lastPage = UINT32_MAX;
    const uint8_t *lastBase = nullptr;
    uint32_t reads = 0, misses = 0;
};
extern RomCache g_romData, g_romOpcodes, g_romSamples;
const uint8_t *RomCacheLookup(RomCache &c, uint32_t address, uint32_t page); // (port_n64/rom_cache.cpp)

inline const uint8_t *RomPtr(RomCache &c, uint32_t address) {
    const uint32_t page = address >> RomCache::kPageBits;
    c.reads++;
    if (__builtin_expect(page == c.lastPage, 1)) {
        return c.lastBase + (address & (RomCache::kPage - 1));
    }
    return RomCacheLookup(c, address, page);
}
// Uma palavra (endereco par: nunca atravessa uma pagina)
inline uint32_t RomWord(RomCache &c, uint32_t address) {
    const uint8_t *p = RomPtr(c, address);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    typedef uint16_t Alias16 __attribute__((may_alias));
    return *reinterpret_cast<const Alias16*>(p);
#else
    return static_cast<uint32_t>((p[0] << 8) | p[1]);
#endif
}
// Uma palavra longa: um unico lookup quando contida na pagina de 512 bytes
inline uint32_t RomLong(RomCache &c, uint32_t address) {
    if (__builtin_expect((address & (RomCache::kPage - 1)) <= RomCache::kPage - 4, 1)) {
        const uint8_t *p = RomPtr(c, address);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
        return *reinterpret_cast<const Alias32*>(p);
#else
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
               (static_cast<uint32_t>(p[2]) << 8) | p[3];
#endif
    }
    const uint32_t high = RomWord(c, address);
    return (high << 16) | RomWord(c, address + 2);
}
#endif
} // namespace cps2recomp

