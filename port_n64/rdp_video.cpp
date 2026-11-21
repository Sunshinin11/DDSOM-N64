// Shadow over Mystara N64: desenha o estado do video CPS-2 com o RDP (camadas de scroll e objetos como retangulos
// texturizados CI4), lendo direto a placa nativa (board.h): os mapas de tiles, a lista de objetos e a paleta que o
// jogo montou. As regras de camadas, prioridade e enderecos sao as do hardware (as mesmas de video/video.cpp).
#include <libdragon.h>

#include "rdp_video.h"
#include "board.h"
#include "run_options.h"
#define DDSOM_NO_ACCESS_MACROS // (tem um W16 proprio; nao le memoria do 68000)
#include "native.h"
#ifdef CPS2RECOMP_N64
#include "audio.h"
#endif

#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <array>

#ifdef CPS2RECOMP_N64
DEFINE_RSP_UCODE(rsp_tiles); // (port_n64/rsp_tiles.S)
extern "C" void __rdpq_autosync_use(uint32_t res); // (libdragon rdpq.c: marca recursos do RDP como em uso)
#endif

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
            if (b == 128) {
                opacity = new uint16_t[n * 16];
                opacityValid = new uint8_t[n]();
            }
        }
    };
    Pool g_pool8, g_pool16, g_pool32;
    uint32_t g_gfxPi = 0;       // endereco PI dos graficos no cartucho
    size_t g_gfxSize = 0;
    const uint8_t *g_empty = nullptr; // 1 bit por meio bloco de 64 bytes
    inline uint8_t EmptyByte(unsigned at) { return g_empty[at]; }
    uint32_t g_frame = 1;
    Stats *g_stats = nullptr;
    surface_t *g_target=nullptr;
    int g_top = kTop; // first screen line of the target being drawn (0 while a plane is drawn into its cache)
    // Cache of the scroll planes 0 and 1 (CPS-2 tile layers): a plane whose inputs did not change since the last frame
    // is drawn once into an RGBA16 surface and then put on the screen with a few copy-mode strips instead of hundreds
    // of tile rectangles. A plane that moves (scroll, palette, map) is drawn directly, as before.
    struct PlaneCache {
        surface_t surf = {};
#ifdef CPS2RECOMP_N64
        rspq_block_t *blitBlock = nullptr; // (a copia da superficie para a tela, gravada uma vez)
#endif
        bool allocated = false, failed = false, valid = false;
        uint32_t regsKey = 0, mapHash = 0;
    };
    PlaneCache g_pc[2];
    uint32_t g_planePalVersion[2] = {};
    bool g_planeCacheOn = true;
    uint32_t g_pcHits = 0, g_pcRenders = 0, g_pcDirect = 0, g_pcFrames = 0;
    uint32_t g_priority[3][kH][kW/32];
    uint32_t g_claimed[kH][kW/32];
    uint32_t g_rowGen[kH], g_rowGenNow = 0; // linha y de g_claimed/g_priority ja zerada neste quadro quando g_rowGen[y]==g_rowGenNow
    uint8_t g_spriteCells[(kW/8)*(kH/8)];
    unsigned g_priorityMark=0;
    constexpr unsigned kResolvedSlots = 128;
    alignas(16) uint8_t g_resolved[kResolvedSlots][128];
    void FlushCaches();
    // Eight CI4 pens -> eight opacity bits. Bit zero is the leftmost screen pixel.
    unsigned Opaque8(const uint8_t*p,bool flip){
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        typedef uint32_t WordAlias __attribute__((may_alias, aligned(1)));
        uint32_t v = ~(*reinterpret_cast<const WordAlias*>(p));
#else
        uint32_t v=~(uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]);
#endif
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
    template <bool crossesWord>
    inline unsigned ClaimAndGetHidden(uint32_t *row, unsigned shift, unsigned opacity) {
        const uint32_t first = row[0];
        row[0] = first | (opacity << shift);
        unsigned occupied = first >> shift;
        if constexpr (crossesWord) {
            const unsigned remaining = 32 - shift;
            const uint32_t second = row[1];
            row[1] = second | (opacity >> remaining);
            occupied |= second << remaining;
        }
        return opacity & occupied;
    }
    unsigned BackgroundHidden(unsigned mask, unsigned a, unsigned b, unsigned c) {
        switch (mask) {
        case 0x00: return 0;
        case 0x02: return a & ~b & ~c;
        case 0x0C: return b & ~c;
        case 0x0E: return (a | b) & ~c;
        case 0xF0: return c;
        case 0xF2: return c | (a & ~b);
        case 0xFC: return b | c;
        case 0xFE: return a | b | c;
        case 0xFF: return ~0u;
        default:
            unsigned hidden = 0;
            for (unsigned p = 0; p < 8; ++p) {
                if (mask & (1u << p))
                    hidden |= (p & 1 ? a : ~a) & (p & 2 ? b : ~b) & (p & 4 ? c : ~c);
            }
            return hidden;
        }
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
            uint32_t v;
            if(flip){
                v=uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0];
                v=((v>>4)&0x0f0f0f0f)|((v&0x0f0f0f0f)<<4);
            }else{
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint32_t Word32 __attribute__((may_alias, aligned(4)));
                v=*reinterpret_cast<const Word32*>(p);
#else
                v=uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
#endif
            }
            v|=~g_penMasks[(visible>>(half*8))&255];
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint32_t Word32 __attribute__((may_alias, aligned(4)));
            *reinterpret_cast<Word32*>(out+half*4)=v;
#else
            out[half*4]=v>>24;out[half*4+1]=v>>16;out[half*4+2]=v>>8;out[half*4+3]=v;
#endif
        }
    }

    // Paletas: as 0xC00 cores em RGBA5551, a pen 15 de cada grupo transparente
    uint16_t *g_tlut = nullptr;
    int g_tlutGroup[16];
    bool g_tlutMap = true, g_tmemCache = true; // (teste: L e R alternam)
    bool g_hideSprites = false, g_hidePlanes = false; // (painel de medicao no console)
    uint32_t g_tlutUsed[16];
    uint32_t g_tlutTick = 0;

    __attribute__((always_inline)) inline bool Empty(uint32_t at, int bytes, int half) {
        // Each size is aligned to its own size in the cartridge graphics.
        const unsigned byte = at >> 8;
        const unsigned flags = EmptyByte(byte);
        const unsigned shift = (at >> 5) & 7;
        if (bytes == 64) return (flags >> (shift + half)) & 1;
        if (bytes == 128) return ((flags >> shift) & 15) == 15;
        return flags == 0xFF && EmptyByte(byte + 1) == 0xFF;
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
                if (pool.opacityValid) pool.opacityValid[s] = 0;
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

    // O mesmo tile de novo no quadro: se a posicao guardada ainda e dele, so a marca como usada (sem a busca do Fetch)
    inline const uint8_t *Refetch(Pool &pool, uint32_t at, const uint8_t *tex) {
        if (tex) {
            const unsigned s = unsigned((tex - pool.data) / pool.bytes);
            if (pool.tag[s] == at + 1) { pool.used[s] = g_frame; return tex; }
        }
        return Fetch(pool, at);
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
    uint8_t g_tmemHead[256], g_tmemNext[kTmemSlots];
    inline unsigned TmemHashOf(const uint8_t *t) { return (static_cast<unsigned>(reinterpret_cast<uintptr_t>(t)) >> 6) & 255u; }
    void TmemUnlink(int slot) { // remove the tile that occupies the slot from its hash chain
        const uint8_t *old = g_tmemTex[slot];
        if (!old) return;
        uint8_t *link = &g_tmemHead[TmemHashOf(old)];
        while (*link) {
            if (*link == slot + 1) { *link = g_tmemNext[slot]; break; }
            link = &g_tmemNext[*link - 1];
        }
        g_tmemTex[slot] = nullptr;
    }
    int g_lastSlot = -1, g_lastPalette = -1;
    int g_lastWidth = 0, g_lastHeight = 0, g_lastStride = 0;

    void ResetGpuState(){
        rdpq_set_mode_standard();rdpq_mode_tlut(TLUT_RGBA16);rdpq_mode_alphacompare(1);rdpq_mode_filter(FILTER_POINT);
        rdpq_set_scissor(0,g_top,kW,g_top+kH);
        for(int s=0;s<16;s++){g_tlutGroup[s]=-1;g_tlutUsed[s]=0;}
        for(int k=0;k<=kTmemSlots;k++){g_tmemTex[k]=nullptr;g_tmemUsed[k]=0;}
        for(int k=0;k<256;k++)g_tmemHead[k]=0;
        for(int g=0;g<0xc0;g++)g_groupSlot[g]=-1;
        g_lastSlot=g_lastPalette=-1;
        g_lastWidth=g_lastHeight=g_lastStride=0;
    }
    void ListFlush();
    void FlushCaches(){
        assertf(g_target, "Missing render target");
        ListFlush(); // (os tiles da lista leem posicoes dos depositos: vao antes de esperar o RDP e reciclar)
        const uint32_t start = get_ticks_us();
        rdpq_detach_wait();
        g_stats->cacheWaitUs += get_ticks_us() - start;
        ++g_stats->cacheWaits;
        rdpq_attach(g_target, nullptr);
        g_frame++;ResetGpuState();
    }
    // A parte de CPU do Blit que nao gera comandos: as mascaras de prioridade dos cenarios (so com g_priorityMark)
    void PriorityBits(const uint8_t *tex, int stride, int x, int y, int w, int h, int s0, int t0, bool flipX, bool flipY);
    void BlitCommands(const uint8_t *tex, int tw, int th, int stride, int group, int x, int y, int w, int h, int s0, int t0,
                      bool flipX, bool flipY);
    void Blit(const uint8_t *tex, int tw, int th, int stride, int group, int x, int y, int w, int h, int s0, int t0,
              bool flipX, bool flipY) {
#ifdef CPS2RECOMP_N64
        static unsigned service=0;if((++service&31)==0)ddsom::AudioPoll();
#endif
        PriorityBits(tex, stride, x, y, w, h, s0, t0, flipX, flipY);
        BlitCommands(tex, tw, th, stride, group, x, y, w, h, s0, t0, flipX, flipY);
    }
    void PriorityBits(const uint8_t *tex, int stride, int x, int y, int w, int h, int s0, int t0, bool flipX, bool flipY) {
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
    }
    void BlitCommands(const uint8_t *tex, int tw, int th, int stride, int group, int x, int y, int w, int h, int s0, int t0,
                      bool flipX, bool flipY) {
        const int palette = TlutSlot(group);
        g_tmemTick++;
        int slot;
        if (!g_tmemCache) {
            slot = 0; // (uma posicao so: recarrega sempre que o tile muda, como antes)
        } else if (tw == 32) {
            slot = kTmemSlots; // (a posicao de 512 bytes)
        } else {
            const unsigned h8 = TmemHashOf(tex);
            slot = -1;
            if (g_lastSlot >= 0 && g_lastSlot < kTmemSlots && g_tmemTex[g_lastSlot] == tex) {
                slot = g_lastSlot; // (the tile of the last rectangle again: no search)
            } else {
                for (unsigned e = g_tmemHead[h8]; e; e = g_tmemNext[e - 1]) {
                    if (g_tmemTex[e - 1] == tex) { slot = static_cast<int>(e) - 1; break; }
                }
            }
            if (slot < 0) { // real miss: the least recently used slot is evicted
                slot = 0;
                for (int k = 1; k < kTmemSlots; k++) if (g_tmemUsed[k] < g_tmemUsed[slot]) slot = k;
                TmemUnlink(slot);
            }
        }
        const int tmem = slot * 128;
        if (g_tmemTex[slot] != tex) {
            if (slot < kTmemSlots) TmemUnlink(slot);
            const int bytes = stride * th;
            rdpq_set_texture_image_raw(0, PhysicalAddr(tex), FMT_RGBA16, bytes / 2, 1);
            rdpq_set_tile(TILE1, FMT_RGBA16, tmem, 0, NULL);
            rdpq_load_block(TILE1, 0, 0, bytes / 2, stride);
            g_tmemTex[slot] = tex;
            if (slot < kTmemSlots) {
                uint8_t &head = g_tmemHead[TmemHashOf(tex)];
                g_tmemNext[slot] = head;
                head = static_cast<uint8_t>(slot + 1);
            }
            g_stats->uploads++;
        }
        g_tmemUsed[slot] = g_tmemTick;
        if (slot != g_lastSlot || palette != g_lastPalette || stride != g_lastStride) {
            rdpq_tileparms_t parms = {};
            parms.palette = static_cast<uint8_t>(palette);
            rdpq_set_tile(TILE0, FMT_CI4, tmem, stride, &parms);
            g_lastSlot = slot;
            g_lastPalette = palette;
            g_lastStride = stride;
            g_stats->descriptors++;
        }
        if (tw != g_lastWidth || th != g_lastHeight) {
            rdpq_set_tile_size(TILE0, 0, 0, tw, th);
            g_lastWidth = tw;
            g_lastHeight = th;
        }
        // Mesmo resultado de rdpq_texture_rectangle_scaled: os incrementos sao sempre +-1.0 (tile 1:1), sem divisoes nem float.
        // (verificado contra a versao do libdragon em 5 milhoes de casos: palavras de comando identicas)
        int32_t X0 = x * 4, Y0 = (y + g_top) * 4, X1 = (x + w) * 4, Y1 = (y + g_top + h) * 4;
        int32_t S = (flipX ? s0 + w : s0) * 32, T = (flipY ? t0 + h : t0) * 32;
        const int32_t dsdx = flipX ? -1024 : 1024, dtdy = flipY ? -1024 : 1024;
        if (w > 0 && h > 0) {
            if (X0 < 0) { S -= X0 * (dsdx >> 7); X0 = 0; }
            if (Y0 < 0) { T -= Y0 * (dtdy >> 7); Y0 = 0; }
            if (X0 < X1 && Y0 < Y1) {
                if (X1 > 4095 || Y1 > 4095) {
                    rdpq_texture_rectangle_scaled(TILE0, x, y + g_top, x + w, y + g_top + h, flipX ? s0 + w : s0, flipY ? t0 + h : t0, flipX ? s0 : s0 + w, flipY ? t0 : t0 + h);
                } else {
                    __rdpq_texture_rectangle_raw_fx(TILE0, X0, Y0, X1, Y1, S, T, dsdx, dtdy);
                }
            }
        }
        g_stats->rects++;
    }

    // --- Tiles desenhados pelo RSP (rsp_tiles.S) ---
    // A CPU so escreve 16 bytes por tile numa lista; o microcodigo escolhe as posicoes de paleta e de textura na TMEM e
    // gera os comandos que BlitCommands gerava (mesmas escolhas, mesmas sincronizacoes). A lista de um quadro so e
    // reaproveitada no quadro seguinte, quando o RDP (e portanto o RSP) ja terminou o anterior (main.cpp).
    uint32_t g_tilesOvl = 0;
    bool g_rspTiles = true;
    constexpr unsigned kListCap = 2048;
    uint32_t *g_list = nullptr;
    unsigned g_listUsed = 0, g_listStart = 0;
    void ForgetTmem() { // a CPU nao sabe mais o que ha na TMEM nem nas paletas
        for (int s = 0; s < 16; s++) { g_tlutGroup[s] = -1; g_tlutUsed[s] = 0; }
        for (int k = 0; k <= kTmemSlots; k++) { g_tmemTex[k] = nullptr; g_tmemUsed[k] = 0; }
        for (int k = 0; k < 256; k++) g_tmemHead[k] = 0;
        for (int g = 0; g < 0xC0; g++) g_groupSlot[g] = -1;
        g_lastSlot = g_lastPalette = -1;
        g_lastWidth = g_lastHeight = g_lastStride = 0;
    }
    void ListFlush() {
        if (g_listUsed == g_listStart) return;
#ifdef CPS2RECOMP_N64
        uint32_t *first = g_list + g_listStart * 4;
        const unsigned n = g_listUsed - g_listStart;
        data_cache_hit_writeback(first, n * 16);
        rspq_write(g_tilesOvl, 0, n, PhysicalAddr(first), PhysicalAddr(g_tlut));
        // os comandos do RSP usaram tiles, TMEM e o pipe: os proximos do rdpq sincronizam como se fossem dele
        __rdpq_autosync_use(AUTOSYNC_PIPE | AUTOSYNC_TILES | AUTOSYNC_TMEMS);
#endif
        g_listStart = g_listUsed;
        ForgetTmem();
    }
    void BlitList(const uint8_t *tex, int tw, int th, int stride, int group, int x, int y, int w, int h, int s0, int t0,
                  bool flipX, bool flipY) {
#ifdef CPS2RECOMP_N64
        static unsigned service=0;if((++service&31)==0)ddsom::AudioPoll();
#endif
        if (g_priorityMark) PriorityBits(tex, stride, x, y, w, h, s0, t0, flipX, flipY);
        const int code =(tw == 16 && th == 8 && stride == 8) ? 0 : (tw == 16 && th == 16 && stride == 8) ? 1
                       : (tw == 32 && th == 32 && stride == 16) ? 2 : -1;
        int32_t X0 = x * 4, Y0 = (y + g_top) * 4, X1 = (x + w) * 4, Y1 = (y + g_top + h) * 4;
        int32_t S = (flipX ? s0 + w : s0) * 32, T = (flipY ? t0 + h : t0) * 32;
        const int32_t dsdx = flipX ? -1024 : 1024, dtdy = flipY ? -1024 : 1024;
        if (w > 0 && h > 0) {
            if (X0 < 0) { S -= X0 * (dsdx >> 7); X0 = 0; }
            if (Y0 < 0) { T -= Y0 * (dtdy >> 7); Y0 = 0; }
        }
        if (!g_rspTiles || !g_list || code < 0 || g_listUsed >= kListCap || X1 > 4095 || Y1 > 4095 ||
            group < 0 || group >= 0xC0) {
            ListFlush(); // (a ordem dos comandos se mantem: o que ja esta na lista vai antes)
            BlitCommands(tex, tw, th, stride, group, x, y, w, h, s0, t0, flipX, flipY);
            return;
        }
        g_stats->rects++;
        if (!(w > 0 && h > 0 && X0 < X1 && Y0 < Y1)) return; // (nada a desenhar)
#ifdef CPS2RECOMP_N64
        uint32_t *e = g_list + g_listUsed * 4;
        e[0] = uint32_t(group) << 24 | uint32_t(X1 & 0xFFF) << 12 | uint32_t(Y1 & 0xFFF);
        e[1] = uint32_t(code) << 30 | (flipY ? 1u << 25 : 0) | (flipX ? 1u << 24 : 0) | uint32_t(X0 & 0xFFF) << 12 | uint32_t(Y0 & 0xFFF);
        e[2] = uint32_t(S & 0xFFFF) << 16 | uint32_t(T & 0xFFFF);
        e[3] = PhysicalAddr(tex);
        g_listUsed++;
#endif
    }
} // namespace

struct N64Video {
    static void Scroll(NativeBoard &v, int which) {
        struct BackgroundTiming {
            const uint32_t start = get_ticks_us();
            ~BackgroundTiming() { g_stats->backgroundUs += get_ticks_us() - start; }
        } timing;
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
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint32_t Word32 __attribute__((may_alias, aligned(4)));
                const uint32_t entry = *reinterpret_cast<const Word32 *>(&map[index * 4]);
                uint32_t code = entry >> 16;
                const uint16_t attr = static_cast<uint16_t>(entry);
#else
                uint32_t code = W16(&map[index * 4]);
                const uint16_t attr = W16(&map[index * 4 + 2]);
#endif
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
                        BlitList(tex, 16, 8, 8, group, out, ly, 8, band, half * 8, t0, flipX, flipY);
                    }
                } else if (which == 1) {
                    const uint32_t at = tiles + code * 128;
                    if (at + 128 > g_gfxSize || Empty(at, 128, -1)) {
                        continue;
                    }
                    if (const uint8_t *tex = Fetch(g_pool16, at)) {
                        BlitList(tex, 16, 16, 8, group, out, ly, 16, band, 0, t0, flipX, flipY);
                    }
                } else {
                    code &= 0x3FFF;
                    const uint32_t at = tiles + code * 512;
                    if (at + 512 > g_gfxSize || Empty(at, 512, -1)) {
                        continue;
                    }
                    if (const uint8_t *tex = Fetch(g_pool32, at)) {
                        BlitList(tex, 32, 32, 16, group, out, ly, 32, band, 0, t0, flipX, flipY);
                    }
                }
            }
            ly += band;
        }
        ListFlush(); // (o plano inteiro num comando do RSP; antes de qualquer outro comando do rdpq)
    }

    // Draws scroll plane `which` (0..2) at its place in the layer order, through the cache when it can.
    static void DrawPlane(NativeBoard &v, int which) {
        ListFlush(); // (os sprites ja na lista vao antes; o cache do plano usa comandos do rdpq)
        if (which > 1 || !g_planeCacheOn || g_priorityMark || (g_pc[which].failed)) {
            // (g_priorityMark: sprites with irregular priorities need this plane's opacity bits, only Scroll writes them)
            g_pcDirect++;
            Scroll(v, which);
            return;
        }
        PlaneCache &pc = g_pc[which];
        const uint16_t *a = v.cpsA;
        const uint8_t *vram = v.videoRam;
        const uint32_t base = (static_cast<uint32_t>(a[kScroll1Base + which]) * 256) & ~0x3FFFu & 0x3FFFF;
        uint32_t key = 2166136261u;
        auto mix = [&](uint32_t w) { key = (key ^ w) * 16777619u; };
        mix(a[kScroll1X + which * 2]); mix(a[kScroll1X + which * 2 + 1]); mix(base); mix(g_planePalVersion[which]);
        const bool rowScroll = which == 1 && (a[kVideoControl] & 1);
        mix(rowScroll ? 1u : 0u);
        if (rowScroll) {
            const uint32_t other = (static_cast<uint32_t>(a[kOtherBase]) * 256) & ~0x7FFu & 0x3FFFF;
            mix(a[kRowScrollOffset]);
            for (int ly = 0; ly < kH; ly++) {
                mix(W16(&vram[other + (((ly + kFirstLine + a[kRowScrollOffset]) & 0x3FF) * 2)]));
            }
        }
        if (key != pc.regsKey) { // moving (or the first frame): drawn directly
            pc.regsKey = key;
            pc.valid = false;
            g_pcDirect++;
            Scroll(v, which);
            return;
        }
        // The tile map is read only when the game wrote its 16 KB page since the last look, and even then a rewrite of
        // the same contents leaves the cache valid
        // Hash do mapa por blocos de 256 bytes: so os blocos que o jogo escreveu desde a ultima olhada sao relidos
        // (antes, qualquer escrita nos 16 KB fazia reler o mapa inteiro, todo quadro)
        uint32_t mh = pc.mapHash;
        {
            static uint32_t blockHash[2][64];
            static uint32_t hashedBase[2] = {UINT32_MAX, UINT32_MAX};
            const uint8_t bit = static_cast<uint8_t>(1u << which);
            uint8_t *dirty = &cps2recomp::native::g_videoDirty[base >> 8];
            const bool all = hashedBase[which] != base || !pc.valid;
            bool changed = all;
            for (unsigned b = 0; b < 64; ++b) {
                if (!all && !(dirty[b] & bit)) continue;
                dirty[b] &= static_cast<uint8_t>(~bit);
                typedef uint32_t Word32 __attribute__((may_alias, aligned(4)));
                const Word32 *w = reinterpret_cast<const Word32 *>(&vram[base + b * 256]);
                uint32_t h = 2166136261u;
                for (unsigned i = 0; i < 64; ++i) h = (h ^ w[i]) * 16777619u;
                if (h != blockHash[which][b]) { blockHash[which][b] = h; changed = true; }
            }
            hashedBase[which] = base;
            if (changed) {
                mh = 2166136261u;
                for (unsigned b = 0; b < 64; ++b) mh = (mh ^ blockHash[which][b]) * 16777619u;
            }
        }
        if (!pc.valid || pc.mapHash != mh) {
            if (!pc.allocated) {
                bool canAlloc = true;
#ifdef CPS2RECOMP_N64
                heap_stats_t heap;
                sys_get_heap_stats(&heap);
                canAlloc = heap.free > 0 && size_t(heap.free) >= size_t(kW * kH * 2) + 96 * 1024;
#endif
                if (canAlloc) pc.surf = surface_alloc(FMT_RGBA16, kW, kH);
                if (!canAlloc || !pc.surf.buffer) { pc.failed = true; g_pcDirect++; Scroll(v, which); return; }
                pc.allocated = true;
            }
            // Draw the plane alone into its surface (transparent background)
            surface_t *real = g_target;
            g_target = &pc.surf;
            g_top = 0;
            rdpq_attach(&pc.surf, nullptr);
            rdpq_clear(RGBA32(0, 0, 0, 0));
            ResetGpuState(); // g_frame so pode avancar quando o RDP ja terminou de ler os tiles
            Scroll(v, which);
            rdpq_detach();
            g_target = real;
            g_top = kTop;
            pc.valid = true;
            pc.mapHash = mh;
            g_pcRenders++;
        } else {
            g_pcHits++;
        }
        struct BackgroundTiming {
            const uint32_t start = get_ticks_us();
            ~BackgroundTiming() { const uint32_t d = get_ticks_us() - start; g_stats->backgroundUs += d; g_stats->cachedBlitUs += d; }
        } timing;
#ifdef CPS2RECOMP_N64
        // A copia do plano guardado e sempre a mesma (mesma superficie, mesmo lugar): gravada uma vez num bloco do rspq
        // e so reexecutada (montar os ~45 pedacos com rdpq_tex_blit custava ~0,85 ms de CPU por plano, todo quadro)
        if (!pc.blitBlock) {
            rspq_block_begin();
            rdpq_set_mode_copy(true);
            rdpq_set_scissor(0, kTop, kW, kTop + kH);
            rdpq_tex_blit(&pc.surf, 0, kTop, nullptr);
            pc.blitBlock = rspq_block_end();
        }
        rspq_block_run(pc.blitBlock);
#else
        rdpq_set_mode_copy(true);
        rdpq_set_scissor(0, kTop, kW, kTop + kH);
        rdpq_tex_blit(&pc.surf, 0, kTop, nullptr);
#endif
        ResetGpuState(); // g_frame so pode avancar quando o RDP ja terminou de ler os tiles
    }

    enum class Ownership : uint8_t { Hidden, Full, Covered, Partial };

    struct Obj {
        int16_t x, y;
        uint32_t at;
        uint8_t mask, group;
        bool flipX, flipY;
        uint8_t depth;
        Ownership ownership = Ownership::Hidden;
    };
    static_assert(sizeof(Obj)==16,"Object queue must not waste padding per tile");
    // Exact RDP insertion positions: these masks are ORs of all layers above that position.
    static unsigned HardwareDepth(uint32_t mask){
        switch(mask){case 0xfe:return 0;case 0xfc:return 1;case 0xf0:return 2;case 0:return 3;case 255:return 5;default:return 4;}
    }

    static void Draw(NativeBoard &v, Stats &stats) {
        g_stats = &stats;
        g_frame++;
        g_listUsed = g_listStart = 0; // (o quadro anterior ja foi desenhado: main.cpp so chega aqui com um buffer livre)
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
            bool planeChanged[2] = {};
            // so as paginas de 0x200 cores que a placa marcou como mudadas (todas na primeira vez)
            const unsigned pages = first ? 0x3Fu : v.palettePagesChanged;
            v.palettePagesChanged = 0;
            for (int n = 0; n < 0xC00; n++) {
                if (!(pages & (1u << (n >> 9)))) { n |= 0x1FF; continue; }
                const uint16_t c = v.palette[n];
                if (!first && c == converted[n]) continue;
                converted[n] = c;
                if (n >= 0x200 && n < 0x400) planeChanged[0] = true;
                else if (n >= 0x400 && n < 0x600) planeChanged[1] = true;
                if (n < firstChanged) firstChanged = n;
                lastChanged = n;
                const uint8_t *row = level[c >> 12];
                const uint32_t r = row[(c >> 8) & 0xF], g = row[(c >> 4) & 0xF], b = row[c & 0xF];
                g_tlut[n] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | ((n & 15) == 15 ? 0 : 1));
            }
            if (planeChanged[0]) g_planePalVersion[0]++;
            if (planeChanged[1]) g_planePalVersion[1]++;
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
        for (int k = 0; k < 256; k++) g_tmemHead[k] = 0; 
        for (int g = 0; g < 0xC0; g++) g_groupSlot[g] = -1;
        g_lastSlot = -1;
        g_lastPalette = -1;
        g_lastWidth = g_lastHeight = g_lastStride = 0;

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
            if (g_hidePlanes) return false;
            switch (layer) {
            case 1: return (control & 0x02) != 0;
            case 2: return (control & 0x04) != 0 && (videoControl & 0x04) != 0;
            case 3: return (control & 0x08) != 0 && (videoControl & 0x08) != 0;
            default: return false;
            }
        };
        const unsigned order[3] = {l0, l1, l2};

        // Objetos: uma passada, em 4 filas pela quantidade de camadas que ficam atras deles
#ifdef CPS2RECOMP_N64
        constexpr unsigned kQueueCap = 2048; // (tiles 16x16 de objetos por quadro: uma cena cheia usa poucas centenas)
#else
        constexpr unsigned kQueueCap = 16384;
#endif
        static Obj queue[kQueueCap];
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
                        if (at + 128 > g_gfxSize || Empty(at, 128, -1) || queued >= kQueueCap) continue;
                        queue[queued++] = {static_cast<int16_t>(sx), static_cast<int16_t>(sy), at,static_cast<uint8_t>(mask),
                                                         static_cast<uint8_t>(attr & 0x1F), flipX, flipY,static_cast<uint8_t>(depth)};
                    }
                }
            }
        }
        if (g_hideSprites) queued = 0;
        const uint32_t tQueued = get_ticks_us();
        stats.queueUs += tQueued - t0;
        stats.queued += queued;
        // Legacy depth buckets exist only to measure their cost; the accepted path retains exact ownership.
        if(!ddsom::g_options.accurateVideo){
            g_priorityMark=0;
            for(unsigned depth=0;depth<4;depth++){
                for(unsigned n=0;n<queued;n++){
                    const Obj&o=queue[n];if(o.depth!=depth||o.mask==255||Empty(o.at,128,-1))continue;
                    const uint8_t*tex=Fetch(g_pool16,o.at);
                    BlitList(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);stats.objects++;
                }
                if(depth<3&&order[depth]&&enabled(order[depth]))DrawPlane(v,int(order[depth])-1);
            }
            stats.sceneUs+=get_ticks_us()-t0;return;
        }
        unsigned uniformDepth = 4;
        if (queued && ddsom::g_options.videoFastPaths) {
            const uint8_t mask = queue[0].mask;
            const unsigned depth = HardwareDepth(mask);
            if (depth < 4) {
                unsigned n = 1;
                while (n < queued && queue[n].mask == mask) ++n;
                if (n == queued) uniformDepth = depth;
            }
        }
        if (!queued || uniformDepth < 4) {
            // At one ordinary depth, every sprite sees the same background
            // layers. Draw in CPS-2 order so the later opaque pixel wins.
            g_priorityMark = 0;
            const uint32_t bgU = stats.backgroundUs;
            struct SpriteTiming { Stats &s; uint32_t t, bg; ~SpriteTiming(){ s.spriteUs += (get_ticks_us() - t) - (s.backgroundUs - bg); } } timing{stats, get_ticks_us(), bgU};
            for (unsigned depth = 0; depth < 4; ++depth) {
                if (depth == uniformDepth) {
                    for (unsigned n = 0; n < queued; ++n) {
                        const Obj &o = queue[n];
                        const uint8_t *tex = Fetch(g_pool16, o.at);
                        BlitList(tex, 16, 16, 8, o.group, o.x, o.y, 16, 16, 0, 0, o.flipX, o.flipY);
                        ++stats.objects;
                    }
                }
                if (depth < 3 && order[depth] && enabled(order[depth])) {
                    DrawPlane(v, int(order[depth]) - 1);
                }
            }
            stats.sceneUs += get_ticks_us() - t0;
            return;
        }
        int minSpriteY = kH, maxSpriteY = 0;
        for (unsigned n = 0; n < queued; ++n) {
            const int y0 = queue[n].y < 0 ? 0 : queue[n].y;
            const int y1 = queue[n].y + 16 > kH ? kH : queue[n].y + 16;
            if (y0 < minSpriteY) minSpriteY = y0;
            if (y1 > maxSpriteY) maxSpriteY = y1;
        }
        // Only the CPU reads these masks. Allocate for the high-water mark of
        // visible tiles and use the exact renderer if the heap cannot grow.
        static uint16_t *owned = nullptr;
        static unsigned ownedTiles = 0;
        bool fast = ddsom::g_options.videoFastPaths && queued <= 4096;
        if (fast && queued > ownedTiles) {
            const size_t bytes = queued * 16 * sizeof(*owned);
            bool canGrow = true;
#ifdef CPS2RECOMP_N64
            heap_stats_t heap;
            sys_get_heap_stats(&heap);
            constexpr size_t audioReserve = 48 * 1024;
            canGrow = heap.free > 0 && size_t(heap.free) >= bytes + audioReserve;
#endif
            auto *grown = canGrow ? static_cast<uint16_t *>(std::realloc(owned, bytes)) : nullptr;
            if (grown) {
                owned = grown;
                ownedTiles = queued;
            } else {
                fast = false;
            }
        }
        if (!fast && minSpriteY < maxSpriteY) { // (o caminho exato abaixo le g_claimed de todos os tiles)
            memset(g_claimed[minSpriteY], 0, size_t(maxSpriteY - minSpriteY) * sizeof(g_claimed[0]));
        }
        stats.ownershipBytes = ownedTiles * 16 * sizeof(*owned);
        stats.ownershipFallback = ddsom::g_options.videoFastPaths && !fast;
        bool needBgMask = false;
        memset(g_spriteCells, 0, sizeof g_spriteCells);
        for(unsigned n=0;n<queued;n++){
            Obj &o = queue[n];
            if (fast) o.depth = HardwareDepth(o.mask);
            if (o.mask == 0 || o.mask == 255 || (fast && o.depth != 4)) continue;
            needBgMask=true;
            for(int gy=(o.y<0?0:o.y)/8;gy<((o.y+16>kH?kH:o.y+16)+7)/8;gy++)
                for(int gx=(o.x<0?0:o.x)/8;gx<((o.x+16>kW?kW:o.x+16)+7)/8;gx++)g_spriteCells[gy*(kW/8)+gx]=1;
        }
        if(fast){
            // Use g_priority[0..2] as temporary scratch for frontmost-claimer depth (b0, b1, valid)
            // during the back-to-front claim pass: if every occluded pixel of an ordinary sprite (d < 4)
            // was claimed by a frontmost ordinary sprite at depth >= d, that frontmost sprite will
            // unconditionally overwrite it on the RDP, avoiding software ResolvedRow + TMEM misses.
            // (as linhas de g_claimed e g_priority sao zeradas sob demanda no laco abaixo: g_rowGen)
            ++g_rowGenNow;
            // Quais tiles encostam em outro (caixas 16x16 com |dx|<16 e |dy|<16): so esses disputam pixels. Um tile
            // sozinho tem todos os seus pixels opacos visiveis e as marcas que ele deixaria nas mascaras nunca seriam
            // lidas por ninguem, entao pula o trabalho pixel a pixel abaixo (o resultado e o mesmo).
            static uint8_t touches[kQueueCap];
            static const uint8_t *texOf[kQueueCap]; // (o tile de cada objeto, achado na posse e reusado no desenho)
            const uint32_t tPrep = get_ticks_us();
            stats.ownPrepUs += tPrep - tQueued;
            {
                constexpr int kCellsX = (kW + 32) / 16, kCellsY = (kH + 32) / 16;
                static int16_t head[kCellsX * kCellsY];
                static int16_t next[kQueueCap];
                for (int c = 0; c < kCellsX * kCellsY; ++c) head[c] = -1;
                for (unsigned n = 0; n < queued; ++n) {
                    touches[n] = 0;
                    const int cx = (queue[n].x + 16) >> 4, cy = (queue[n].y + 16) >> 4;
                    if (cx < 0 || cy < 0 || cx >= kCellsX || cy >= kCellsY) { touches[n] = 1; continue; } // (fora da grade: caminho exato)
                    const int cell = cy * kCellsX + cx;
                    next[n] = head[cell]; head[cell] = int16_t(n);
                }
                // Grupos de tiles que se sobrepoem (uniao por pares). Num grupo em que todos estao na mesma profundidade
                // comum (d < 4), desenhar na ordem do CPS-2 ja da o pixel certo (o mesmo argumento do caminho uniforme
                // acima): a posse pixel a pixel so e necessaria nos grupos com profundidades misturadas ou objetos 255.
                static int16_t parent[kQueueCap];
                static uint8_t groupDepth[kQueueCap];
                auto find = [&](int i) { while (parent[i] != i) { parent[i] = parent[parent[i]]; i = parent[i]; } return i; };
                for (unsigned n = 0; n < queued; ++n) parent[n] = int16_t(n);
                for (unsigned n = 0; n < queued; ++n) {
                    const Obj &o = queue[n];
                    const int cx = (o.x + 16) >> 4, cy = (o.y + 16) >> 4;
                    if (cx < 0 || cy < 0 || cx >= kCellsX || cy >= kCellsY) continue;
                    for (int gy = cy - 1; gy <= cy + 1; ++gy) {
                        if (gy < 0 || gy >= kCellsY) continue;
                        for (int gx = cx - 1; gx <= cx + 1; ++gx) {
                            if (gx < 0 || gx >= kCellsX) continue;
                            for (int m = head[gy * kCellsX + gx]; m >= 0; m = next[m]) {
                                if (unsigned(m) <= n) continue; // (cada par uma vez)
                                const int dx = queue[m].x - o.x, dy = queue[m].y - o.y;
                                if (dx > -16 && dx < 16 && dy > -16 && dy < 16) {
                                    touches[n] = touches[m] = 1;
                                    const int a = find(int(n)), b = find(m);
                                    if (a != b) parent[a] = int16_t(b);
                                }
                            }
                        }
                    }
                }
                // profundidade do grupo: a comum a todos, ou 255 = misturada (ou com objeto 255 / irregular)
                for (unsigned n = 0; n < queued; ++n) groupDepth[n] = 254;
                for (unsigned n = 0; n < queued; ++n) {
                    if (!touches[n]) continue;
                    const int r = find(int(n));
                    const uint8_t d = queue[n].depth < 4 ? queue[n].depth : 255;
                    if (groupDepth[r] == 254) groupDepth[r] = d;
                    else if (groupDepth[r] != d) groupDepth[r] = 255;
                }
                for (unsigned n = 0; n < queued; ++n) {
                    if (!touches[n]) continue;
                    const int cx = (queue[n].x + 16) >> 4, cy = (queue[n].y + 16) >> 4;
                    if (cx < 0 || cy < 0 || cx >= kCellsX || cy >= kCellsY) continue; // (fora da grade: exato)
                    if (groupDepth[find(int(n))] < 4) touches[n] = 0; // grupo uniforme: a ordem de desenho resolve
                }
            }
            stats.ownTouchUs += get_ticks_us() - tPrep;
            // Resolve sprite ownership once. Ordinary priorities are then composed by RDP layer order;
            // only non-monotonic masks require background opacity tests on the CPU.
            for(unsigned n=queued;n>0;n--){
                Obj &o = queue[n - 1];
                auto *rows = owned + (n - 1) * 16;
                if (!touches[n - 1] && o.depth < 4) {
                    // sozinho e numa profundidade comum: desenha inteiro (pixel transparente nao muda nada) e as linhas
                    // de posse nao sao lidas (so os irregulares e coveredByNext, de quem encosta, as leem); o tile e
                    // buscado no desenho
                    texOf[n - 1] = nullptr;
                    o.ownership = Ownership::Full;
                    continue;
                }
                const uint8_t*tex=Fetch(g_pool16,o.at);
                texOf[n - 1] = tex;
                const uint16_t*bits=Opacity16(g_pool16,tex);
                int left=o.x<0?0:o.x,right=o.x+16>kW?kW:o.x+16;
                unsigned offset=left-o.x,limit=(1u<<(right-left))-1;
                const int firstRow = o.y < 0 ? -o.y : 0;
                const int endRow = o.y + 16 > kH ? kH - o.y : 16;
                if (!touches[n - 1]) { // sozinho: todo pixel opaco (dentro da tela) e dele
                    unsigned visibleBits = 0;
                    for (int dy = firstRow; dy < endRow; ++dy) {
                        unsigned opacity = bits[o.flipY ? 15 - dy : dy];
                        if (o.flipX) opacity = Reverse16(opacity);
                        opacity = (opacity >> offset) & limit;
                        rows[dy] = uint16_t(opacity << offset);
                        visibleBits |= opacity;
                    }
                    if (visibleBits) o.ownership = Ownership::Full;
                    continue;
                }
                // Linhas das mascaras zeradas so quando usadas pela primeira vez no quadro (zerar tudo custava ~2,5 ms:
                // ~40 KB de escrita com a cache de dados de 8 KB, cada linha indo ate a RDRAM)
                for (int dy = firstRow; dy < endRow; ++dy) {
                    const int y = o.y + dy;
                    if (g_rowGen[y] == g_rowGenNow) continue;
                    g_rowGen[y] = g_rowGenNow;
                    memset(g_claimed[y], 0, sizeof g_claimed[y]);
                    memset(g_priority[0][y], 0, sizeof g_priority[0][y]);
                    memset(g_priority[1][y], 0, sizeof g_priority[1][y]);
                    memset(g_priority[2][y], 0, sizeof g_priority[2][y]);
                }
                const unsigned wordIdx = unsigned(left) / 32;
                const unsigned shift = left & 31;
                const unsigned d = o.depth;
                unsigned visibleBits = 0, hiddenBits = 0, mustPunchBits = 0;
                auto resolveRows = [&]<bool crossesWord>() {
                    for (int dy = firstRow; dy < endRow; ++dy) {
                        const int y = o.y + dy;
                        unsigned opacity = bits[o.flipY ? 15 - dy : dy];
                        if (o.flipX) opacity = Reverse16(opacity);
                        opacity = (opacity >> offset) & limit;
                        uint32_t *cRow = &g_claimed[y][wordIdx];
                        const uint32_t cFirst = cRow[0];
                        unsigned occupied = cFirst >> shift;
                        if constexpr (crossesWord) {
                            occupied |= cRow[1] << (32 - shift);
                        }
                        const unsigned hidden = opacity & occupied;
                        const unsigned visible = opacity & ~occupied;
                        rows[dy] = visible << offset;
                        visibleBits |= visible;
                        hiddenBits |= hidden;
                        if (hidden) {
                            if (d >= 4) {
                                mustPunchBits |= hidden;
                            } else {
                                const uint32_t *b0Row = &g_priority[0][y][wordIdx];
                                const uint32_t *b1Row = &g_priority[1][y][wordIdx];
                                const uint32_t *vRow = &g_priority[2][y][wordIdx];
                                unsigned val = vRow[0] >> shift;
                                unsigned d0 = b0Row[0] >> shift;
                                unsigned d1 = b1Row[0] >> shift;
                                if constexpr (crossesWord) {
                                    const unsigned rem = 32 - shift;
                                    val |= vRow[1] << rem;
                                    d0 |= b0Row[1] << rem;
                                    d1 |= b1Row[1] << rem;
                                }
                                unsigned uncovered = ~val;
                                if (d == 1) uncovered |= (~d1 & ~d0);
                                else if (d == 2) uncovered |= ~d1;
                                else if (d == 3) uncovered |= ~(d1 & d0);
                                mustPunchBits |= hidden & uncovered;
                            }
                        }
                        if (visible) {
                            const uint32_t loMask = visible << shift;
                            cRow[0] = cFirst | loMask;
                            if constexpr (crossesWord) {
                                cRow[1] |= visible >> (32 - shift);
                            }
                            if (d < 4) {
                                g_priority[2][y][wordIdx] |= loMask;
                                if (d & 1) g_priority[0][y][wordIdx] |= loMask;
                                if (d & 2) g_priority[1][y][wordIdx] |= loMask;
                                if constexpr (crossesWord) {
                                    const uint32_t hiMask = visible >> (32 - shift);
                                    g_priority[2][y][wordIdx + 1] |= hiMask;
                                    if (d & 1) g_priority[0][y][wordIdx + 1] |= hiMask;
                                    if (d & 2) g_priority[1][y][wordIdx + 1] |= hiMask;
                                }
                            }
                        }
                    }
                };
                if (shift + (right - left) > 32) resolveRows.template operator()<true>();
                else resolveRows.template operator()<false>();
                if (visibleBits) {
                    o.ownership = hiddenBits ? (mustPunchBits ? Ownership::Partial : Ownership::Covered) : Ownership::Full;
                }
            }
            if (needBgMask) {
                // Os cenarios so gravam a mascara de fundo nas celulas 8x8 marcadas em g_spriteCells (Blit), e so os
                // sprites irregulares a leem, nas linhas deles (que estao nessas celulas): zera so essas faixas.
                for (int gy = 0; gy < kH / 8; ++gy) {
                    const uint8_t *cells = &g_spriteCells[gy * (kW / 8)];
                    bool any = false;
                    for (int gx = 0; gx < kW / 8; ++gx) if (cells[gx]) { any = true; break; }
                    if (!any) continue;
                    const size_t bytes = 8 * sizeof g_priority[0][0];
                    memset(g_priority[0][gy * 8], 0, bytes);
                    memset(g_priority[1][gy * 8], 0, bytes);
                    memset(g_priority[2][gy * 8], 0, bytes);
                }
            }
            const uint32_t tOwned = get_ticks_us();
            stats.ownerUs += tOwned - tQueued;
            unsigned packet=0;
            // The next sprite may cover the hidden pixels when both draw at the same depth.
            // Test its owned rows, since opacity alone can include pixels claimed at another depth.
            auto coveredByNext = [&](unsigned index, const uint8_t *tex) {
                if (index + 1 == queued) return false;
                const Obj &o = queue[index];
                const Obj &next = queue[index + 1];
                const int dx = next.x - o.x;
                const int dy = next.y - o.y;
                if (next.depth != o.depth || next.ownership == Ownership::Hidden ||
                    dx <= -16 || dx >= 16 || dy <= -16 || dy >= 16)
                    return false;
                const auto *rows = owned + index * 16;
                const auto *nextRows = rows + 16;
                const auto *opacity = Opacity16(g_pool16, tex);
                const int left = o.x < 0 ? -o.x : 0;
                const int right = o.x + 16 > kW ? kW - o.x : 16;
                const unsigned clip = ((1u << right) - 1) & ~((1u << left) - 1);
                for (int y = 0; y < 16; ++y) {
                    if (o.y + y < 0 || o.y + y >= kH) continue;
                    unsigned opaque = opacity[o.flipY ? 15 - y : y];
                    if (o.flipX) opaque = Reverse16(opaque);
                    const unsigned hidden = opaque & clip & ~rows[y];
                    if (!hidden) continue;
                    const int nextY = y - dy;
                    if (nextY < 0 || nextY >= 16) return false;
                    const unsigned coverage = dx >= 0 ? unsigned(nextRows[nextY]) << dx
                                                      : unsigned(nextRows[nextY]) >> -dx;
                    if (hidden & ~coverage) return false;
                }
                return true;
            };
            auto draw=[&](unsigned index,bool irregular){
                const Obj &o = queue[index];
                if (o.mask == 255 || o.ownership == Ownership::Hidden) return;
                const uint8_t *tex = Refetch(g_pool16, o.at, texOf[index]);
                if (!irregular && (o.ownership == Ownership::Full || o.ownership == Ownership::Covered || coveredByNext(index, tex))) {
                    BlitList(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);
                    ++stats.objects;
                    return;
                }
                const uint16_t *rows = owned + index * 16;
                if(packet==kResolvedSlots){FlushCaches();packet=0;}
                auto*out=g_resolved[packet];memset(out,255,128);bool any=false;
                bool unchanged = o.ownership == Ownership::Full;
                const int left = o.x < 0 ? 0 : o.x;
                const unsigned offset = left - o.x;
                for(int dy=0;dy<16;dy++){
                    int y=o.y+dy;if(y<0||y>=kH)continue;
                    auto*src=tex+(o.flipY?15-dy:dy)*8;
                    unsigned visible=rows[dy]>>offset;
                    if(irregular){
                        const unsigned hidden = BackgroundHidden(o.mask, RowBits(g_priority[0][y], left),
                            RowBits(g_priority[1][y], left), RowBits(g_priority[2][y], left));
                        if (visible & hidden) unchanged = false;
                        visible &= ~hidden;
                    }
                    if(!visible)continue;
                    ResolvedRow(out+dy*8,src,visible<<offset,o.flipX);any=true;
                }
                stats.objects++;
                if(!any)return;
                if (unchanged) {
                    BlitList(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);
                } else {
                    data_cache_hit_writeback(out,128);
                    BlitList(out,16,16,8,o.group,o.x,o.y,16,16,0,0,false,false);
                    ++packet;
                    ++stats.resolved;
                }
            };
            const uint32_t bg0 = stats.backgroundUs;
            for(unsigned depth=0;depth<4;depth++){
                g_priorityMark=0;
                for(unsigned n=0;n<queued;n++)if(queue[n].depth == depth)draw(n,false);
                if(depth<3&&order[depth]&&enabled(order[depth])){g_priorityMark=needBgMask?1u<<depth:0;DrawPlane(v,int(order[depth])-1);}
            }
            g_priorityMark=0;
            for(unsigned n=queued;n>0;n--)if(queue[n-1].depth == 4)draw(n-1,true);
            {const uint32_t t=get_ticks_us();stats.spriteUs+=(t-tOwned)-(stats.backgroundUs-bg0);stats.sceneUs+=t-t0;}
            return;
        }
        if (needBgMask) memset(g_priority, 0, sizeof g_priority);
        // Draw the backgrounds and retain their opacity/priority only where sprites can touch them.
        for(int n=0;n<3;n++)if(order[n]&&enabled(order[n])){
            g_priorityMark=1u<<n;DrawPlane(v,int(order[n])-1);
        }
        g_priorityMark=0;
        // CPS-2 sprite ownership is independent of background priority: later objects claim first,
        // even when their own pixel is hidden behind a background layer.
        unsigned packet=0;
        for(unsigned n=queued;n>0;n--){
            const Obj&o=queue[n-1];stats.objects++;if(Empty(o.at,128,-1))continue;
            const uint8_t*tex=Fetch(g_pool16,o.at);
            const uint16_t *opacity = Opacity16(g_pool16, tex);
            if(packet==kResolvedSlots){FlushCaches();packet=0;}
            uint8_t*out=g_resolved[packet];memset(out,255,128);bool any=false,unchanged=true;
            int left=o.x<0?0:o.x,right=o.x+16>kW?kW:o.x+16;
            unsigned offset=left-o.x,limit=(1u<<(right-left))-1;
            for(int dy=0;dy<16;dy++){
                int y=o.y+dy;if(y<0||y>=kH)continue;
                const int row = o.flipY ? 15 - dy : dy;
                const uint8_t *src = tex + row * 8;
                unsigned opaque = opacity[row];
                if (o.flipX) opaque = Reverse16(opaque);
                opaque=(opaque>>offset)&limit;
                unsigned visible=opaque&~RowBits(g_claimed[y],left);
                Claim(g_claimed[y],left,opaque);
                if(visible!=opaque)unchanged=false;
                if(!visible)continue;
                if(!ddsom::g_options.videoFastPaths||o.mask){
                    if(ddsom::g_options.videoFastPaths&&o.mask==255){unchanged=false;continue;}
                    const unsigned hidden = BackgroundHidden(o.mask, RowBits(g_priority[0][y], left),
                        RowBits(g_priority[1][y], left), RowBits(g_priority[2][y], left));
                    if(visible&hidden)unchanged=false;
                    visible&=~hidden;if(!visible)continue;
                }
                ResolvedRow(out+dy*8,src,visible<<offset,o.flipX);any=true;
            }
            if(any){
                if(ddsom::g_options.videoFastPaths&&unchanged&&o.mask==0)BlitList(tex,16,16,8,o.group,o.x,o.y,16,16,0,0,o.flipX,o.flipY);
                else{data_cache_hit_writeback(out,128);BlitList(out,16,16,8,o.group,o.x,o.y,16,16,0,0,false,false);packet++;}
            }
        }
        stats.sceneUs += get_ticks_us() - t0;
    }
};

void RdpVideoInit(uint32_t gfxPi, size_t gfxSize, const uint8_t *empty) {
    g_gfxPi = gfxPi;
    g_gfxSize = gfxSize;
    g_empty = empty;
    g_pool8.Init(64, 512);
    g_pool16.Init(128, 512); // (512 tiles 16x16 = 64 KB: mais que os de uma tela; a RAM foi para o cache de opcodes)
    // Only one plane uses 32x32 tiles. At most 13 by 8 tiles intersect the viewport.
    g_pool32.Init(512, 128);
    g_tlut = static_cast<uint16_t *>(memalign(16, 0xC00 * 2)); // (escrito pela CPU: writeback antes do RDP)
#ifdef CPS2RECOMP_N64
    g_list = static_cast<uint32_t *>(memalign(16, kListCap * 16));
    g_tilesOvl = rspq_overlay_register(&rsp_tiles);
#endif
}

void RdpVideoSetRspTiles(bool on) {
    g_rspTiles = on;
}

void RdpVideoSetTest(bool tlutMap, bool tmemCache) {
    g_tlutMap = tlutMap;
    g_tmemCache = tmemCache;
}

void RdpVideoSetHide(bool sprites, bool planes) {
    g_hideSprites = sprites;
    g_hidePlanes = planes;
}

void RdpVideoDraw(NativeBoard &video, Stats &stats, void*target) {
    g_target=static_cast<surface_t*>(target);
    N64Video::Draw(video, stats);
    ListFlush(); // (o resto da lista do quadro, antes do texto do painel e do fim do quadro)
    if ((++g_pcFrames & 511) == 0) {
        debugf("[PLANECACHE] hits=%u renders=%u direct=%u\n", g_pcHits, g_pcRenders, g_pcDirect);
        g_pcHits = g_pcRenders = g_pcDirect = 0;
    }
}

} // namespace cps2recomp


