// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// What translated code is written in terms of (tools/recomp.py writes the code; this is included by it).
//
// A translated routine is a C++ function over the CPU's state: the game's instructions as statements, its
// branches as gotos. It is entered at the block cpu.pc names and returns when control leaves the routine
// (a return, a computed jump, a call that did not come back where expected) or when time is up, with
// cpu.pc saying where to go on. Nothing is kept on the host's stack that is not also in the CPU's state, so
// returning early is always right, and calls between routines on the host's stack are only a short cut.
#pragma once
#include "cart_n64.h"

#include "../machine/m68k.h"
#include "lift.h"

#include <bit>
#include <cstdint>

namespace cps2recomp {

// The translated blocks, sorted by address, in parallel arrays (8 bytes a block): where it starts, the routine it is in
// (an index into routines) and how long it is in bytes. The blocks do not overlap.
struct NativeCode {
    const uint32_t *addresses;
    const uint16_t *routineOf;
    const uint16_t *lengths;
    const NativeFn *routines;
    size_t entryCount;
    const uint32_t *hooks; // the addresses at which the translated code stops by for a hook (lift.h)
    size_t hookCount;
    // Set by the build without translated code only (runtime/no_blocks.cpp): words a release's check looks for in a
    // program (tools/release/check_archive.sh), to tell that build from a release's
    const char *none = nullptr;
    uint32_t Address(size_t n) const { return addresses[n]; }
    uint32_t End(size_t n) const { return addresses[n] + lengths[n]; }
    NativeFn Fn(size_t n) const { return routines[routineOf[n]]; }
};
// The game's translated code (generated; an empty set when there is none)
extern const NativeCode kNativeCode;
bool HasNativeCode();

#ifdef CPS2RECOMP_CHECK
namespace check {
    extern bool side, noting;
    void Note(uint32_t address, unsigned size, uint32_t value);
} // namespace check
uint64_t BlocksChecked();
#endif

#if defined(CPS2RECOMP_N64) && defined(DDSOM_NATIVE_BOARD)
uint16_t ReadNativeJoypad(M68K &c, unsigned port);
#endif

namespace native {
    // Video RAM pages (16 KB) written since a reader (a bit each) last looked: the N64 renderer's plane cache uses it
    // to know a tile map did not change without reading it
    inline uint8_t g_videoDirty[0x30000 >> 8]; // escritas na RAM de video por bloco de 256 bytes (bits: quem ainda nao viu; NativeBoard::Reset marca tudo)


    constexpr unsigned kMaxDepth = 64;
#ifdef DDSOM_NATIVE_TIME
    // (what a run of the game wrote, in order: a pass of the scheduler that writes what the one before it did, from the same
    // registers, changes nothing: the game is waiting; port_n64/board.cpp)
#define WRITE_HASH(c, address, value) ((c).writeHash = ((c).writeHash * 33u) ^ (address) ^ (value))
#else
#define WRITE_HASH(c, address, value)
#endif

    // A routine was entered at an address none of its blocks starts at (the tables are wrong)
    [[noreturn]] void NotHere(M68K &c, uint32_t routine);

#ifdef CPS2RECOMP_CHECK
    // The check build: translated code runs a block at a time on the side, its writes kept in a list and
    // compared with what the interpreter does (runtime/recomp.cpp)
    uint32_t CheckRead(M68K &c, uint32_t address, unsigned size);
    void CheckWrite(M68K &c, uint32_t address, unsigned size, uint32_t value);
    inline uint32_t R8(M68K &c, uint32_t address) {
        c.BusWait(address);
        return CheckRead(c, address, 0);
    }
    inline uint32_t R16(M68K &c, uint32_t address) {
        c.BusWait(address);
        return CheckRead(c, address, 1);
    }
    inline uint32_t R32(M68K &c, uint32_t address) {
        c.BusWait(address);
        c.BusWait(address + 2);
        return CheckRead(c, address, 2);
    }
    inline void W8(M68K &c, uint32_t address, uint32_t value) {
        c.BusWait(address);
        CheckWrite(c, address, 0, value);
    }
    inline void W16(M68K &c, uint32_t address, uint32_t value) {
        c.BusWait(address);
        CheckWrite(c, address, 1, value);
    }
    inline void W32(M68K &c, uint32_t address, uint32_t value) {
        c.BusWait(address);
        c.BusWait(address + 2);
        CheckWrite(c, address, 2, value);
    }
#else
    // Memory as data: work RAM and the ROM directly, everything else through the machine
    inline uint32_t R8(M68K &c, uint32_t address) {
        address &= 0xFFFFFF;
        if (address >= 0xFF0000) {
            return c.ram[address & 0xFFFF];
        }
        if (address < c.romSize) {
#ifdef CPS2RECOMP_N64
            return *RomPtr(g_romData, address);
#else
            return CartByte(&c.rom[address]);
#endif
        }
#ifdef DDSOM_NATIVE_BOARD
        if (address - 0x900000u < 0x30000u) return c.video[address - 0x900000u];
        if (address - 0x708000u < 0x8000u) return c.objWrite[address & 0x1FFF];
        if (address - 0x660000u < 0x4000u) return c.extra[address - 0x660000u];
#endif
        return c.bus.Read8(address);
    }
    inline uint32_t R16(M68K &c, uint32_t address) {
        address &= 0xFFFFFE;
        if (address >= 0xFF0000) {
            const uint8_t *p = &c.ram[address & 0xFFFF];
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            return *reinterpret_cast<const Alias*>(p);
#else
            return static_cast<uint32_t>((p[0] << 8) | p[1]);
#endif
        }
        if (address < c.romSize) {
#ifdef CPS2RECOMP_N64
            return RomWord(g_romData, address);
#else
            const uint8_t *p = &c.rom[address];
            return static_cast<uint32_t>((CartByte(p) << 8) | CartByte(p + 1));
#endif
        }
#ifdef DDSOM_NATIVE_BOARD
        if (address - 0x900000u < 0x30000u) {
            const uint8_t *p = c.video + address - 0x900000u;
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            return *reinterpret_cast<const Alias*>(p);
#else
            return uint32_t(p[0]) << 8 | p[1];
#endif
        }
        if (address - 0x708000u < 0x8000u) {
            const uint8_t *p = c.objWrite + (address & 0x1FFE);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            return *reinterpret_cast<const Alias*>(p);
#else
            return uint32_t(p[0]) << 8 | p[1];
#endif
        }
        if (address - 0x660000u < 0x4000u) {
            const uint8_t *p = c.extra + (address - 0x660000u);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            return *reinterpret_cast<const Alias*>(p);
#else
            return uint32_t(p[0]) << 8 | p[1];
#endif
        }
#endif
        return c.bus.Read16(address);
    }
    inline uint32_t R32(M68K &c, uint32_t address) {
        address &= 0xFFFFFE;
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if (address >= 0xFF0000) {
            if ((address & 0xFFFF) <= 0xFFFC) {
                typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
                return *reinterpret_cast<const Alias32*>(&c.ram[address & 0xFFFF]);
            }
        } else if (address + 4 <= c.romSize) {
            return RomLong(g_romData, address);
        }
#ifdef DDSOM_NATIVE_BOARD
        else if (address - 0x900000u <= 0x2FFFCu) {
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            return *reinterpret_cast<const Alias32*>(c.video + address - 0x900000u);
        } else if (address - 0x708000u < 0x8000u && (address & 0x1FFE) <= 0x1FFC) {
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            return *reinterpret_cast<const Alias32*>(c.objWrite + (address & 0x1FFE));
        } else if (address - 0x660000u <= 0x3FFCu) {
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            return *reinterpret_cast<const Alias32*>(c.extra + (address - 0x660000u));
        }
#endif
#endif
        const uint32_t high = R16(c, address);
        return (high << 16) | R16(c, address + 2);
    }
    inline void W8(M68K &c, uint32_t address, uint32_t value) {
        address &= 0xFFFFFF;
        WRITE_HASH(c, address, value);
        if (address >= 0xFF0000) {
            c.ram[address & 0xFFFF] = static_cast<uint8_t>(value);
        } else {
#ifdef DDSOM_NATIVE_BOARD
            if (address - 0x900000u < 0x30000u) { g_videoDirty[(address - 0x900000u) >> 8] = 255; c.video[address - 0x900000u] = uint8_t(value); return; }
            if (address - 0x708000u < 0x8000u) { c.objWrite[address & 0x1FFF] = uint8_t(value); return; }
            if (address - 0x660000u < 0x4000u) { c.extra[address - 0x660000u] = uint8_t(value); return; }
#endif
            c.bus.Write8(address, static_cast<uint8_t>(value));
        }
    }
    inline void W16NoHash(M68K &c, uint32_t address, uint32_t value) {
        if (address >= 0xFF0000) {
            uint8_t *p = &c.ram[address & 0xFFFF];
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            *reinterpret_cast<Alias*>(p) = static_cast<uint16_t>(value);
#else
            p[0] = static_cast<uint8_t>(value >> 8);
            p[1] = static_cast<uint8_t>(value);
#endif
        } else {
#ifdef DDSOM_NATIVE_BOARD
            if (address - 0x900000u < 0x30000u) {
                g_videoDirty[(address - 0x900000u) >> 8] = 255;
                uint8_t *p = c.video + address - 0x900000u;
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint16_t Alias __attribute__((may_alias));
                *reinterpret_cast<Alias*>(p) = uint16_t(value);
#else
                p[0] = value >> 8; p[1] = value;
#endif
                return;
            }
            if (address - 0x708000u < 0x8000u) {
                uint8_t *p = c.objWrite + (address & 0x1FFE);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint16_t Alias __attribute__((may_alias));
                *reinterpret_cast<Alias*>(p) = uint16_t(value);
#else
                p[0] = value >> 8; p[1] = value;
#endif
                return;
            }
            if (address - 0x660000u < 0x4000u) {
                uint8_t *p = c.extra + (address - 0x660000u);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint16_t Alias __attribute__((may_alias));
                *reinterpret_cast<Alias*>(p) = uint16_t(value);
#else
                p[0] = value >> 8; p[1] = value;
#endif
                return;
            }
#endif
            c.bus.Write16(address, static_cast<uint16_t>(value));
        }
    }
    inline void W16(M68K &c, uint32_t address, uint32_t value) {
        address &= 0xFFFFFE;
        WRITE_HASH(c, address, value);
        W16NoHash(c, address, value);
    }
    inline void W32(M68K &c, uint32_t address, uint32_t value) {
        address &= 0xFFFFFE;
        WRITE_HASH(c, address, value);
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        if (address >= 0xFF0000) {
            if ((address & 0xFFFF) <= 0xFFFC) {
                typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
                *reinterpret_cast<Alias32*>(&c.ram[address & 0xFFFF]) = value;
                return;
            }
        } else {
#ifdef DDSOM_NATIVE_BOARD
            if (address - 0x900000u <= 0x2FFFCu) {
                const uint32_t off = address - 0x900000u;
                g_videoDirty[off >> 8] = 255;
                g_videoDirty[(off + 2) >> 8] = 255;
                typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
                *reinterpret_cast<Alias32*>(c.video + off) = value;
                return;
            }
            if (address - 0x708000u < 0x8000u && (address & 0x1FFE) <= 0x1FFC) {
                typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
                *reinterpret_cast<Alias32*>(c.objWrite + (address & 0x1FFE)) = value;
                return;
            }
            if (address - 0x660000u <= 0x3FFCu) {
                typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
                *reinterpret_cast<Alias32*>(c.extra + (address - 0x660000u)) = value;
                return;
            }
#endif
        }
#endif
        W16NoHash(c, address, value >> 16);
        W16NoHash(c, (address + 2) & 0xFFFFFE, value);
    }
#endif

    inline uint32_t Joypad8(M68K &c, uint32_t address) {
#if defined(CPS2RECOMP_N64) && defined(DDSOM_NATIVE_BOARD) && !defined(CPS2RECOMP_CHECK)
        const uint16_t input = ReadNativeJoypad(c, (address - 0x804000u) >> 4);
        return address & 1 ? input & 0xFFu : input >> 8;
#else
        return R8(c, address);
#endif
    }
    inline uint32_t Joypad16(M68K &c, uint32_t address) {
#if defined(CPS2RECOMP_N64) && defined(DDSOM_NATIVE_BOARD) && !defined(CPS2RECOMP_CHECK)
        return ReadNativeJoypad(c, (address - 0x804000u) >> 4);
#else
        return R16(c, address);
#endif
    }
    // Memory as program (what addressing relative to the PC reads): the deciphered ROM
    inline uint32_t P16(M68K &c, uint32_t address) {
        address &= 0xFFFFFE;
        if (address < c.romSize) {
#ifdef CPS2RECOMP_N64
            return RomWord(g_romOpcodes, address);
#else
            const uint8_t *p = &c.opcodes[address];
            return static_cast<uint32_t>((CartByte(p) << 8) | CartByte(p + 1));
#endif
        }
        return c.bus.Fetch16(address);
    }
    inline uint32_t P8(M68K &c, uint32_t address) {
        const uint32_t word = P16(c, address);
        return (address & 1) ? (word & 0xFF) : (word >> 8);
    }
    inline uint32_t P32(M68K &c, uint32_t address) {
        address &= 0xFFFFFE;
#ifdef CPS2RECOMP_N64
        if (address + 4 <= c.romSize) {
            return RomLong(g_romOpcodes, address);
        }
#endif
        const uint32_t high = P16(c, address);
        return (high << 16) | P16(c, address + 2);
    }

#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ && !defined(CPS2RECOMP_CHECK)
    // Acesso a memoria do codigo gerado no N64. Compilado com -Os (o executavel precisa caber na RAM), cada acesso
    // virava a chamada de uma funcao grande, que montava pilha mesmo para a RAM de trabalho (FF0000-FFFFFF, de longe o
    // caso comum). Agora: uma funcao minima resolve a RAM de trabalho sem pilha e so o resto vai para a completa
    // (SlowX = as definicoes acima). Nos arquivos mais quentes (DDSOM_FAST_RAM, Makefile.test) a RAM fica embutida.
    __attribute__((noinline)) inline uint32_t SlowR8(M68K &c, uint32_t a) { return R8(c, a); }
    __attribute__((noinline)) inline uint32_t SlowR16(M68K &c, uint32_t a) { return R16(c, a); }
    __attribute__((noinline)) inline uint32_t SlowR32(M68K &c, uint32_t a) { return R32(c, a); }
    __attribute__((noinline)) inline void SlowW8(M68K &c, uint32_t a, uint32_t v) { W8(c, a, v); }
    __attribute__((noinline)) inline void SlowW16(M68K &c, uint32_t a, uint32_t v) { W16(c, a, v); }
    __attribute__((noinline)) inline void SlowW32(M68K &c, uint32_t a, uint32_t v) { W32(c, a, v); }
    __attribute__((noinline)) inline uint32_t SlowP32(M68K &c, uint32_t a) { return P32(c, a); }
#ifdef DDSOM_FAST_RAM
#define DDSOM_ACCESS __attribute__((always_inline)) static inline
#else
#define DDSOM_ACCESS __attribute__((noinline)) inline
#endif
    DDSOM_ACCESS uint32_t FastR8(M68K &c, uint32_t a) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000, 1)) return c.ram[a & 0xFFFF];
        return SlowR8(c, a);
    }
    DDSOM_ACCESS uint32_t FastR16(M68K &c, uint32_t a) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000, 1)) {
            typedef uint16_t Alias __attribute__((may_alias));
            return *reinterpret_cast<const Alias *>(&c.ram[a & 0xFFFE]);
        }
        return SlowR16(c, a);
    }
    DDSOM_ACCESS uint32_t FastR32(M68K &c, uint32_t a) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000 && (a & 0xFFFE) <= 0xFFFC, 1)) {
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            return *reinterpret_cast<const Alias32 *>(&c.ram[a & 0xFFFE]);
        }
        return SlowR32(c, a);
    }
    DDSOM_ACCESS void FastW8(M68K &c, uint32_t a, uint32_t v) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000, 1)) {
            WRITE_HASH(c, a & 0xFFFFFF, v);
            c.ram[a & 0xFFFF] = static_cast<uint8_t>(v);
            return;
        }
        SlowW8(c, a, v);
    }
    DDSOM_ACCESS void FastW16(M68K &c, uint32_t a, uint32_t v) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000, 1)) {
            WRITE_HASH(c, a & 0xFFFFFE, v);
            typedef uint16_t Alias __attribute__((may_alias));
            *reinterpret_cast<Alias *>(&c.ram[a & 0xFFFE]) = static_cast<uint16_t>(v);
            return;
        }
        SlowW16(c, a, v);
    }
    DDSOM_ACCESS void FastW32(M68K &c, uint32_t a, uint32_t v) {
        if (__builtin_expect((a & 0xFFFFFF) >= 0xFF0000 && (a & 0xFFFE) <= 0xFFFC, 1)) {
            WRITE_HASH(c, a & 0xFFFFFE, v);
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            *reinterpret_cast<Alias32 *>(&c.ram[a & 0xFFFE]) = v;
            return;
        }
        SlowW32(c, a, v);
    }
    DDSOM_ACCESS uint32_t FastP32(M68K &c, uint32_t a) {
        a &= 0xFFFFFE;
        if (__builtin_expect((a >> RomCache::kPageBits) == g_romOpcodes.lastPage &&
                             (a & (RomCache::kPage - 1)) <= RomCache::kPage - 4, 1)) {
            typedef uint32_t Alias32 __attribute__((may_alias, aligned(2)));
            return *reinterpret_cast<const Alias32 *>(g_romOpcodes.lastBase + (a & (RomCache::kPage - 1)));
        }
        return SlowP32(c, a);
    }
#ifndef DDSOM_NO_ACCESS_MACROS // (arquivos com funcoes proprias desses nomes, como rdp_video.cpp, pedem para ficar de fora)
#define R8(c, a) FastR8(c, a)
#define R16(c, a) FastR16(c, a)
#define R32(c, a) FastR32(c, a)
#define W8(c, a, v) FastW8(c, a, v)
#define W16(c, a, v) FastW16(c, a, v)
#define W32(c, a, v) FastW32(c, a, v)
#define P32(c, a) FastP32(c, a)
#endif
#endif

    inline uint32_t S8(uint32_t value) {
        return static_cast<uint32_t>(static_cast<int8_t>(value));
    }
    inline uint32_t S16(uint32_t value) {
        return static_cast<uint32_t>(static_cast<int16_t>(value));
    }
    // A register's low byte or word replaced
    inline void Set8(uint32_t &reg, uint32_t value) {
        reg = (reg & 0xFFFFFF00) | (value & 0xFF);
    }
    inline void Set16(uint32_t &reg, uint32_t value) {
        reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
    }

} // namespace native

} // namespace cps2recomp

// --- how a routine's control flow is written ---

#ifdef DDSOM_NATIVE_TIME
// (no 68000 time: c.cycles counts blocks run, and c.budget is only the watchdog of a run that never goes idle)
#define BLOCK_TICK(c) (++(c).cycles)
#else
#define BLOCK_TICK(c) ((c).cycles)
#endif
// (the routine's registers kept in locals by the translator: written back to the machine where the routine leaves, calls or runs a hook)
#define SPILL_REGS()
#define RELOAD_REGS()
// The start of a block: time is checked here, and nowhere else
#define BLOCK(label, address)                                                                                          \
    label:                                                                                                             \
    if (BLOCK_TICK(c) >= c.budget) {                                                                                        \
        SPILL_REGS();                                                                                                  \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    }
// A block the game's sources have a hook at (lift.h)
#define HOOK(address)                                                                                                  \
    SPILL_REGS();                                                                                                      \
    if (cps2recomp::RunHook(c, address)) {                                                                             \
        return;                                                                                                        \
    }                                                                                                                  \
    RELOAD_REGS();
// Leaving the routine for the block at `address`
#define LEAVE(address)                                                                                                 \
    do {                                                                                                               \
        SPILL_REGS();                                                                                                  \
        c.pc = address;                                                                                                \
        return;                                                                                                        \
    } while (0)

#ifdef CPS2RECOMP_CHECK
// (the check build runs one block per call)
#define GOTO(label, address) LEAVE(address)
#define FALL(address) LEAVE(address)
#define CALL(fn, address, label, back) LEAVE(address)
#define CALL_AWAY(fn, address) LEAVE(address)
#define TAIL(fn, address) LEAVE(address)
#else
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(gnu::musttail)
#define TAIL_RETURN(call) [[gnu::musttail]] return call
#endif
#endif
#ifndef TAIL_RETURN
#define TAIL_RETURN(call) return call
#endif
// To a block of this routine
#define GOTO(label, address) goto label
// To a block of another routine, known when translated: the routine goes on from there, in place of this one
#define TAIL(fn, address)                                                                                              \
    do {                                                                                                               \
        SPILL_REGS();                                                                                                  \
        c.pc = address;                                                                                                \
        TAIL_RETURN(fn(c));                                                                                            \
    } while (0)
// Running on into the next block
#define FALL(address)
// A call of the game's (the return address is already on its stack) to the block at `address`, in routine
// `fn`; `label` is the block after the call, at `back`. If the routine comes back anywhere else (its time ran
// out, it switched stacks, it dropped its return address), so does this one.
#define CALL(fn, address, label, back)                                                                                 \
    do {                                                                                                               \
        c.pc = address;                                                                                                \
        SPILL_REGS();                                                                                                  \
        if (c.depth >= cps2recomp::native::kMaxDepth) {                                                                \
            return;                                                                                                    \
        }                                                                                                              \
        c.depth++;                                                                                                     \
        fn(c);                                                                                                         \
        c.depth--;                                                                                                     \
        RELOAD_REGS();                                                                                                 \
        if (c.pc != back) {                                                                                            \
            return;                                                                                                    \
        }                                                                                                              \
        goto label;                                                                                                    \
    } while (0)
// The same where the block after the call is not in this routine
#define CALL_AWAY(fn, address) LEAVE(address)
#endif
