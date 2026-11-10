// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Tag-team play for more people than the board's two panels: each character of a team played by a person of its own,
// in the tag-team Vs games. "Co-op vs. computer": players 1 and 2 are one team (player 1's side), against the computer.
// "2 vs 2": players 1 and 3 are player 1's side, players 2 and 4 player 2's.
//
// The game reads only players 1 and 2, so this routes: each frame, before the game runs, it decides whose controls
// go into each side's panel, from the game's own memory (the game's sources say where, in a Description):
//   - choosing characters: the side's first pick is its first person's, the second its second person's (only the one
//     choosing moves the cursor); what is chosen after both (the speed, say) is the first person's.
//   - in a match: whoever plays the character on point, as the game's own byte for it says (a tag, a knock-out, a
//     partner coming in for a team super all change it; this never guesses). The partner off point tags in by pressing
//     the tag buttons together (the game's own, HP+HK): the side's panel gets them. The person on point may still tag
//     out as the game lets them (the tag buttons pass, as do the team supers made with them).
//   - anywhere else (the title, the continue): the first person's controls, and the second's Start and coins.
//   - "Co-op vs. computer": player 2's panel is never pressed, so nobody comes in on the computer's side; a team
//     person's Start or coin is player 1's.
// Players 3 and 4 never reach the board's own lines (a six-button game reads player 1's and 2's kicks there).
//
// Over the picture, under each side's bars while a match is on, the person playing the character on point (P1-P4),
// beside what else the game's sources show there.
// The page's choice is a feature on the Alternative modes page (or a page the game names), kept in settings.ini. The
// modes of the sources' own (training, attract mode, the viewers, the battle modes) keep the board's two players as
// they are, unless the game says otherwise (Description::otherMode: the team battle, battle.h).
#pragma once

#include "machine.h"

#include <cstdint>

namespace cps2recomp::tag {

constexpr const char *kFeature = "tag_team";
enum Mode { kOff, kCoop, kVersus };

struct Description {
    // A side's character on point in a match: which of its picks (0 the one chosen first, 1 the second); -1 with no
    // match on
    int (*onPoint)(int side) = nullptr;
    // A side choosing its characters: the pick being chosen (0, 1), 2 once both are taken; -1 when it is not choosing
    int (*picking)(int side) = nullptr;
    // A round is being fought (the partner's tag works only then)
    bool (*fighting)() = nullptr;
    // A mode of the sources' own is on (it works the controls itself): no routing
    bool (*otherMode)() = nullptr;
    // The buttons pressed together that tag the partner in (0: there is no tagging in)
    uint16_t tagButtons = kPunch3 | kKick3;
    // Where the labels go, as parts of the picture (under each side's bars)
    float labelY = 0.2f, labelLeft = 0.02f, labelRight = 0.98f;
    // ... or, where the picture changes width, the ends asked for each frame
    void (*labelEnds)(float &left, float &right) = nullptr;
    // What the menu says of the modes (nullptr: as for a tag-team Vs game), and the page they are on (nullptr: the
    // Alternative modes page)
    const char *help = nullptr, *page = nullptr;
};

void Register(const Description &description);
Mode Chosen();
// The person (0-3) who plays a side's pick in the mode chosen, or -1 (the computer)
int Person(int side, int pick);

// Before the port's machine runs a frame (first in the game's beforeFrame: the others see the routed panels), and after
void BeforeFrame(Machine &machine);
void Frame(Machine &machine);

} // namespace cps2recomp::tag
