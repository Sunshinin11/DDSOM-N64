// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Boards of other games, run beside the game's and never seen as they are (game.cfg's guests: those games' ids): for a
// game whose sources show something of another game in their own (its stages behind their fighters, say:
// fight/guest_stages.h). Each is a whole machine of its own, with that game's ROM set: its 68000 runs on the interpreter
// (the translated code is the game's), with hooks of its own at that game's addresses, and its sound side as the
// board's. What the game's sources take of it (its picture, its sound) is theirs to put together; its state is theirs
// to keep with their own (online play runs frames again from a save state, and a guest must come back with them).
//
// Every set is optional: a game with guests plays without them, and its sources leave out what needs one.
#pragma once

#include "../machine/machine.h"
#include "../machine/romset.h"

#include <cstdint>
#include <string>

namespace cps2recomp {

// A guest's game, as its game.cfg has it (tools/gamecfg.py writes them into the game's header: GAME_GUEST_TABLE)
struct GuestGame {
    const char *id, *name, *set, *program, *graphics, *soundProgram, *samples, *key;
};

// The port loads a guest's set (from --guest ID=PATH, or beside --rom); false, and `error` why, when it is not there
bool LoadGuest(const GuestGame &game, const std::string &path, std::string &error);
// A guest's machine (nullptr: its set is not loaded)
Machine *Guest(const std::string &id);
// Another of the same (nullptr: no set), made when first asked for: for the game's sources to run ahead of what they
// need (a guest's board taken somewhere and kept, before the game is there), outside the game's state; it may be run on
// a thread of their own (it shares nothing with the guest but the ROM set and the hooks, which do not change once the
// game runs, and it is never freed)
Machine *GuestSpare(const std::string &id);
// A hook in a guest's code (as a game's own: runtime/lift.h), at that game's address. Hooks are added before the game
// runs (whether the set is loaded yet or not) and not changed after.
void AddGuestHook(const std::string &id, uint32_t address, bool (*fn)(M68K &c));

} // namespace cps2recomp
