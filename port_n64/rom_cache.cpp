// Shadow over Mystara N64: o cache de paginas do programa do 68000 (runtime/cart_n64.h)
#include <libdragon.h>

#include "cart_n64.h"
#include "perf_n64.h"

#include <malloc.h>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

RomCache g_romData, g_romOpcodes, g_romSamples;
PerfCounters g_perf;
namespace {uint64_t invalidateTicks=0,dmaTicks=0;}
void RomCacheReport(){debugf("[DDSOM_DMA_COST] invalidate_us=%llu transfer_us=%llu fills=%lu\n",(unsigned long long)TICKS_TO_US(invalidateTicks),(unsigned long long)TICKS_TO_US(dmaTicks),(unsigned long)(g_romData.misses+g_romOpcodes.misses));}

const uint8_t *RomCacheLookup(RomCache &c, uint32_t address, uint32_t page) {
    const uint32_t set = (page ^ (page >> 5) ^ (page >> 10)) & c.setMask;
    const uint32_t key = page + 1;
    uint32_t *tags = c.tag[set];
    for (unsigned w = 0; w < RomCache::kWays; ++w) {
        if (tags[w] == key) {
            c.mru[set] = static_cast<uint8_t>(w);
            const uint8_t *base = c.data + (set * RomCache::kWays + w) * RomCache::kPage;
            c.lastPage = page;
            c.lastBase = base;
            return base + (address & (RomCache::kPage - 1));
        }
    }
    if (!c.pi || address >= c.size) { debugf("[DDSOM_FATAL] ROM range %08lx/%08lx\n",(unsigned long)address,(unsigned long)c.size); abort(); }
    unsigned victim = c.hand[set];
    if (victim == c.mru[set]) {
        victim = (victim + 1) & (RomCache::kWays - 1);
    }
    c.hand[set] = static_cast<uint8_t>((victim + 1) & (RomCache::kWays - 1));
    c.mru[set] = static_cast<uint8_t>(victim);
    uint8_t *dst = c.data + (set * RomCache::kWays + victim) * RomCache::kPage;
    uint32_t begin=TICKS_READ();data_cache_hit_invalidate(dst, RomCache::kPage);invalidateTicks+=uint32_t(TICKS_READ()-begin);begin=TICKS_READ();
    dma_read(dst, c.pi + page * RomCache::kPage, RomCache::kPage);
    dmaTicks+=uint32_t(TICKS_READ()-begin);
    tags[victim] = key;
    c.lastPage = page;
    c.lastBase = dst;
    c.misses++;
    return dst + (address & (RomCache::kPage - 1));
}

void RomCacheInit(RomCache &c, uint32_t pi, uint32_t size, unsigned slots) {
    const unsigned sets = slots / RomCache::kWays;
    assertf(sets >= 1 && sets <= RomCache::kMaxSets && (sets & (sets - 1)) == 0, "Invalid ROM cache set count");
    c.pi = pi;
    c.size = size;
    c.setMask = sets - 1;
    c.data = static_cast<uint8_t *>(memalign(16, slots * RomCache::kPage));
    assertf(c.data && pi && size, "Invalid ROM cache allocation/resource");
    memset(c.tag, 0, sizeof c.tag);
    memset(c.mru, 0, sizeof c.mru);
    memset(c.hand, 0, sizeof c.hand);
    c.lastPage = UINT32_MAX;
    c.lastBase = nullptr;
}

} // namespace cps2recomp
