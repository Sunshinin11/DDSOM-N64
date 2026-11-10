// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Other games' stages behind a fighting game's fighters, as those games show them. Each other game ("a guest") is a
// board of its own run beside the game's (runtime/guest.h; game.cfg's guests), held in a two-player match on the
// stage, its fighters left standing and never drawn, its clock held, its camera put where the game's is every frame.
// The picture is the guest's stage with the game's fighters and panel over it (drawn alone, marked:
// Video::markObjects); the music is the guest stage's own, in place of the game's stage tune (Soundtrack::Take), with
// the game's fades.
//
// Each stage's round is made once, on a spare board of the guest (GuestSpare) and a thread of its own, from the
// program's start: the board played with made-up controls (credits, both Starts, a punch at each screen that waits for
// one) into a two-player match on the stage, to the round. That is the same board, frame for frame, every time, so a
// match on a stage always starts from it, however long ago it was made: the same in a run played straight, its frames
// run again (online play's rollback), a save state loaded or the other player's game. A match that wants a round not
// made yet waits for it. The guest's board is kept with the game's state.
//
// What a game says of itself (Host) and of each guest (Guest) is all of it: a guest is described once, by the facts
// its own port found (its steps, its credits, where it sets a match's stage, its clock, its stage's objects, its
// camera), and can then be shown by any game. Vampire Savior Definitive shows Darkstalkers' and Street Fighter Alpha
// 2's (games/vsav_definitive/src/stages.cpp; the guests' facts in games/dstlk/guest.h, games/sfa2/guest.h).
#pragma once

#include "../machine/m68k.h"

#include <cstdint>
#include <vector>

namespace cps2recomp {
class Machine;
}

namespace cps2recomp::guest_stages {

// A word (or a byte) of the guest's memory and the value it has
struct Is {
    uint32_t address;
    uint16_t value;
    bool byte = false;
};

struct Guest {
    const char *id;                       // its game's id (game.cfg's guests)
    std::vector<const char *> stageNames; // its stages, by its own numbers from 0, as the lists name them
    // Its way to a round: its character choice is on, both players in; its match is on (the made-up controls stop);
    // a round of it is fought (each: all of these)
    std::vector<Is> choosing, match, fought;
    uint32_t credits = 0;  // a byte, kept at 2 on the way
    uint32_t playersIn = 0; // a byte: 3 with both players in
    // Where it sets a match's stage: a hook there, at which `setStage` gives it the stage (0 the first)
    uint32_t stageAt = 0;
    void (*setStage)(M68K &c, int stage) = nullptr;
    // Its round's clock (a byte), held at `clockFull`
    uint32_t clock = 0;
    uint8_t clockFull = 0x99;
    // Its objects that are not its stage's (its fighters, its panel): not drawn. And the scroll screens drawn (layerMask)
    bool (*notTheStage)(uint32_t tile, uint16_t attributes) = nullptr;
    unsigned layers = 0xF;
    // Its camera's ends across: the game's camera's place goes to the same place between them; and how much lower
    // (in pixels) its camera is put than the game's
    int cameraLeft = 256, cameraRight = 640, lower = 0;
    // How its camera is put where it is wanted, by one of:
    // - Steps: the routines that step its camera (an object, A6) across and up, done here: the camera's place
    //   (words), the step it took (words: the scroll screens behind move by it) and which way (bytes)
    // - Extents: the routines its camera asks where the players are (D0 and D1: their outer edges across, their
    //   heights), answered so that its own camera goes where it is wanted (as far as it goes in a frame)
    enum class Camera { Steps, Extents } camera = Camera::Steps;
    uint32_t across = 0, up = 0;
    uint32_t placeX = 0x10, placeY = 0x14, stepX = 0x34, stepY = 0x36, wayX = 0x2C, wayY = 0x2D;
    uint8_t wayRight = 4, wayLeft = 0, wayUp = 0x0C, wayDown = 0x08;
    int middle = 0xC0, halfApart = 0x60, above = 0x60; // (Extents: the picture's middle; the players' edges; height)
};

struct Host {
    uint32_t cameraX = 0, cameraY = 0; // the game's camera's place (words)
    int cameraLeft = 256, cameraRight = 640;
    bool (*shown)() = nullptr;  // a frame in which the stage is shown (a match's, not its versus screen; the viewer's)
    bool (*viewer)() = nullptr; // the stage viewer: nothing of the game's own is drawn
    bool (*notTheStage)(uint32_t tile, uint16_t attributes) = nullptr; // the game's objects that are not its stage's
    std::vector<uint16_t> stageTunes; // its stages' tunes: the guest's music plays in their place
    // The stage wanted now: 1 the first guest's first, on through each guest's in turn (0: the game's own)
    int (*wanted)() = nullptr;
    // A feature with which the guests' stages are made ready from the start (else from the first wanted)
    const char *feature = nullptr;
};

// The stages of all the guests, in turn (whether their sets are there or not), and whether one can be shown
int Stages();
const char *StageName(int stage); // (1 the first)
bool Available(int stage);
// The game's sources describe themselves and their guests, after the rest of their registering (this goes around the
// frame's callbacks and the state's)
void Register(const Host &host, std::vector<Guest> guests);

} // namespace cps2recomp::guest_stages
