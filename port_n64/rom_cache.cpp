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

const uint8_t *RomCacheMiss(RomCache &c, uint32_t address) {
    const uint32_t page = address >> RomCache::kPageBits;
    const uint32_t slot = page & (RomCache::kSlots - 1);
    uint8_t *dst = c.data + slot * RomCache::kPage;
    if (!c.pi || address >= c.size) { debugf("[DDSOM_FATAL] ROM range %08lx/%08lx\n",(unsigned long)address,(unsigned long)c.size); abort(); }
    uint32_t begin=TICKS_READ();data_cache_hit_invalidate(dst, RomCache::kPage);invalidateTicks+=uint32_t(TICKS_READ()-begin);begin=TICKS_READ();
    dma_read(dst, c.pi + page * RomCache::kPage, RomCache::kPage);
    dmaTicks+=uint32_t(TICKS_READ()-begin);
    c.tag[slot] = page + 1;
    c.misses++;
    return dst + (address & (RomCache::kPage - 1));
}

void RomCacheInit(RomCache &c, uint32_t pi, uint32_t size) {
    c.pi = pi;
    c.size = size;
    c.data = static_cast<uint8_t *>(memalign(16, RomCache::kSlots * RomCache::kPage));
    assertf(c.data && pi && size, "Invalid ROM cache allocation/resource");
    memset(c.tag,0,sizeof c.tag);
}

} // namespace cps2recomp
