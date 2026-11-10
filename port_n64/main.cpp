// Shadow over Mystara para Nintendo 64: o port nativo.
// O codigo do jogo (o programa do 68000, traduzido para MIPS) roda sobre a placa nativa (board.cpp): memoria e
// registradores como dados, o vblank uma vez por frame e o frame encerrado quando o agendador do jogo fica ocioso.
// Static MIPS recompilation retains the 68000 register/cycle representation. A imagem e desenhada pelo RDP
// (rdp_video.cpp) a partir dos mapas de tiles, da lista de objetos e da paleta que o jogo montou.
// Os ROMs ficam no cartucho: o programa lido por um cache de paginas (rom_cache.cpp), os graficos em CI4 por DMA.
// Tela 384x240 (a do CPS-2, 384x224, com 8 linhas em cima e embaixo).
// Driver de som 1.15 traduzido estaticamente para MIPS; PCM de 16 vozes pelo mixer do RSP.
// Telemetria [DD_SOM_PERF] no log a cada 60 frames (run_ares.ps1 grava em telemetria.log).
#include <libdragon.h>

#include "audio.h"
#include "save.h"
#include "run_options.h"
#ifndef DDSOM_PROFILE
#define DDSOM_PROFILE 0
#endif
#ifndef DDSOM_BENCH_FRAMES
#define DDSOM_BENCH_FRAMES 0
#endif
#include "board.h"
#include "cart_n64.h"
#include "perf_n64.h"
#include "rdp_video.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace cps2recomp;

namespace cps2recomp {
void RomCacheReport();
void RomCacheInit(RomCache &c, uint32_t pi, uint32_t size); // (rom_cache.cpp)
void RegisterGame();                                       // (runtime/no_game.cpp)
}

namespace {
    struct CartFile {
        uint32_t pi = 0;
        uint32_t size = 0;
        const uint8_t *p = nullptr; // (KSEG1: o N64 nao aceita cache no cartucho)
    };

    CartFile Cart(const char *name) {
        CartFile out;
        const uint32_t addr = dfs_rom_addr(name);
        if (!addr) {
            debugf("arquivo ausente no cartucho: %s\n", name);
            return out;
        }
        FILE *f = fopen((std::string("rom:/") + name).c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            out.size = static_cast<uint32_t>(ftell(f));
            fclose(f);
        }
        out.pi = addr;
        out.p = reinterpret_cast<const uint8_t *>(0xA0000000u | addr);
        return out;
    }

    std::vector<uint8_t> Load(const char *name) {
        std::vector<uint8_t> v;
        FILE *f = fopen((std::string("rom:/") + name).c_str(), "rb");
        if (!f) {
            return v;
        }
        fseek(f, 0, SEEK_END);
        v.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        fread(v.data(), 1, v.size(), f);
        fclose(f);
        return v;
    }

    uint16_t Pad(const joypad_buttons_t &b, const joypad_inputs_t &in) {
        uint16_t out = 0;
        if (b.d_right || in.stick_x > 40) out |= kPadRight;
        if (b.d_left || in.stick_x < -40) out |= kPadLeft;
        if (b.d_down || in.stick_y < -40) out |= kPadDown;
        if (b.d_up || in.stick_y > 40) out |= kPadUp;
        if (b.b) out |= kPadB1;       // Attack
        if (b.a) out |= kPadB2;       // Jump
        if (b.c_right) out |= kPadB3; // Select
        if (b.c_down) out |= kPadB4;  // Use
        if (b.start) out |= kPadStart;
        if (b.z) out |= kPadCoin;
        return out;
    }

    void Status(const char *text) {
        surface_t *fb = display_get();
        graphics_fill_screen(fb, 0);
        graphics_draw_text(fb, 16, 16, "Shadow over Mystara N64");
        graphics_draw_text(fb, 16, 32, text);
        display_show(fb);

        fprintf(stderr, "%s\n", text);
    }

    volatile uint32_t g_vblanks = 0;
    void OnVblank() {
        g_vblanks++;
    }
} // namespace

int main() {
    ddsom::g_options.audio=DDSOM_PROFILE==1?ddsom::AudioMode::NativeDriver:DDSOM_PROFILE==2?ddsom::AudioMode::Silent:ddsom::AudioMode::Recorded;
    ddsom::g_options.accurateVideo=DDSOM_PROFILE!=4;
    ddsom::g_options.videoFastPaths=DDSOM_PROFILE==0||DDSOM_PROFILE>=5;
    ddsom::g_options.save=DDSOM_PROFILE==0||DDSOM_PROFILE==7;
    g_profileNative=DDSOM_BENCH_FRAMES>0;
    debug_init_isviewer();
    debug_init_usblog();
    dfs_init(DFS_DEFAULT_LOCATION);
    resolution_t res = {};
    res.width = 384;
    res.height = 240;
    res.interlaced = INTERLACE_OFF;
    res.aspect_ratio = 4.0f / 3.0f;
    res.overscan_margin = 0;
    display_init(res, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    joypad_init();
    register_VI_handler(OnVblank);
    debugf("[DDSOM_BUILD_PROFILE] profile=%d benchmark_frames=%d\n",DDSOM_PROFILE,DDSOM_BENCH_FRAMES);
    Status("iniciando");

    const CartFile program = Cart("program.bin"), opcodes = Cart("opcodes.bin"), gfx = Cart("gfx.bin");
    static std::vector<uint8_t> empty = Load("empty.bin");
    RomCacheInit(g_romData, program.pi, program.size);
    RomCacheInit(g_romOpcodes, opcodes.pi, opcodes.size);
    RdpVideoInit(gfx.pi, gfx.size, empty.data());

    ddsom::AudioInit();
    RegisterGame();
    assertf(program.pi&&opcodes.pi&&gfx.pi&&empty.size()>=(gfx.size+255)/256,"Missing or truncated cartridge resources");
    static NativeBoard *board = new NativeBoard();
    board->program = program.p;
    board->opcodes = opcodes.p;
    board->programSize = program.size;
    board->Reset();
    board->optimizePolling=DDSOM_PROFILE==0||DDSOM_PROFILE>=6;
    Status("rodando");
    rdpq_text_register_font(FONT_BUILTIN_DEBUG_MONO, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_MONO));

    if(ddsom::g_options.save)ddsom::SaveInit(*board);

    uint32_t nextLogic=get_ticks_us(),logicFrames=0,winLogic=0,skipped=0,maxLogic=0,maxScene=0;
    uint64_t sumAudio=0,sumAcquire=0,sumSave=0,pureLogic=0,pureScene=0;
    uint32_t overruns=0;uint64_t resyncUs=0;
    uint32_t maxDebt=0;
    constexpr uint32_t logicPeriod=16768; // 512*262 / 8 MHz, the reference board's exact frame period.
    uint32_t frames = 0, vblank0 = g_vblanks, winFrames = 0, winStart = get_ticks_us();
    uint64_t sumLogic = 0, sumScene = 0, sumWait = 0;
    uint32_t sumRects = 0, sumMisses = 0, sumUploads = 0;
    char shown[64] = "";
    bool tlutMap = true, tmemCache = DDSOM_PROFILE!=8;

    for (;;) {
        joypad_poll();
        for (int p = 0; p < 4; p++) {
            const joypad_port_t port = static_cast<joypad_port_t>(p);
            board->players[p] = DDSOM_BENCH_FRAMES?0:Pad(joypad_get_buttons(port), joypad_get_inputs(port));
        }

        RdpVideoSetTest(tlutMap,tmemCache);
        if(!DDSOM_BENCH_FRAMES){static uint16_t last=0;if(board->players[0]!=last){last=board->players[0];debugf("[DDSOM_INPUT] p1=%04x\n",unsigned(last));}}
        while(int32_t(get_ticks_us()-nextLogic)<0)ddsom::AudioPoll();
        {
            uint64_t audioBefore=ddsom::AudioTimeTicks();
            uint32_t t0=get_ticks_us();board->RunFrame();uint32_t dt=get_ticks_us()-t0;
            uint32_t audioInside=TICKS_TO_US(ddsom::AudioTimeTicks()-audioBefore);pureLogic+=dt>audioInside?dt-audioInside:0;
            sumLogic+=dt;if(dt>maxLogic)maxLogic=dt;logicFrames++;winLogic++;nextLogic+=logicPeriod;
            uint8_t request[16];t0=get_ticks_us();
            while(board->PopSound(request))ddsom::AudioRequest(request);
            ddsom::AudioPoll();sumAudio+=get_ticks_us()-t0;
        }
        uint32_t now=get_ticks_us();
        if(int32_t(now-nextLogic)>0&&now-nextLogic>maxDebt)maxDebt=now-nextLogic;
        // Skip drawing when a small logic backlog can be recovered; never discard game updates.
        // Exactly one simulated frame per presentation; never batch four expensive updates.
        uint32_t acquire=get_ticks_us();
        surface_t *fb = display_get();
        sumAcquire+=get_ticks_us()-acquire;
        Stats st;
        rdpq_attach(fb, NULL);
        uint64_t sceneAudio=ddsom::AudioTimeTicks();
        RdpVideoDraw(*board, st, fb);
        if (shown[0]) {
            rdpq_mode_push();
            rdpq_set_mode_standard();
            rdpq_text_printf(nullptr, FONT_BUILTIN_DEBUG_MONO, res.width - 88, 18, "%s", shown);
            rdpq_mode_pop();
        }
        uint32_t audioInScene=TICKS_TO_US(ddsom::AudioTimeTicks()-sceneAudio);pureScene+=st.sceneUs>audioInScene?st.sceneUs-audioInScene:0;
        if(st.sceneUs>maxScene)maxScene=st.sceneUs;
        ddsom::AudioPoll();
        const uint32_t t2 = get_ticks_us();
        rdpq_detach_wait();
        sumWait += get_ticks_us() - t2;
        sumScene += st.sceneUs;
        sumRects += st.rects;
        sumMisses += st.misses;
        sumUploads += st.uploads;
        display_show(fb);
        uint32_t saveBegin=get_ticks_us();if(ddsom::g_options.save)ddsom::SavePoll(*board);sumSave+=get_ticks_us()-saveBegin;

        uint32_t after=get_ticks_us();if(int32_t(after-nextLogic)>int32_t(logicPeriod)){resyncUs+=after-nextLogic;nextLogic=after;overruns++;}
        frames++;
        if (++winFrames == 60) {
            const uint32_t now = get_ticks_us();
            const uint32_t fps100 = static_cast<uint32_t>(6000000000ull / (now - winStart ? now - winStart : 1));
            fprintf(stderr, "[DD_SOM_PERF] tlut_map=%d tmem_cache=%d ", tlutMap ? 1 : 0, tmemCache ? 1 : 0);
            fprintf(stderr, "[DD_SOM_PERF] frames=%lu fps100=%lu vblanks=%lu logic_us=%lu scene_walk_us=%lu gpu_wait_us=%lu "
                   "rects=%lu tile_misses=%lu tex_loads=%lu sound_requests=%lu interp_total=%lu\n",
                   (unsigned long)frames, (unsigned long)fps100, (unsigned long)(g_vblanks - vblank0),
                   (unsigned long)(sumLogic / 60), (unsigned long)(sumScene / 60), (unsigned long)(sumWait / 60),
                   (unsigned long)(sumRects / 60), (unsigned long)(sumMisses / 60), (unsigned long)(sumUploads / 60),
                   (unsigned long)board->soundRequests, (unsigned long)board->cpu.interpreted);
            snprintf(shown, sizeof(shown), "FPS %lu.%lu", (unsigned long)(fps100 / 100),
                     (unsigned long)(fps100 % 100 / 10));
            debugf("[DDSOM_AUDIT] logic_frames=%lu skipped=%lu max_logic_us=%lu max_scene_us=%lu max_debt_us=%lu audio_us=%lu acquire_us=%lu save_us=%lu sound_overflow=%u rom_reads=%lu rom_misses=%lu dispatch_misses=%lu\n",
                (unsigned long)logicFrames,(unsigned long)skipped,(unsigned long)maxLogic,(unsigned long)maxScene,(unsigned long)maxDebt,
                (unsigned long)(sumAudio/60),(unsigned long)(sumAcquire/60),(unsigned long)(sumSave/60),board->soundOverflows,
                (unsigned long)g_romData.reads,(unsigned long)g_romData.misses,(unsigned long)g_perf.dispatchMiss68);
            debugf("[DDSOM_CLOCK] logic_fps100=%lu tick_us=%lu\n",(unsigned long)(uint64_t(winLogic)*100000000/(now-winStart?now-winStart:1)),(unsigned long)(winLogic?sumLogic/winLogic:0));
            winLogic=0;
            debugf("[DDSOM_STATE] pc=%06lx ready=%02x busy=%02x\n",(unsigned long)board->cpu.pc,unsigned(board->soundRam[0xfff]),unsigned(board->soundRam[15]));
            ddsom::AudioReport();RomCacheReport();
            debugf("[DDSOM_PURE_COST] logic_us=%llu scene_us=%llu resyncs=%lu resync_us=%llu\n",(unsigned long long)(pureLogic/60),(unsigned long long)(pureScene/60),(unsigned long)overruns,(unsigned long long)resyncUs);pureLogic=pureScene=0;
            maxLogic=maxScene=maxDebt=0;sumAudio=sumAcquire=sumSave=0;
            vblank0 = g_vblanks;
            winFrames = 0;
            winStart = now;
            sumLogic = sumScene = sumWait = 0;
            sumRects = sumMisses = sumUploads = 0;
        }
        if(DDSOM_BENCH_FRAMES&&frames>=DDSOM_BENCH_FRAMES){ReportNativeProfile();debugf("[DDSOM_BENCH_DONE] frames=%lu\n",(unsigned long)frames);for(;;)wait_ms(1000);}
    }
}







