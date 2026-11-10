// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The fighting modes of a fighting game's "Alternative modes" page (battle.h): survival, the endless battle and
// the ring out, and the team battle, on what a game's sources describe. First written for Vampire Savior, then Street
// Fighter Alpha 2.
#include "battle.h"

#include "spare.h"
#include "state.h"
#include "tag.h"

#include <algorithm>
#include <cstdlib>
#include <array>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

namespace cps2recomp {

namespace {
    battle::Description d; // the game's (battle::Register)
    bool g_described = false;

    constexpr const char *kModes = "Alternative modes", *kSurvival = "Survival", *kEndless = "Endless battle";
    constexpr const char *kRingOut = "Ring out", *kTeam = "Tag team battle";
    constexpr int kTeamMost = 3;            // fighters a team has at most

    constexpr int kStartMost = 9000;        // frames after which getting to a round is given up
    constexpr int kAfter = 230;             // frames a decided round is left to play out before the next match
    constexpr int kFade = 24;               //   the last of which, and the next match's first, go to and from black
    constexpr int kIdle = 600;              // frames without a press after which the game takes a fighter back
    constexpr int kOnTheWay = 4;            // frames a frame that the next match's machine runs until its round is on
    constexpr int kOverFor = 480;           // frames the count is shown for when survival is over
    // The endless battle's stage: one stage; or the fighters drawn over a stage that another machine shows,
    // which can then change under them: by fading into another, or by scrolling on into the next
    enum { kStageOne, kStageShuffle, kStageScroll, kStageBehind }; // (the last: behind the fighters, and never changing by itself)
    // The ring out
    constexpr int kFallFor = 56;             // frames a fighter that is out is seen flung out and falling for
    // The way down: the winner jumps off the old stage, and at the top of its jump goes on in the next match (its
    // machine's camera held a picture's height above that stage, the old stage's picture above it), the camera
    // following it down to the next stage's floor, onto which it lands
    constexpr int kDropReady = 2, kDropReadyFalling = 4; // frames a frame the next match's machines run on their way to
                                                         // its round, while the fight goes on and while the one out falls
    constexpr int kDropJump = 8;             // the frame of the way down at which the winner jumps off the old stage
    constexpr int kDropInside = 48;          // the winner is kept at least this far inside the picture's edges
    constexpr int kDropFollow = 4;           // the camera goes a quarter of the way to where it follows the winner a frame
    constexpr int kDropShown = 3;            // the frame of it from which the next match's fighters are drawn
    constexpr int kDropMost = 400;           // frames after which the way down is over, landed or not
    constexpr int kShuffleMinutes[] = {1, 2, 3, 5, 10}; // of fighting on a stage before it fades into another (the menu's choice)
    constexpr int kBlend = 150;                 //   and frames the fade takes
    constexpr int32_t kScrollSpeeds[] = {64, 128, 256}; // the scrolling stage's speed, in 256ths of a pixel a frame: slow, medium, fast
    constexpr size_t kSoundAhead = 16;          // samples of another machine's sound kept in hand
    // The launcher's Multi game page's endless battle (--multi endless:shuffle:SECONDS, endless:scroll:SPEED): every
    // stage once (shuffled, faded one into another; or scrolling by in order), then the game ends for the next one's turn
    constexpr int kTourMost = 64;               // stages a game has at most
    constexpr int kTourOut = 90;                // frames the picture and sound fade out for at the end
    struct Tour {
        bool on = false;
        int way = 0;                            // kStageShuffle or kStageScroll
        int seconds = 60;                       // shuffle: on each stage
        int speed = 0;                          // scroll: of kScrollSpeeds
        bool started = false;                   // (the endless battle has been begun for it)
    } g_tour;
    void ReadTour() {
        static bool read = false;
        if (read) {
            return;
        }
        read = true;
        const std::string &spec = MultiGame();
        if (spec.rfind("endless:", 0) != 0) {
            return;
        }
        const std::string rest = spec.substr(8);
        const size_t colon = rest.find(':');
        const std::string way = rest.substr(0, colon);
        const int n = colon == std::string::npos ? -1 : atoi(rest.c_str() + colon + 1);
        g_tour.on = true;
        g_tour.way = way == "scroll" ? kStageScroll : kStageShuffle;
        if (g_tour.way == kStageScroll) {
            g_tour.speed = std::clamp(n < 0 ? 1 : n, 0, static_cast<int>(std::size(kScrollSpeeds)) - 1);
        } else {
            g_tour.seconds = std::clamp(n < 0 ? 60 : n, 10, 3600);
        }
    }
    constexpr uint16_t kAnyButton = kLeft | kRight | kUp | kDown | kPunch1 | kPunch2 | kPunch3 | kKick1 | kKick2 | kKick3;

    uint32_t Player(int n) {
        return d.playerAt ? d.playerAt(n) : d.player1 + static_cast<uint32_t>(n) * d.playerSize;
    }
    int CameraX() {
        return static_cast<int16_t>(Ram16(d.cameraXAt ? d.cameraXAt() : d.cameraX));
    }
    int CameraY() {
        return static_cast<int16_t>(Ram16(d.cameraYAt ? d.cameraYAt() : d.cameraY));
    }
    // The camera's ends across, on the stage of the machine that is running
    int CameraLeft() {
        int left = d.cameraLeft, right = d.cameraRight;
        if (d.cameraEnds) {
            d.cameraEnds(left, right);
        }
        return left;
    }
    int CameraRight() {
        int left = d.cameraLeft, right = d.cameraRight;
        if (d.cameraEnds) {
            d.cameraEnds(left, right);
        }
        return right;
    }
    // The picture's left edge in the stage, and half a player's width
    int PictureLeft() {
        return CameraX() + d.pictureLeft;
    }
    int HalfWidth(uint32_t player) {
        return d.halfWidth ? Ram16(player + d.halfWidth) : d.fixedHalfWidth;
    }
    // The player is its own master (not knocked about by the other)
    bool OwnMaster(uint32_t player) {
        return d.ownMaster ? d.ownMaster(player) : Ram8(player + d.control) == d.controlOwn;
    }
    // Who plays a player, told to the game (and to its partner's record, in a tag-team game)
    void SetComputer(int n, bool computer) {
        const uint8_t value = static_cast<uint8_t>(computer != d.computerIsZero ? 1 : 0);
        SetRam8(Player(n) + d.computer, value);
        if (d.partnerAt) {
            SetRam8(d.partnerAt(n) + d.computer, value);
        } else if (d.partner) {
            SetRam8(Player(n) + d.partner + d.computer, value);
        }
    }
    MatchWanted &Wanted() {
        return d.wanted();
    }
    CameraWanted &CameraAt() {
        return d.cameraAt();
    }
    // The camera's ends and the picture's width, in 256ths of a pixel, for the picture being played in (a wide
    // picture's camera stops short of where the board's does)
    int32_t LeftEnd() {
        return (CameraLeft() + d.wideMargin()) * 256;
    }
    int32_t RightEnd() {
        return (CameraRight() - d.wideMargin()) * 256;
    }
    int PictureWidth() {
        return Video::kWidth + 2 * d.wideSide();
    }
    int32_t Across() {
        return PictureWidth() * 256;
    }
    int Arena() { // where the fighters' own camera is kept while the stage scrolls
        return (CameraLeft() + CameraRight()) / 2;
    }

    enum class Kind : int32_t { None, Survival, Endless, RingOut, Team };
    enum class Phase : int32_t { Starting, Fighting, Decided, Over, Falling, Dropping };
    // (the menu's choices can be changed while the battle goes on: read each frame)
    int ShuffleAfter() {
        if (g_tour.on) {
            return g_tour.seconds * 60;
        }
        return kShuffleMinutes[std::clamp(FeatureValue("endless_shuffle"), 0, static_cast<int>(std::size(kShuffleMinutes)) - 1)] * 60 * 60;
    }
    int32_t ScrollBy() {
        if (g_tour.on) {
            return kScrollSpeeds[g_tour.speed];
        }
        return kScrollSpeeds[std::clamp(FeatureValue("endless_scroll"), 0, static_cast<int>(std::size(kScrollSpeeds)) - 1)];
    }

    struct State {
        Kind kind = Kind::None;
        Phase phase = Phase::Starting;
        int32_t frames = 0;              // on the way to the first round
        int32_t since = 0;               // frames in this phase
        int32_t stage = 0;               // 1 the first; 0: the game's choice
        int32_t characters[2] = {-1, -1};
        int32_t colours = -1;            // the player's character's colours, kept from match to match
        bool computer[2] = {false, false};
        int32_t idle[2] = {0, 0};
        int32_t score = 0;
        int32_t winner = 0;
        int32_t health = 0, ceiling = 0; // survival: what the player has left
        int32_t fadeIn = 0;              // frames of coming from black still to go
        uint32_t random = 1;
        // The stage behind the fighters, when another machine shows it
        int32_t way = kStageOne;
        int32_t back = 0, backNext = 0;  // its stage, and the one that comes after (0: none yet)
        int32_t backFor = 0;             // frames of fighting on it
        int32_t blend = 0;               // shuffle: how far the fade to the next has got (of kBlend)
        int32_t backCamera = 0;          // scroll: where its picture's left edge is, in 256ths of a pixel
        int32_t slide = 0;               //   and how far the next stage has come in
        // The Multi game page's endless battle: the stages in the order they come, how many, which is shown, and frames
        // of fading out at the end (0: not yet)
        int32_t tourOrder[kTourMost] = {};
        int32_t tourStages = 0, tourAt = 0, tourOut = 0;
        // The ring out
        int32_t out = -1;                // who is out and falling (0, 1)
        int32_t outSide = 0;             //   over which end (-1 the left, 1 the right)
        int32_t dropStage = 0;           // the stage the next match is on
        bool flung = false;              // the one out has been flung out (player.cpp)
        bool handing = false;            // the winner is to be put in the next match at its next frame
        bool dropped = false;            //   and is in it
        bool landed = false;             //   and has landed there
        int32_t dropCameraX = 0, dropCameraY = 0; // where the next match's camera is (up and down: as its fighters stand)
        int32_t raised = 0;              // how far above that its camera is held (256ths of a pixel)
        int32_t raisedShown = 0;         //   and was for the picture its frame makes (that of the frame before)
        int32_t handedY = 0, dropY = 0;  // the winner's height in the next match as it was put there, and now
        int32_t flightX = 0, flightY = 0; // where it is put there (RingOutMove)
        std::array<uint8_t, battle::kMostRecord> flight = {}; // and its record as it was on the old stage
        // The team battle: each side's fighters in the order they come on, how many it has, which is on now (the
        // number knocked down), and the side whose fighter stays on with what health it had left (-1: neither)
        int32_t team[2][kTeamMost] = {{-1, -1, -1}, {-1, -1, -1}};
        int32_t teamColours[2][kTeamMost] = {}; // (each a set of its character's colours no other of it in the match has)
        int32_t teamSize = 2;
        int32_t on[2] = {0, 0};
        int32_t stays = -1;
        int32_t full = 0;                // a fighter's health when it comes on
        // The tag style: each fighter's health and ceiling while it is not on (down: 0), the side's gauges kept across
        // a swap, a swap's health still to be put back after the game's frame, frames before a side can tag again,
        // Start held the frame before
        bool tag = false;
        int32_t kept[2][kTeamMost] = {}, keptCeiling[2][kTeamMost] = {};
        uint8_t meter[2][8] = {};
        bool putBack[2] = {false, false};
        int32_t cool[2] = {0, 0};
        bool startWas[2] = {false, false};
        int32_t rest = 0;                // frames counted for the health that comes back while resting
        // A side's tag in progress (the movements): what it is doing, for how long, who comes on, from which end of
        // the picture (-1 the left; knocked down: the frame it landed), and whether the newcomer has jumped (knocked
        // down: been in the air), attacked, been put off the stage (knocked down: landed)
        int32_t move[2] = {0, 0}, moveFor[2] = {0, 0}, coming[2] = {-1, -1}, from[2] = {0, 0};
        bool jumped[2] = {false, false}, attacked[2] = {false, false}, placed[2] = {false, false};
        // The computer's play (the coach): its own numbers at random (not the battle's, which choose the matches), the
        // frames of fighting, each fighter's frames its own master and not, since the side's last tag, and of the side
        // wanting to tag; a ring out's plan of the side's, for how long, how long it may go on, and frames to the next
        // look at the fight; in the air on the way (a jump's)
        uint32_t coachRandom = 1;
        int32_t clock = 0;
        int32_t free[2] = {0, 0}, busy[2] = {0, 0}, sinceTag[2] = {0, 0}, want[2] = {0, 0};
        int32_t plan[2] = {0, 0}, planFor[2] = {0, 0}, planMost[2] = {0, 0}, think[2] = {0, 0};
        bool planAir[2] = {false, false};
        int32_t planWay[2] = {0, 0}; // (a throw's: 1 the stick towards the other, -1 away)
        int32_t lastFree[2] = {0, 0}; // (a ring out's: how far from its end each was when last its own master)
        int32_t tagAsked[2] = {0, 0}; // (a game whose computer plays the modes itself: frames its ask for a tag holds)
    } g;
    Spare g_dropBack;                    // the ring out: the machine of the next match's stage
    bool g_open = false;                 // the ring out: the port's machine is running a fight with open ends
    bool g_portFrame = false;            //   the port's machine is running (not a spare)
    bool g_dropIn = false;               //   the next match's machine is running, and the winner is to come down
    Spare g_next;
    Spare g_back, g_backNext;            // the machines of the stage behind and of the one after
    // A machine at the character choice with both players in (the first a spare came to): where every later match
    // and stage is begun, not from switching on (that took about 2,000 frames to a round, this a few hundred)
    std::vector<uint8_t> g_atChoice;
    uint32_t Random();
    // A spare begun again: from the character choice, once one has been there
    void Begin(Spare &spare) {
        spare.Clear();
        spare.state = g_atChoice;
    }
    // A spare's controls on its way to its round (Description::toRound); the first time one is at the character
    // choice, it is kept (before its frame) to begin the others from, and one begun from there is given a random
    // number of its own
    bool ToRound(Machine &machine, int32_t &frames) {
        if (frames == 0 && !g_atChoice.empty() && d.random) {
            if (d.randomNext) {
                uint16_t random = Ram16(d.random);
                for (uint32_t steps = Random() & 0xFFFF; steps; steps--) {
                    random = d.randomNext(random);
                }
                SetRam16(d.random, random);
            } else {
                SetRam16(d.random, static_cast<uint16_t>(Random()));
            }
        }
        if (g_atChoice.empty() && d.choosing()) {
            Archive ar(g_atChoice, true);
            machine.Serialize(ar);
        }
        return d.toRound(machine, frames, false);
    }
    // Sound another machine has made and that has not been heard yet (two machines' frames do not make quite
    // the same number of samples)
    struct Stream {
        std::vector<int16_t> waiting;
        void Start() {
            waiting.assign(kSoundAhead * 2, 0);
        }
        void Add(const std::vector<int16_t> &sound) {
            waiting.insert(waiting.end(), sound.begin(), sound.end());
        }
        // Mixed into `sound` (as many samples as it has), `gain` of 256
        void Mix(std::vector<int16_t> &sound, int gain) {
            if (waiting.size() < sound.size()) {
                waiting.resize(sound.size(), 0);
            }
            for (size_t i = 0; i < sound.size(); i++) {
                sound[i] = static_cast<int16_t>(std::clamp(sound[i] + waiting[i] * gain / 256, -32768, 32767));
            }
            waiting.erase(waiting.begin(), waiting.begin() + static_cast<std::ptrdiff_t>(sound.size()));
        }
    } g_backSound, g_backNextSound, g_dropSound, g_dropBackSound;
    bool g_fast = false;
    size_t g_soundBefore = 0;
    int64_t g_frame = 0; // (the port's frames, for the tests' log)

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }

    void End() {
        if (g.kind == Kind::None) {
            return;
        }
        g = State();
        g_next.Clear();
        g_back.Clear();
        g_backNext.Clear();
        g_dropBack.Clear();
        g_atChoice.clear();
        g_open = false;
        Wanted() = MatchWanted();
        CameraAt() = CameraWanted();
        Fast(false);
        SetHud({});
    }

    uint32_t Random() {
        g.random ^= g.random << 13;
        g.random ^= g.random >> 17;
        g.random ^= g.random << 5;
        return g.random;
    }
    // A character at random: not `not1`, not `not2`
    int AnyCharacter(int not1 = -1, int not2 = -1) {
        for (;;) {
            const int c = static_cast<int>(Random() % static_cast<uint32_t>(d.characterSpan));
            if (d.choosable(c) && c != not1 && c != not2) {
                return c;
            }
        }
    }
    // A feature's choice of character: 0 at random, then the characters in the order of their numbers
    int ChosenCharacter(const char *feature) {
        int choice = FeatureValue(feature);
        if (choice <= 0) {
            return -1;
        }
        return d.rosterNumber(choice);
    }
    // The endless battle's stage: its feature's choices are two ways of changing it, then the stages
    int ChosenStage() {
        const int choice = FeatureValue("endless_stage");
        return choice >= 2 && choice < 2 + d.stages ? choice - 1 : static_cast<int>(Random() % static_cast<uint32_t>(d.stages)) + 1;
    }
    int AnyStage(int notThis) {
        for (;;) {
            const int stage = static_cast<int>(Random() % static_cast<uint32_t>(d.stages)) + 1;
            if (stage != notThis) {
                return stage;
            }
        }
    }
    bool Layered() {
        return g.way != kStageOne;
    }
    // The stage that comes after the one shown (0: none, the Multi game page's last)
    int NextStage() {
        if (g.tourStages) {
            return g.tourAt + 1 < g.tourStages ? g.tourOrder[g.tourAt + 1] : 0;
        }
        return AnyStage(g.back);
    }
    // Every stage once, for the Multi game page: in order to scroll by, shuffled to fade one into another
    void PlanTour() {
        g.tourStages = std::min(d.stages, kTourMost);
        for (int n = 0; n < g.tourStages; n++) {
            g.tourOrder[n] = n + 1;
        }
        if (g_tour.way == kStageShuffle) {
            for (int n = g.tourStages - 1; n > 0; n--) {
                std::swap(g.tourOrder[n], g.tourOrder[Random() % static_cast<uint32_t>(n + 1)]);
            }
        }
        g.tourAt = 0;
        g.tourOut = 0;
    }

    // --- the team battle ---

    // The menu's rows of a team's fighters: side, then which of them (0 the first on)
    constexpr const char *kTeamRows[2][kTeamMost] = {{"team_1_1", "team_1_2", "team_1_3"}, {"team_2_1", "team_2_2", "team_2_3"}};
    // Side 2 is the game's: chosen so, or player 2 is on player 1's team (tag.h's co-op)
    bool TeamComputer() {
        const tag::Mode people = tag::Chosen();
        return people == tag::kCoop || (people == tag::kOff && FeatureValue("team_opponent") == 0);
    }
    // A character's name, as the menu's rows have it (nullptr: none of them)
    const char *NameOf(int character) {
        for (size_t choice = 1; choice < d.fighterChoices.size(); choice++) {
            if (d.rosterNumber(static_cast<int>(choice)) == character) {
                return d.fighterChoices[choice];
            }
        }
        return nullptr;
    }
    // The teams as the menu has them (a fighter at random: not one already on that side), on one stage throughout
    void StartTeams() {
        g.teamSize = FeatureValue("team_size") == 1 ? 3 : 2;
        for (int side = 0; side < 2; side++) {
            for (int n = 0; n < g.teamSize; n++) {
                const int chosen = ChosenCharacter(kTeamRows[side][n]);
                g.team[side][n] = chosen >= 0 ? chosen : AnyCharacter(n > 0 ? g.team[side][0] : -1, n > 1 ? g.team[side][1] : -1);
            }
            g.characters[side] = g.team[side][0];
        }
        // (the same character twice or more in the match, on either team: each in a set of colours of its own)
        for (int side = 0; side < 2; side++) {
            for (int n = 0; n < g.teamSize; n++) {
                int before = 0;
                for (int s = 0; s <= side; s++) {
                    for (int m = 0; m < (s < side ? g.teamSize : n); m++) {
                        before += g.team[s][m] == g.team[side][n];
                    }
                }
                g.teamColours[side][n] = before;
            }
        }
        g.computer[1] = TeamComputer();
        g.tag = d.tagIn && FeatureValue("team_style") == 0; // (the tag style first: what a team battle is unless chosen)
        const int stage = FeatureValue("team_stage");
        g.stage = stage >= 1 && stage <= d.stages ? stage : AnyStage(0);
    }
    // A match decided (the round's result: 1 player 1 has won, 2 player 2, 3 both down): the one knocked down is
    // out and the next of that side comes on; the other stays on with what it has left, and a little back
    void TeamDecided(uint8_t result) {
        const bool down[2] = {result == 2 || result == 3, result == 1 || result == 3};
        for (int side = 0; side < 2; side++) {
            g.on[side] += down[side];
        }
        const bool out[2] = {g.on[0] >= g.teamSize, g.on[1] >= g.teamSize};
        if (out[0] || out[1]) {
            g.phase = Phase::Over;
            g.winner = out[0] && out[1] ? -1 : out[0] ? 1 : 0;
            return;
        }
        g.stays = down[0] == down[1] ? -1 : down[0] ? 1 : 0;
        if (g.stays >= 0) {
            // (a little back: as survival gives, in the proportion of the health a fighter comes on with)
            const int full = g.full > 0 ? g.full : d.healthFull;
            const auto left = [full](uint32_t which) {
                const int now = static_cast<int16_t>(Ram16(Player(g.stays) + which));
                return std::min<int>(std::max(now, 0) + d.healthBack * full / std::max<int>(d.healthFull, 1), full);
            };
            g.health = left(d.health);
            g.ceiling = d.ceilingShown ? g.health : std::max(left(d.healthCeiling), g.health);
        }
        for (int side = 0; side < 2; side++) {
            g.characters[side] = g.team[side][g.on[side]];
        }
    }
    // The ends of the bars' part of the picture, as parts of the picture (a wide one's bars stay in the board's middle)
    float BarsLeft() {
        const float width = static_cast<float>(PictureWidth());
        return (static_cast<float>(d.wideSide()) + 0.02f * Video::kWidth) / width;
    }
    float BarsRight() {
        return 1.0f - BarsLeft();
    }
    // Under each side's bars: who of the side is still to come on (the tag style: the others, with their health)
    void TeamLines(std::vector<HudLine> &lines) {
        for (int side = 0; side < 2; side++) {
            std::string text;
            if (g.tag) {
                for (int n = 0; n < g.teamSize; n++) {
                    if (n == g.on[side]) {
                        continue;
                    }
                    const char *name = NameOf(g.team[side][n]);
                    char health[16];
                    if (g.kept[side][n] > 0) {
                        snprintf(health, sizeof(health), " %d%%", std::max(1, g.kept[side][n] * 100 / std::max(g.full, 1)));
                    } else {
                        snprintf(health, sizeof(health), " down");
                    }
                    text += std::string(text.empty() ? "" : "  ") + (name ? name : "?") + health;
                }
                if (!text.empty()) {
                    lines.push_back({side == 0 ? BarsLeft() : BarsRight(), d.teamLinesY, side == 0 ? -1 : 1, text});
                }
                continue;
            }
            for (int n = g.on[side] + 1; n < g.teamSize; n++) {
                const char *name = NameOf(g.team[side][n]);
                text += std::string(text.empty() ? "Next: " : ", ") + (name ? name : "?");
            }
            if (!text.empty()) {
                lines.push_back({side == 0 ? BarsLeft() : BarsRight(), d.teamLinesY, side == 0 ? -1 : 1, text});
            }
        }
    }

    // --- the team battle's tag style ---

    constexpr int kTagCool = 30; // frames after a swap before the side can tag again
    constexpr int kRestEvery = 6; // a resting fighter gets back a point of its health that comes back this often

    // A side's next fighter that is not down, after the one on (-1: none)
    int NextUp(int side) {
        for (int k = 1; k < g.teamSize; k++) {
            const int n = (g.on[side] + k) % g.teamSize;
            if (g.kept[side][n] > 0) {
                return n;
            }
        }
        return -1;
    }
    // The side's fighter on goes off, with what it has, and `next` comes on
    void Swap(int side, int next) {
        const int16_t health = static_cast<int16_t>(Ram16(Player(side) + d.health));
        const int16_t ceiling = static_cast<int16_t>(Ram16(Player(side) + d.healthCeiling));
        g.kept[side][g.on[side]] = std::max<int>(health, 0);
        g.keptCeiling[side][g.on[side]] = std::max<int>(std::max<int>(ceiling, health), 0);
        for (uint32_t n = 0; n < std::min<uint32_t>(d.meterSize, sizeof(g.meter[side])); n++) {
            g.meter[side][n] = Ram8(Player(side) + d.meter + n);
        }
        g.on[side] = next;
        g.characters[side] = g.team[side][next];
        d.tagIn(side, g.team[side][next], g.teamColours[side][next]);
        g.putBack[side] = true;
        g.cool[side] = kTagCool;
        g.sinceTag[side] = 0;
    }
    // The movements of a tag (with Description::offStage): the one on jumps back and flies off the picture, or, its
    // bar emptied, is knocked down and lies a while; then the next one, put off the picture at that end, jumps in fast
    // with a jump attack that can hit, and is in the fight once it lands
    enum Move : int32_t { kStill, kLeaving, kDown, kComing };
    constexpr int kLeaveSpeed = 9;   // pixels a step, flying off
    constexpr int kLeaveMost = 50;   // frames, at most, before the one leaving is taken off
    constexpr int kLeaveJump = 8;    // frames by which the one leaving is in the air
    constexpr int kLieFor = 120;     // frames the one knocked out is seen lying, from landing (left lying: the game's)
    constexpr int kDownMost = 240;   // frames, at most, from the knock-down to the next one coming in
    constexpr int kHiddenMost = 10;  // frames, at most, a newcomer put off the picture is not drawn before it jumps
    constexpr int kDownSaid = 30;    // frames from the knock-down to the announcer's call (as the game makes it)
    constexpr int kStillUp = 20;     // frames after which one whose bar emptied but who never left the floor leaves
    constexpr int kOffBy = 48;       // how far beyond the picture's edge one is gone, and one coming starts
    constexpr int kComeMost = 120;   // frames, at most, of coming in
    constexpr int kComeSteps = 34;   // about the steps a jump in takes, for its speed across

    int X(int side) {
        return static_cast<int16_t>(Ram16(Player(side) + d.x));
    }
    // The picture's ends in the stage, wide or not
    int SeenLeft() {
        return PictureLeft() - d.wideSide();
    }
    int SeenRight() {
        return SeenLeft() + PictureWidth();
    }
    // Which way is towards the other player (-1 the left)
    int Toward(int side) {
        return X(1 - side) < X(side) ? -1 : 1;
    }
    uint16_t Stick(int way) {
        return way < 0 ? kLeft : kRight;
    }
    void SetSpeedX(int side, int pixels256) { // (in 256ths of a pixel a step)
        if (d.speedWords) {
            SetRam16(Player(side) + d.speedX, static_cast<uint16_t>(pixels256));
        } else {
            SetRam32(Player(side) + d.speedX, static_cast<uint32_t>(pixels256 * 256));
        }
    }
    // Going up no more (the speed up nothing or below)
    bool Falling(int side) {
        return d.speedWords ? static_cast<int16_t>(Ram16(Player(side) + d.speedY)) <= 0 : static_cast<int32_t>(Ram32(Player(side) + d.speedY)) <= 0;
    }
    // Moved across by so many 256ths of a pixel (the game's own speed left at nothing: a character's jump may be a
    // move of its own that goes by speeds of its own)
    void MoveX(int side, int pixels256) {
        SetSpeedX(side, 0);
        SetRam32(Player(side) + d.x, Ram32(Player(side) + d.x) + static_cast<uint32_t>(pixels256 * 256));
    }

    // --- the computer's play of the modes (the coach) ---
    //
    // The game's computer player fights every fight as a fight: it knows nothing of a tag (the toolkit's, not the game's),
    // nor that in a ring out only the stage's ends count. A side the game plays is coached in these, as a person would
    // play them. In the tag style the coach says when the side tags and whom it tags in; in a ring out it takes the
    // side's controls for a moment, the game's play of it off (as the endless battle's person does), to walk the other
    // fighter towards an end, jump out of a corner, or knock the other over the end it is near, and gives them back once
    // the fighter can act again. The rest of the time the game plays it. The difficulty says how soon it sees a chance
    // and how often it takes one.
    constexpr int kDownFrames = 20;  // frames of reeling after which the other fighter is taken to be down (or dizzy)
    constexpr int kOutOfHit = 8;     // frames a fighter is its own master again before it tags (not inside a combo)

    // (for tests, CPS2RECOMP_BATTLE_LOG: each tag, coached move and ring out, on stderr)
    FILE *Log() {
        static FILE *log = getenv("CPS2RECOMP_BATTLE_LOG") ? stderr : nullptr;
        return log;
    }
    // (for tests, CPS2RECOMP_COACH=MASK: the sides the game plays that are coached, 3 both; 0: the game's play alone)
    bool Coached(int side) {
        static const char *mask = getenv("CPS2RECOMP_COACH");
        return g.computer[side] && (d.canAct || d.canTag) && ((mask ? strtoul(mask, nullptr, 10) : 3) >> side & 1);
    }
    // (for tests, CPS2RECOMP_COACH_PARTS=MASK: the ring out's parts used, 15 all: 1 getting out from near one's end, 2
    // knocking the other over its end, 4 walking and jumping to the better side, 8 not walking back to one's end)
    bool Part(int part) {
        static const char *mask = getenv("CPS2RECOMP_COACH_PARTS");
        return ((mask ? strtoul(mask, nullptr, 10) : 15) & static_cast<unsigned long>(part)) != 0;
    }
    bool CanAct(int side) {
        return d.canAct ? d.canAct(side) : d.canTag(side);
    }
    bool OwnMaster(int side) {
        return Ram8(Player(side) + d.control) == d.controlOwn;
    }
    // The difficulty, of 1000: the menu's in the team battle, or the game's
    int Skill() {
        const int chosen = g.kind == Kind::Team ? FeatureValue("team_difficulty") : 0;
        const int level = chosen > 0 ? chosen - 1 : d.difficultyNow ? std::clamp(d.difficultyNow(), 0, 7) : 3;
        return level * 1000 / 7;
    }
    // A number between the easiest's and the hardest's, by the difficulty
    int Between(int easiest, int hardest) {
        return easiest + (hardest - easiest) * Skill() / 1000;
    }
    bool Chance(int perMille) {
        g.coachRandom ^= g.coachRandom << 13;
        g.coachRandom ^= g.coachRandom >> 17;
        g.coachRandom ^= g.coachRandom << 5;
        return static_cast<int>(g.coachRandom % 1000) < perMille;
    }
    int Distance() {
        return std::abs(X(0) - X(1));
    }
    int Back(int side);
    // Each frame of fighting: how long each fighter has been its own master, or not
    void Watch() {
        g.clock++;
        for (int side = 0; side < 2; side++) {
            const bool own = OwnMaster(side);
            g.free[side] = own ? g.free[side] + 1 : 0;
            g.busy[side] = own ? 0 : g.busy[side] + 1;
            g.sinceTag[side]++;
            if (own && g.kind == Kind::RingOut) {
                g.lastFree[side] = Back(side);
            }
        }
    }

    // The tag style. Whom a side the game plays tags in: the partner with the most health (-1: none left)
    int Healthiest(int side) {
        int best = -1;
        for (int n = 0; n < g.teamSize; n++) {
            if (n != g.on[side] && g.kept[side][n] > 0 && (best < 0 || g.kept[side][n] > g.kept[side][best])) {
                best = n;
            }
        }
        return best;
    }
    // Whether it tags now (its fighter can act; asked each such frame): when the one on is low and a partner has more,
    // or a partner who has rested has much more; best while the other fighter is down, far off or in a tag of its own;
    // not soon after the last tag, nor just out of a hit. Waiting long for a chance, it tags anyway, if not in the other's
    // face.
    bool ComputerTags(int side) {
        const int next = Healthiest(side);
        const int full = std::max<int>(g.full, 1);
        const int on = std::max<int>(static_cast<int16_t>(Ram16(Player(side) + d.health)), 0) * 1000 / full;
        const int partner = next >= 0 ? g.kept[side][next] * 1000 / full : 0;
        const bool low = on < Between(250, 400) && partner >= on + 150;
        const bool rested = partner >= on + 450 && g.sinceTag[side] > Between(1800, 1200);
        if (next < 0 || g.sinceTag[side] < Between(720, 360) || !(low || rested)) {
            g.want[side] = 0;
            return false;
        }
        g.want[side]++;
        if (g.free[side] < kOutOfHit) {
            return false;
        }
        const int other = 1 - side;
        const bool opening = g.busy[other] >= kDownFrames || g.move[other] != kStill || Distance() > 170;
        if (opening) {
            return Chance(Between(80, 400)); // (seen at once by the hardest, a few frames on by the easiest)
        }
        return g.want[side] > Between(120, 300) && Distance() > 90 && Chance(Between(40, 20));
    }

    // The ring out. Where each fighter's back is from the end of the stage behind it (pixels; the end a fighter goes
    // out by: LookForOut's), and how wide the stage is between its ends
    int Back(int side) {
        const int left = CameraLeft() + d.pictureLeft, right = CameraRight() + d.pictureLeft + Video::kWidth;
        const int half = HalfWidth(Player(side));
        return Toward(side) > 0 ? X(side) - half - left : right - (X(side) + half);
    }
    int StageWidth() {
        return CameraRight() - CameraLeft() + Video::kWidth;
    }
    // A side's plan, its controls the coach's until it is done: walk towards the other (pushing it to its end, or out of
    // one's own end's way), jump over it (out of a corner, or round to the side away from one's own end), a low sweep (a
    // knock-down: with the other's back to its end, it goes over), or a throw. Which way a throw sends the other is the
    // stick's: held away, the other goes on away (with its back to its end, over it); held towards, most often over one's
    // head, behind (with one's own back to one's end: over that). Counted in Street Fighter Alpha 2's ring outs.
    enum Plan : int32_t { kPlay, kWalk, kJumpOver, kSweep, kThrow };
    constexpr const char *kPlanNames[] = {"play", "walk", "jump over", "sweep", "throw"};
    // (where the ones knocked out were the last time they were their own masters, Alpha 2's: half within 30 pixels of
    // their end, nine in ten within 145: a guarded hit pushes a fighter on, a knock-down carries it, and at the end a hit
    // or a guard sends it over)
    constexpr int kNearEnd = 150;    // pixels from one's own end at which a fighter is to get out
    constexpr int kAtTheEnd = 100;   // pixels from the other's end at which that one can be knocked over it
    constexpr int kClose = 50;       // pixels between the two at which a throw reaches
    constexpr int kOver = 110;       //   and over which a jump is too short to go over the other
    constexpr int kPlanMost = 120;   // frames, at most, of a plan
    Machine *g_machine = nullptr;    // (the port's, for the game's guard test)
    bool AttackComing(int side) {
        return d.attackComing && g_machine && d.attackComing(*g_machine, side);
    }
    void SetPlan(int side, Plan plan, int most, int way = 1) {
        g.planWay[side] = way;
        g.plan[side] = plan;
        g.planFor[side] = 0;
        g.planMost[side] = most;
        g.planAir[side] = false;
        if (Log()) {
            fprintf(Log(), "coach %d side=%d %s back=%d other=%d distance=%d x=%d,%d frame=%lld\n", g.clock, side, kPlanNames[plan], Back(side),
                    Back(1 - side), Distance(), X(side), X(1 - side), static_cast<long long>(g_frame));
        }
    }
    // A look at the fight, every so often while the side's fighter can act on the floor (more often near its end): a plan,
    // or the game's play. Never into an attack coming (the game's play guards it, or answers it); a jump or a throw while
    // the other is in no state to answer: down, in the end of a move of its own, or (less often) standing.
    void Think(int side) {
        if (--g.think[side] > 0 || !CanAct(side) || d.inAir(Player(side))) {
            return;
        }
        const int other = 1 - side;
        const int mine = Back(side), theirs = Back(other), distance = Distance();
        const bool near = mine < kNearEnd && mine < theirs;
        g.think[side] = near ? Between(10, 3) + static_cast<int>(g.coachRandom % 3) : Between(24, 8) + static_cast<int>(g.coachRandom % 8);
        if (AttackComing(side)) {
            return;
        }
        const bool down = g.busy[other] >= kDownFrames;
        const bool recovering = OwnMaster(other) && !CanAct(other) && !d.inAir(Player(other));
        const bool standing = CanAct(other);
        const int opening = down || recovering ? Between(400, 900) : standing ? Between(100, 300) : 0;
        if (near && Part(1)) {
            // (out: a throw over one's head, a jump over, or a walk out of the end's way)
            if (distance < kClose && !down && Chance(opening)) {
                SetPlan(side, kThrow, kPlanMost, 1);
            } else if (distance < kOver && Chance(opening)) {
                SetPlan(side, kJumpOver, kPlanMost);
            } else if (distance >= kOver && Chance(Between(300, 700))) {
                SetPlan(side, kWalk, 24);
            }
        } else if (near) {
        } else if (Part(2) && theirs < kAtTheEnd && distance < kClose && !down && Chance(opening)) {
            SetPlan(side, kThrow, kPlanMost, -1);
        } else if (Part(2) && theirs < kAtTheEnd && distance < 100 && recovering && Chance(opening)) {
            SetPlan(side, kSweep, kPlanMost);
        } else if (Part(4) && theirs < mine && distance > 90 && !down && Chance(Between(250, 600))) {
            SetPlan(side, kWalk, 16 + static_cast<int>(g.coachRandom % 16));
        } else if (Part(4) && mine < theirs && mine < StageWidth() / 3 && distance < kOver && Chance(opening / 2)) {
            SetPlan(side, kJumpOver, kPlanMost);
        }
    }
    // A frame of the side's plan: its controls. Over when done, and the fighter can act again (a move begun by a person's
    // controls is not left to the game's computer to finish); when it is hit; and a walk when an attack is coming (the
    // game's play guards it)
    uint16_t Follow(int side) {
        const int f = g.planFor[side]++;
        const uint16_t toward = Stick(Toward(side));
        const bool air = d.inAir(Player(side));
        g.planAir[side] = g.planAir[side] || air;
        bool done = f >= g.planMost[side] || (f > 3 && !OwnMaster(side));
        uint16_t panel = 0;
        switch (g.plan[side]) {
        case kWalk:
            panel = toward;
            done = done || Distance() < 70 || AttackComing(side);
            break;
        case kJumpOver:
            panel = f < 3 ? static_cast<uint16_t>(kUp | toward) : 0;
            done = done || (g.planAir[side] && !air && CanAct(side)) || (f > 8 && !g.planAir[side]);
            break;
        case kSweep:
            // (the stick's down: the tag's kDown is a move)
            panel = f == 2 ? static_cast<uint16_t>(cps2recomp::kDown | kKick3) : f < 10 ? static_cast<uint16_t>(cps2recomp::kDown) : 0;
            done = done || (f > 12 && CanAct(side));
            break;
        case kThrow: {
            const uint16_t way = g.planWay[side] > 0 ? toward : Stick(-Toward(side));
            panel = f == 1 ? static_cast<uint16_t>(way | kPunch3) : f < 1 ? way : 0;
            done = done || (f > 6 && CanAct(side));
            break;
        }
        }
        if (done && Log()) {
            fprintf(Log(), "done %d side=%d %s back=%d other=%d distance=%d hit=%d x=%d,%d way=%d\n", g.clock, side, kPlanNames[g.plan[side]],
                    Back(side), Back(1 - side), Distance(), !OwnMaster(side), X(side), X(1 - side), g.planWay[side]);
        }
        if (done) {
            g.plan[side] = kPlay;
            g.think[side] = Between(24, 8);
        }
        return panel;
    }
    // Before the frame, in a ring out's fight: the coached sides' plans
    void RingOutCoach(uint16_t &one, uint16_t &two) {
        uint16_t *panels[2] = {&one, &two};
        for (int side = 0; side < 2; side++) {
            if (!Coached(side) || d.computerPlaysModes) {
                continue;
            }
            if (g.plan[side] == kPlay) {
                Think(side);
            }
            if (g.plan[side] != kPlay) {
                *panels[side] = Follow(side);
            }
        }
    }
    // A side's controls are the toolkit's this frame, not the game's play (a tag's movements, or the coach's plan)
    bool Steered(int side) {
        return (g.kind == Kind::Team && g.move[side] != kStill) || (g.kind == Kind::RingOut && g.plan[side] != kPlay);
    }
    // A side's fighter not drawn while it is swapped for the next and put off the picture (the game sets the newcomer
    // up where a round begins, in the picture, for a frame); its face beside the bars still is
    uint32_t g_hiddenSets = 0;
    bool HideTagged(uint32_t tile, uint16_t attributes) {
        if (d.thePanel && d.thePanel(tile, attributes)) {
            return false;
        }
        return (g_hiddenSets >> (attributes & 0x1F)) & 1;
    }
    // The next fighter on: swapped in now, and coming in from the end of the picture the last one left by
    void BringIn(int side) {
        g.from[side] = X(side) < (SeenLeft() + SeenRight()) / 2 ? -1 : 1;
        // (where the newcomer starts, off the picture at that end on the floor, put in first: the game's sources keep
        // it there through setting the newcomer up, which puts a fighter where a round begins)
        const int x = g.from[side] < 0 ? SeenLeft() - kOffBy : SeenRight() + kOffBy;
        SetRam32(Player(side) + d.x, static_cast<uint32_t>(x) << 16);
        SetRam32(Player(side) + d.y, static_cast<uint32_t>(d.floor) << 16);
        d.offStage(side, true);
        Swap(side, g.coming[side]);
        g.move[side] = kComing;
        g.moveFor[side] = 0;
        g.jumped[side] = g.attacked[side] = g.placed[side] = false;
    }
    // Before the frame: a side whose fighter can act tags the next one in on Start (a person's side) or when the coach
    // says (the game's), or must (its fighter's bar emptied); the movements of a tag going on work the side's controls
    void TagBeforeFrame(uint16_t &one, uint16_t &two) {
        uint16_t *panels[2] = {&one, &two};
        const bool moves = d.offStage != nullptr;
        for (int side = 0; side < 2; side++) {
            uint16_t &panel = *panels[side];
            const bool start = (panel & (kStart | kTag)) != 0; // (Start, or a gamepad's left trigger)
            const bool pressed = start && !g.startWas[side] && !g.computer[side];
            g.startWas[side] = start;
            if (g.cool[side] > 0) {
                g.cool[side]--;
            }
            if (g.tagAsked[side] > 0) {
                g.tagAsked[side]--;
            }
            const int toward = Toward(side);
            switch (g.move[side]) {
            case kStill: {
                const bool emptied = static_cast<int16_t>(Ram16(Player(side) + d.health)) <= 0;
                const int next = Coached(side) ? Healthiest(side) : NextUp(side);
                if (next < 0 || g.putBack[side]) {
                    break;
                }
                const bool asked = d.computerPlaysModes ? g.tagAsked[side] > 0 : ComputerTags(side);
                const bool computerTags = Coached(side) && !emptied && g.cool[side] == 0 && d.canTag(side) && asked;
                if (computerTags && Log()) {
                    const int full = std::max<int>(g.full, 1);
                    fprintf(Log(), "tag %d side=%d health=%d partner=%d other_down=%d distance=%d\n", g.clock, side,
                            static_cast<int16_t>(Ram16(Player(side) + d.health)) * 100 / full, g.kept[side][next] * 100 / full,
                            g.busy[1 - side] >= kDownFrames, Distance());
                }
                if (emptied && moves) {
                    // (knocked down where it is; the next one comes in a while after)
                    g.move[side] = kDown;
                    g.moveFor[side] = 0;
                    g.coming[side] = next;
                    g.jumped[side] = g.placed[side] = false;
                } else if (((pressed && g.cool[side] == 0) || computerTags || emptied) && d.canTag(side)) {
                    if (moves) {
                        g.move[side] = kLeaving;
                        g.moveFor[side] = 0;
                        g.coming[side] = next;
                        // (from this frame: the buttons pressed with Start do nothing, or the game takes them, as a
                        // super move say)
                        panel = static_cast<uint16_t>(kUp | Stick(-toward));
                        g.moveFor[side]++;
                        // (the one on calls the next one in)
                        if (d.tagCall && d.playSound) {
                            if (const uint16_t call = d.tagCall(g.team[side][g.on[side]])) {
                                d.playSound(call);
                            }
                        }
                    } else {
                        Swap(side, next);
                    }
                }
                break;
            }
            case kLeaving: {
                // A jump back, and once in the air, off the picture at speed
                const int away = -toward;
                if ((g.moveFor[side] > 3 && !OwnMaster(Player(side))) ||
                    (g.moveFor[side] == kLeaveJump && !d.inAir(Player(side)))) {
                    // (hit on the way, or doing a move of its own and not jumping off: the tag is off, and the
                    // fighter stays)
                    if (Log()) {
                        fprintf(Log(), "tag off %d side=%d\n", g.clock, side);
                    }
                    d.offStage(side, false);
                    g.move[side] = kStill;
                    g.cool[side] = kTagCool;
                    break;
                }
                panel = g.moveFor[side]++ < 3 ? static_cast<uint16_t>(kUp | Stick(away)) : 0;
                if (g.moveFor[side] > 3 && d.inAir(Player(side))) {
                    d.offStage(side, true);
                    MoveX(side, away * kLeaveSpeed * 256);
                }
                const bool gone = X(side) < SeenLeft() - kOffBy || X(side) > SeenRight() + kOffBy;
                if (gone || g.moveFor[side] > kLeaveMost) {
                    BringIn(side);
                }
                break;
            }
            case kDown: {
                // (thrown up by the knock-down, then lying: the next one comes in a while after it has landed; no
                // more hits land on it)
                panel = 0;
                if (d.untouchable) {
                    d.untouchable(side);
                }
                const bool air = d.inAir(Player(side));
                if (air) {
                    g.jumped[side] = true;
                } else if (g.jumped[side] && !g.placed[side]) {
                    g.placed[side] = true; // (landed)
                    g.from[side] = g.moveFor[side];
                }
                g.moveFor[side]++;
                if (g.moveFor[side] == kDownSaid && d.downSound && d.playSound) {
                    d.playSound(d.downSound);
                }
                if ((g.placed[side] && g.moveFor[side] - g.from[side] >= kLieFor) || g.moveFor[side] >= kDownMost) {
                    BringIn(side);
                } else if (!g.jumped[side] && g.moveFor[side] > kStillUp && d.canTag(side)) {
                    // (not knocked down, a guarded hit's: it leaves as on a tag)
                    g.move[side] = kLeaving;
                    g.moveFor[side] = 0;
                }
                break;
            }
            case kComing: {
                panel = 0;
                if (g.putBack[side]) {
                    break; // (the game has not set the newcomer up yet)
                }
                g.moveFor[side]++;
                if (!g.placed[side]) {
                    // Off the picture at that end, on the floor
                    g.placed[side] = true;
                    d.offStage(side, true);
                    const int x = g.from[side] < 0 ? SeenLeft() - kOffBy : SeenRight() + kOffBy;
                    SetRam32(Player(side) + d.x, static_cast<uint32_t>(x) << 16);
                    SetRam32(Player(side) + d.y, static_cast<uint32_t>(d.floor) << 16);
                    SetSpeedX(side, 0);
                    break;
                }
                const bool air = d.inAir(Player(side));
                if (!g.jumped[side]) {
                    panel = static_cast<uint16_t>(kUp | Stick(toward));
                    if (air) {
                        g.jumped[side] = true;
                    }
                } else if (air && OwnMaster(Player(side))) {
                    // (across fast enough to land by the other player; the jump attack on the way down)
                    const int distance = std::abs(X(1 - side) - X(side)) - 40;
                    MoveX(side, toward * std::clamp(distance * 256 / kComeSteps, 3 * 256, 12 * 256));
                    if (!g.attacked[side] && Falling(side)) {
                        panel = kKick3;
                        g.attacked[side] = true;
                    }
                }
                const bool inside = X(side) > SeenLeft() + 32 && X(side) < SeenRight() - 32;
                if (inside) {
                    d.offStage(side, false);
                }
                if ((g.jumped[side] && !air) || g.moveFor[side] > kComeMost) {
                    d.offStage(side, false);
                    g.move[side] = kStill;
                    g.cool[side] = kTagCool;
                }
                break;
            }
            }
        }
    }
    // After the frame: the health and gauges of the ones who came on put back (the game set them up with full health
    // and none), and the health that comes back by itself coming back to the ones resting
    void TagFrame() {
        for (int side = 0; side < 2; side++) {
            if (g.putBack[side] && (!d.taggedIn || d.taggedIn(side))) {
                g.putBack[side] = false;
                SetRam16(Player(side) + d.health, static_cast<uint16_t>(g.kept[side][g.on[side]]));
                SetRam16(Player(side) + d.healthCeiling, static_cast<uint16_t>(g.keptCeiling[side][g.on[side]]));
                for (uint32_t n = 0; n < std::min<uint32_t>(d.meterSize, sizeof(g.meter[side])); n++) {
                    SetRam8(Player(side) + d.meter + n, g.meter[side][n]);
                }
            }
        }
        if (++g.rest >= kRestEvery) {
            g.rest = 0;
            for (int side = 0; side < 2; side++) {
                for (int n = 0; n < g.teamSize; n++) {
                    if (n != g.on[side] && g.kept[side][n] > 0 && g.kept[side][n] < g.keptCeiling[side][n]) {
                        g.kept[side][n]++;
                    }
                }
            }
        }
    }

    void Start(Machine &machine, Kind kind) {
        d.endModes();
        End();
        SetFeature("training_mode", false);
        g.kind = kind;
        g.phase = Phase::Starting;
        // (for tests, CPS2RECOMP_BATTLE_SEED=N: the same choices at random every time, so that a movie plays back)
        static const char *seed = getenv("CPS2RECOMP_BATTLE_SEED");
        g.random = (seed ? static_cast<uint32_t>(strtoul(seed, nullptr, 10)) : std::random_device()()) | 1;
        g.coachRandom = g.random * 2654435761u | 1;
        if (kind == Kind::Team) {
            StartTeams();
        } else if (kind != Kind::Endless) {
            g.characters[0] = -1; // (the player's choice)
            g.characters[1] = AnyCharacter();
            g.computer[1] = true;
            g.stage = 0;
            if (kind == Kind::RingOut) {
                g.way = kStageBehind;
                g.back = AnyStage(0);
                g.dropStage = AnyStage(g.back);
                g_backSound.Start();
            }
        } else {
            g.characters[0] = ChosenCharacter("endless_one");
            g.characters[1] = ChosenCharacter("endless_two");
            if (g.characters[0] < 0) {
                g.characters[0] = AnyCharacter(g.characters[1]);
            }
            if (g.characters[1] < 0) {
                g.characters[1] = AnyCharacter(g.characters[0]);
            }
            g.computer[0] = g.computer[1] = true;
            g.stage = ChosenStage();
            const int choice = FeatureValue("endless_stage");
            g.way = choice == 0 ? kStageShuffle : choice == 1 ? kStageScroll : kStageOne;
            if (Layered()) {
                g.back = g.stage;
                g.backCamera = LeftEnd();
                g_backSound.Start();
            }
        }
        // (for tests, CPS2RECOMP_BATTLE_CPU=1: player 1's side the game's too, its fighter at random, chosen by nobody)
        static const bool computerOnly = getenv("CPS2RECOMP_BATTLE_CPU") != nullptr;
        if (computerOnly && kind != Kind::Endless) {
            g.computer[0] = true;
            if (g.characters[0] < 0) {
                g.characters[0] = AnyCharacter(g.characters[1]);
            }
        }
        machine.Reset();
    }
    void StartTeam(Machine &machine) {
        Start(machine, Kind::Team);
    }
    void StartSurvival(Machine &machine) {
        Start(machine, Kind::Survival);
    }
    void StartEndless(Machine &machine) {
        Start(machine, Kind::Endless);
    }
    void StartRingOut(Machine &machine) {
        Start(machine, Kind::RingOut);
    }
    bool Chooses() {
        return (g.kind == Kind::Survival || g.kind == Kind::RingOut) && !g.computer[0]; // (the player chooses a character)
    }

    void Want() {
        if (g.kind == Kind::Team) {
            if (d.oneBarEach && FeatureValue("team_bars") == 0) {
                d.oneBarEach();
            }
            if (d.noEntrances) {
                d.noEntrances();
            }
            const int level = FeatureValue("team_difficulty");
            if (d.difficulty && level > 0) {
                d.difficulty(level - 1);
            }
        }
        Wanted().stage = g.stage;
        Wanted().characters[0] = g.characters[0];
        Wanted().characters[1] = g.characters[1];
        Wanted().colours[0] = g.colours;
        if (g.kind == Kind::Team) {
            for (int side = 0; side < 2; side++) {
                Wanted().colours[side] = g.teamColours[side][std::min(g.on[side], g.teamSize - 1)];
            }
        }
    }

    // Both players stay players that are in the game (Description::plays), or the panel would count down for the
    // one that is not to come back
    void BothPlay() {
        if (d.plays) {
            for (int n = 0; n < 2; n++) {
                SetRam8(Player(n) + d.plays, 2);
            }
        }
    }
    // Who the game plays, told to the game: the players' records say
    void SayWhoPlays(bool nobody = false) {
        for (int n = 0; n < 2; n++) {
            // (the tag style: a fighter knocked down with its bar emptied lies still, and one tagging out or in moves as
            // the toolkit works its controls, the game not playing either; nor one the coach has a plan for)
            const bool computer = g.computer[n] && !nobody && !Steered(n);
            const bool now = (Ram8(Player(n) + d.computer) != 0) != d.computerIsZero;
            if (computer != now && d.handOver && !d.handOver(Player(n))) {
                continue; // (not yet: once the move under way is over)
            }
            SetComputer(n, computer);
        }
        BothPlay();
    }

    void BeforeFrame(Machine &machine) {
        g_soundBefore = machine.audio.size();
        g_machine = &machine;
        Want();
        uint16_t &one = machine.players[0], &two = machine.players[1];
        if (g.phase == Phase::Starting) {
            // Survival: the player chooses a character, at the game's own pace; the rest goes by unseen
            const bool choosing = Chooses() && d.choosing();
            Fast(!choosing);
            if (d.toRound(machine, g.frames, Chooses()) && (!Layered() || g_back.ready)) {
                Fast(false);
                g.phase = Phase::Fighting;
                g.since = 0;
                g.fadeIn = kFade;
                g.characters[0] = Ram8(Player(0) + d.character);
                g.characters[1] = Ram8(Player(1) + d.character);
                g.full = static_cast<int16_t>(Ram16(Player(0) + d.health));
                for (int side = 0; side < 2; side++) {
                    for (int n = 0; n < kTeamMost; n++) {
                        g.kept[side][n] = g.keptCeiling[side][n] = g.full;
                    }
                }
                if (Chooses() && d.colours) {
                    g.colours = Ram8(Player(0) + d.colours);
                }
                if (Layered()) {
                    d.playSound(d.tuneOff); // (the music is the stage's, from its own machine)
                }
                if (g.kind == Kind::RingOut) {
                    g.characters[1] = AnyCharacter(g.characters[1]); // (from here: who the next match is against)
                    Begin(g_next);
                    Begin(g_dropBack); // (the stage behind has been to the character choice by now)
                }
            } else if (g.frames > kStartMost && !choosing) {
                if (g.tourStages) {
                    EndProgram(kNextGame); // (the Multi game page's: on to the next game)
                }
                End();
            }
            return;
        }
        if (g.phase == Phase::Over) {
            one = two = 0;
            return;
        }
        if (g.phase == Phase::Falling || g.phase == Phase::Dropping) {
            // Nobody fights on; the winner jumps off, over the end the loser went over
            one = two = 0;
            if (g.phase == Phase::Dropping && g.since >= kDropJump && g.since < kDropJump + 4) {
                one = static_cast<uint16_t>(kUp | (g.outSide < 0 ? kLeft : kRight));
            }
            SayWhoPlays(true);
            return;
        }
        g_open = g.kind == Kind::RingOut;
        if (g.kind == Kind::Endless) {
            // A press takes a fighter over; none for a while gives it back
            const uint16_t held[2] = {one, two};
            for (int n = 0; n < 2; n++) {
                if (held[n] & kAnyButton) {
                    g.computer[n] = false;
                    g.idle[n] = 0;
                } else if (!g.computer[n] && ++g.idle[n] > kIdle) {
                    g.computer[n] = true;
                }
            }
        }
        if ((g.kind == Kind::Team || g.kind == Kind::RingOut) && g.phase == Phase::Fighting) {
            Watch();
        }
        if (g.kind == Kind::RingOut && g.phase == Phase::Fighting) {
            RingOutCoach(one, two);
        }
        if (g.kind == Kind::Team && g.tag && g.phase == Phase::Fighting) {
            TagBeforeFrame(one, two);
            g_hiddenSets = 0;
            for (int side = 0; side < 2; side++) {
                // (until it is in the air: before that it stands, then crouches to jump, off the picture's edge, and a
                // wide character reaches into it)
                if (g.move[side] == kComing && !(g.placed[side] && (g.jumped[side] || g.moveFor[side] > kHiddenMost)) && d.playerSets) {
                    g_hiddenSets |= d.playerSets(side);
                }
            }
            if (g_hiddenSets) {
                machine.video.hideObject = HideTagged;
            }
        }
        // (a fighter the game plays hears nothing of the controls; nobody's Start or coin does anything)
        // (... but a side the toolkit steers: in a tag's movements, or the coach's plan)
        one = g.computer[0] && !Steered(0) ? 0 : one & kAnyButton;
        two = g.computer[1] && !Steered(1) ? 0 : two & kAnyButton;
        SayWhoPlays();
        if (g.way == kStageScroll) {
            // The floor moves under the fighters: they and what they have thrown are carried left with it,
            // and the picture's left edge (the fighters' camera stays where it is) pushes them along
            CameraAt().x = Arena();
            const uint32_t by = static_cast<uint32_t>(ScrollBy()) << 8;
            for (int n = 0; n < d.playerRecords; n++) {
                const uint32_t record = d.player1 + static_cast<uint32_t>(n) * d.playerSize;
                if (Ram8(record + d.active)) {
                    SetRam32(record + d.x, Ram32(record + d.x) - by);
                }
            }
            for (int n = 0; n < d.thrownRecords; n++) {
                const uint32_t record = d.thrown + static_cast<uint32_t>(n) * d.thrownSize;
                if (Ram8(record + d.active)) {
                    SetRam32(record + d.x, Ram32(record + d.x) - by);
                }
            }
        }
    }

    // --- the stage behind the fighters ---

    // One of the stage machines for a frame: on its way to its round, or showing its stage with the camera
    // where it is wanted
    void RunStage(Machine &machine, Spare &spare, int stage, int cameraX, int cameraY, Made *made) {
        RunSpare(machine, spare, [&](Machine &m) {
            Wanted().stage = stage;
            if (!spare.ready) {
                spare.ready = ToRound(m, spare.frames);
            }
            if (spare.ready) {
                m.players[0] = m.players[1] = 0;
                m.video.hideObject = d.notTheStage;
                m.video.layerMask = 0xF & ~d.panelLayers;
                CameraAt().x = cameraX;
                CameraAt().y = cameraY;
            }
        }, made);
    }

    // The fighters laid over a stage's picture: the pixels the fighters' machine drew an object at. `show` of
    // 256 is how much of them is seen. The player `falls` (-1: nobody), falling off the stage, is drawn behind
    // the other.
    void Over(std::vector<uint32_t> &stage, const std::vector<uint32_t> &fighters, int show, int falls = -1) {
        const uint32_t sets = falls >= 0 ? d.playerSets(falls) : 0;
        const uint32_t keep = static_cast<uint32_t>(256 - show), put = static_cast<uint32_t>(show);
        if (stage.size() != fighters.size()) {
            return; // (two pictures of different widths: a frame in which one was not yet the stage's)
        }
        const int width = static_cast<int>(fighters.size() / Video::kHeight);
        for (int pass = sets ? 0 : 1; pass < 2; pass++) { // (the one falling first: behind the other)
            for (int y = 0; y < Video::kHeight; y++) {
                for (int x = 0; x < width; x++) {
                    const uint32_t f = fighters[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
                    if (!(f & 0x80000000)) {
                        continue;
                    }
                    const bool falling = (sets >> ((f >> 24) & 0x1F)) & 1;
                    if (falling != (pass == 0)) {
                        continue;
                    }
                    uint32_t &b = stage[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
                    b = show >= 256 ? f & 0xFFFFFF :
                        ((((b & 0xFF00FF) * keep + (f & 0xFF00FF) * put) >> 8) & 0xFF00FF) | ((((b & 0x00FF00) * keep + (f & 0x00FF00) * put) >> 8) & 0x00FF00);
                }
            }
        }
    }
    // After the fighters' frame: the stage machines' frames, and the picture and sound put together. `show`
    // of 256 is how much of the fighters is seen (they fade when one match follows another).
    void Layer(Machine &machine, int show) {
        static Made fighters, back, next;
        Take(machine, g_soundBefore, fighters);
        const int cameraY = CameraY();
        const int cameraX = g.way == kStageScroll ? g.backCamera / 256 : CameraX();
        const bool fighting = g.phase != Phase::Starting;

        // The stage after this one: wanted when this one has been fought on long enough (shuffle), or as
        // soon as this one is the one shown (scroll)
        if (fighting && !g.backNext && (g.way == kStageScroll || (g.way == kStageShuffle && g.backFor >= ShuffleAfter()))) {
            g.backNext = NextStage();
            if (g.backNext) {
                Begin(g_backNext);
                g_backNextSound.Start();
            }
        }
        RunStage(machine, g_back, g.back, cameraX, cameraY, &back);
        if (!g_back.ready) {
            RunStage(machine, g_back, g.back, cameraX, cameraY, &back); // (twice as fast as the fighters' machine on the way)
            Give(machine, fighters);
            return;
        }
        g_backSound.Add(back.sound);
        bool nextShown = false;
        if (g.backNext) {
            const bool wasReady = g_backNext.ready;
            // (on its way to its round: three frames a frame; when the stages scroll by, more if that is what
            // has it ready when the shown stage has been crossed, so that the scrolling does not wait for it. A
            // wide picture's stage is crossed sooner, and at the fast speed three were too few.)
            int onTheWay = 3;
            if (g.way == kStageScroll) {
                const int32_t framesLeft = std::max((RightEnd() - g.backCamera) / std::max(ScrollBy(), 1), 1);
                onTheWay = std::clamp((d.stageToRound - g_backNext.frames + framesLeft - 1) / framesLeft, 3, 6);
            }
            for (int n = 0; n < (wasReady ? 1 : onTheWay); n++) {
                RunStage(machine, g_backNext, g.backNext, g.way == kStageScroll ? LeftEnd() / 256 : cameraX, cameraY, &next);
                if (g_backNext.ready) {
                    break;
                }
            }
            if (wasReady) {
                g_backNextSound.Add(next.sound);
                nextShown = true;
            }
        }
        if (!fighting) {
            g_backSound.waiting.resize(kSoundAhead * 2); // (not heard yet)
            Give(machine, fighters);
            return;
        }
        g.backFor++;

        // The stage's picture: one stage, or two (faded, or side by side)
        int nextGain = 0;
        nextShown = nextShown && next.picture.size() == back.picture.size();
        if (nextShown && g.way == kStageShuffle) {
            g.blend = std::min(g.blend + 1, kBlend);
            nextGain = g.blend * 256 / kBlend;
            for (size_t i = 0; i < back.picture.size(); i++) {
                const uint32_t a = back.picture[i], b = next.picture[i];
                const uint32_t rb = (((a & 0xFF00FF) * static_cast<uint32_t>(256 - nextGain) + (b & 0xFF00FF) * static_cast<uint32_t>(nextGain)) >> 8) & 0xFF00FF;
                const uint32_t gr = (((a & 0x00FF00) * static_cast<uint32_t>(256 - nextGain) + (b & 0x00FF00) * static_cast<uint32_t>(nextGain)) >> 8) & 0x00FF00;
                back.picture[i] = rb | gr;
            }
        } else if (g.way == kStageScroll) {
            // (onwards: across this stage, then this stage out and the next in, if that one is ready)
            const int32_t by = ScrollBy();
            if (g.slide > 0 || g.backCamera + by >= RightEnd()) {
                g.backCamera = RightEnd();
                g.slide = nextShown ? std::min(g.slide + by, Across()) : 0;
            } else {
                g.backCamera += by;
            }
            const int width = static_cast<int>(back.picture.size() / Video::kHeight);
            const int slide = std::min(g.slide / 256, width);
            nextGain = slide * 256 / width;
            if (slide > 0) {
                for (int y = 0; y < Video::kHeight; y++) {
                    uint32_t *row = &back.picture[static_cast<size_t>(y) * width];
                    memmove(row, row + slide, static_cast<size_t>(width - slide) * sizeof(uint32_t));
                    memcpy(row + width - slide, &next.picture[static_cast<size_t>(y) * width], static_cast<size_t>(slide) * sizeof(uint32_t));
                }
            }
        }
        const bool falling = g.out >= 0 && (g.phase == Phase::Falling || g.phase == Phase::Dropping || g.phase == Phase::Over);
        Over(back.picture, fighters.picture, show, falling ? g.out : -1);
        fighters.picture = back.picture;
        // The sound: the fighters', and the stages' music, one fading into the other
        g_backSound.Mix(fighters.sound, 256 - nextGain);
        if (nextShown) {
            g_backNextSound.Mix(fighters.sound, nextGain);
        }
        // The next stage is all there is: it is the stage now
        if ((g.way == kStageShuffle && g.blend >= kBlend) || (g.way == kStageScroll && g.slide >= Across())) {
            g_back = std::move(g_backNext);
            g_backNext.Clear();
            g_backSound = g_backNextSound;
            g.back = g.backNext;
            g.backNext = 0;
            g.tourAt += g.tourStages ? 1 : 0;
            g.backFor = 0;
            g.blend = 0;
            g.slide = 0;
            g.backCamera = LeftEnd();
        }
        Give(machine, fighters);
    }

    // The picture nearer black by `part` of 256
    void Darken(Machine &machine, int part) {
        const int keep = 256 - std::clamp(part, 0, 256);
        uint32_t *picture = machine.Picture();
        for (int i = 0; i < machine.PictureWidth() * Video::kHeight; i++) {
            const uint32_t p = picture[i];
            picture[i] = ((((p & 0xFF00FF) * static_cast<uint32_t>(keep)) >> 8) & 0xFF00FF) | ((((p & 0x00FF00) * static_cast<uint32_t>(keep)) >> 8) & 0x00FF00);
        }
    }

    // The Multi game page's endless battle: when the last stage has been shown (fought on for its time, or scrolled
    // across), the picture and sound fade out, and the game ends for the next game's turn
    void TourEnd(Machine &machine) {
        const bool last = g.tourAt >= g.tourStages - 1 && !g.backNext;
        if (!g.tourOut && last && ((g.way == kStageShuffle && g.backFor >= ShuffleAfter()) || (g.way == kStageScroll && g.backCamera >= RightEnd()))) {
            g.tourOut = 1;
        }
        if (!g.tourOut) {
            return;
        }
        const int part = std::min(g.tourOut, kTourOut) * 256 / kTourOut;
        Darken(machine, part);
        for (int16_t &sample : machine.audio) {
            sample = static_cast<int16_t>(sample * (256 - part) / 256);
        }
        if (++g.tourOut > kTourOut) {
            EndProgram(kNextGame);
        }
    }

    // The next match made ready on the spare machine, a few frames a frame until its round is on
    void MakeNext(Machine &machine) {
        for (int n = 0; n < kOnTheWay && !g_next.ready && g_next.frames <= kStartMost; n++) {
            RunSpare(machine, g_next, [](Machine &spare) {
                Want();
                g_next.ready = ToRound(spare, g_next.frames);
            });
        }
    }

    // --- the ring out ---

    // Is a fighter out? Knocked about (not its own master), it has gone past where it would have been kept in at
    // an end of the stage (RingOutLetsOut): its side past the picture's edge.
    void LookForOut() {
        const int camera = CameraX(), edge = PictureLeft();
        for (int n = 0; n < 2 && g.out < 0; n++) {
            if (OwnMaster(Player(n))) {
                continue;
            }
            const int x = static_cast<int16_t>(Ram16(Player(n) + d.x)), half = HalfWidth(Player(n));
            const int margin = d.wideMargin();
            const int side = camera <= CameraLeft() + margin && x - half < edge - margin ? -1 :
                             camera >= CameraRight() - margin && x + half > edge + Video::kWidth + margin ? 1 : 0;
            if (side) {
                if (Log()) {
                    fprintf(Log(), "out %d side=%d end=%d computer=%d plan=%s free_at=%d\n", g.clock, n, side, g.computer[n],
                            kPlanNames[g.plan[1 - n]], g.lastFree[n]);
                }
                g.plan[0] = g.plan[1] = kPlay;
                g.out = n;
                g.outSide = side;
                g.phase = Phase::Falling;
                g.since = 0;
                g.score += n == 1;
            }
        }
    }

    // The next match, and its stage, made ready while this one is fought (who it is against is known: the
    // player's character against someone new)
    void MakeDrop(Machine &machine) {
        const int frames = g.phase == Phase::Falling ? kDropReadyFalling : kDropReady;
        for (int n = 0; n < frames && !g_next.ready && g_next.frames <= kStartMost; n++) {
            RunSpare(machine, g_next, [](Machine &spare) {
                Want();
                g_next.ready = ToRound(spare, g_next.frames);
            }, nullptr, [](Machine &) {
                // (where its camera is with the fighters standing: the way down's bottom)
                g.dropCameraX = CameraX();
                g.dropCameraY = CameraY();
            });
        }
        for (int n = 0; n < frames && !g_dropBack.ready && g_dropBack.frames <= kStartMost; n++) {
            RunSpare(machine, g_dropBack, [](Machine &spare) {
                Wanted().stage = g.dropStage;
                g_dropBack.ready = ToRound(spare, g_dropBack.frames);
            });
        }
    }

    // At the top of its jump on the old stage the winner is handed over to the next match, where it is in the
    // picture: as high above the next stage's floor as the camera is held, and more (RingOutMove puts it there, at
    // the next match's next frame)
    void HandOver() {
        const uint32_t winner = Player(0);
        if (g.dropped || g.handing || g.since <= kDropJump + 2 || !d.atJumpTop(winner)) {
            return;
        }
        const int across = static_cast<int16_t>(Ram16(winner + d.x)) - PictureLeft();
        g.flightX = g.dropCameraX + d.pictureLeft + std::clamp(across, kDropInside, PictureWidth() - kDropInside);
        // (a picture above the next stage's: up from its bottom, or with heights downwards up from its top)
        g.flightY = d.yDown ? static_cast<int16_t>(Ram16(winner + d.y)) - CameraY() + g.dropCameraY - Video::kHeight :
                              static_cast<int16_t>(Ram16(winner + d.y)) - CameraY() + g.dropCameraY + Video::kHeight;
        for (uint32_t n = 0; n < std::min(d.playerSize, battle::kMostRecord); n++) {
            g.flight[n] = Ram8(winner + n);
        }
        g.handedY = g.flightY;
        g.handing = true;
    }

    // The way down, after the picture and sound of the old stage are put together (with the winner in it until it is
    // handed over): the next match's machines run, the camera follows the winner down, and the picture is the old
    // stage going up and out as the next one comes up from below, the fighters over both.
    void Drop(Machine &machine) {
        static Made old, fighters, stage;
        Take(machine, g_soundBefore, old);
        const int height = Video::kHeight;
        if (g.since == 1) {
            g_dropSound.Start();
            g_dropBackSound.Start();
        }
        // (the camera: held up till then; then, as the winner falls, down as far as it has fallen)
        if (g.dropped) {
            const int fallen = d.yDown ? g.dropY - g.handedY : g.handedY - g.dropY;
            const int32_t follow = std::clamp((height - fallen) * 256, 0, height * 256);
            g.raised += (follow - g.raised) / kDropFollow;
            if (std::abs(follow - g.raised) < 256) {
                g.raised = follow;
            }
        }
        const int32_t shown = g.raisedShown; // (a machine's picture is the frame before's: drawn where its camera was then)
        g.raisedShown = g.raised;
        RunSpare(machine, g_next, [](Machine &m) {
            m.video.hideObject = d.notTheFighters;
            m.video.layerMask = 1 | d.panelLayers;
            m.video.markObjects = true;
            m.video.markLayers = d.panelLayers;
            m.players[0] = 0;
            for (int n = 0; n < 2; n++) {
                SetComputer(n, false);
            }
            BothPlay();
            if (g.since == 1) {
                d.playSound(d.tuneOff);
            }
            CameraAt().y = d.yDown ? g.dropCameraY - g.raised / 256 : g.dropCameraY + g.raised / 256;
            CameraAt().beyondEnds = true;
            g_dropIn = g.handing;
            g.handing = false;
        }, &fighters, [](Machine &) {
            g_dropIn = false;
            g.dropCameraX = CameraX();
            g.dropY = static_cast<int16_t>(Ram16(Player(0) + d.y));
            g.landed = g.dropped && !d.inAir(Player(0));
        });
        RunStage(machine, g_dropBack, g.dropStage, g.dropCameraX, g.dropCameraY, &stage);
        g_dropSound.Add(fighters.sound);
        g_dropBackSound.Add(stage.sound);

        const int up = height - shown / 256;
        if (old.picture.size() == stage.picture.size()) {
            if (up > 0) {
                const size_t width = old.picture.size() / Video::kHeight, row = width * sizeof(uint32_t);
                memmove(old.picture.data(), old.picture.data() + static_cast<size_t>(up) * width, static_cast<size_t>(height - up) * row);
                memcpy(old.picture.data() + static_cast<size_t>(height - up) * width, stage.picture.data(), static_cast<size_t>(up) * row);
            }
            if (g.since >= kDropShown) { // (the first pictures are from before the camera was held up)
                Over(old.picture, fighters.picture, 256);
            }
        }
        const int gain = up * 256 / height;
        for (int16_t &sample : old.sound) {
            sample = static_cast<int16_t>(sample * (256 - gain) / 256);
        }
        g_dropSound.Mix(old.sound, gain);
        g_dropBackSound.Mix(old.sound, gain);
        Give(machine, old);
    }

    // At the bottom: the next match's machines are the ones seen from here, and the match after it is begun
    void Landed(Machine &machine) {
        Promote(machine, g_next);
        Begin(g_next);
        g_back = std::move(g_dropBack);
        Begin(g_dropBack);
        g_backSound = g_dropBackSound;
        g.back = g.dropStage;
        g.dropStage = AnyStage(g.back);
        g.characters[1] = AnyCharacter(Ram8(Player(1) + d.character));
        g.out = -1;
        g.plan[0] = g.plan[1] = kPlay;
        g.flung = false;
        g.handing = false;
        g.dropped = false;
        g.landed = false;
        g.phase = Phase::Fighting;
        g.since = 0;
    }

    // A mode over: back to the title; or, for tests (CPS2RECOMP_BATTLE_CPU), begun again on its own numbers at random,
    // for the next series of matches
    void Again(Machine &machine) {
        const uint32_t random = g.random, coach = g.coachRandom;
        const int32_t clock = g.clock;
        const Kind kind = g.kind;
        End();
        if (!getenv("CPS2RECOMP_BATTLE_CPU")) {
            machine.Reset();
            return;
        }
        Start(machine, kind);
        g.random = random;
        g.coachRandom = coach;
        g.clock = clock;
        if (kind == Kind::Team) {
            StartTeams(); // (new teams, by the new numbers)
            g.computer[0] = true;
        } else if (kind == Kind::RingOut) {
            g.characters[1] = AnyCharacter();
            g.characters[0] = AnyCharacter(g.characters[1]);
        }
        if (Log()) {
            fprintf(Log(), "series %d\n", g.clock);
        }
    }

    void Frame(Machine &machine) {
        g_portFrame = false; // (from here the machines run are spares, until the port's next frame)
        g_open = false;
        if (g.phase != Phase::Starting && !d.inGame()) {
            End(); // (the board was reset)
            return;
        }
        std::vector<HudLine> lines;
        int dark = 0;         // how far the fighters (or, with one stage, the whole picture) are faded, of 256
        bool layered = false; // the picture and sound have been put together already
        if (g.kind == Kind::RingOut && g.phase != Phase::Starting && g.phase != Phase::Over) {
            // (there is no knock-down here: a fighter is out, or the fight goes on)
            if (g.phase == Phase::Fighting) {
                LookForOut();
                MakeDrop(machine);
            } else if (g.phase == Phase::Falling) {
                g.since++;
                MakeDrop(machine);
                if (g.since >= kFallFor && g.out == 0) {
                    g.phase = Phase::Over;
                    g.since = 0;
                    if (g.score > FeatureValue("ringout_best")) {
                        SetFeatureValue("ringout_best", std::min(g.score, 99));
                    }
                } else if (g.since >= kFallFor && g_next.ready && g_dropBack.ready) {
                    g.phase = Phase::Dropping;
                    g.since = 0;
                    g.raised = g.raisedShown = Video::kHeight * 256; // (the next match's camera held up from the first)
                } else if (g_next.frames > kStartMost || g_dropBack.frames > kStartMost) {
                    End();
                    return;
                }
            } else if (g.phase == Phase::Dropping) {
                g.since++;
                HandOver();
            }
        } else if (g.phase == Phase::Fighting) {
            const uint8_t result = d.resultNow ? d.resultNow() : Ram8(d.result);
            if (result) {
                g.winner = result == 2 ? 1 : 0; // (both down at once: the first player's)
                g.phase = Phase::Decided;
                g.since = 0;
                Begin(g_next);
                if (g.kind == Kind::Team && g.tag) {
                    // (the tag style: the last fighter of a side knocked out, as only the last one can be)
                    g.phase = Phase::Over;
                    g.winner = result == 3 ? -1 : result == 2 ? 1 : 0;
                    if (Log()) {
                        fprintf(Log(), "over %d winner=%d\n", g.clock, g.winner);
                    }
                } else if (g.kind == Kind::Team) {
                    TeamDecided(result);
                } else if (g.kind == Kind::Survival) {
                    if (result != 1) {
                        g.phase = Phase::Over;
                        if (g.score > FeatureValue("survival_best")) {
                            SetFeatureValue("survival_best", std::min(g.score, 99));
                        }
                    } else {
                        g.score++;
                        const auto left = [](uint32_t which) {
                            const int now = static_cast<int16_t>(Ram16(Player(0) + which));
                            return std::min<int>(std::max(now, 0) + d.healthBack, d.healthFull);
                        };
                        g.health = left(d.health);
                        g.ceiling = d.ceilingShown ? g.health : std::max(left(d.healthCeiling), g.health);
                        g.characters[1] = AnyCharacter(g.characters[1]);
                    }
                } else {
                    // The one knocked down is replaced (by the one chosen for that side in the menu, if that
                    // is not who has just lost)
                    const int loser = 1 - g.winner;
                    const int chosen = ChosenCharacter(loser == 0 ? "endless_one" : "endless_two");
                    g.characters[loser] = chosen >= 0 && chosen != g.characters[loser] ? chosen : AnyCharacter(g.characters[0], g.characters[1]);
                    g.score++;
                    if (!Layered()) {
                        g.stage = ChosenStage();
                    }
                }
            }
        } else if (g.phase == Phase::Decided) {
            g.since++;
            MakeNext(machine);
            if (g.since > kAfter - kFade) {
                dark = (g.since - (kAfter - kFade)) * 256 / kFade;
            }
            if (g.since >= kAfter && g_next.ready) {
                if (Layered()) {
                    Layer(machine, 0); // (this frame's stage and sound, before the machine is another)
                    layered = true;
                }
                Promote(machine, g_next);
                // (the one who stays on, with the health it had left)
                const int stays = g.kind == Kind::Survival ? 0 : g.kind == Kind::Team ? g.stays : -1;
                if (stays >= 0) {
                    SetRam16(Player(stays) + d.health, static_cast<uint16_t>(g.health));
                    SetRam16(Player(stays) + d.healthCeiling, static_cast<uint16_t>(g.ceiling));
                }
                if (Layered()) {
                    d.playSound(d.tuneOff);
                }
                dark = 256;
                g.phase = Phase::Fighting;
                g.since = 0;
                g.fadeIn = kFade;
            } else if (g_next.frames > kStartMost) {
                if (g.tourStages) {
                    EndProgram(kNextGame);
                }
                End();
                return;
            }
        } else if (g.phase == Phase::Over && g.kind == Kind::Team) {
            g.since++;
            const bool people = !g.computer[1];
            lines.push_back({0.5f, 0.26f, 0, g.winner < 0 ? "DRAW" : g.winner == 0 ? (people ? "TEAM 1 WINS" : "YOUR TEAM WINS") :
                                                                              (people ? "TEAM 2 WINS" : "CPU TEAM WINS")});
            if (g.since > kOverFor) {
                Again(machine);
                return;
            }
        } else if (g.phase == Phase::Over) {
            g.since++;
            char text[64];
            const bool ring = g.kind == Kind::RingOut;
            lines.push_back({0.5f, 0.36f, 0, ring ? "RING OUT" : "SURVIVAL OVER"});
            snprintf(text, sizeof(text), ring ? "Ring outs: %d" : "Opponents beaten: %d", g.score);
            lines.push_back({0.5f, 0.46f, 0, text});
            snprintf(text, sizeof(text), "Best: %d", std::max(g.score, FeatureValue(ring ? "ringout_best" : "survival_best")));
            lines.push_back({0.5f, 0.54f, 0, text});
            if (g.since > kOverFor) {
                Again(machine);
                return;
            }
        }
        if (g.phase == Phase::Fighting && g.fadeIn > 0) {
            dark = std::max(dark, g.fadeIn * 256 / kFade);
            g.fadeIn--;
        }
        if (Layered()) {
            if (!layered) {
                // (on the way down the winner is seen over the old stage until the next match's picture has it)
                Layer(machine, g.phase == Phase::Dropping && g.dropped ? 0 : 256 - dark);
            }
            if (g.phase == Phase::Dropping) {
                Drop(machine);
                if ((g.landed && g.raised == 0) || g.since >= kDropMost) {
                    Landed(machine);
                }
            }
        } else if (dark) {
            Darken(machine, dark);
        }
        if (g.tourStages && g.phase != Phase::Starting) {
            TourEnd(machine);
        }
        // The score so far, all the while (the ring out's through the fall and the drop to the next stage too)
        if ((g.kind == Kind::Survival || g.kind == Kind::RingOut) && g.phase != Phase::Starting && g.phase != Phase::Over) {
            char text[32];
            snprintf(text, sizeof(text), g.kind == Kind::RingOut ? "Ring outs: %d" : "Beaten: %d", g.score);
            lines.push_back({0.5f, 0.21f, 0, text});
        }
        if (g.kind == Kind::Team && g.tag && g.phase == Phase::Fighting) {
            TagFrame();
        }
        if (g.kind == Kind::Team && (g.phase == Phase::Fighting || g.phase == Phase::Decided)) {
            TeamLines(lines);
        }
        SetHud(std::move(lines));
        SetBoxes({});
    }
} // namespace

bool BattleMode() {
    return g.kind != Kind::None;
}
void EndBattle() {
    End();
}
bool BattleNoClock() {
    return g.kind == Kind::Endless || g.kind == Kind::RingOut;
}
bool BattleNoKnockOut() {
    return g.kind == Kind::RingOut;
}
bool TeamBattle() {
    return g.kind == Kind::Team;
}
int TeamTimeOverResult() {
    if (g.kind != Kind::Team || !g.tag) {
        return 0;
    }
    int total[2] = {0, 0};
    for (int side = 0; side < 2; side++) {
        total[side] = std::max<int>(static_cast<int16_t>(Ram16(Player(side) + d.health)), 0);
        for (int n = 0; n < g.teamSize; n++) {
            total[side] += n != g.on[side] ? g.kept[side][n] : 0;
        }
    }
    return total[0] == total[1] ? 3 : total[0] > total[1] ? 1 : 2;
}
bool TeamHoldsKnockOut(int player) {
    if (g.kind != Kind::Team || !g.tag || g.phase != Phase::Fighting || (player != 0 && player != 1)) {
        return false;
    }
    for (int n = 0; n < g.teamSize; n++) {
        if (n != g.on[player] && g.kept[player][n] > 0) {
            return true;
        }
    }
    return false;
}

void BattleBeforeFrame(Machine &machine) {
    g_portFrame = true;
    g_frame++;
    // Started by the launcher's Multi game page: its endless battle, every stage once
    ReadTour();
    if (g_tour.on && !g_tour.started && d.stages > 0) {
        g_tour.started = true;
        Start(machine, Kind::Endless);
        g.way = g_tour.way;
        PlanTour();
        g.stage = g.back = g.tourOrder[0];
        g.backCamera = LeftEnd();
        g_backSound.Start();
    }
    // The player asked for the next game: the fade-out the last stage ends with, from here
    if (g_tour.on && NextGameAsked() >= 0 && g.tourStages && g.phase != Phase::Starting && !g.tourOut) {
        g.tourOut = 1;
    }
    if (g.kind != Kind::None) {
        BeforeFrame(machine);
    }
}

void BattleFrame(Machine &machine) {
    if (g.kind != Kind::None) {
        Frame(machine);
    }
    // What the next frame of the port's machine draws: all of it; or no panel; or the fighters alone, marked,
    // to be laid over the stage that another machine shows
    // The ring out: the shadow of the one who went over the end does not stay on the floor
    static int noShadow = -1;
    const int out = g.kind == Kind::RingOut ? g.out : -1;
    if (d.noShadow && out != noShadow) {
        noShadow = out;
        d.noShadow(machine, out);
    }
    if (g.kind != Kind::None || !d.stageViewer()) {
        const bool fighting = (g.kind == Kind::Endless || g.kind == Kind::RingOut) && g.phase != Phase::Starting;
        const bool layered = fighting && Layered();
        machine.video.hideObject = layered ? d.notTheFighters : fighting ? d.thePanel : nullptr;
        machine.video.layerMask = layered ? 1 | d.panelLayers : 0xF;
        machine.video.markObjects = layered;
        machine.video.markLayers = layered ? d.panelLayers : 0;
    }
}

void BattleState(Archive &ar) {
    ar.Mark(0x42415454);
    ar.Pod(g);
    ar.Vec(g_next.state);
    ar.Pod(g_next.ready);
    ar.Pod(g_next.frames);
    for (Spare *spare : {&g_back, &g_backNext, &g_dropBack}) {
        ar.Vec(spare->state);
        ar.Pod(spare->ready);
        ar.Pod(spare->frames);
    }
    if (!ar.Saving() && !RunningAhead()) { // (not when run-ahead puts the frame back: runtime/game.h)
        g_fast = false;
        g_backSound.Start();
        g_backNextSound.Start();
    }
}

RingOut RingOutMove(uint32_t player) {
    RingOut ringOut;
    if (g.kind != Kind::RingOut) {
        return ringOut;
    }
    if (g_dropIn && player == Player(0)) {
        g.dropped = true;
        ringOut.move = RingOut::kDropsIn;
        ringOut.x = g.flightX;
        ringOut.y = g.flightY;
        ringOut.record = g.flight.data();
    } else if (g_portFrame && g.out >= 0 && player == Player(g.out) &&
               (g.phase == Phase::Falling || g.phase == Phase::Dropping || g.phase == Phase::Over)) {
        ringOut.move = g.flung ? RingOut::kFalling : RingOut::kFlungOut;
        ringOut.side = g.outSide;
        g.flung = true;
    }
    return ringOut;
}

bool RingOutMayWalkBack(uint32_t player) {
    const int side = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
    if (g.kind != Kind::RingOut || g.phase != Phase::Fighting || side < 0 || !Coached(side)) {
        return true;
    }
    return !Part(8) || Back(side) >= Between(100, 200);
}

namespace {
    int SideOf(uint32_t player) {
        return player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
    }
} // namespace

ComputerMode ComputerModeOf(uint32_t player) {
    const int side = SideOf(player);
    if (side < 0 || !d.computerPlaysModes || g.phase != Phase::Fighting || !Coached(side)) {
        return ComputerMode::kNone;
    }
    if (g.kind == Kind::RingOut) {
        return ComputerMode::kRingOut;
    }
    if (g.kind == Kind::Team && g.tag && g.move[side] == kStill) {
        return ComputerMode::kTag;
    }
    return ComputerMode::kNone;
}

int ComputerBack(uint32_t player, bool other) {
    const int side = SideOf(player);
    return side < 0 || g.kind != Kind::RingOut ? 0 : Back(other ? 1 - side : side);
}

// The tag style's want (ComputerTags' first part): the one on low and a partner with more, or a partner who has rested
// with much more; not soon after the last tag, nor just out of a hit
bool ComputerTagWanted(uint32_t player) {
    const int side = SideOf(player);
    if (ComputerModeOf(player) != ComputerMode::kTag || g.cool[side] > 0 || g.free[side] < kOutOfHit) {
        return false;
    }
    const int next = Healthiest(side);
    const int full = std::max<int>(g.full, 1);
    const int on = std::max<int>(static_cast<int16_t>(Ram16(Player(side) + d.health)), 0) * 1000 / full;
    const int partner = next >= 0 ? g.kept[side][next] * 1000 / full : 0;
    const bool low = on < Between(250, 400) && partner >= on + 150;
    const bool rested = partner >= on + 450 && g.sinceTag[side] > Between(1800, 1200);
    return next >= 0 && g.sinceTag[side] >= Between(720, 360) && (low || rested);
}

int ComputerSkill() {
    return Skill();
}

void ComputerAsksTag(uint32_t player) {
    const int side = SideOf(player);
    if (side >= 0 && ComputerModeOf(player) == ComputerMode::kTag) {
        g.tagAsked[side] = 20;
        if (Log()) {
            fprintf(Log(), "asked to tag %d side=%d\n", g.clock, side);
        }
    }
}

// In a ring out fight a player that is not its own master (reeling from a hit, guarding one, thrown) is not
// kept in at a side where the picture is at the stage's end: it goes over the end (the game's sources ask where the
// game keeps the players in).
bool RingOutLetsOut(uint32_t player) {
    if (!g_open || (player != Player(0) && player != Player(1)) || OwnMaster(player)) {
        return false;
    }
    const int x = static_cast<int16_t>(Ram16(player + d.x)), half = HalfWidth(player);
    const int camera = CameraX(), edge = PictureLeft();
    const int margin = d.wideMargin(); // (the picture's edges and the camera's ends)
    const bool left = camera <= CameraLeft() + margin && x - half <= edge - margin;
    const bool right = camera >= CameraRight() - margin && x + half >= edge + Video::kWidth + margin;
    return left || right;
}

int battle::RoundResultNow() {
    return !g_described ? -1 : d.resultNow ? d.resultNow() : Ram8(d.result);
}

int battle::RoundsToWin() {
    return d.roundsToWin;
}

void battle::Register(const Description &description) {
    d = description;
    g_described = true;
    Game &game = TheGame();
    game.pages.push_back({kSurvival, kModes, "One opponent after another on the health you have left. How many can you beat?"});
    game.pages.push_back({kEndless, kModes, "Two fighters the game plays, for as long as you like. Touch the controls to take one over."});
    game.commands.push_back({"survival_start", kSurvival, "Start",
                             "Choose a character; then one opponent after another, each on a knock-down. Your health is what the "
                             "fight before left you, and a little back. Over the first time you go down.",
                             StartSurvival});
    {
        // (kept with the features so that it is remembered; the numbers are its choices)
        static std::vector<std::string> numbers;
        std::vector<const char *> choices;
        for (int n = 0; n < 100; n++) {
            numbers.push_back(std::to_string(n));
        }
        for (const std::string &number : numbers) {
            choices.push_back(number.c_str());
        }
        game.features.push_back({"survival_best", kSurvival, "Best", "The most opponents beaten in one survival. (Left puts it back.)", 0, choices});
    }
    game.pages.push_back({kRingOut, kModes, "No health: knock your opponent over the end of the stage, and jump down to the next."});
    game.commands.push_back({"ringout_start", kRingOut, "Start",
                             "Choose a character. Nobody loses health: the ends of a stage are open, and a fighter knocked over "
                             "one falls. Knock your opponent out and you jump down to the next stage; over when you fall. The ring outs so "
                             "far are shown at the top.",
                             StartRingOut});
    {
        static std::vector<std::string> numbers;
        std::vector<const char *> choices;
        for (int n = 0; n < 100; n++) {
            numbers.push_back(std::to_string(n));
        }
        for (const std::string &number : numbers) {
            choices.push_back(number.c_str());
        }
        game.features.push_back({"ringout_best", kRingOut, "Best", "The most ring outs in one go. (Left puts it back.)", 0, choices});
    }
    game.commands.push_back({"endless_start", kEndless, "Start",
                             "Two fighters played by the game: no bars, no clock, and the one knocked down is replaced. Press "
                             "anything to take a fighter over; ten idle seconds give it back.",
                             StartEndless});
    game.features.push_back({"endless_one", kEndless, "Fighter 1", "Who is on player 1's side: someone new each time, or always the same.", 0, d.fighterChoices});
    game.features.push_back({"endless_two", kEndless, "Fighter 2", "Who is on player 2's side: someone new each time, or always the same.", 0, d.fighterChoices});
    game.features.push_back({"endless_stage", kEndless, "Stage",
                             "Where they fight. Shuffle: a stage that fades into another every so often. Scroll: the stages "
                             "go by, one after another, and the fighters are carried along. Or one stage.",
                             0, d.stageChoices});
    game.features.push_back({"endless_shuffle", kEndless, "Shuffle every",
                             "Shuffle: how long they fight on a stage before it fades into another.", 0,
                             {"1 minute", "2 minutes", "3 minutes", "5 minutes", "10 minutes"}});
    game.features.push_back({"endless_scroll", kEndless, "Scroll speed", "Scroll: how fast the stages go by.", 0, {"Slow", "Medium", "Fast"}});
    if (!d.teams) {
        return;
    }
    game.pages.push_back({kTeam, kModes, "Two teams: knock a fighter down and the next of that team comes on. The last team standing wins."});
    game.commands.push_back({"team_start", kTeam, "Start",
                             "Each side's fighters come on one at a time, in the order below. The one knocked down is out, and "
                             "the next of that side comes on with full health; the winner stays on with the health it has left, "
                             "and a little back. Over when a side has nobody left.",
                             StartTeam});
    if (d.tagIn) {
        game.features.push_back({"team_style", kTeam, "Style",
                                 "Tag: each fighter has its own health; the left trigger (or Start) tags the next one in whenever "
                                 "yours can act, and when a fighter's health runs out the next comes in. Resting fighters get back the "
                                 "health that comes back by itself. The match is over when a team has nobody left. Knock-out: one "
                                 "fighter of each team at a time, the next coming on when one is knocked out.",
                                 0, {"Tag", "Knock-out"}});
    }
    game.features.push_back({"team_size", kTeam, "Fighters", "How many fighters each team has.", 0, {"2", "3"}});
    if (d.oneBarEach) {
        game.features.push_back({"team_bars", kTeam, "Health bars",
                                 "How much health each fighter comes on with: one bar (a team of two has two bars between them), or as "
                                 "many as the board's \"Damage gauges\" setting gives.",
                                 0, {"One each", "As the board is set"}});
    }
    game.features.push_back({"team_opponent", kTeam, "Team 2",
                             "Who plays the second team: the CPU (the game), or player 2. (With \"Tag team\" below, that says who plays.)",
                             0, {"CPU", "Player 2"}});
    if (d.difficulty) {
        game.features.push_back({"team_difficulty", kTeam, "CPU difficulty",
                                 "How well the CPU's team fights, 1 (easiest) to 8 (hardest), or as the board is set (the Board page).",
                                 0, {"As the board is set", "1 (easiest)", "2", "3", "4", "5", "6", "7", "8 (hardest)"}});
    }
    {
        // (the stages: the choices after the endless battle's two ways of changing them)
        static std::vector<const char *> stages = {"Random"};
        stages.insert(stages.end(), d.stageChoices.begin() + std::min<size_t>(2, d.stageChoices.size()), d.stageChoices.end());
        game.features.push_back({"team_stage", kTeam, "Stage", "Where the teams fight, from the first fight to the last.", 0, stages});
    }
    static const char *const kOrder[kTeamMost] = {"first", "second", "third"};
    static std::string titles[2][kTeamMost], helps[2][kTeamMost];
    for (int side = 0; side < 2; side++) {
        for (int n = 0; n < kTeamMost; n++) {
            titles[side][n] = "Team " + std::to_string(side + 1) + ", " + kOrder[n];
            helps[side][n] = std::string("Team ") + std::to_string(side + 1) + "'s " + kOrder[n] + " fighter on" +
                             (n == 2 ? " (with three fighters a team)" : "") + ": someone at random, or this one.";
            game.features.push_back({kTeamRows[side][n], kTeam, titles[side][n].c_str(), helps[side][n].c_str(), 0, d.fighterChoices});
        }
    }
    // (the fighters' rows and the stage are this game's own, by its roster and its stages: the collection's launcher
    // is not to set another game's from them)
    OwnFeatures({"team_1_1", "team_1_2", "team_1_3", "team_2_1", "team_2_2", "team_2_3", "team_stage"});
}

void battle::RegisterTeamTag(float labelY) {
    tag::Description t;
    // (no fighter is on outside a match; a team of three's third is its first person's again)
    t.onPoint = [](int side) { return g.kind == Kind::Team && (g.phase == Phase::Fighting || g.phase == Phase::Decided) ? g.on[side] % 2 : -1; };
    t.otherMode = [] { return g.kind != Kind::Team; };
    // (the tag style: the partner's left trigger tags them in)
    t.fighting = [] { return g.kind == Kind::Team && g.tag && g.phase == Phase::Fighting; };
    t.tagButtons = kTag;
    t.labelY = labelY;
    t.labelEnds = [](float &left, float &right) {
        left = BarsLeft();
        right = BarsRight();
    };
    t.page = kTeam;
    t.help = "In the team battle, each fighter of a team played by a person of its own. Co-op: players 1 and 2 are one team "
             "against the computer's. 2 vs 2: players 1 and 3 against players 2 and 4. A team's panel is whoever's fighter is on "
             "(in a team of three, the third is the first person's again), and in the tag style the partner tags in with "
             "their left trigger. The labels under the bars say who is on.";
    tag::Register(t);
}

} // namespace cps2recomp
