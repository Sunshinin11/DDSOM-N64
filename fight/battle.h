// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The fighting modes of a fighting game's "Alternative modes" page, for every game on the board that has them.
// Each is a two-player match that is set up (who fights, where, and which of the two the game itself plays),
// watched, and followed with another: the next match is made ready on a spare machine (runtime/spare.h) while
// the last one's ending plays, and then takes its place.
//
//   Survival        the player against one opponent after another, with the health left from the fight
//                   before; over at the first knock-down, with the count of opponents beaten.
//   Endless battle  two fighters the game plays, with no panel; the one knocked down is replaced. A person
//                   takes a fighter over by touching the controls, and gives it back by leaving them alone.
//                   Started by the launcher's Multi game page (--multi endless:shuffle:SECONDS or
//                   endless:scroll:SPEED; runtime/game.h, MultiGame), it is begun at once and shows every stage
//                   once (shuffled and faded into one another, or scrolling by in order), then fades out and ends
//                   the program with kNextGame, for the next game's turn.
//   Ring out        the player against one opponent after another, with no health: a stage's ends are open,
//                   and whoever is knocked over one is thrown out and falls through the floor. The winner jumps
//                   off, and the camera follows it down onto the next stage.
//   Team battle     two teams of two or three, chosen on the menu: one fighter of each side at a time, the one
//                   knocked down out and the next of that side in, with full health; the winner stays on with
//                   the health it has left. Over when a side has nobody left. Side 2 is a person or the game; with
//                   the tag-team modes (tag.h) each fighter of a side may be a person of its own. Its tag style
//                   (a game that can swap a fighter mid-fight): each fighter has a bar of its own, Tag (a gamepad's left trigger) or Start tags
//                   the next one in whenever the fighter on can act, and a bar emptied brings the next in.
//
// A game's sources describe where the game keeps what these need (Description) and register it; they also
// make the ring out's own movements of a player at its step of a fight (RingOutMove) and let a player knocked
// about go over a stage's end (RingOutLetsOut), where the game keeps the players in.
#pragma once

#include "game.h"
#include "machine.h"

#include <cstdint>
#include <vector>

namespace cps2recomp {

class Archive;

// What the machine that is running is to fight when the game next makes a match (the game's sources hook where
// the game chooses them): the stage (1 the first; 0: the game's choice), the two characters and their colours
// (-1: the ones chosen at the character choice). Whoever runs a machine sets this first.
struct MatchWanted {
    int stage = 0;
    int characters[2] = {-1, -1};
    int colours[2] = {-1, -1}; // which of a character's sets of colours (-1: as chosen)
    int partners[2] = {-1, -1}; // a tag-team game's second character of each side (-1: as chosen)
};

// Where the machine that is running is to have its camera (the picture's left edge and bottom, in the stage),
// whatever the players do: -1 leaves that to the game (its camera routines ask). Kept within the stage's ends,
// unless `beyondEnds` (the ring out's way down: the camera above a stage, for the fighters alone).
struct CameraWanted {
    int x = -1, y = -1;
    bool beyondEnds = false;
};

// The ring out's own movements of a player, made by the game's sources at the player's step of a fight: flung out
// over the end of the stage (at the first step; `side` -1 the left end, 1 the right), falling from there (the
// game's routine for the player not run), or (at one step) put in the next match in the air as it was in its
// jump over the old stage: at `x`, `y`, the rest of it as `record` has it (the player's record on the old stage,
// the description's playerSize bytes; which of it to take is the game's)
struct RingOut {
    enum Move { kNone, kFlungOut, kFalling, kDropsIn } move = kNone;
    int side = 0;
    int x = 0, y = 0;
    const uint8_t *record = nullptr;
};

namespace battle {

constexpr uint32_t kMostRecord = 0x400; // the most bytes of a player's record (kept for the way down)

// The random number of the Alphas and the Vampire Savior games (sfa's 016462): three times itself, the top byte of
// that its new top byte, and added to its bottom one
inline uint16_t CapcomRandomNext(uint16_t random) {
    const uint8_t top = static_cast<uint8_t>((random * 3) >> 8);
    return static_cast<uint16_t>(top << 8 | static_cast<uint8_t>(random + top));
}

struct Description {
    // --- the game's memory ---
    uint32_t result = 0;            // byte: the round's result: 0 still going, 1 player 1 has won, 2 player 2, 3 neither
    // ... or, for a game that keeps it some other way, the same worked out from its memory (nullptr: the byte)
    uint8_t (*resultNow)() = nullptr;
    int roundsToWin = 2;            // rounds a player wins a match with (the board's usual: online play's win streaks)
    uint32_t cameraX = 0, cameraY = 0; // words: where the camera is (the picture's left edge and bottom)
    int cameraLeft = 256, cameraRight = 640; // the camera's ends across on every stage (the board's picture)
    // A camera whose words move (with the stage, say): their addresses now; and stages whose camera's ends differ:
    // the stage's that is running (none: the above)
    uint32_t (*cameraXAt)() = nullptr;
    uint32_t (*cameraYAt)() = nullptr;
    void (*cameraEnds)(int &left, int &right) = nullptr;
    // The players' records, and the records of what they throw (a byte `active` not zero for one in use); the
    // players' first, their objects' places across (longs, 16 bits of fraction) at `x`
    uint32_t player1 = 0, playerSize = 0;
    int playerRecords = 0;
    // A tag-team game that keeps a side's two characters in records of their own (not swapped at a tag): the records
    // of the side's character on point now and of its partner (none: player1 and the one after it, and `partner` on)
    uint32_t (*playerAt)(int n) = nullptr;
    uint32_t (*partnerAt)(int n) = nullptr;
    uint32_t thrown = 0, thrownSize = 0;
    int thrownRecords = 0;
    uint32_t active = 0, x = 0, y = 0;
    // A player's: who it is (byte), a byte set to 1 while the game plays it, who is in charge of it (byte;
    // `controlOwn` the player itself), half its width (word)
    uint32_t character = 0, computer = 0, control = 0, halfWidth = 0;
    uint8_t controlOwn = 0;
    // ... or, for a game that keeps no such byte, whether the player is its own master, worked out from its memory
    // (nullptr: the byte)
    bool (*ownMaster)(uint32_t player) = nullptr;
    // Whether who plays the player (the computer byte) may change now (nullptr: at any time): for a game whose computer
    // and person work a move under way by bytes of their own, so that the one that began it ends it
    bool (*handOver)(uint32_t player) = nullptr;
    // The computer byte is 0 for the game playing it, 1 for a person (not the other way round)
    bool computerIsZero = false;
    // A tag-team game's: how far after a player's record its partner's is (0: none), told the same of who plays it
    uint32_t partner = 0;
    // A game whose camera's word across is not the picture's left edge (the edge is this far on from it), whose heights
    // grow downwards (the camera's word down is the picture's top), and which keeps no word of a player's half width
    // (halfWidth 0: this)
    int pictureLeft = 0;
    bool yDown = false;
    int fixedHalfWidth = 0;
    // Bytes set to 2 for both players while a mode runs (the game counts down for one not set), or 0: none
    uint32_t plays = 0;
    // A byte: which of its sets of colours the player chose (kept from match to match), or 0: none
    uint32_t colours = 0;
    // A word: the game's random number (a match begun from the character choice is given its own, or every one
    // would play out the same way from there), or 0: none
    uint32_t random = 0;
    // How the game's routine steps it on, or none. With it, a match's own is the game's stepped on so many times at
    // random: never a word the game's routine would not come to (without it, a word at random), as one of the few it
    // keeps going round between two, or one, and every choice made by it the same (a script of the computer's
    // breaking in on itself without end)
    uint16_t (*randomNext)(uint16_t random) = nullptr;
    // Health (words): what the player has, and the most it can have back (the bar's other end), and in all;
    // what survival gives back for an opponent beaten. With `ceilingShown` the second is what the bar shows,
    // put at what is left.
    uint32_t health = 0, healthCeiling = 0;
    uint16_t healthFull = 0, healthBack = 0;
    bool ceilingShown = false;

    // --- the game ---
    int stages = 0;                 // the stages (1 to this)
    int stageToRound = 2000;        // frames a stage's machine takes to its round, at most, and a few
    int characterSpan = 0;          // characters at random: 0 to this, those choosable
    bool (*choosable)(int number) = nullptr;
    // A choice of the menu's "Fighter" rows (0: at random) as a character's number (-1: none)
    int (*rosterNumber)(int choice) = nullptr;
    // The menu's choices: who (first "Random"), and where (first "Shuffle", "Scroll", then the stages)
    std::vector<const char *> fighterChoices, stageChoices;
    uint16_t tuneOff = 0xFF05;      // a sound asked for before another tune
    bool teams = false;             // the team battle is on the menu
    float teamLinesY = 0.27f;       //   where the line of a team's others goes, as a part of the picture (under its bars)
    // The team battle's machines, before each of their frames, with "one health bar each" chosen: the game's own
    // setting for it put in (nullptr: no such choice)
    void (*oneBarEach)() = nullptr;
    // ... always: fighters begin a match ready, with no entrance (nullptr: the game's entrances); and the computer's
    // difficulty put in, 0 (the easiest) to 7 (nullptr: no such choice)
    void (*noEntrances)() = nullptr;
    void (*difficulty)(int level) = nullptr;
    // The team battle's tag style (nullptr: no such style): whether a side's fighter can be swapped now (it can act),
    // and swapping it: the game sets the side's player up again as this character, where it is when swapped (the
    // health and the bytes of `meter` are put back by the toolkit after the game's frame)
    bool (*canTag)(int side) = nullptr;
    void (*tagIn)(int side, int character, int colours) = nullptr;
    // ... and whether the game has set the side's newcomer up yet (it may not have run the player's code that frame)
    bool (*taggedIn)(int side) = nullptr;
    uint32_t meter = 0, meterSize = 0; // bytes of a player's record kept by the side across a swap (its gauges)
    // The tag style's movements (offStage nullptr: a fighter is swapped where it stands): a side's fighter let out of
    // the picture, the camera not following it (or not, again); its speeds (longs at these, 65536 a pixel a step:
    // across, up) and the floor's height. (A hit that empties a bar with a partner left is the game's to make a
    // knock-down of: TeamHoldsKnockOut.)
    void (*offStage)(int side, bool off) = nullptr;
    // ... and a fighter knocked out kept from being hit (asked every frame it lies there; nullptr: it can be)
    void (*untouchable)(int side) = nullptr;
    // ... its sounds (playSound's codes; 0: none): a character's call as it tags out (the next one called in), and
    // the announcer's at each knock-out (as at a match's end)
    uint16_t (*tagCall)(int character) = nullptr;
    uint16_t downSound = 0;
    uint32_t speedX = 0, speedY = 0;
    int floor = 0;
    // ... the speeds as words of 256ths of a pixel a step, not longs (Super Street Fighter II Turbo's)
    bool speedWords = false;
    // The computer's play of the tag style and the ring out (a side the game plays coached in them, as a person would
    // play them: battle.cpp): whether a side's fighter can act now, on the floor or in the air (nullptr: canTag; neither:
    // no coaching), and the difficulty the game plays at now, 0 (the easiest) to 7 (nullptr: the middle)
    bool (*canAct)(int side) = nullptr;
    int (*difficultyNow)() = nullptr;
    // ... and whether something of the other's is near enough for the side's fighter to guard (the game's own test, as
    // the training dummy's guard asks it; nullptr: never), when the coach lets the game's play guard it
    bool (*attackComing)(Machine &machine, int side) = nullptr;
    // ... or none of that: the game's computer plays the modes itself (its scripts ask what is below, ComputerMode...,
    // and ask for its tags), and the coach takes no side's controls and tags for none
    bool computerPlaysModes = false;

    // A game is being played (no: the board was reset)
    bool (*inGame)() = nullptr;
    // Player 1 is choosing a character (survival and the ring out: at the game's own pace, seen)
    bool (*choosing)() = nullptr;
    // The controls of a machine on its way from switching on to a round of a two-player game; true once the
    // round is on (player 1 left to its own controls when `choose`)
    bool (*toRound)(Machine &machine, int32_t &frames, bool choose) = nullptr;
    void (*playSound)(uint16_t code) = nullptr;
    MatchWanted &(*wanted)() = nullptr;
    CameraWanted &(*cameraAt)() = nullptr;
    // The game's other modes ended (one mode at a time), and whether the stage viewer draws the picture itself
    void (*endModes)() = nullptr;
    bool (*stageViewer)() = nullptr;
    // The wide picture (0: none): columns it adds on each side, and how far short of each end the camera stops
    int (*wideSide)() = nullptr;
    int (*wideMargin)() = nullptr;
    // Objects by what they are (Video::hideObject): all but the stage's pieces, the panel only, the panel and
    // the stage's pieces; a player's own colour sets, as a mask of bits
    bool (*notTheStage)(uint32_t tile, uint16_t attributes) = nullptr;
    bool (*thePanel)(uint32_t tile, uint16_t attributes) = nullptr;
    bool (*notTheFighters)(uint32_t tile, uint16_t attributes) = nullptr;
    uint32_t (*playerSets)(int player) = nullptr;
    // The scroll screens that are the panel (Video::layerMask's bits; 0: the panel is objects): drawn with the
    // fighters over a stage another machine shows, and left out of that machine's picture
    unsigned panelLayers = 0;
    // A player's shadow not drawn from the next frame on (-1: both drawn), or nullptr
    void (*noShadow)(Machine &machine, int player) = nullptr;
    // The ring out's way down: the player is in the air, and at the top of a jump (going up no more)
    bool (*inAir)(uint32_t player) = nullptr;
    bool (*atJumpTop)(uint32_t player) = nullptr;
};

// The game's description, and the menu's pages, commands and features of the modes
void Register(const Description &description);
// The round's result now (0 still going, 1 player 1 has won, 2 player 2, 3 neither), or -1 for a game that has no
// description; and how many rounds win a match (online play's win streaks: port/main.cpp)
int RoundResultNow();
int RoundsToWin();
// The tag-team modes (tag.h) for the team battle: each fighter of a side played by a person of its own, the side's
// panel whoever's fighter is on (no tagging in: the next comes on at a knock-down). `labelY`: where the labels go.
void RegisterTeamTag(float labelY);

} // namespace battle

// Whether one of these modes is on, and ending it
bool BattleMode();
void EndBattle();
bool BattleNoClock();    // the round's clock stands still
bool BattleNoKnockOut(); // nobody's health goes below nothing
bool TeamBattle();
// The tag style: a player's health stops at nothing, not below (a partner comes in instead of a knock-out)
bool TeamHoldsKnockOut(int player);
// The tag style: a time over's result by the teams' health, all their fighters' (1: team 1 has won, 2: team 2, 3:
// neither), or 0: not the tag style (the game's own, by the two players')
int TeamTimeOverResult();
// Around the port's machine's frame (the game's beforeFrame and frame), and its save states
void BattleBeforeFrame(Machine &machine);
void BattleFrame(Machine &machine);
void BattleState(Archive &ar);
// The ring out: what the player whose record is at `player` is to do at its step (above), and whether one knocked
// about at an end of the stage is not to be kept in there (it goes over the end)
RingOut RingOutMove(uint32_t player);
bool RingOutLetsOut(uint32_t player);
// The ring out: whether a player the game plays may walk back now, as its computer wants to (not with its back near its
// end, coached: battle.cpp); the game's sources ask where the computer's walk takes its way
bool RingOutMayWalkBack(uint32_t player);
// For a game whose computer plays the modes itself (Description::computerPlaysModes), what the computer playing the
// player whose record is at `player` is told: the mode its fight is in now (or kNone: none, or a side not the
// computer's); in a ring out, how far its fighter's back is from the end behind it and the other's from theirs
// (pixels); in the tag style, whether its fighter wants to go off (low, with a partner who has more, and not just
// after a tag); and how well it plays, by the menu's CPU difficulty or the game's, 0 to 1000. It asks for a tag,
// taken when its fighter can tag (in the frames after).
enum class ComputerMode { kNone, kRingOut, kTag };
ComputerMode ComputerModeOf(uint32_t player);
int ComputerBack(uint32_t player, bool other);
bool ComputerTagWanted(uint32_t player);
int ComputerSkill();
void ComputerAsksTag(uint32_t player);

} // namespace cps2recomp
