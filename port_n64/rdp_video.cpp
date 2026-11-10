// Shadow over Mystara N64: desenha o estado do video CPS-2 com o RDP (camadas de scroll e objetos como retangulos
// texturizados CI4), lendo direto a placa nativa (board.h): os mapas de tiles, a lista de objetos e a paleta que o
// jogo montou. As regras de camadas, prioridade e enderecos sao as do hardware (as mesmas de video/video.cpp).
#include <libdragon.h>

#include "rdp_video.h"
#include "board.h"
#include "run_options.h"
#ifdef CPS2RECOMP_N64
#include "audio.h"
#endif

#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <array>

namespace cps2recomp {

namespace {
    // Registradores (video/video.cpp)
    enum { kScroll1Base = 1, kOtherBase = 4, kScroll1X = 6, kRowScrollOffset = 16, kVideoControl = 17 };
    enum { kObjXOffset = 4, kObjYOffset = 5 };
    constexpr int kW = 384, kH = 224, kTop = (240 - kH) / 2;
    constexpr int kFirstColumn = 64, kFirstLine = 16;

    inline uint16_t W16(const uint8_t *p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }

    // --- Cache de tiles na RDRAM: o RDP nao le o cartucho; os tiles vem por DMA ---
    struct Pool {
        int bytes = 0, count = 0, mask = 0, shift=0;
        unsigned cursor=0,tombstones=0;
        uint8_t *data = nullptr;
        uint32_t *tag = nullptr;  // (endereco nos graficos + 1; 0: vazio)
        uint32_t *used = nullptr; // (o frame em que foi usado por ultimo)
        uint16_t *opacity=nullptr; // 16 rows of opacity bits a slot (16x16 tiles), made when first asked for
        uint8_t *opacityValid=nullptr;
        uint32_t *lookup=nullptr; // open-addressed key -> stable physical slot; 0 empty, UINT32_MAX tombstone.
        struct Shortcut{uint32_t key=0,slot=0;};Shortcut hot[64];
        uint32_t Hash(uint32_t key)const{uint32_t h=((key-1)>>shift)*2654435761u;return h^(h>>15);}
        void Put(uint32_t key,unsigned slot){
            unsigned at=Hash(key)&mask;
            while(lookup[at]&&lookup[at]!=UINT32_MAX)at=(at+1)&mask;
            if(lookup[at]==UINT32_MAX)tombstones--;
            lookup[at]=slot+1;
        }
        void Erase(unsigned slot){
            auto&cached=hot[Hash(tag[slot])&63];if(cached.key==tag[slot])cached.key=0;
            unsigned at=Hash(tag[slot])&mask;
            while(lookup[at]!=slot+1){assertf(lookup[at],"Missing tile cache mapping");at=(at+1)&mask;}
            lookup[at]=UINT32_MAX;tombstones++;
        }
        void Rehash(){
            memset(lookup,0,(count*2)*sizeof(uint32_t));tombstones=0;
            for(int i=0;i<count;i++)if(tag[i])Put(tag[i],i);
        }
        void Init(int b, int n) {
            bytes = b;
            shift=b==64?6:b==128?7:9;
            count = n;
            mask = n * 2 - 1;
            data = static_cast<uint8_t *>(memalign(16, static_cast<size_t>(b) * n));
            tag = new uint32_t[n]();
            used = new uint32_t[n]();
            lookup = new uint32_t[n*2]();
            opacity = new uint16_t[n*16];
            opacityValid = new uint8_t[n]();
        }
    };
    Pool g_pool8, g_pool16, g_pool32;
    uint32_t g_gfxPi = 0;       // endereco PI dos graficos no cartucho
    size_t g_gfxSize = 0;
    const uint8_t *g_empty = nullptr; // 1 bit por meio bloco de 64 bytes
    uint32_t g_frame = 1;
    Stats *g_stats = nullptr;
    surface_t *g_target=nullptr;
    uint32_t g_priority[3][kH][kW/32];
    uint32_t g_claimed[kH][kW/32];
    uint8_t g_spriteCells[(kW/8)*(kH/8)];
    unsigned g_priorityMark=0;
    alignas(16) uint8_t g_resolved[256][128];
    void FlushCaches();
    // Eight CI4 pens -> eight opacity bits. Bit zero is the leftmost screen pixel.
    unsigned Opaque8(const uint8_t*p,bool flip){
        uint32_t v=~(uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]);
        v=(v|(v>>1)|(v>>2)|(v>>3))&0x11111111u;
        v=(v|(v>>3))&0x03030303u;v=(v|(v>>6))&0x000f000fu;v=(v|(v>>12))&255;
        if(!flip){v=((v>>1)&0x55)|((v&0x55)<<1);v=((v>>2)&0x33)|((v&0x33)<<2);v=(v>>4)|(v<<4);}
        return v&255;
    }
    constexpr auto ReverseBytes(){
        std::array<uint8_t,256> t{};
        for(unsigned i=0;i<256;i++){unsigned r=0;for(unsigned b=0;b<8;b++)if(i&(1u<<b))r|=1u<<(7-b);t[i]=uint8_t(r);}
        return t;
    }
    constexpr auto g_reverse8=ReverseBytes();
    unsigned Reverse16(unsigned v){return unsigned(g_reverse8[v&255])<<8|g_reverse8[(v>>8)&255];}
    // The opacity bits of a 16x16 tile's 16 rows as the tile is stored (not flipped), kept with the tile in its pool
    const uint16_t*Opacity16(Pool&pool,const uint8_t*tex){
        const unsigned slot=unsigned((tex-pool.data)/pool.bytes);
        uint16_t*rows=pool.opacity+slot*16;
        if(!pool.opacityValid[slot]){
            for(unsigned dy=0;dy<16;dy++)rows[dy]=uint16_t(Opaque8(tex+dy*8,false)|(Opaque8(tex+dy*8+4,false)<<8));
            pool.opacityValid[slot]=1;
        }
        return rows;
    }
    unsigned RowBits(const uint32_t*row,unsigned x){
        unsigned shift=x&31,bits=row[x/32]>>shift;
        if(shift&&x/32+1<kW/32)bits|=row[x/32+1]<<(32-shift);
        return bits;
    }
    void Claim(uint32_t*row,unsigned x,unsigned bits){
        unsigned shift=x&31;row[x/32]|=bits<<shift;
        if(shift&&x/32+1<kW/32)row[x/32+1]|=bits>>(32-shift);
    }
    constexpr auto PenMasks(){
        std::array<uint32_t,256> result{};
        for(unsigned bits=0;bits<256;bits++)for(unsigned x=0;x<8;x++)if(bits&(1u<<x))result[bits]|=15u<<((7-x)*4);
        return result;
    }
    constexpr auto g_penMasks=PenMasks();
    void ResolvedRow(uint8_t*out,const uint8_t*src,unsigned visible,bool flip){
        for(unsigned half=0;half<2;half++){
            auto*p=src+(flip?1-half:half)*4;
            uint32_t v=uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
            if(flip){v=__builtin_bswap32(v);v=((v>>4)&0x0f0f0f0f)|((v&0x0f0f0f0f)<<4);}
            v|=~g_penMasks[(visible>>(half*8))&255];
            out[half*4]=v>>24;out[half*4+1]=v>>16;out[half*4+2]=v>>8;out[half*4+3]=v;
        }
    }

    // Paletas: as 0xC00 cores em RGBA5551, a pen 15 de cada grupo transparente
    uint16_t *g_tlut = nullptr;
    int g_tlutGroup[16];
    bool g_tlutMap = true, g_tmemCache = true; // (teste: L e R alternam)
    uint32_t g_tlutUsed[16];
    uint32_t g_tlutTick = 0;

    bool Empty(uint32_t at, int bytes, int half) {
        // half: -1 os dois meios de cada bloco; 0/1 so um (tiles 8x8)
        for (uint32_t b = at / 64; b < (at + bytes + 63) / 64; b++) {
            const uint32_t i0 = b * 2;
            const bool e0 = (g_empty[i0 >> 3] >> (i0 & 7)) & 1;
            const bool e1 = (g_empty[(i0 + 1) >> 3] >> ((i0 + 1) & 7)) & 1;
            if (half == 0 ? !e0 : half == 1 ? !e1 : !(e0 && e1)) {
                return false;
            }
        }
        return true;
    }

    const uint8_t *Fetch(Pool &pool, uint32_t at) {
        unsigned hash=pool.Hash(at+1);auto&cached=pool.hot[hash&63];
        if(cached.key==at+1){pool.used[cached.slot]=g_frame;return pool.data+size_t(cached.slot)*pool.bytes;}
        unsigned bucket=hash&pool.mask;
        for(unsigned probe=0;probe<unsigned(pool.count*2);probe++,bucket=(bucket+1)&pool.mask){
            unsigned entry=pool.lookup[bucket];if(!entry)break;
            if(entry==UINT32_MAX)continue;
            unsigned s=entry-1;
            if(pool.tag[s]==at+1){
                cached={at+1,s};
                pool.used[s] = g_frame;
                return pool.data + static_cast<size_t>(s) * pool.bytes;
            }
        }
        for (int probe = 0; probe < pool.count; probe++) {
            const unsigned s = (pool.cursor+probe)&(pool.count-1);
            if (pool.used[s] != g_frame) {
                pool.cursor=(s+1)&(pool.count-1);
                uint8_t *dst = pool.data + static_cast<size_t>(s) * pool.bytes;
                const uint32_t t0 = get_ticks_us();
                data_cache_hit_invalidate(dst, pool.bytes);
                dma_read(dst, g_gfxPi + at, pool.bytes);
                g_stats->uploadUs += get_ticks_us() - t0;
                g_stats->misses++;
                pool.opacityValid[s]=0;
                if(pool.tag[s])pool.Erase(s);
                pool.tag[s] = at + 1;
                pool.used[s] = g_frame;
                if(pool.tombstones>unsigned(pool.count/2))pool.Rehash();else pool.Put(at+1,s);
                cached={at+1,s};
                return dst;
            }
        }
        g_stats->overflow++; // Capacity pressure: complete pending reads and recycle, never drop a tile.
        FlushCaches();
        return Fetch(pool,at);
    }

    // Paletas na TMEM: 16 posicoes; grupo -> posicao por tabela direta (sem busca a cada retangulo)
    int g_groupSlot[0xC0];
    int TlutSlot(int group) {
        g_tlutTick++;
        if (!g_tlutMap) { // (busca nos 16 slots, como antes)
            int best = 0;
            for (int k = 0; k < 16; k++) {
                if (g_tlutGroup[k] == group) { g_tlutUsed[k] = g_tlutTick; return k; }
                if (g_tlutUsed[k] < g_tlutUsed[best]) best = k;
            }
            rdpq_tex_upload_tlut(&g_tlut[group * 16], best * 16, 16);
            g_tlutGroup[best] = group;
            g_tlutUsed[best] = g_tlutTick;
            g_stats->tlutLoads++;
            return best;
        }
        int s = g_groupSlot[group];
        if (s >= 0) {
            g_tlutUsed[s] = g_tlutTick;
            return s;
        }
        s = 0;
        for (int k = 1; k < 16; k++) {
            if (g_tlutUsed[k] < g_tlutUsed[s]) s = k;
        }
        if (g_tlutGroup[s] >= 0) g_groupSlot[g_tlutGroup[s]] = -1;
        rdpq_tex_upload_tlut(&g_tlut[group * 16], s * 16, 16);
        g_tlutGroup[s] = group;
        g_groupSlot[group] = s;
        g_tlutUsed[s] = g_tlutTick;
        g_stats->tlutLoads++;
        return s;
    }

    // Um tile: textura de tw x th (CI4, linhas de stride bytes), a parte s0..s0+w, t0..t0+h desenhada em x, y.
    // Os tiles ficam guardados na TMEM: 12 posicoes de 128 bytes (tiles 8x8 e 16x16) e uma de 512 (32x32), na metade
    // de baixo (a de cima e das paletas). Um tile que ja esta la so troca o descritor; senao, entra por LOAD_BLOCK
    // (o bloco CI4 como texels de 16 bits) no lugar do usado ha mais tempo.
    constexpr int kTmemSlots = 12;
    const uint8_t *g_tmemTex[kTmemSlots + 1];
    uint32_t g_tmemUsed[kTmemSlots + 1];
    uint32_t g_tmemTick = 0;
    int g_lastSlot = -1, g_lastPalette = -1;

    void ResetGpuState(){
        rdpq_set_mode_standard();rdpq_mode_tlut(TLUT_RGBA16);rdpq_mode_alphacompare(1);rdpq_mode_filter(FILTER_POINT);
        rdpq_set_scissor(0,kTop,kW,kTop+kH);
        for(int s=0;s<16;s++){g_tlutGroup[s]=-1;g_tlutUsed[s]=0;}
        for(int k=0;k<=kTmemSlots;k++){g_tmemTex[k]=nullptr;g_tmemUsed[k]=0;}
        for(int g=0;g<0xc0;g++)g_groupSlot[g]=-1;
        g_lastSlot=g_lastPalette=-1;
    }
    void FlushCaches(){
        assertf(g_target,"Missing render target");rdpq_detach_wait();rdpq_attach(g_target,nullptr);
        g_frame++;ResetGpuState();
    }
    void Blit(const uint8_t *tex, int tw, int th, int stride, int group, int x, int y, int w, int h, int s0, int t0,
              bool flipX, bool flipY) {
#ifdef CPS2RECOMP_N64
        static unsigned service=0;if((++service&31)==0)ddsom::AudioPoll();
#endif
        if(g_priorityMark){
            int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>kW?kW:x+w,y1=y+h>kH?kH:y+h;
            bool covered=false;
            for(int gy=y0/8;gy<(y1+7)/8&&!covered;gy++)for(int gx=x0/8;gx<(x1+7)/8;gx++)if(g_spriteCells[gy*(kW/8)+gx]){covered=true;break;}
            unsigned layer=g_priorityMark==1?0:g_priorityMark==2?1:2;
            if(covered)for(int py=y0;py<y1;py++)for(int chunk=0;chunk<w;chunk+=8){
                int px=x+chunk;if(px+8<=0||px>=kW)continue;
                int left=px<0?0:px,right=px+8>kW?kW:px+8;
                if(!g_spriteCells[(py/8)*(kW/8)+left/8]&&!g_spriteCells[(py/8)*(kW/8)+(right-1)/8])continue;
                int sx=s0+(flipX?w-8-chunk:chunk),sy=t0+(flipY?h-1-(py-y):py-y);
                unsigned bits=Opaque8(tex+sy*stride+sx/2,flipX);
                if(px<0){bits>>=-px;px=0;}
                bits&=(1u<<(right-px))-1;
                auto*row=g_priority[layer][py];unsigned shift=px&31;
                row[px/32]|=bits<<shift;
                if(shift>24&&px/32+1<kW/32)row[px/32+1]|=bits>>(32-shift);
            }
        }
        const int palette = TlutSlot(group);
        g_tmemTick++;
        int slot;
        if (!g_tmemCache) {
            slot = 0; // (uma posicao so: recarrega sempre que o tile muda, como antes)
        } else if (tw == 32) {
            slot = kTmemSlots; // (a posicao de 512 bytes)
        } else {
            slot = 0;
            if (g_lastSlot >= 0 && g_lastSlot < kTmemSlots && g_tmemTex[g_lastSlot] == tex) {
                slot = g_lastSlot; // (the tile of the last rectangle again: no search)
            } else
            for (int k = 0; k < kTmemSlots; k++) {
                if (g_tmemTex[k] == tex) { slot = k; break; }
                if (g_tmemUsed[k] < g_tmemUsed[slot]) slot = k;
            }
        }
        const int tmem = slot * 128;
        if (g_tmemTex[slot] != tex) {
            const int bytes = stride * th;
            rdpq_set_texture_image_raw(0, PhysicalAddr(tex), FMT_RGBA16, bytes / 2, 1);
            rdpq_set_tile(TILE1, FMT_RGBA16, tmem, 0, NULL);
            rdpq_load_block(TILE1, 0, 0, bytes / 2, stride);
            g_tmemTex[slot] = tex;
            g_lastSlot = -1;
            g_stats->uploads++;
        }
        g_tmemUsed[slot] = g_tmemTick;
        if (slot != g_lastSlot || palette != g_lastPalette) {
            rdpq_tileparms_t parms = {};
            parms.palette = static_cast<uint8_t>(palette);
            rdpq_set_tile(TILE0, FMT_CI4, tmem, stride, &parms);
            rdpq_set_tile_size(TILE0, 0, 0, tw, th);
            g_lastSlot = slot;
            g_lastPalette = palette;
        }
        const float sa = flipX ? s0 + w : s0, sb = flipX ? s0 : s0 + w;
        const float ta = flipY ? t0 + h : t0, tb = flipY ? t0 : t0 + h;
        rdpq_texture_rectangle_scaled(TILE0, x, y + kTop, x + w, y + kTop + h, sa, ta, sb, tb);
        g_stats->rects++;
    }
} // namespace

struct N64Video {
    static void Scroll(NativeBoard &v, int which) {
        const uint16_t *a = v.cpsA;
        const uint8_t *vram = v.videoRam;
        const uint32_t base = (static_cast<uint32_t>(a[kScroll1Base + which]) * 256) & ~0x3FFFu & 0x3FFFF;
        const uint8_t *map = &vram[base];
        const int shift = 3 + which, size = 1 << shift;
        const uint32_t mapMask = (64u << shift) - 1;
        const uint32_t scrollY = a[kScroll1X + which * 2 + 1];
        const bool rowScroll = which == 1 && (a[kVideoControl] & 1);
        const uint32_t other = (static_cast<uint32_t>(a[kOtherBase]) * 256) & ~0x7FFu & 0x3FFFF;
        auto lineX = [&](int ly) -> uint32_t {
            uint32_t x = a[kScroll1X + which * 2];
            if (rowScroll) {
                x += W16(&vram[other + (((ly + kFirstLine + a[kRowScrollOffset]) & 0x3FF) * 2)]);
            }
            return x;
        };
        const uint32_t tiles = 0x800000;
        const int group0 = 0x20 * (which + 1);
        // Faixas de linhas com o mesmo scroll horizontal e na mesma linha de tiles
        for (int ly = 0; ly < kH;) {
            const uint32_t sx = lineX(ly);
            const uint32_t y = (static_cast<uint32_t>(ly + kFirstLine) + scrollY) & mapMask;
            const int l0 = static_cast<int>(y & (size - 1));
            int band = size - l0;
            if (ly + band > kH) {
                band = kH - ly;
            }
            if (rowScroll) {
                int n = 1;
                while (n < band && lineX(ly + n) == sx) {
                    n++;
                }
                band = n;
            }
            const uint32_t row = y >> shift;
            const uint32_t firstX = (static_cast<uint32_t>(kFirstColumn) + sx) & mapMask;
            int out = -static_cast<int>(firstX & (size - 1));
            uint32_t column = firstX >> shift;
            for (; out < kW; out += size, column = (column + 1) & 63) {
                uint32_t index;
                switch (which) {
                case 0: index = (row & 0x1F) + (column << 5) + ((row & 0x20) << 6); break;
                case 1: index = (row & 0x0F) + (column << 4) + ((row & 0x30) << 6); break;
                default: index = (row & 0x07) + (column << 3) + ((row & 0x38) << 6); break;
                }
                uint32_t code = W16(&map[index * 4]);
                const uint16_t attr = W16(&map[index * 4 + 2]);
                const bool flipX = attr & 0x20, flipY = attr & 0x40;
                const int group = static_cast<int>(attr & 0x1F) + group0;
                const int t0 = flipY ? size - l0 - band : l0;
                g_stats->tiles++;
                if (which == 0) {
                    const uint32_t at = tiles + code * 64;
                    const int half = (column & 1) ? 1 : 0;
                    if (at + 64 > g_gfxSize || Empty(at, 64, half)) {
                        continue;
                    }
                    if (const uint8_t *tex = Fetch(g_pool8, at)) {
                        Blit(tex, 16, 8, 8, group, out, ly, 8, band, half * 8, t0, flipX, flipY);
                    }
                } else if (which == 1) {
                    const uint32_t at = tiles + code * 128;
                    if (at + 128 > g_gfxSize || Empty(at, 128, -1)) {
                        continue;
                    }
                    if (const uint8_t *tex = Fetch(g_pool16, at)) {
                        Blit(tex, 16, 16, 8, group, out, ly, 16, band, 0, t0, flipX, flipY);
                    }
                } else {
                    code &= 0x3FFF;
                    const uint32_t at = tiles + code * 512;
                    if (at + 512 > g_gfxSize || Empty(at, 512, -1)) {
                        continue;
                    }
                    if (const uint8_t *tex = Fetch(g_pool32, at)) {
                        Blit(tex, 32, 32, 16, group, out, ly, 32, band, 0, t0, flipX, flipY);
                    }
                }
            }
            ly += band;
        }
    }

    struct Obj {
        int16_t x, y;
        uint32_t at;
        uint32_t mask;
        uint8_t group;
        bool flipX, flipY;
        uint8_t depth;
    };
    static_assert(sizeof(Obj)==16,"Object queue must not waste padding per tile");
    // Exact RDP insertion positions: these masks are ORs of all layers above that position.
    static unsigned HardwareDepth(uint32_t mask){
        switch(mask){case 0xfe:return 0;case 0xfc:return 1;case 0xf0:return 2;case 0:return 3;case 255:return 5;default:return 4;}
    }

    static void Draw(NativeBoard &v, Stats &stats) {
        g_stats = &stats;
        g_frame++;
        const uint32_t t0 = get_ticks_us();

        // Paletas: convertidas para RGBA5551 so quando o jogo entrega uma paleta nova (a conta do hardware:
        // nivel * 0x11 * brilho / 0x2D, por tabela)
        static uint8_t level[16][16];
        static bool ready = false;
        if (!ready) {
            for (unsigned br = 0; br < 16; br++)
                for (unsigned lv = 0; lv < 16; lv++) level[br][lv] = static_cast<uint8_t>(lv * 0x11 * (0x0F + (br << 1)) / 0x2D);
            ready = true;
        }
        if (v.paletteChanged) {
            // (only the colors that changed since the last conversion; the rest of g_tlut is already right)
            static uint16_t converted[0xC00];
            static bool first = true;
            int firstChanged = 0xC00, lastChanged = -1;
            for (int n = 0; n < 0xC00; n++) {
                const uint16_t c = v.palette[n];
                if (!first && c == converted[n]) continue;
                converted[n] = c;
                if (n < firstChanged) firstChanged = n;
                lastChanged = n;
                const uint8_t *row = level[c >> 12];
                const uint32_t r = row[(c >> 8) & 0xF], g = row[(c >> 4) & 0xF], b = row[c & 0xF];
                g_tlut[n] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | ((n & 15) == 15 ? 0 : 1));
            }
            v.paletteChanged = false;
            first = false;
            if (lastChanged >= firstChanged) {
                uint8_t *from = reinterpret_cast<uint8_t *>(g_tlut + firstChanged);
                data_cache_hit_writeback(from, (lastChanged - firstChanged + 1) * 2);
            }
        }
        for (int s = 0; s < 16; s++) {
            g_tlutGroup[s] = -1;
            g_tlutUsed[s] = 0;
        }
        for (int k = 0; k <= kTmemSlots; k++) { g_tmemTex[k] = nullptr; g_tmemUsed[k] = 0; }
        for (int g = 0; g < 0xC0; g++) g_groupSlot[g] = -1;
        g_lastSlot = -1;
        g_lastPalette = -1;

        // So a area do CPS-2 (384x224): o que passa dela nao entra nas margens
        rdpq_set_scissor(0, kTop, kW, kTop + kH);

        // Fundo: a cor 0xBFF
        const uint16_t bd = g_tlut[0xBFF] | 1;
        rdpq_set_mode_fill(color_from_packed16(bd));
        rdpq_fill_rectangle(0, kTop, kW, kTop + kH);

        rdpq_set_mode_standard();
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_mode_alphacompare(1);
        rdpq_mode_filter(FILTER_POINT);

        // Ordem das camadas e mascaras de prioridade (RenderLineAs)
        const uint16_t control = v.cpsB[0x26 / 2]; // (o controle de camadas do CPS-B da placa)
        const uint16_t videoControl = v.cpsA[kVideoControl];
        unsigned l0 = (control >> 6) & 3, l1 = (control >> 8) & 3, l2 = (control >> 10) & 3, l3 = (control >> 12) & 3;
        unsigned p0 = (v.priorityControl >> (4 * l0)) & 0xF, p1 = (v.priorityControl >> (4 * l1)) & 0xF;
        unsigned p2 = (v.priorityControl >> (4 * l2)) & 0xF, p3 = (v.priorityControl >> (4 * l3)) & 0xF;
        if (l0 == 0) { l0 = l1; l1 = 0; p0 = p1; }
        if (l1 == 0) { l1 = l2; l2 = 0; p1 = p2; }
        if (l2 == 0) { l2 = l3; l3 = 0; p2 = p3; }
        uint32_t masks[8] = {};
        {
            uint32_t mask0 = 0xAA, mask1 = 0xCC;
            if (p0 > p1) mask0 &= ~0x88u;
            if (p0 > p2) mask0 &= ~0xA0u;
            if (p1 > p2) mask1 &= ~0xC0u;
            masks[0] = 0xFF;
            for (unsigned n = 1; n < 8; n++) {
                if (n <= p0 && n <= p1 && n <= p2) { masks[n] = 0xFE; continue; }
                masks[n] = 0;
                if (n <= p0) masks[n] |= mask0;
                if (n <= p1) masks[n] |= mask1;
                if (n <= p2) masks[n] |= 0xF0;
            }
        }
        auto enabled = [&](unsigned layer) {
            switch (layer) {
            case 1: return (control & 0x02) != 0;
            case 2: return (control & 0x04) != 0 && (videoControl & 0x04) != 0;
            case 3: return (control & 0x08) != 0 && (videoControl & 0x08) != 0;
            default: return false;
            }
        };
        const unsigned order[3] = {l0, l1, l2};

        // Objetos: uma passada, em 4 filas pela quantidade de camadas que ficam atras deles
        static Obj queue[16384];
        unsigned queued=0;
        {
            const uint16_t *regs = v.objectRegs;
            const int xOffset = 64 - regs[kObjXOffset];
            const int yOffset = 16 - regs[kObjYOffset];
            for (int i = 0; i <= v.lastObject; i += 4) {
                int x = v.objects[i];
                int y = v.objects[i + 1];
                const unsigned level = (x >> 13) & 7;
                const uint32_t mask = masks[level];
                // Invisible front objects still claim pixels, as the reference hardware rules require.
                int depth = 0;
                while (depth < 3 && !((mask >> (1u << depth)) & 1)) {
                    depth++;
                }
                const uint32_t code = v.objects[i + 2] + ((y & 0x6000u) << 3);
                const uint16_t attr = v.objects[i + 3];
                if (attr & 0x80) {
                    x += regs[kObjXOffset];
                    y += regs[kObjYOffset];
                }
                const int across = ((attr >> 8) & 0x0F) + 1, down = ((attr >> 12) & 0x0F) + 1;
                const bool flipX = attr & 0x20, flipY = attr & 0x40;
                for (int ty = 0; ty < down; ty++) {
                    const int sy = ((y + ty * 16 + yOffset) & 0x3FF) - kFirstLine;
                    if (sy <= -16 || sy >= kH) continue;
                    for (int tx = 0; tx < across; tx++) {
                        const int sx = ((x + tx * 16 + xOffset) & 0x3FF) - kFirstColumn;
                        if (sx <= -16 || sx >= kW) continue;
                        uint32_t tile;
                        if (across == 1 && down == 1) {
                            tile = code;
                        } else {
                            const int cx = flipX ? across - 1 - tx : tx;
                            const int cy = flipY ? down - 1 - ty : ty;
                            tile = (!flipX && !flipY) ? (code & ~0xFu) + ((code + cx) & 0xF) + 0x10 * cy : code + cx + 0x10 * cy;
                        }
                        const uint32_t at = tile * 128;
                        if (at + 128 > g_gfxSize) continue;
                        assertf(queued<16384,"Object tile list capacity exceeded");
                        queue[queued++] = {static_cast<int16_t>(sx), static_cast<int16_t>(sy), at,mask,
                                                         static_cast<uint8_t>(attr & 0x1F), flipX, flipY,static_cast<uint8_t>(depth)};
                    }
                }
            }
        }
        // Legacy depth buckets exist only to measure their cost; the accepted path retains exact ownership.
        if(!ddsom::g_options.accurateVideo){
            g_priorityMark=0;
            for(unsigned depth=0;depth<4;depth++){
                for(unsigned n=0;n<queued;n++){
                    const Obj&o=queue[n];if(o.depth!=depth||o.mask==255||Empty(o.at,128,-1))continue;
                    const uint8_t*tex=Fetch(g_pool16,o.at);
                    Blit(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);stats.objects++;
                }
                if(depth<3&&order[depth]&&enabled(order[depth]))Scroll(v,int(order[depth])-1);
            }
            stats.sceneUs+=get_ticks_us()-t0;return;
        }
        if(!queued){
            g_priorityMark=0;
            for(unsigned depth=0;depth<3;depth++)if(order[depth]&&enabled(order[depth]))Scroll(v,int(order[depth])-1);
            stats.sceneUs+=get_ticks_us()-t0;return;
        }
        memset(g_claimed,0,sizeof g_claimed);memset(g_spriteCells,0,sizeof g_spriteCells);
        // Only the CPU reads these masks. Allocate for the high-water mark of
        // visible tiles and use the exact renderer if the heap cannot grow.
        static uint16_t *owned = nullptr;
        static unsigned ownedTiles = 0;
        bool fast = ddsom::g_options.videoFastPaths && queued <= 4096;
        if (fast && queued > ownedTiles) {
            auto *grown = static_cast<uint16_t *>(std::realloc(owned, queued * 16 * sizeof(*owned)));
            if (grown) {
                owned = grown;
                ownedTiles = queued;
            } else {
                fast = false;
            }
        }
        bool needBgMask = false;
        for(unsigned n=0;n<queued;n++){
            const Obj&o=queue[n];if(o.mask==0||o.mask==255||(fast&&HardwareDepth(o.mask)!=4))continue;
            needBgMask=true;
            for(int gy=(o.y<0?0:o.y)/8;gy<((o.y+16>kH?kH:o.y+16)+7)/8;gy++)
                for(int gx=(o.x<0?0:o.x)/8;gx<((o.x+16>kW?kW:o.x+16)+7)/8;gx++)g_spriteCells[gy*(kW/8)+gx]=1;
        }
        if(needBgMask)memset(g_priority,0,sizeof g_priority);
        if(fast){
            // Resolve sprite ownership once. Ordinary priorities are then composed by RDP layer order;
            // only non-monotonic masks require background opacity tests on the CPU.
            for(unsigned n=queued;n>0;n--){
                const Obj&o=queue[n-1];auto*rows=owned+(n-1)*16;memset(rows,0,32);
                if(Empty(o.at,128,-1))continue;
                const uint8_t*tex=Fetch(g_pool16,o.at);
                const uint16_t*bits=Opacity16(g_pool16,tex);
                int left=o.x<0?0:o.x,right=o.x+16>kW?kW:o.x+16;
                unsigned offset=left-o.x,limit=(1u<<(right-left))-1;
                for(int dy=0;dy<16;dy++){
                    int y=o.y+dy;if(y<0||y>=kH)continue;
                    unsigned opacity=bits[o.flipY?15-dy:dy];if(o.flipX)opacity=Reverse16(opacity);
                    opacity=(opacity>>offset)&limit;
                    rows[dy]=(opacity&~RowBits(g_claimed[y],left))<<offset;
                    Claim(g_claimed[y],left,opacity);
                }
            }
            unsigned packet=0;
            auto draw=[&](unsigned index,bool irregular){
                const Obj&o=queue[index];if(o.mask==255)return;
                const uint16_t*rows=owned+index*16;unsigned visibleRows=0;for(unsigned dy=0;dy<16;dy++)visibleRows|=rows[dy];if(!visibleRows)return;
                const uint8_t*tex=Fetch(g_pool16,o.at);const uint16_t*bits=Opacity16(g_pool16,tex);
                if(packet==256){FlushCaches();packet=0;}
                auto*out=g_resolved[packet];memset(out,255,128);bool any=false,unchanged=true;
                int left=o.x<0?0:o.x,right=o.x+16>kW?kW:o.x+16;
                unsigned offset=left-o.x,limit=(1u<<(right-left))-1;
                for(int dy=0;dy<16;dy++){
                    int y=o.y+dy;if(y<0||y>=kH)continue;
                    auto*src=tex+(o.flipY?15-dy:dy)*8;
                    unsigned visible=rows[dy]>>offset;
                    if(irregular){
                        unsigned a=RowBits(g_priority[0][y],left),b=RowBits(g_priority[1][y],left),c=RowBits(g_priority[2][y],left),hidden=0;
                        for(unsigned p=0;p<8;p++)if(o.mask&(1u<<p))hidden|=(p&1?a:~a)&(p&2?b:~b)&(p&4?c:~c);
                        visible&=~hidden;
                    }
                    unsigned opacity=bits[o.flipY?15-dy:dy];if(o.flipX)opacity=Reverse16(opacity);
                    if(visible!=((opacity>>offset)&limit))unchanged=false;
                    if(!visible)continue;
                    ResolvedRow(out+dy*8,src,visible<<offset,o.flipX);any=true;
                }
                stats.objects++;
                if(!any)return;
                if(unchanged)Blit(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);
                else{data_cache_hit_writeback(out,128);Blit(out,16,16,8,o.group,o.x,o.y,16,16,0,0,false,false);packet++;}
            };
            for(unsigned depth=0;depth<4;depth++){
                g_priorityMark=0;
                for(unsigned n=queued;n>0;n--)if(HardwareDepth(queue[n-1].mask)==depth)draw(n-1,false);
                if(depth<3&&order[depth]&&enabled(order[depth])){g_priorityMark=needBgMask?1u<<depth:0;Scroll(v,int(order[depth])-1);}
            }
            g_priorityMark=0;
            for(unsigned n=queued;n>0;n--)if(HardwareDepth(queue[n-1].mask)==4)draw(n-1,true);
            stats.sceneUs+=get_ticks_us()-t0;return;
        }
        // Draw the backgrounds and retain their opacity/priority only where sprites can touch them.
        for(int n=0;n<3;n++)if(order[n]&&enabled(order[n])){
            g_priorityMark=1u<<n;Scroll(v,int(order[n])-1);
        }
        g_priorityMark=0;
        // CPS-2 sprite ownership is independent of background priority: later objects claim first,
        // even when their own pixel is hidden behind a background layer.
        unsigned packet=0;
        for(unsigned n=queued;n>0;n--){
            const Obj&o=queue[n-1];stats.objects++;if(Empty(o.at,128,-1))continue;
            const uint8_t*tex=Fetch(g_pool16,o.at);
            if(packet==256){FlushCaches();packet=0;}
            uint8_t*out=g_resolved[packet];memset(out,255,128);bool any=false,unchanged=true;
            int left=o.x<0?0:o.x,right=o.x+16>kW?kW:o.x+16;
            unsigned offset=left-o.x,limit=(1u<<(right-left))-1;
            for(int dy=0;dy<16;dy++){
                int y=o.y+dy;if(y<0||y>=kH)continue;
                const uint8_t*src=tex+(o.flipY?15-dy:dy)*8;
                unsigned opaque=Opaque8(src+(o.flipX?4:0),o.flipX)|(Opaque8(src+(o.flipX?0:4),o.flipX)<<8);
                opaque=(opaque>>offset)&limit;
                unsigned visible=opaque&~RowBits(g_claimed[y],left);
                Claim(g_claimed[y],left,opaque);
                if(visible!=opaque)unchanged=false;
                if(!visible)continue;
                if(!ddsom::g_options.videoFastPaths||o.mask){
                    if(ddsom::g_options.videoFastPaths&&o.mask==255){unchanged=false;continue;}
                    unsigned a=RowBits(g_priority[0][y],left),b=RowBits(g_priority[1][y],left),c=RowBits(g_priority[2][y],left),hidden=0;
                    for(unsigned p=0;p<8;p++)if(o.mask&(1u<<p))hidden|=(p&1?a:~a)&(p&2?b:~b)&(p&4?c:~c);
                    if(visible&hidden)unchanged=false;
                    visible&=~hidden;if(!visible)continue;
                }
                ResolvedRow(out+dy*8,src,visible<<offset,o.flipX);any=true;
            }
            if(any){
                if(ddsom::g_options.videoFastPaths&&unchanged&&o.mask==0)Blit(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);
                else{data_cache_hit_writeback(out,128);Blit(out,16,16,8,o.group,o.x,o.y,16,16,0,0,false,false);packet++;}
            }
        }
        stats.sceneUs += get_ticks_us() - t0;
    }
};

void RdpVideoInit(uint32_t gfxPi, size_t gfxSize, const uint8_t *empty) {
    g_gfxPi = gfxPi;
    g_gfxSize = gfxSize;
    g_empty = empty;
    g_pool8.Init(64, 1024);
    g_pool16.Init(128, 2048);
    g_pool32.Init(512, 256);
    g_tlut = static_cast<uint16_t *>(memalign(16, 0xC00 * 2)); // (escrito pela CPU: writeback antes do RDP)
}

void RdpVideoSetTest(bool tlutMap, bool tmemCache) {
    g_tlutMap = tlutMap;
    g_tmemCache = tmemCache;
}

void RdpVideoDraw(NativeBoard &video, Stats &stats, void*target) {
    g_target=static_cast<surface_t*>(target);
    N64Video::Draw(video, stats);
}

} // namespace cps2recomp


