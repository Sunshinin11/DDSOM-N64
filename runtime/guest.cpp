// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The guests' boards (guest.h).
#include "guest.h"

#include <map>
#include <memory>
#include <vector>

namespace cps2recomp {

namespace {
    // A guest: its set, its machine and spare, its hooks. (Never freed: a spare may run on a thread of the game's
    // sources' own until the program is gone.)
    struct One {
        RomSet roms;
        bool loaded = false;
        std::unique_ptr<Machine> machine;
        Machine *spare = nullptr;
        std::vector<uint8_t> hooked; // a byte a word of its ROM: a hook there
        std::map<uint32_t, bool (*)(M68K &c)> hooks;
    };
    std::map<std::string, One *> &Guests() {
        static auto &guests = *new std::map<std::string, One *>;
        return guests;
    }
    One &Named(const std::string &id) {
        One *&one = Guests()[id];
        if (!one) {
            one = new One;
        }
        return *one;
    }

    bool RunGuestHook(M68K &c, uint32_t address) {
        const One &one = *static_cast<const One *>(c.ownContext);
        const auto found = one.hooks.find(address);
        if (found == one.hooks.end()) {
            return false;
        }
        c.pc = address;
        return found->second(c);
    }

    std::unique_ptr<Machine> MakeMachine(One &one) {
        auto made = std::make_unique<Machine>(one.roms);
        Machine &m = *made;
        m.cpuMode = M68K::Mode::Interpreter;
        // (the sound program as ordinary code where there is one for it, as the port's own runs; else the Z80's own code)
        m.soundSide = m.soundDriver ? Machine::SoundSide::Driver : Machine::SoundSide::Z80;
        m.cpu.ownHooked = one.hooked.data();
        m.cpu.ownHookedSize = static_cast<uint32_t>(one.roms.program.size());
        m.cpu.ownHook = RunGuestHook;
        m.cpu.ownContext = &one;
        m.Reset();
        return made;
    }
} // namespace

bool LoadGuest(const GuestGame &game, const std::string &path, std::string &error) {
    One &one = Named(game.id);
    const RomList list = ParseRomList(game.program, game.graphics, game.soundProgram, game.samples, game.key, game.set, game.set);
    if (!LoadRomSet(path, list, one.roms, error) && !LoadRomSet(path + ".zip", list, one.roms, error)) {
        return false;
    }
    one.hooked.assign(one.roms.program.size() / 2 + 1, 0);
    for (const auto &[address, fn] : one.hooks) {
        if (address / 2 < one.hooked.size()) {
            one.hooked[address / 2] = 1;
        }
    }
    one.loaded = true;
    one.machine = MakeMachine(one);
    return true;
}

Machine *Guest(const std::string &id) {
    const auto found = Guests().find(id);
    return found != Guests().end() && found->second->loaded ? found->second->machine.get() : nullptr;
}

Machine *GuestSpare(const std::string &id) {
    const auto found = Guests().find(id);
    if (found == Guests().end() || !found->second->loaded) {
        return nullptr;
    }
    One &one = *found->second;
    if (!one.spare) {
        one.spare = MakeMachine(one).release(); // (never freed: as above)
    }
    return one.spare;
}

void AddGuestHook(const std::string &id, uint32_t address, bool (*fn)(M68K &c)) {
    One &one = Named(id);
    one.hooks[address] = fn;
    if (address / 2 < one.hooked.size()) {
        one.hooked[address / 2] = 1;
    }
}

} // namespace cps2recomp
