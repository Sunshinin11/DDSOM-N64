// Shadow over Mystara N64: renderizador RDP (rdp_video.cpp)
#pragma once
#include <cstddef>
#include <cstdint>

namespace cps2recomp {
struct NativeBoard;
struct Stats {
    uint32_t sceneUs = 0, uploadUs = 0;
    uint32_t tiles = 0, rects = 0, objects = 0, misses = 0, overflow = 0, tlutLoads = 0, uploads = 0;
};
void RdpVideoInit(uint32_t gfxPi, size_t gfxSize, const uint8_t *empty);
void RdpVideoDraw(NativeBoard &video, Stats &stats, void*target=nullptr);
void RdpVideoSetTest(bool tlutMap, bool tmemCache); // (teste: L e R)
} // namespace cps2recomp
