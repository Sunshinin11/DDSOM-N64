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

// A block's start and the routine it is in; and the stretch of program a block covers
struct NativeEntry {
    uint32_t address;
    NativeFn fn;
};
struct NativeRange {
    uint32_t start, end;
};
struct NativeCode {
    const NativeEntry *entries;
    size_t entryCount;
    const NativeRange *ranges;
    size_t rangeCount;
    const uint32_t *hooks; // the addresses at which the translated code stops by for a hook (lift.h)
    size_t hookCount;
    // Set by the build without translated code only (runtime/no_blocks.cpp): words a release's check looks for in a
    // program (tools/release/check_archive.sh), to tell that build from a release's
    const char *none = nullptr;
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

namespace native {

    constexpr unsigned kMaxDepth = 64;
#ifdef DDSOM_NATIVE_TIME
    // (what a run of the game wrote, in order: a pass of the scheduler that writes what the one before it did, from the same
    // registers, changes nothing: the game is waiting; port_n64/board.cpp)
#define WRITE_HASH(c, address, value) ((c).writeHash = ((c).writeHash + (address)) * 31u + (value))
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
        if(address-0x900000u<c.videoSize)return c.video[address-0x900000u];
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
        if(address-0x900000u<c.videoSize){
            const uint8_t*p=c.video+address-0x900000u;
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));return *reinterpret_cast<const Alias*>(p);
#else
            return uint32_t(p[0])<<8|p[1];
#endif
        }
#endif
        return c.bus.Read16(address);
    }
    inline uint32_t R32(M68K &c, uint32_t address) {
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
            if(address-0x900000u<c.videoSize){c.video[address-0x900000u]=uint8_t(value);return;}
#endif
            c.bus.Write8(address, static_cast<uint8_t>(value));
        }
    }
    inline void W16(M68K &c, uint32_t address, uint32_t value) {
        address &= 0xFFFFFE;
        WRITE_HASH(c, address, value);
        if (address >= 0xFF0000) {
            uint8_t *p = &c.ram[address & 0xFFFF];
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            typedef uint16_t Alias __attribute__((may_alias));
            *reinterpret_cast<Alias*>(p)=static_cast<uint16_t>(value);
#else
            p[0] = static_cast<uint8_t>(value >> 8);
            p[1] = static_cast<uint8_t>(value);
#endif
        } else {
#ifdef DDSOM_NATIVE_BOARD
            if(address-0x900000u<c.videoSize){
                uint8_t*p=c.video+address-0x900000u;
#if defined(CPS2RECOMP_N64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                typedef uint16_t Alias __attribute__((may_alias));*reinterpret_cast<Alias*>(p)=uint16_t(value);
#else
                p[0]=value>>8;p[1]=value;
#endif
                return;
            }
#endif
            c.bus.Write16(address, static_cast<uint16_t>(value));
        }
    }
    inline void W32(M68K &c, uint32_t address, uint32_t value) {
        W16(c, address, value >> 16);
        W16(c, address + 2, value);
    }
#endif

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
        const uint32_t high = P16(c, address);
        return (high << 16) | P16(c, address + 2);
    }

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
