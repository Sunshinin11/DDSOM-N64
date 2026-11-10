// Shadow over Mystara N64: a placa nativa. Substitui a maquina CPS-2 do cps2recomp (machine.cpp): so a memoria e os
// registradores que o codigo do jogo usa, como dados simples, o vblank uma vez por frame, e nada de emulacao de chip
// (sem Z80, sem QSound, sem renderizador por linha, sem contagem de linhas, raster ou DMA).
#pragma once
#include "eeprom.h"
#include "m68k.h"

#include <cstdint>

namespace cps2recomp {

// Botoes de um jogador (o mesmo arranjo das linhas da placa)
enum PadBit : uint16_t {
    kPadRight = 1 << 0, kPadLeft = 1 << 1, kPadDown = 1 << 2, kPadUp = 1 << 3,
    kPadB1 = 1 << 4, kPadB2 = 1 << 5, kPadB3 = 1 << 6, kPadB4 = 1 << 7, kPadStart = 1 << 10, kPadCoin = 1 << 11,
};

struct NativeBoard final : M68KBus {
    NativeBoard();
    M68K cpu{*this};

    // Memoria do jogo
    uint8_t ram[0x10000];          // FF0000-FFFFFF
    alignas(16) uint8_t videoRam[0x30000];     // 900000-92FFFF: mapas de tiles, paleta, tabela de scroll por linha
    alignas(16) uint8_t objectRam[2][0x2000];  // 700000 (banco escrito) e 708000 (o outro)
    uint8_t extraRam[0x4000];      // 660000-663FFF
    uint8_t soundRam[0x1000];      // 618000-619FFF (byte baixo): a caixa de pedidos de som

    // Registradores de video que o jogo escreve (os mesmos indices de video/video.cpp)
    uint16_t cpsA[32] = {}, cpsB[32] = {}, objectRegs[6] = {};
    int objectBank = 0;

    // O que o renderizador le no fim do frame
    alignas(16) uint16_t objects[0x1000];      // lista de objetos do frame (1024 entradas de 4 palavras), copiada no vblank
    int lastObject = -4;           // inicio da ultima entrada em uso (-4: nenhuma)
    uint16_t priorityControl = 0;
    alignas(16) uint16_t palette[0xC00];       // as 6 paginas de 512 cores, como o jogo as entregou (copiadas quando aponta a paleta)
    bool paletteChanged = true;

    // O programa do 68000 (no N64: no cartucho, lido pelo cache de paginas)
    const uint8_t *program = nullptr, *opcodes = nullptr;
    uint32_t programSize = 0;

    uint16_t players[4] = {};
    Eeprom eeprom;

    // Pedidos de som (soundRam[0x00F] escrito com algo diferente de FF): para o som de alto nivel
    static constexpr unsigned kSoundQueue=64;
    uint8_t soundQueue[kSoundQueue][16] = {};
    unsigned soundRead=0,soundWrite=0,soundOverflows=0;
    bool PopSound(uint8_t *out);
    void CaptureSound();
    uint32_t soundRequests = 0;
    uint8_t lastSoundRequest[16] = {};

    void Reset();
    bool schedulerPassSeen=false,schedulerWorked=false;
    bool optimizePolling=false,pollValid=false;
    // The scheduler's passes seen since the frame's phase began, the registers each began with and what it wrote
    unsigned passRun=0;uint32_t passState[18]={},passWrites=0,passHashNow=0;
    uint64_t pollBudget=0,pollCycles=0,pollSkipped=0;
    uint32_t pollState[18]={};
    void RunFrame(M68K::Mode mode=M68K::Mode::Native); // um frame do jogo: a logica ate ficar ociosa, o vblank, e de novo ate ficar ociosa

    // M68KBus
    uint8_t Read8(uint32_t address) override;
    uint16_t Read16(uint32_t address) override;
    void Write8(uint32_t address, uint8_t value) override;
    void Write16(uint32_t address, uint16_t value) override;
    uint16_t Fetch16(uint32_t address) override;
    void InterruptAck(unsigned level) override;

private:
    uint16_t Input(int port) const;
    void WriteRegister(uint32_t address, uint16_t value, uint16_t mask);
    void TakePalette();
};

} // namespace cps2recomp
