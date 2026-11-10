// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The engine of Capcom's Marvel fighting games on the board: Marvel Super Heroes, X-Men vs. Street Fighter and Marvel
// Super Heroes vs. Street Fighter (each made from the one before: tools/portmap.py places much of its code in the
// other), and their kin. Their players' records, cameras, boxes and animations work alike, at other places and offsets
// in each game; what their sources would otherwise each write the same is here, from a description of where a game
// keeps it (Engine):
//
//   - the training session's view of a player at its step (frame data), a hit landing, and the boxes as the game's
//     hit tests read them, drawn through the game's camera (fight/session.h)
//   - the ring out's own movements at a player's step (fight/battle.h)
//   - widescreen and the invisible walls: the camera kept between its ends and a player kept inside the picture with
//     the wide picture's margin, and the picture made wide (Video::wide)
//   - the model viewer: a round of a two-player match reached unseen, the second player put out of the way, the
//     camera following the first, and the character's animations as the game keeps them (model/model.h)
//
// A game's sources describe it (Describe) before registering these, and give the places of the hooks these make as
// HookAt(0x...), so that the translator finds them in the game's sources (runtime/lift.h). The modes' and the session's own descriptions stay in the game's sources (they are already a
// description of the game to the toolkit); they name the functions here where these do the work.
#pragma once

#include "fight.h"
#include "game.h"
#include "lift.h"
#include "machine.h"

#include <cstdint>
#include <vector>

namespace cps2recomp {
class Archive;
}

namespace cps2recomp::marvel {

struct Engine {
    // --- the players ---
    // The records the game steps: the players' (a tag-team game's point characters are the first two), `records`
    // of them, `playerSize` apart
    uint32_t player1 = 0xFF4000, playerSize = 0x400;
    int records = 2;
    // A tag-team game that keeps a side's two characters in records of their own (not swapped at a tag): the record of
    // the side's character on point now (none: the first two records)
    uint32_t (*playerAt)(int n) = nullptr;
    // A player's record: its state (a word: 2 fighting), who is in charge of it (a word: 0 itself), where it stands
    // (longs, 16 bits of fraction, down growing downwards), its speeds across and down and the pull down (longs),
    // the frame of its animation (a long, in the ROM), which way it faces (a byte: bit 0 facing right, bit 1 upside
    // down), who it is (a word, even), what it is doing (a byte), its attack's phase (a byte: 80 out, 40 recovering),
    // the steps it stays frozen as a hit lands (a byte), and how near the picture's edge it is held (a byte)
    uint32_t state = 0x04, control = 0x06, x = 0x0C, y = 0x10, speedX = 0x14, speedY = 0x1C, pull = 0x20;
    uint32_t frame = 0x34, flip = 0, character = 0, action = 0xA0, attackPhase = 0, hitStop = 0, atEdge = 0xA9;
    // The bits of the byte `attackPhase` that say an attack is going on (its phases: out, recovering)
    uint8_t attackPhaseMask = 0xC0;
    // What a player does, by the byte `action`
    uint8_t actionStand = 0x00, actionWalk = 0x02, actionJump = 0x06, actionCrouch = 0x0C, actionGuard = 0x24,
            actionCrouchGuard = 0x26;
    // The boxes: a long, the table of the character's boxes (eight bytes each: the middle across from where it stands
    // and half the width, then the same up and down, signed words); words, each a box's number in it (the low 13 bits,
    // 0 none; the high bit a throw's reach): two attacks, then four places it can be hit; a byte, its push box's number
    // in `pushTable` (then the character's own of it, four bytes a character); bytes not 0 while its attacks can hit and
    // while it can be hit
    uint32_t boxTable = 0, attackBoxes = 0, hurtBoxes = 0, pushBox = 0, canHit = 0, canBeHit = 0;
    uint32_t pushTable = 0;
    // What a player throws: a list of records (longs, the newest last) ending at `thrownList`, player 2's
    // `thrownListSpace` on; the list ends where a long is not a record
    uint32_t thrownList = 0, thrownListSpace = 0;
    // The control panel as the game read it this frame, a word a player: the stick (bit 0 right, 1 left, 2 down, 3 up)
    // and the punches (bits 4-6) in the low byte, the kicks in bits 8-10
    uint32_t panel[2] = {0, 0};
    // The characters' names, by their word `character` / 2
    std::vector<const char *> characterNames;
    std::vector<const char *> stageNames;
    // The model viewer's order of the characters, by their word `character` / 2 (none: the game's numbers' order)
    std::vector<int> viewerOrder;

    // --- the stage and the camera ---
    // The camera's record by the stage that is on, and in it: its place across (the picture's left edge less
    // `pictureLeft`) and down (the picture's top), and its ends across and down (words)
    uint32_t (*cameraRecord)() = nullptr;
    uint32_t cameraX = 0x0C, cameraY = 0x10, cameraLeft = 0x72, cameraRight = 0x74, cameraTop = 0x76;
    int pictureLeft = 0x40;
    // Where the camera's step works on its place, if not in cameraX and cameraY themselves: a copy of them it makes as
    // it starts (words), kept between the ends and put back at its end (0: none). With a copy, the step keeps it between
    // its ends across and then down as two pieces of its own code: the second's place (a hook: RegisterWidescreen's
    // keepCamera is the first's), and where each piece is over (the copy put back)
    uint32_t cameraWorkX = 0, cameraWorkY = 0;
    uint32_t keepCameraDown = 0, keptAcross = 0, keptDown = 0;
    // Where the camera's place is kept between its ends, where that is not one routine but a piece of each stage's own
    // (RegisterWidescreen's keepCamera then 0): the piece's place (a hook), and where the stage's code goes on after it
    // (0: the piece ends the routine)
    struct KeepCameraPlace {
        uint32_t at, after;
    };
    std::vector<KeepCameraPlace> keepCameraPlaces;
    // How near the picture's edges a player can go (the game keeps it between the camera's word + 0x55 and + 0x1AB),
    // and a player's plane (a byte) and the table of the planes' cameras (a long by the plane: the camera word's place)
    int wall = 0x15;
    uint32_t plane = 0x30, planes = 0;
    // The floor's height on the stage that is on (where a player stands when not in the air)
    int (*ground)() = nullptr;
    // A stage's camera ends across (by the game's stage number), and the byte that is the stage
    void (*cameraEnds)(int stage, int &left, int &right) = nullptr;
    uint32_t stage = 0;
    // How the camera moves, a byte: `followsOne` after player 1 alone; a second byte like it two on, which the game
    // goes by in some fights (none: one)
    uint32_t cameraWay = 0;
    uint8_t followsOne = 4;
    bool cameraWayTwice = false;
    // A byte like `cameraWay` that the camera's step down goes by, where that is a piece of its own (none: one)
    uint32_t cameraWayDown = 0;
    // The round's clock (a BCD byte)
    uint32_t clock = 0;

    // --- the game ---
    // A game is being played; a round is being fought (the picture is a fight's)
    bool (*playing)() = nullptr;
    bool (*inFight)() = nullptr;
    // Objects by what they are (Video::hideObject): all but the stage's pieces; the panel
    bool (*notTheStage)(uint32_t tile, uint16_t attributes) = nullptr;
    bool (*thePanel)(uint32_t tile, uint16_t attributes) = nullptr;
    // The colour sets player 1 and player 2 are drawn in (each `palettes` of them), and the shadows'
    unsigned player1Set = 0, player2Set = 0, shadowSet = 0;
    int palettes = 3;

    // --- the animations (the model viewer) ---
    // A long a character (by its word `character`, twice): its groups of animations, each a word of offsets to its
    // animations; an animation is frames of `frameBytes`, each a word of its picture, a word of how many frames of the
    // game it is shown for (its top bit: the frame after it is the one the long after it names), and the picture (a
    // long); `groups` groups of at most `perGroup`. The ROM's code and data from `romLow` to `romHigh`.
    uint32_t animationLists = 0, frameBytes = 0x10;
    // Where a frame has its time (a word) and its picture (a long, under `framePictureMask`), from its start
    uint32_t frameTimeAt = 0x02, framePictureAt = 0x04, framePictureMask = 0xFFFFFFFF;
    int groups = 26, perGroup = 96;
    uint32_t romLow = 0x080000, romHigh = 0x380000;
    // A frame put in a player's record as the game's own routine puts it (what it copies of the frame, beyond what is
    // the same in every game of the engine: the frame's place, its time and the frame after it)
    void (*copyFrame)(Machine &machine, uint32_t player, uint32_t frame) = nullptr;
    // The time left of a frame (a word) and the frame after it (a long), in a player's record
    uint32_t frameTimer = 0x3E, nextFrame = 0x38;
};

// A frame put in a player's record as X-Men vs. Street Fighter's 001AE6 puts it, and its successors' (Marvel Super
// Heroes vs. Street Fighter's 001E08): a word for its picture, then (after its time) the picture, two words more, and
// the number of its set of boxes in the character's table of them (16 bytes a set, the long at the record's +68),
// copied to the record's box numbers (+70 on). For Engine::copyFrame.
void CopyFrameBoxSets(Machine &machine, uint32_t player, uint32_t frame);

// The game's description, before anything else here is used
void Describe(const Engine &engine);
const Engine &Described();

// --- the players ---

uint32_t Player(int n);
// Which of the two players' records this is (0, 1), or -1
int PlayerOf(uint32_t record);
bool InAir(uint32_t player);
bool FacingLeft(int n);
// What the game read off the panel for a player this frame, as Button bits
uint16_t PanelHeld(int n);
const char *CharacterName(uint32_t player);

// --- the training session (fight/session.h) ---

// The hook at a player's step, as the game goes on to what its record's state says (A6 the record): the ring out's
// own movements, then the frame data's view of it
bool PlayerStep(M68K &c);
fight::MoveLog::Start MoveStart(int n);
std::vector<fight::StageBox> Boxes(Machine &machine);
void Ends(int &stageLeft, int &stageRight, int &cameraLeft, int &cameraRight);
uint32_t CameraXAt();
uint32_t CameraYAt();
constexpr int kFloorLine = 223; // the session's floor in the picture (fight::ToPicture)

// --- the fighting modes (fight/battle.h) ---

// The ring out's own movements at a player's step (A6): true when it has made the step, the game's own routine for
// the player not run
bool RingOutStep(M68K &c);

// --- widescreen ---

// The hooks: where the camera's step keeps it between its ends (A6 the camera's record; 0 where the stages each keep their
// own: Engine::keepCameraPlaces), and where a player is kept inside the picture of its plane (A6 the player's record);
// and the features
void RegisterWidescreen(uint32_t keepCamera, uint32_t keepIn);
void WidescreenBeforeFrame(Machine &machine);
void WidescreenState(Archive &ar); // (in the game's save state)
bool WidePlay();
int WideSide();
int WideStage();
int WideMargin();
void WideForSpare(Machine &machine);
void WideForPort(Machine &machine);

// --- the model viewer ---

// The hook where the camera's step goes the way the byte `cameraWay` says (A6 the camera's record), and where it goes
// down, where that is a piece of its own after the first (0: none); and the viewer's page; the words of the page are
// the game's (model::Description's pageWords and the rest are filled in here)
void RegisterModel(uint32_t cameraWayAt, int largeLift, uint32_t cameraWayDownAt = 0);
bool ModelViewer();
void StartModelViewer(Machine &machine);
void ModelBeforeFrame(Machine &machine);
void ModelFrame(Machine &machine);
void ModelState(Archive &ar);

} // namespace cps2recomp::marvel
