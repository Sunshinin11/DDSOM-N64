// Shadow over Mystara N64: renderizador RDP (rdp_video.cpp)
#pragma once
#include <cstddef>
#include <cstdint>

namespace cps2recomp {
struct NativeBoard;
struct Stats {
    uint32_t sceneUs = 0, uploadUs = 0, cacheWaitUs = 0;
    uint32_t cacheWaits = 0, descriptors = 0, ownershipBytes = 0;
    uint32_t backgroundUs = 0;
    // fases da montagem da cena (medicao): fila de objetos, posse dos pixels, desenho dos sprites
    uint32_t queueUs = 0, ownerUs = 0, spriteUs = 0, queued = 0, resolved = 0;
    uint32_t ownPrepUs = 0, ownTouchUs = 0; // (dentro da posse: limpezas/preparo e o teste de quem encosta em quem)
    uint32_t cachedBlitUs = 0;              // (dos cenarios: a copia do plano guardado; o resto e o desenho tile a tile)
    bool ownershipFallback = false;
    uint32_t tiles = 0, rects = 0, objects = 0, misses = 0, overflow = 0, tlutLoads = 0, uploads = 0;
};
void RdpVideoInit(uint32_t gfxPi, size_t gfxSize, const uint8_t *empty);
void RdpVideoDraw(NativeBoard &video, Stats &stats, void*target=nullptr);
void RdpVideoSetTest(bool tlutMap, bool tmemCache); // (teste: L e R)
void RdpVideoSetHide(bool sprites, bool planes);    // (painel de medicao: esconde sprites / cenarios)
void RdpVideoSetRspTiles(bool on);                  // (cenarios pelo RSP, rsp_tiles.S; desligado = caminho antigo, para comparar)
} // namespace cps2recomp
