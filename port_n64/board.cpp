// Shadow over Mystara N64: a placa nativa (board.h).
#include "board.h"

#include "cart_n64.h"
#include "lift.h"

#include <cstring>

namespace cps2recomp {

namespace {
    // Um frame da placa: 262 linhas de 1024 ciclos do 68000; o vblank comeca na linha 240
    constexpr uint64_t kLineCycles = 1024;
    constexpr uint64_t kFrameCycles = 262 * kLineCycles, kBlankAt = 240 * kLineCycles;

    // (the C library compares byte by byte)
    inline bool SameWords(const void *a, const void *b, size_t bytes) {
        const uint32_t *x = static_cast<const uint32_t *>(a), *y = static_cast<const uint32_t *>(b);
        for (size_t n = 0; n < bytes / 4; n++) {
            if (x[n] != y[n]) {
                return false;
            }
        }
        return true;
    }
    inline uint16_t Word(const uint8_t *p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    inline void SetWord(uint8_t *p, uint16_t v) {
        p[0] = static_cast<uint8_t>(v >> 8);
        p[1] = static_cast<uint8_t>(v);
    }
    inline uint16_t ProgramWord(const uint8_t *base, uint32_t address, bool asOpcodes) {
#ifdef CPS2RECOMP_N64
        (void)base;
        return static_cast<uint16_t>(RomWord(asOpcodes ? g_romOpcodes : g_romData, address));
#else
        (void)asOpcodes;
        return Word(&base[address]);
#endif
    }

    // --- Ociosidade: o agendador de tarefas do jogo (0x932-0x95E) passa pelas 16 tarefas; uma passada inteira sem
    // nenhuma tarefa rodar quer dizer que o jogo so espera o proximo vblank. Ai o frame termina, sem girar a toa.

    void SkipRepeatedPoll(M68K&c,NativeBoard&b){
        if(!b.optimizePolling)return;
        auto invalidate=[&](){b.pollValid=false;};
        // The only runnable task must be the read-only vblank poll at EE080.
        // With bit 7 set and either gate zero, it immediately yields via A86.
        if(c.a[5]!=0xff8000||!(Word(b.ram+0xffd0)&0x80)||(b.ram[0x8030]&&b.ram[0x8033])){invalidate();return;}
        auto longword=[](const uint8_t*p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];};
        unsigned runnable=0;
        for(unsigned i=0;i<16;i++){
            auto*p=b.ram+0x64+i*16;if(p[0]<4)continue;
            uint32_t stack=longword(p+8);
            if(p[0]!=4||longword(p+4)!=0xa8c||stack<0xff0000||stack>0xffffbf||longword(b.ram+(stack&65535)+60)!=0xee0fa){invalidate();return;}
            runnable++;
        }
        if(runnable!=1){invalidate();return;}
        uint32_t state[18];memcpy(state,c.d,32);memcpy(state+8,c.a,32);state[16]=c.otherSP;state[17]=c.SR();
        if(b.pollValid&&b.pollBudget==c.budget&&!memcmp(state,b.pollState,sizeof state)){
            uint64_t period=c.cycles-b.pollCycles;
#if defined(DDSOM_NATIVE_TIME)||defined(DDSOM_IDLE_FRAMES)
            // The same pass again, from the same state, reading nothing that can change before the vblank: the game is idle
            if(period&&c.cycles<c.budget){b.pollSkipped++;c.cycles=c.budget;}
#else
            if(period&&c.cycles<c.budget){uint64_t n=(c.budget-c.cycles-1)/period;c.cycles+=n*period;b.pollSkipped+=n;}
#endif
        }
        memcpy(b.pollState,state,sizeof state);b.pollValid=true;b.pollCycles=c.cycles;b.pollBudget=c.budget;
    }
    // A pass of the game's scheduler that began where the one before it did (the same registers) and wrote the same things,
    // as the one before that: nothing changes from one pass to the next until the vblank. The game only waits.
    bool RepeatsItself(M68K &c, NativeBoard &b) {
#ifdef DDSOM_NATIVE_TIME
        uint32_t state[18];memcpy(state,c.d,32);memcpy(state+8,c.a,32);state[16]=c.otherSP;state[17]=c.SR();
        const uint32_t wrote=c.writeHash-b.passHashNow; // (what the pass before this one wrote)
        const bool same=b.passRun>=2&&wrote==b.passWrites&&!memcmp(state,b.passState,sizeof state);
        b.passRun++;b.passWrites=wrote;b.passHashNow=c.writeHash;memcpy(b.passState,state,sizeof state);
        return same;
#else
        (void)c;(void)b;return false;
#endif
    }
    bool PassStarts(M68K &c) {
        auto& b=static_cast<NativeBoard&>(c.bus);
        if(b.optimizePolling&&RepeatsItself(c,b)){
            b.pollSkipped++;
            c.cycles=c.budget;
            return true;
        }
        SkipRepeatedPoll(c,b);
        if (b.schedulerPassSeen && !b.schedulerWorked) {
            c.cycles = c.budget; // (o resto do tempo do frame: o jogo so esperaria)
            c.pc = 0x000932;
            return true;
        }
        b.schedulerPassSeen = true;
        b.schedulerWorked = false;
        return false;
    }
    bool TaskRuns(M68K &c) {
        auto& b=static_cast<NativeBoard&>(c.bus);
        b.schedulerWorked = true;
        return false;
    }
    bool g_hooksAdded = false;
} // namespace

NativeBoard::NativeBoard() {
    Reset();
}

void NativeBoard::Reset() {
    if (!g_hooksAdded) {
        AddHook(0x000932, "SchedulerPass", PassStarts);
        AddHook(0x000960, "SchedulerTask", TaskRuns);
        g_hooksAdded = true;
    }
    memset(ram, 0, sizeof(ram));
    memset(videoRam, 0, sizeof(videoRam));
    memset(objectRam, 0, sizeof(objectRam));
    memset(extraRam, 0, sizeof(extraRam));
    memset(soundRam, 0, sizeof(soundRam));
    memset(cpsA, 0, sizeof(cpsA));
    memset(cpsB, 0, sizeof(cpsB));
    memset(objectRegs, 0, sizeof(objectRegs));
    memset(objects, 0, sizeof(objects));
    memset(palette, 0, sizeof(palette));
    // (onde as areas do video ficam ate o jogo dizer)
    cpsA[0] = 0x9200;
    cpsA[1] = 0x9000;
    cpsA[2] = 0x9040;
    cpsA[3] = 0x9080;
    cpsA[4] = 0x9100;
    objectBank = 0;
    lastObject = -4;
    priorityControl = 0;
    soundRequests = 0; soundRead=soundWrite=soundOverflows=0;
    soundRam[0x00F]=0xFF;
    cpu.ram = ram;
    cpu.video = videoRam;
    cpu.videoSize = sizeof(videoRam);
    cpu.rom = program;
    cpu.opcodes = opcodes;
    cpu.romSize = programSize;
    cpu.busWait = 0;
    cpu.cycles = 0;
    cpu.Reset(true);
    schedulerPassSeen = schedulerWorked = false;
    pollValid=false;pollBudget=pollCycles=pollSkipped=0;
}

void NativeBoard::CaptureSound(){
 if(soundRam[0x00F]==0xFF||soundRam[0xFFF]!=0x77||soundRam[0xFFD]!=0x88)return;
 if(soundWrite-soundRead==kSoundQueue){soundOverflows++;return;}
 memcpy(soundQueue[soundWrite++%kSoundQueue],soundRam,16);
 memcpy(lastSoundRequest,soundRam,16);soundRequests++;soundRam[0x00F]=0xFF;
}
bool NativeBoard::PopSound(uint8_t*out){
 if(soundRead==soundWrite)return false;
 memcpy(out,soundQueue[soundRead++%kSoundQueue],16);CaptureSound();return true;
}
void NativeBoard::RunFrame(M68K::Mode mode) {
    // Capture a complete committed packet; writes also capture immediately once the boot RAM test ends.
    CaptureSound();
#if defined(DDSOM_NATIVE_TIME)||defined(DDSOM_IDLE_FRAMES)
#ifdef DDSOM_NATIVE_TIME
    // Blocks in place of 68000 time: a frame is cut where the board's frame would be (about 20 cycles a block) when the game
    // has not gone idle by then, as the vblank interrupt came while the game was still at work (and in loops that wait
    // for it without being the scheduler's poll)
#ifdef DDSOM_NT_IDLE_ONLY // (the equivalence test: only idleness ends a phase, as in its reference)
    constexpr uint64_t kBlankBlocks = 400000000, kRestBlocks = 400000000;
#else
    constexpr uint64_t kBlankBlocks = kBlankAt / 20, kRestBlocks = (kFrameCycles - kBlankAt) / 20;
#endif
#else
    constexpr uint64_t kBlankBlocks = 400000000, kRestBlocks = 400000000; // (test reference: only idleness ends a phase)
#endif
    passRun = 0;
    cpu.Run(cpu.cycles + kBlankBlocks, mode);
#else
    const uint64_t start = cpu.cycles - (cpu.cycles % kFrameCycles);
    // A logica ate o vblank (ou ate ficar ociosa)
    cpu.Run(start + kBlankAt, mode);
    if (cpu.cycles < start + kBlankAt) {
        cpu.cycles = start + kBlankAt;
    }
#endif
    // Vblank: a lista de objetos do frame e o pedido de interrupcao (nivel 2)
    // The list ends at its first terminator; only what is before it is copied (the renderer reads no further)
    {
        const uint8_t *list = objectRam[objectBank];
        int end = 0x1000;
        lastObject = 0x1000 - 4;
        for (int n = 0; n < 0x1000; n += 4) {
            if (Word(&list[(n + 1) * 2]) >= 0x8000 || Word(&list[(n + 3) * 2]) >= 0xFF00) {
                lastObject = n - 4;
                end = n + 4;
                break;
            }
        }
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        memcpy(objects, list, end * 2);
#else
        for (int n = 0; n < end; n++) {
            objects[n] = Word(&list[n * 2]);
        }
#endif
    }
    priorityControl = objectRegs[2];
    cpu.SetInterruptLevel(2);
    schedulerPassSeen = false;
    // O resto do frame: a rotina do vblank e a logica seguinte, ate ficar ociosa
#if defined(DDSOM_NATIVE_TIME)||defined(DDSOM_IDLE_FRAMES)
    passRun = 0;
    cpu.Run(cpu.cycles + kRestBlocks, mode);
#else
    cpu.Run(start + kFrameCycles, mode);
    if (cpu.cycles < start + kFrameCycles) {
        cpu.cycles = start + kFrameCycles;
    }
#endif
}

void NativeBoard::InterruptAck(unsigned) {
    cpu.SetInterruptLevel(0);
    schedulerPassSeen = false; // (a passada em andamento foi cortada pela interrupcao)
    passRun = 0;
}

// --- entrada: cada linha le 0 quando apertada ---
uint16_t NativeBoard::Input(int port) const {
    const uint16_t p1 = players[0], p2 = players[1], p3 = players[2], p4 = players[3];
    uint16_t pressed = 0;
    switch (port) {
    case 0: pressed = static_cast<uint16_t>((p1 & 0x7F) | ((p2 & 0x7F) << 8)); break;
    case 1: pressed = static_cast<uint16_t>(((p1 >> 7) & 7) | (((p2 >> 7) & 3) << 4) | (p3 & 0x7F) | ((p4 & 0x7F) << 8)); break;
    default:
        pressed = static_cast<uint16_t>(((p1 & kPadStart) ? 0x0100 : 0) | ((p2 & kPadStart) ? 0x0200 : 0) |
                                        ((p3 & kPadStart) ? 0x0400 : 0) | ((p4 & kPadStart) ? 0x0800 : 0) |
                                        ((p1 & kPadCoin) ? 0x1000 : 0) | ((p2 & kPadCoin) ? 0x2000 : 0) |
                                        ((p3 & kPadCoin) ? 0x4000 : 0) | ((p4 & kPadCoin) ? 0x8000 : 0));
        return static_cast<uint16_t>((~pressed & 0xFFFE) | (eeprom.DataOut() ? 1 : 0));
    }
    return static_cast<uint16_t>(~pressed);
}

// --- leituras ---
uint16_t NativeBoard::Fetch16(uint32_t address) {
    if (address < programSize) {
        return ProgramWord(opcodes, address, true);
    }
    return Read16(address); // (codigo na RAM nao e cifrado)
}

uint16_t NativeBoard::Read16(uint32_t address) {
    if (address >= 0xFF0000) {
        return Word(&ram[address & 0xFFFE]);
    }
    if (address < programSize) {
        return ProgramWord(program, address, false);
    }
    if (address >= 0x900000 && address < 0x930000) {
        return Word(&videoRam[address - 0x900000]);
    }
    if (address >= 0x400000 && address < 0x40000C) {
        return objectRegs[(address - 0x400000) / 2];
    }
    if (address >= 0x618000 && address < 0x61A000) {
        return static_cast<uint16_t>(0xFF00 | soundRam[(address - 0x618000) / 2]);
    }
    if (address >= 0x660000 && address < 0x664000) {
        return Word(&extraRam[address - 0x660000]);
    }
    if (address >= 0x708000 && address < 0x710000) {
        return Word(&objectRam[objectBank ^ 1][address & 0x1FFE]);
    }
    if ((address & ~0x4000u) >= 0x800140 && (address & ~0x4000u) < 0x800180) {
        switch ((address & 0x3E) / 2) {
        case 0x04 / 2: return static_cast<uint16_t>(cpsB[0] * cpsB[1]); // (o multiplicador do CPS-B)
        case 0x06 / 2: return static_cast<uint16_t>((static_cast<uint32_t>(cpsB[0]) * cpsB[1]) >> 16);
        default: return 0xFFFF;
        }
    }
    switch (address) {
    case 0x804000: return Input(0);
    case 0x804010: return Input(1);
    case 0x804020: return Input(2);
    case 0x804030: return 0xE021; // (volume no maximo; sem placas extras)
    default: return 0xFFFF;
    }
}

uint8_t NativeBoard::Read8(uint32_t address) {
    if (address >= 0xFF0000) {
        return ram[address & 0xFFFF];
    }
    if (address < programSize) {
        const uint16_t w = ProgramWord(program, address & ~1u, false);
        return static_cast<uint8_t>((address & 1) ? w : (w >> 8));
    }
    if (address >= 0x900000 && address < 0x930000) {
        return videoRam[address - 0x900000];
    }
    const uint16_t w = Read16(address & ~1u);
    return static_cast<uint8_t>((address & 1) ? w : (w >> 8));
}

// --- escritas ---
void NativeBoard::Write16(uint32_t address, uint16_t value) {
    if (address >= 0xFF0000) {
        SetWord(&ram[address & 0xFFFE], value);
    } else if (address >= 0x900000 && address < 0x930000) {
        SetWord(&videoRam[address - 0x900000], value);
    } else {
        WriteRegister(address, value, 0xFFFF);
    }
}

void NativeBoard::Write8(uint32_t address, uint8_t value) {
    if (address >= 0xFF0000) {
        ram[address & 0xFFFF] = value;
    } else if (address >= 0x900000 && address < 0x930000) {
        videoRam[address - 0x900000] = value;
    } else if (address & 1) {
        WriteRegister(address & ~1u, value, 0x00FF);
    } else {
        WriteRegister(address, static_cast<uint16_t>(value << 8), 0xFF00);
    }
}

void NativeBoard::TakePalette() {
    // As paginas ligadas, a partir do endereco que o jogo deu (o mesmo de Video::CopyPalette); a conversao para as
    // cores do N64 fica com o renderizador
    const uint32_t base = (static_cast<uint32_t>(cpsA[5]) * 256) & ~0x3FFu & 0x3FFFF;
    const uint16_t control = cpsB[0x30 / 2];
    uint32_t at = base;
    bool changed = false;
    for (int page = 0; page < 6; page++) {
        if (control & (1u << page)) {
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            // (a page never crosses the end of the video RAM; the game writes the same palette again and again,
            // and only a different one is a change for the renderer)
            if (!SameWords(&palette[0x200 * page], &videoRam[at & 0x3FFFF], 0x400)) {
                memcpy(&palette[0x200 * page], &videoRam[at & 0x3FFFF], 0x400);
                changed = true;
            }
            at += 0x400;
#else
            for (int n = 0; n < 0x200; n++) {
                const uint16_t color = Word(&videoRam[at & 0x3FFFF]);
                changed |= palette[0x200 * page + n] != color;
                palette[0x200 * page + n] = color;
                at += 2;
            }
#endif
        } else if (at != base) {
            at += 0x400;
        }
    }
    if (changed) {
        paletteChanged = true;
    }
}

void NativeBoard::WriteRegister(uint32_t address, uint16_t value, uint16_t mask) {
    auto combine = [&](uint16_t &reg) {
        reg = static_cast<uint16_t>((reg & ~mask) | (value & mask));
        return reg;
    };
    auto combineBytes = [&](uint8_t *p) {
        if (mask & 0xFF00) p[0] = static_cast<uint8_t>(value >> 8);
        if (mask & 0x00FF) p[1] = static_cast<uint8_t>(value);
    };
    if (address >= 0x400000 && address < 0x40000C) {
        combine(objectRegs[(address - 0x400000) / 2]);
        return;
    }
    if (address >= 0x618000 && address < 0x61A000) {
        if (mask & 0x00FF) {
            const uint32_t at = (address - 0x618000) / 2;
            soundRam[at] = static_cast<uint8_t>(value);
            if(at==0x00F)CaptureSound();
            // (um pedido: soundRam[0x00F] diferente de FF; o lado do som o pega no inicio do proximo frame, RunFrame)
        }
        return;
    }
    if (address >= 0x660000 && address < 0x664000) {
        combineBytes(&extraRam[address - 0x660000]);
        return;
    }
    if (address >= 0x700000 && address < 0x702000) {
        combineBytes(&objectRam[objectBank][address & 0x1FFE]);
        return;
    }
    if (address >= 0x708000 && address < 0x710000) {
        combineBytes(&objectRam[objectBank ^ 1][address & 0x1FFE]);
        return;
    }
    const uint32_t chip = address & ~0x4000u;
    if (chip >= 0x800100 && chip < 0x800140) {
        const unsigned index = (address & 0x3E) / 2;
        combine(cpsA[index]);
        if (index == 0x0A / 2) {
            TakePalette(); // (o jogo apontou a paleta: a placa a copia nesse momento)
        }
        return;
    }
    if (chip >= 0x800140 && chip < 0x800180) {
        combine(cpsB[(address & 0x3E) / 2]);
        return;
    }
    if (address == 0x804040) {
        if (mask & 0xFF00) {
            eeprom.SetLines(value & 0x4000, value & 0x2000, value & 0x1000);
        }
        if ((mask & 0x00FF) && (value & 0x0008)) {
            soundRam[0xFFF] = 0x77; // (o jogo liga o lado do som e espera o "pronto" do driver da Capcom em 619FFF)
        }
        return; // (o resto do byte baixo: contadores de ficha)
    }
    if (address == 0x8040E0 && (mask & 0x00FF)) {
        objectBank = value & 1;
    }
}

} // namespace cps2recomp


