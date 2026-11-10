// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The engine of Capcom's Marvel fighting games: what their sources share (marvel.h). Written first for X-Men vs.
// Street Fighter (its training.cpp, battle.cpp, widescreen.cpp and model.cpp), then made to take Marvel Super Heroes
// from a description of each; the two run as they did before.
#include "marvel.h"

#include "alternative.h"
#include "battle.h"
#include "colours.h"
#include "model.h"
#include "session.h"
#include "state.h"

#include <algorithm>

namespace cps2recomp::marvel {

namespace {
    Engine e;

    int16_t Word(uint32_t address) {
        return static_cast<int16_t>(Ram16(address));
    }
} // namespace

void CopyFrameBoxSets(Machine &machine, uint32_t player, uint32_t frame) {
    const auto long_ = [&](uint32_t at) { return (static_cast<uint32_t>(machine.Read16(at)) << 16) | machine.Read16(at + 2); };
    SetRam16(player + 0x3C, machine.Read16(frame));
    SetRam32(player + 0x40, long_(frame + 0x04));
    SetRam32(player + 0x62, long_(frame + 0x08));
    SetRam16(player + 0x66, machine.Read16(frame + 0x0C));
    const uint32_t boxes = Ram32(player + 0x68) + static_cast<uint32_t>(machine.Read16(frame + 0x0E)) * 16;
    for (uint32_t at = 0; at < 16; at += 4) {
        SetRam32(player + 0x70 + at, long_(boxes + at));
    }
}

void Describe(const Engine &engine) {
    e = engine;
}
const Engine &Described() {
    return e;
}

// --- the players ---

uint32_t Player(int n) {
    return e.playerAt ? e.playerAt(n) : e.player1 + static_cast<uint32_t>(n) * e.playerSize;
}
int PlayerOf(uint32_t record) {
    record &= 0xFFFFFF;
    return record == Player(0) ? 0 : record == Player(1) ? 1 : -1;
}
bool InAir(uint32_t player) {
    return Word(player + e.y) != e.ground();
}
bool FacingLeft(int n) {
    return (Ram8(Player(n) + e.flip) & 1) == 0;
}
uint16_t PanelHeld(int n) {
    const uint16_t word = Ram16(e.panel[n]);
    return static_cast<uint16_t>((word & 0x7F) | (((word >> 8) & 0x07) << 7));
}
const char *CharacterName(uint32_t player) {
    const unsigned who = Ram16(player + e.character) / 2;
    return who < e.characterNames.size() ? e.characterNames[who] : "?";
}

// --- the training session ---

namespace {
    // The character is free to do anything: in charge of itself, and standing, crouching, walking or guarding
    bool CanAct(uint32_t player) {
        const uint8_t action = Ram8(player + e.action);
        return Ram16(player + e.control) == 0 && (action == e.actionStand || action == e.actionCrouch || action == e.actionWalk ||
                                                  action == e.actionGuard || action == e.actionCrouchGuard);
    }

    bool AttackOut(uint32_t player) {
        return Ram8(player + e.canHit) != 0 &&
               ((Ram16(player + e.attackBoxes) & 0x1FFF) != 0 || (Ram16(player + e.attackBoxes + 2) & 0x1FFF) != 0);
    }

    uint8_t g_hitStop[2] = {0, 0}; // each player's freeze as it last stepped
} // namespace

// Seen as the step starts, the record is as the last step left it, which is what the picture showed and the hit tests
// tested; state 2 is fighting
bool PlayerStep(M68K &c) {
    if (RingOutStep(c)) {
        Return(c);
        return true;
    }
    const uint32_t player = c.a[6] & 0xFFFFFF;
    const int n = PlayerOf(player);
    if (n < 0 || Ram16(player + e.state) != 2) {
        return false;
    }
    // A hit landing freezes both characters: the one hit is told to the frame data as hit, or as guarding
    const uint8_t stop = Ram8(player + e.hitStop);
    if (stop != 0 && g_hitStop[n] == 0 && !AttackOut(player)) {
        const uint8_t action = Ram8(player + e.action);
        session::Contact(n, action == e.actionGuard || action == e.actionCrouchGuard);
    }
    if (!RunningAhead()) {
        g_hitStop[n] = stop; // (run-ahead's frames are taken back)
    }
    fight::StepView view;
    view.control = Ram16(player + e.control) == 0 ? fight::StepView::Own : fight::StepView::Reeling;
    view.frozen = stop != 0;
    view.canAct = CanAct(player);
    view.attacking = (Ram8(player + e.attackPhase) & e.attackPhaseMask) != 0;
    view.jumping = Ram8(player + e.action) == e.actionJump || InAir(player);
    view.attackOut = AttackOut(player);
    session::Step(n, view);
    return false;
}

fight::MoveLog::Start MoveStart(int n) {
    const uint32_t player = Player(n);
    fight::MoveLog::Start start;
    start.character = CharacterName(player);
    start.input = fight::Notation(PanelHeld(n), FacingLeft(n));
    start.air = InAir(player);
    start.id = Ram32(player + e.frame);
    return start;
}

namespace {
    // The boxes go to the session in its own terms: across as the picture's place plus the camera's word, and up, the
    // camera's word down being the picture's bottom (fight::ToPicture puts them back)
    void AddBox(M68K &c, std::vector<fight::StageBox> &boxes, fight::BoxKind kind, uint32_t object, uint32_t box, bool thrown) {
        const int halfWidth = static_cast<int>(c.Read(box + 2, 1)), halfHeight = static_cast<int>(c.Read(box + 6, 1));
        if (halfWidth == 0 || halfHeight == 0) {
            return;
        }
        const uint8_t flip = Ram8(object + e.flip);
        const int offsetX = static_cast<int16_t>(c.Read(box + 0, 1)), offsetY = static_cast<int16_t>(c.Read(box + 4, 1));
        const int x = Word(object + e.x) + ((flip & 1) ? -offsetX : offsetX);
        const int y = Word(object + e.y) + ((flip & 2) && kind != fight::BoxKind::Push ? -offsetY : offsetY);
        // (the picture's place: across x - the camera's word - pictureLeft, down y - the camera's word - 0x0F)
        const uint32_t camera = e.cameraRecord();
        const int cameraY = Word(camera + e.cameraY);
        const int pictureY = y - cameraY - 0x0F;
        boxes.push_back({kind, x - e.pictureLeft, kFloorLine - pictureY + cameraY, halfWidth, halfHeight, thrown});
    }

    void ObjectBoxes(M68K &c, std::vector<fight::StageBox> &boxes, uint32_t object, bool thrown, uint16_t character) {
        const uint32_t table = Ram32(object + e.boxTable);
        if (Ram8(object + e.canBeHit) != 0) {
            for (uint32_t n = 0; n < 4; n++) {
                if (const uint16_t id = Ram16(object + e.hurtBoxes + 2 * n) & 0x1FFF) {
                    AddBox(c, boxes, fight::BoxKind::Hurt, object, table + id * 8u, thrown);
                }
            }
        }
        if (Ram8(object + e.canHit) != 0) {
            for (uint32_t n = 0; n < 2; n++) {
                const uint16_t id = Ram16(object + e.attackBoxes + 2 * n);
                // (the high bit: how near a throw reaches, drawn as an outline, and only while the character is free)
                if ((id & 0x1FFF) && (!(id & 0x8000) || Ram16(object + e.control) == 0)) {
                    AddBox(c, boxes, (id & 0x8000) ? fight::BoxKind::ThrowRange : fight::BoxKind::Attack, object,
                           table + (id & 0x1FFFu) * 8u, thrown);
                }
            }
        }
        if (!thrown) {
            const uint8_t push = Ram8(object + e.pushBox);
            AddBox(c, boxes, fight::BoxKind::Push, object, c.Read(e.pushTable + push * 2u, 2) + character * 4u, false);
        }
    }
} // namespace

std::vector<fight::StageBox> Boxes(Machine &machine) {
    M68K &c = machine.cpu;
    std::vector<fight::StageBox> boxes;
    for (int n = 0; n < 2; n++) {
        const uint32_t player = Player(n);
        if (Ram16(player + e.state) != 2) {
            continue;
        }
        ObjectBoxes(c, boxes, player, false, Ram16(player + e.character));
        // What the player has thrown: the list's records, newest first, to the first that is not one
        const uint32_t list = e.thrownList + static_cast<uint32_t>(n) * e.thrownListSpace;
        for (uint32_t i = 1; i < 32; i++) {
            const uint32_t thrown = Ram32(list - 4 * i) & 0xFFFFFF;
            if (thrown < e.player1 + static_cast<uint32_t>(e.records) * e.playerSize || thrown >= 0xFFFF00) {
                break;
            }
            ObjectBoxes(c, boxes, thrown, true, 0);
        }
    }
    return boxes;
}

void Ends(int &stageLeft, int &stageRight, int &cameraLeft, int &cameraRight) {
    const uint32_t camera = e.cameraRecord();
    cameraLeft = Word(camera + e.cameraLeft);
    cameraRight = Word(camera + e.cameraRight);
    stageLeft = cameraLeft + e.pictureLeft + e.wall;
    stageRight = cameraRight + e.pictureLeft + 384 - e.wall;
}
uint32_t CameraXAt() {
    return e.cameraRecord() + e.cameraX;
}
uint32_t CameraYAt() {
    return e.cameraRecord() + e.cameraY;
}

// --- the fighting modes ---

namespace {
    constexpr int32_t kFlungAcross = 0xC000; // knocked out: away over the end, three quarters of a pixel a step
    constexpr int32_t kFlungUp = -0x60000;   //   and up, six pixels a step at first
    constexpr int32_t kPull = 0x6C00;        //   pulled down as a jump is
    constexpr int kGoneBelow = 400;          // how far under the floor one falling stops (long out of the picture)
    // The part of a player's record that is how it moves and is drawn (its steps, its frame, its speeds, its boxes):
    // taken from its record on the old stage when it comes down onto the next, but for its place and its opponent's
    // record
    constexpr uint32_t kMovingFrom = 0x04, kMovingTo = 0x100, kOpponent = 0xB0;
} // namespace

// Flung out and falling, the player keeps the picture it had and goes through the air by the speeds kept in its record
bool RingOutStep(M68K &c) {
    const uint32_t player = c.a[6] & 0xFFFFFF;
    const RingOut ringOut = RingOutMove(player);
    if (ringOut.move == RingOut::kFlungOut) {
        SetRam32(player + e.speedX, static_cast<uint32_t>(ringOut.side * kFlungAcross));
        SetRam32(player + e.speedY, static_cast<uint32_t>(kFlungUp));
        SetRam8(player + e.hitStop, 0);
        return true;
    }
    if (ringOut.move == RingOut::kFalling) {
        if (Word(player + e.y) < e.ground() + kGoneBelow) {
            SetRam32(player + e.x, Ram32(player + e.x) + Ram32(player + e.speedX));
            SetRam32(player + e.y, Ram32(player + e.y) + Ram32(player + e.speedY));
            SetRam32(player + e.speedY, Ram32(player + e.speedY) + static_cast<uint32_t>(kPull));
        }
        return true;
    }
    if (ringOut.move == RingOut::kDropsIn) {
        // Coming down onto the next stage: as it was in its jump over the old stage, at its place here; the game's jump
        // goes on from there and lands it
        for (uint32_t at = kMovingFrom; at < kMovingTo; at++) {
            if ((at < e.x || at >= e.y + 4) && (at < kOpponent || at >= kOpponent + 4)) {
                SetRam8(player + at, ringOut.record[at]);
            }
        }
        SetRam32(player + e.x, static_cast<uint32_t>(ringOut.x) << 16);
        SetRam32(player + e.y, static_cast<uint32_t>(ringOut.y) << 16);
    }
    return false;
}

// --- widescreen ---
//
// A fight is shown 512 columns wide where the board shows 384 (16:9 for 4:3, the height and the size of everything as
// they were): more of the stage on each side. The fight is played in that picture too: the characters can go to its
// edges, and the camera stops where its edges come to the stage's ends (64 nearer the middle at each end). With
// "Invisible walls" as well, the fight is shown in the wide picture but played as on the board. The two rules the wide
// picture changes are the game's own code with the margin in it, in place of the game's while the fight is played wide.

namespace {
    constexpr int kSide = Video::kMaxSide;

    bool g_play = false;  // fights are played in the wide picture
    bool g_shown = false; // fights are shown in the wide picture (played in it, or played as on the board)
    int g_side = 0, g_shift = 0, g_cameraBefore = 0;

    int Margin() {
        return g_play ? kSide : 0;
    }

    // The end of the camera's step (A6 its record): its place across and up kept between its ends; played wide, 64
    // nearer the middle across; a mode's camera wanted beyond its ends is not kept in at all
    bool KeepCamera(M68K &c) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        if (e.cameraWorkX) {
            // (the game's own keeping across, its copy put back after: not kept in at all, or first kept 64 nearer the
            // middle, inside the game's own ends, so that its keeping changes nothing)
            if (modes::CameraAt().beyondEnds) {
                c.pc = e.keptAcross;
                return true;
            }
            if (g_play) {
                const int x = Word(camera + e.cameraWorkX);
                const int left = Word(camera + e.cameraLeft) + kSide, right = Word(camera + e.cameraRight) - kSide;
                SetRam16(camera + e.cameraWorkX, static_cast<uint16_t>(std::clamp(x, left, std::max(left, right))));
            }
            return false;
        }
        // (where the stages each keep their own camera, the stage's code goes on after the piece that keeps it)
        uint32_t after = 0;
        for (const Engine::KeepCameraPlace &place : e.keepCameraPlaces) {
            if (place.at == c.pc) {
                after = place.after;
            }
        }
        const auto done = [&] {
            if (after) {
                c.pc = after;
            } else {
                Return(c);
            }
            return true;
        };
        if (modes::CameraAt().beyondEnds) {
            return done();
        }
        if (!g_play) {
            return false;
        }
        const int x = Word(camera + e.cameraX);
        const int left = Word(camera + e.cameraLeft) + kSide, right = Word(camera + e.cameraRight) - kSide;
        SetRam16(camera + e.cameraX, static_cast<uint16_t>(std::clamp(x, left, std::max(left, right))));
        const int y = Word(camera + e.cameraY);
        const int top = Word(camera + e.cameraTop), bottom = Word(camera + e.cameraTop + 2);
        SetRam16(camera + e.cameraY, static_cast<uint16_t>(std::clamp(y, top, std::max(top, bottom))));
        return done();
    }

    // The game's own keeping of the camera's copy down (with cameraWorkY): not done for a mode's camera wanted beyond
    // its ends (down, the wide picture keeps the game's ends)
    bool KeepCameraDown(M68K &c) {
        if (modes::CameraAt().beyondEnds) {
            c.pc = e.keptDown;
            return true;
        }
        return false;
    }

    // A character (A6) kept inside the picture of the plane it stands in: between 0x55 and 0x1AB on from the plane's
    // camera word (the board's picture's edges less the wall); played wide, 64 further out on each side. The ring out
    // lets one knocked about over an end.
    bool KeepIn(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        if (RingOutLetsOut(player)) {
            SetRam8(player + e.atEdge, 0);
            Return(c);
            return true;
        }
        if (!g_play) {
            return false;
        }
        SetRam8(player + e.atEdge, 0);
        const uint8_t plane = Ram8(player + e.plane);
        if (plane) {
            const uint32_t word = (static_cast<uint32_t>(c.Read(e.planes + plane, 1)) << 16) | c.Read(e.planes + plane + 2, 1);
            const int camera = Word(word & 0xFFFFFF);
            const int x = Word(player + e.x);
            if (x <= camera + 0x55 - kSide) {
                SetRam16(player + e.x, static_cast<uint16_t>(camera + 0x55 - kSide));
                SetRam8(player + e.atEdge, 2);
            } else if (x >= camera + 0x1AB + kSide) {
                SetRam16(player + e.x, static_cast<uint16_t>(camera + 0x1AB + kSide));
                SetRam8(player + e.atEdge, 1);
            }
        }
        Return(c);
        return true;
    }

    // --- the picture ---

    int CameraLeftEnd() {
        return Word(e.cameraRecord() + e.cameraLeft);
    }
    int CameraRightEnd() {
        return Word(e.cameraRecord() + e.cameraRight);
    }
    bool OnStage(int camera) {
        return CameraRightEnd() >= CameraLeftEnd() + 2 * kSide && camera >= CameraLeftEnd() && camera <= CameraRightEnd();
    }

    // An object's place: the panel's stay in the middle, the stage's and the fighters' go with the view
    int MoveObject(uint32_t tile, uint16_t attributes, int) {
        return e.thePanel(tile, attributes) ? 0 : g_shift;
    }

    // The wide picture's switches for the frame about to run (the port's, or a spare's): the camera's place now and
    // where it was for the picture being made (a frame behind)
    void SetWide(Machine &machine, int camera, int shown) {
        g_side = g_shift = 0;
        if (g_shown && OnStage(camera) && OnStage(shown)) {
            g_side = kSide;
            // (the view's left edge is 64 left of the board's, and no further left than the camera's own end allows:
            // nearer the end than that, the stage is drawn to the side by what is missing)
            g_shift = shown - std::clamp<int>(shown, CameraLeftEnd() + kSide, CameraRightEnd() - kSide);
        }
        Video::Wide &wide = machine.video.wide;
        wide.side = g_side;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = g_shift;
        wide.object = g_side ? MoveObject : nullptr;
        wide.tile = nullptr;
        wide.fillHoles = g_side != 0;
    }

    struct Kept {
        int side, shift;
    } g_kept;
} // namespace

bool WidePlay() {
    return g_play;
}
int WideMargin() {
    return Margin();
}
int WideSide() {
    return g_side;
}
int WideStage() {
    return g_side + g_shift;
}

// The camera's place a frame ago, which the picture is drawn from (it is a frame behind), in a save state: a frame
// put back (a save state, online play's rollback) is drawn, and its players kept in, as it was the first time
void WidescreenState(Archive &ar) {
    if (ar.version == 0 || ar.version >= 8) { // (a save state from before has none)
        ar.Pod(g_cameraBefore);
    }
}

void WidescreenBeforeFrame(Machine &machine) {
    const int camera = Word(e.cameraRecord() + e.cameraX), shown = g_cameraBefore;
    g_cameraBefore = camera;
    // (fights only; the stage viewer keeps the board's picture. With invisible walls the rules stay the board's)
    g_shown = FeatureOn("widescreen") && e.inFight() && !modes::StageViewer();
    g_play = g_shown && !FeatureOn("widescreen_walls");
    SetWide(machine, camera, shown);
}

// A spare machine's frame (the modes' other machines) is drawn as wide as the port's picture is, so that the two can
// be put together: from the spare's own camera (where it is now, for want of where it was)
void WideForSpare(Machine &machine) {
    g_kept = {g_side, g_shift};
    const int camera = Word(e.cameraRecord() + e.cameraX);
    SetWide(machine, camera, camera);
    if (g_side != g_kept.side) {
        Video::Wide &wide = machine.video.wide;
        g_side = wide.side = g_kept.side;
        g_shift = 0;
        wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = 0;
        wide.object = g_side ? MoveObject : nullptr;
    }
}
void WideForPort(Machine &) {
    g_side = g_kept.side;
    g_shift = g_kept.shift;
}

void RegisterWidescreen(uint32_t keepCamera, uint32_t keepIn) {
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    if (keepCamera) {
        AddHook(keepCamera, "KeepCamera", KeepCamera);
    }
    for (const Engine::KeepCameraPlace &place : e.keepCameraPlaces) {
        AddHook(place.at, "KeepCamera", KeepCamera);
    }
    if (e.keepCameraDown) {
        AddHook(e.keepCameraDown, "KeepCameraDown", KeepCameraDown);
    }
    AddHook(keepIn, "KeepIn", KeepIn);
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown and played in 16:9, with more of the stage on each side: the characters can go to "
                                  "the wide picture's edges, so they can stand further apart than on the board. Off unless switched "
                                  "on here, in every preset: it changes how the game plays.",
                                  kFeatureFirstPage});
    TheGame().features.push_back({"widescreen_walls", "Display", "Invisible walls",
                                  "With widescreen: fights are still shown in 16:9, but played exactly as on the board. The characters "
                                  "are stopped where the board's picture ends, by walls that are not seen, and the camera goes as far "
                                  "as the board's; near a stage's end the view stays on the stage, so the play area is then to one "
                                  "side of the picture. Nothing without widescreen.",
                                  0});
}

// --- the model viewer ---
//
// One character alone, on a stage or on plain white or black, at any size, standing as it does in a fight (and moved
// with player 1's controls), or going through any one of its animations. It is a round of a two-player match, reached
// unseen as the stage viewer's is (fight/alternative.h), with the second player put out of the way: not followed by
// the camera, out of the picture, and never drawn. Showing an animation is putting its frame in the player's record
// each frame, as the game's own routine does, with the time left kept high so that the game's own stepping never comes
// round.

namespace {
    constexpr const char *kModel = "Model viewer";
    constexpr int kStartMost = 9000; // frames after which getting to a round is given up

    struct State {
        bool on = false;
        bool starting = false;
        int32_t frames = 0;
    } g;
    bool g_fast = false;

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }
    void End() {
        if (g.on) {
            g = State();
            modes::Wanted() = MatchWanted();
            Fast(false);
            model::End();
        }
    }

    uint32_t Read32(Machine &machine, uint32_t address) {
        return (static_cast<uint32_t>(machine.Read16(address)) << 16) | machine.Read16(address + 2);
    }
    bool InRom(uint32_t address) {
        return address >= e.romLow && address < e.romHigh && !(address & 1);
    }
    bool IsFrame(Machine &machine, uint32_t frame) {
        return InRom(frame) && InRom(Read32(machine, frame + e.framePictureAt) & e.framePictureMask) &&
               (machine.Read16(frame + e.frameTimeAt) & 0x7FFF) != 0;
    }
    // Where an animation of the character's starts (by its number in the menu: its word `character` is twice it)
    uint32_t AnimationAt(Machine &machine, int character, int group, int number) {
        const uint32_t list = Read32(machine, e.animationLists + static_cast<uint32_t>(character) * 4);
        if (!InRom(list)) {
            return 0;
        }
        const uint32_t groupAt = list + machine.Read16(list + static_cast<uint32_t>(group) * 2);
        const uint32_t frame = groupAt + machine.Read16(groupAt + static_cast<uint32_t>(number) * 2);
        return IsFrame(machine, frame) ? frame : 0;
    }
    int FrameTime(Machine &machine, uint32_t frame) {
        return machine.Read16(frame + e.frameTimeAt) & 0x7FFF;
    }
    uint32_t NextFrame(Machine &machine, uint32_t frame) {
        const uint32_t next = machine.Read16(frame + e.frameTimeAt) & 0x8000 ? Read32(machine, frame + e.frameBytes) : frame + e.frameBytes;
        return next == frame || !IsFrame(machine, next) ? 0 : next;
    }
    // A frame put in player 1's record as the game puts it (back to standing: the character's first animation, which is
    // its standing, with the game's stepping let go on from it)
    void ShowFrame(Machine &machine, uint32_t frame) {
        const uint32_t player = Player(0);
        const bool standing = frame == 0;
        if (standing) {
            frame = AnimationAt(machine, Ram16(player + e.character) / 2, 0, 0);
            if (!frame) {
                return;
            }
        }
        SetRam32(player + e.frame, frame);
        e.copyFrame(machine, player, frame);
        SetRam16(player + e.frameTimer, standing ? 1 : 0x7FFF);
        SetRam32(player + e.nextFrame, frame + e.frameBytes);
    }

    // Objects by what they are: for the frame's own picture, the stage's alone (notTheStage); for the character's, the
    // fighters' sets that touch player 1 (model::Description::nearestOnly); for the picture as the board would show it,
    // everything but the panel and the second player's sets
    bool IsModel(uint32_t tile, uint16_t attributes) {
        const unsigned set = attributes & 0x1F;
        return set >= e.player1Set && set < e.player1Set + static_cast<unsigned>(e.palettes) && !e.thePanel(tile, attributes);
    }
    bool InAFight(uint32_t tile, uint16_t attributes) {
        const unsigned set = attributes & 0x1F;
        return !e.thePanel(tile, attributes) && !(set >= e.player2Set && set < e.player2Set + static_cast<unsigned>(e.palettes)) &&
               set != e.shadowSet;
    }

    void StartModel(Machine &machine) {
        End();
        modes::End();
        EndBattle();
        SetFeature("training_mode", false);
        g.on = true;
        g.starting = true;
        model::Begin();
        modes::Wanted().stage = model::Plain(model::Background()) ? 2 : model::Background() + 1;
        const int characters = static_cast<int>(e.characterNames.size());
        const int who = model::Number(model::Character());
        modes::Wanted().characters[0] = modes::Wanted().partners[0] = who * 2;
        modes::Wanted().characters[1] = modes::Wanted().partners[1] = (who + 1) % characters * 2;
        machine.Reset();
    }

    // The second player out of the way: far off to the right (the game keeps it at the picture's edge), with the camera
    // following the first alone (CameraWay)
    void PutAway() {
        SetRam16(Player(1) + e.x, static_cast<uint16_t>(Ram16(Player(0) + e.x) + 0x300));
    }

    // The camera's step (A6 its record): it goes the way the byte `cameraWay` says. In the viewer, after player 1 alone,
    // its own way of doing that; where a mode wants the camera (fight/battle.h, CameraWanted), there, and the game's step
    // not made.
    bool g_downApart = false; // the camera's step moves it down in a piece of its own (RegisterModel)

    // The camera's step, across (and down, unless that is a piece of its own) or down alone
    bool CameraStep(M68K &c, bool across, bool down) {
        if (across && g.on && !g.starting) {
            SetRam8(e.cameraWay, e.followsOne);
            if (e.cameraWayTwice) {
                SetRam8(e.cameraWay + 2, e.followsOne);
            }
        }
        if (down && !across && e.cameraWayDown && g.on && !g.starting) {
            SetRam8(e.cameraWayDown, e.followsOne);
        }
        const CameraWanted &wanted = modes::CameraAt();
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const bool x = across && wanted.x >= 0, y = down && wanted.y >= 0;
        if ((x || y) && camera == e.cameraRecord()) {
            // (either where it is wanted, the other where it is; kept between its ends unless wanted beyond them)
            const auto place = [&](uint32_t word, int at, uint32_t ends) {
                const int low = Word(camera + ends), high = Word(camera + ends + 2);
                SetRam16(camera + word, static_cast<uint16_t>(wanted.beyondEnds ? at : std::clamp(at, low, high)));
            };
            // (where the step works on a copy of its place, the copy: it is put back as the step ends)
            if (x) {
                place(e.cameraWorkX ? e.cameraWorkX : e.cameraX, wanted.x, e.cameraLeft);
            }
            if (y) {
                place(e.cameraWorkY ? e.cameraWorkY : e.cameraY, wanted.y, e.cameraTop);
            }
            Return(c);
            return true;
        }
        return false;
    }
    bool CameraWay(M68K &c) {
        return CameraStep(c, true, !g_downApart);
    }
    bool CameraWayDown(M68K &c) {
        return CameraStep(c, false, true);
    }
} // namespace

bool ModelViewer() {
    return g.on;
}
void StartModelViewer(Machine &machine) {
    StartModel(machine);
}

void ModelBeforeFrame(Machine &machine) {
    if (!g.on) {
        return;
    }
    modes::Wanted().stage = model::Plain(model::Background()) ? 2 : model::Background() + 1;
    if (g.starting) {
        Fast(true);
        if (modes::ToRound(machine, g.frames)) {
            g.starting = false;
            Fast(false);
            // (the character in the middle of the stage: the camera's middle, and the picture's)
            int left = 0, right = 0;
            e.cameraEnds(Ram8(e.stage), left, right);
            SetRam32(Player(0) + e.x, static_cast<uint32_t>((left + right) / 2 + e.pictureLeft + 0xC0) << 16);
        } else if (g.frames > kStartMost) {
            End();
        }
        return;
    }
    machine.players[1] = 0;
    PutAway();
    if (colours::EditorAnimation()) {
        model::Scan(machine);
    }
    colours::EditorBeforeFrame(machine); // (takes player 1's controls, when it is on)
    SetRam8(e.clock, 0x99);
    model::BeforeFrame(machine);
}

void ModelFrame(Machine &machine) {
    if (!g.on) {
        return;
    }
    // (the board reset, by the menu or by another mode starting: the viewer is over)
    if (!g.starting && !e.playing()) {
        End();
        return;
    }
    if (g.starting) {
        return;
    }
    if (model::Changed()) {
        StartModel(machine);
        return;
    }
    machine.video.hideObject = e.notTheStage;
    model::Frame(machine);
    colours::EditorFrame(machine);
}

// (Nothing of the viewer is kept in a save state: a state loaded while viewing ends the viewer, and what is loaded is
// the fight it holds.)
void ModelState(Archive &ar) {
    if (!ar.Saving() && !RunningAhead()) {
        End();
    }
}

void RegisterModel(uint32_t cameraWayAt, int largeLift, uint32_t cameraWayDownAt) {
    model::Description d;
    d.page = kModel;
    d.pageWords = "One character by itself, on a stage or on white or black, at any size, standing or going through any of its "
                  "animations.";
    d.resetWords = "Puts every choice of this page back: the first character and stage, its own size, standing, full speed, "
                   "repeating at once, one animation only. (While viewing, the viewer goes to that character and stage.)";
    d.animationWords = "What the character does: stands as in a fight, moved with player 1's controls, or goes through one of its "
                       "animations over and over. They are named by the game's groups of them and a number (A34); the ones a "
                       "character has not got are passed over. While one is showing, left and right go to the one before and the "
                       "one after, and its name is shown.";
    d.foregroundWords = "What the stage has in front of the fighters: in front of the character too, as in a fight, or behind it so "
                        "that all of it is seen. In front only at the character's own size, and not in the colour editor.";
    d.characters = e.characterNames;
    if (!e.viewerOrder.empty()) {
        d.characters.clear();
        for (const int n : e.viewerOrder) {
            d.characters.push_back(e.characterNames[static_cast<size_t>(n)]);
        }
        d.numbers = e.viewerOrder;
    }
    d.stages = e.stageNames;
    d.start = [](Machine &machine) {
        colours::EndEditor();
        StartModel(machine);
    };
    d.lists = e.groups;
    d.perList = e.perGroup;
    d.animationAt = AnimationAt;
    d.isFrame = IsFrame;
    d.frameTime = FrameTime;
    d.nextFrame = NextFrame;
    d.showFrame = ShowFrame;
    d.animationChoice = colours::EditorAnimation;
    d.isModel = IsModel;
    d.inAFight = InAFight;
    d.nearestOnly = true;
    d.largeLift = largeLift;
    d.anchor = [](Machine &, int &x, int &y) {
        const uint32_t camera = e.cameraRecord();
        x = Word(Player(0) + e.x) - Word(camera + e.cameraX) - e.pictureLeft;
        y = -1; // (its feet found from its picture)
    };
    d.shift = colours::EditorShift;
    d.linesInSet = e.palettes;
    d.firstLine = [](Machine &) { return static_cast<int>(e.player1Set); };
    d.spareFrame = [](Machine &) { SetRam8(e.clock, 0x99); };
    model::Register(d);
    AddHook(cameraWayAt, "CameraWay", CameraWay);
    if (cameraWayDownAt) {
        g_downApart = true;
        AddHook(cameraWayDownAt, "CameraWayDown", CameraWayDown);
    }
    // The window's zoom (modes::Focus): where the fighter stands in the picture as the boxes are put there (AddBox),
    // heights growing downwards; a tag-team game's point characters are the first two records
    modes::Focus(
        [](int player, int &x, int &y) {
            const uint32_t camera = e.cameraRecord();
            x = Word(Player(player) + e.x) - Word(camera + e.cameraX) - e.pictureLeft + WideStage();
            y = Word(Player(player) + e.y) - Word(camera + e.cameraY) - 0x0F;
        },
        40, 136);
}

} // namespace cps2recomp::marvel
