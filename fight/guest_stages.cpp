// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Other games' stages behind a fighting game's fighters (guest_stages.h).
#include "guest_stages.h"

#include "../machine/machine.h"
#include "../runtime/game.h"
#include "../runtime/guest.h"
#include "../runtime/lift.h"
#include "../runtime/state.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace cps2recomp::guest_stages {

namespace {
    Host h;
    std::vector<Guest> &Guests() {
        static auto &guests = *new std::vector<Guest>; // (never freed: the threads read them)
        return guests;
    }
    constexpr int kMostGuests = 8;

    struct State {
        int8_t guest = 0;  // the guest whose stage is shown, 1 the first (0: the game's own); its board is in its round
        int8_t stage = 0;  // ... its stage, 1 its first
        int8_t follow = 0; // its camera goes where the game's is (while its stage is shown)
        int16_t cameraX = 0, cameraY = 0; // where its camera is to be (the game's, as its last frame left it)
    } g;
    std::vector<uint8_t> g_guestState; // (the guest's machine, for a save state: kept so a peer without the set can read past it)
    size_t g_soundBefore = 0;

    uint8_t G8(Machine &m, uint32_t address) {
        return m.ram[address & 0xFFFF];
    }
    uint16_t G16(Machine &m, uint32_t address) {
        const uint8_t *p = &m.ram[address & 0xFFFF];
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    void SetG8(Machine &m, uint32_t address, uint8_t value) {
        m.ram[address & 0xFFFF] = value;
    }
    uint16_t C16(M68K &c, uint32_t address) {
        const uint8_t *p = &c.ram[address & 0xFFFF];
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    void SetC16(M68K &c, uint32_t address, uint16_t value) {
        uint8_t *p = &c.ram[address & 0xFFFF];
        p[0] = static_cast<uint8_t>(value >> 8);
        p[1] = static_cast<uint8_t>(value);
    }
    void SetC8(M68K &c, uint32_t address, uint8_t value) {
        c.ram[address & 0xFFFF] = value;
    }
    void ReturnGuest(M68K &c) {
        const uint8_t *p = &c.ram[c.a[7] & 0xFFFF];
        c.pc = static_cast<uint32_t>((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]) & 0xFFFFFF;
        c.a[7] += 4;
    }
    bool All(Machine &m, const std::vector<Is> &all) {
        for (const Is &is : all) {
            if ((is.byte ? G8(m, is.address) : G16(m, is.address)) != is.value) {
                return false;
            }
        }
        return true;
    }

    // --- each stage's round, made ready on a thread of its own (guest_stages.h) ---

    struct Ready {
        std::mutex lock;
        std::condition_variable done;
        std::vector<std::vector<uint8_t>> round; // (each written once, by the thread; read once `made` says so)
        std::unique_ptr<std::atomic<bool>[]> made;
        std::atomic<int> wanted{0};
        bool started = false;
        Machine *ahead = nullptr;       // (the thread's board)
        std::atomic<int> aheadStage{0}; // (the stage it is taking to its round, 1 the first: 0 none yet)
    };
    Ready &TheReady(int guest) { // (0 the first)
        static Ready *ready = new Ready[kMostGuests]; // (never freed: the threads may run until the program is gone)
        return ready[guest];
    }

    // The guest whose board this 68000 is (0 the first; -1 none), and whether it is the thread's
    int GuestOf(M68K &c, bool &ahead) {
        for (int n = 0; n < static_cast<int>(Guests().size()); n++) {
            if (TheReady(n).ahead && &c == &TheReady(n).ahead->cpu) {
                ahead = true;
                return n;
            }
            if (Machine *m = cps2recomp::Guest(Guests()[n].id); m && &c == &m->cpu) {
                ahead = false;
                return n;
            }
        }
        return -1;
    }

    // The guest's controls on its way to a round: credits kept, each player's Start until both are in, then a punch
    // now and then (the character choice, anything else that waits for a button)
    void Controls(const Guest &guest, Machine &m, uint8_t &pressing) {
        memset(m.players, 0, sizeof(m.players));
        SetG8(m, guest.credits, 2);
        const bool down = (pressing & 7) < 3;
        pressing++;
        if (!down || All(m, guest.match)) {
            return;
        }
        if (G8(m, guest.playersIn) != 3) {
            m.players[(pressing >> 3) & 1] = kStart;
            return;
        }
        m.players[(pressing >> 3) & 1] = kPunch1;
    }
    std::vector<uint8_t> Saved(Machine &m) {
        std::vector<uint8_t> data;
        Archive ar(data, true);
        m.Serialize(ar);
        return data;
    }
    void Load(Machine &m, std::vector<uint8_t> data) {
        Archive ar(data, false);
        m.Serialize(ar);
    }

    void MakeRounds(int index) {
        const Guest &guest = Guests()[index];
        Ready &ready = TheReady(index);
        Machine &m = *ready.ahead;
        const int stages = static_cast<int>(guest.stageNames.size());
        m.drawing = false;
        // To the character choice, kept
        uint8_t pressing = 0;
        constexpr int kMostFrames = 20000; // (never more than about 1,000: a board that does not get there is given up)
        m.Reset();
        for (int n = 0; n < kMostFrames && !(All(m, guest.choosing) && G8(m, guest.playersIn) == 3); n++) {
            Controls(guest, m, pressing);
            m.RunFrame();
            m.audio.clear();
        }
        const std::vector<uint8_t> choosing = Saved(m);
        const uint8_t choosingPressing = pressing;
        for (int left = stages; left > 0; left--) {
            // (the stage wanted first, else the next not made)
            int stage = ready.wanted.load();
            if (stage < 1 || stage > stages || ready.made[stage - 1]) {
                stage = 0;
                for (int s = 1; s <= stages && !stage; s++) {
                    stage = ready.made[s - 1] ? 0 : s;
                }
            }
            if (!stage) {
                break;
            }
            Load(m, choosing);
            pressing = choosingPressing;
            ready.aheadStage = stage;
            for (int n = 0; n < kMostFrames && !All(m, guest.fought); n++) {
                Controls(guest, m, pressing);
                m.RunFrame();
                m.audio.clear();
            }
            memset(m.players, 0, sizeof(m.players));
            {
                std::lock_guard<std::mutex> hold(ready.lock);
                ready.round[stage - 1] = Saved(m);
                ready.made[stage - 1] = true;
            }
            ready.done.notify_all();
        }
    }
    void StartMaking(int index) {
        Ready &ready = TheReady(index);
        if (ready.started || !cps2recomp::Guest(Guests()[index].id)) {
            return;
        }
        ready.started = true;
        ready.ahead = GuestSpare(Guests()[index].id);
        std::thread(MakeRounds, index).detach();
    }
    // A guest's stage's round (1 its first), into its board (waiting for it, if it is not made yet)
    void ToRound(int index, int stage) {
        Ready &ready = TheReady(index);
        StartMaking(index);
        ready.wanted = stage;
        std::unique_lock<std::mutex> hold(ready.lock);
        ready.done.wait(hold, [&] { return ready.made[stage - 1].load(); });
        Load(*cps2recomp::Guest(Guests()[index].id), ready.round[stage - 1]);
    }

    // --- the guests' hooks ---

    // Where a guest sets a match's stage: the one its spare board is taking to its round
    bool StageSet(M68K &c) {
        bool ahead = false;
        const int index = GuestOf(c, ahead);
        const int stage = index >= 0 && ahead ? TheReady(index).aheadStage.load() : 0;
        if (stage > 0) {
            Guests()[index].setStage(c, stage - 1);
        }
        return false;
    }
    // The guest's camera, while its stage is shown, goes where the game's is, as far as it goes in a frame: the game
    // paints the stage's new columns and rows as its camera comes to them
    bool Following(M68K &c, int &index) {
        bool ahead = false;
        index = GuestOf(c, ahead);
        return index >= 0 && !ahead && g.follow && g.guest == index + 1;
    }
    // Steps: the camera (A6) steps across, and up or down, by no more than kMostStep (it catches up a camera put
    // somewhere else at once in a few frames)
    constexpr int kMostStep = 8;
    int16_t StepTo(int16_t was, int16_t to) {
        return static_cast<int16_t>(std::clamp(to - was, -kMostStep, kMostStep));
    }
    bool StepAcross(M68K &c) {
        int index;
        if (!Following(c, index)) {
            return false;
        }
        const Guest &guest = Guests()[index];
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const uint16_t was = C16(c, camera + guest.placeX);
        const int16_t step = StepTo(static_cast<int16_t>(was), g.cameraX);
        SetC16(c, camera + guest.placeX, static_cast<uint16_t>(was + step));
        SetC16(c, camera + guest.stepX, static_cast<uint16_t>(step));
        SetC8(c, camera + guest.wayX, step >= 0 ? guest.wayRight : guest.wayLeft);
        ReturnGuest(c);
        return true;
    }
    bool StepUp(M68K &c) {
        int index;
        if (!Following(c, index)) {
            return false;
        }
        const Guest &guest = Guests()[index];
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const uint16_t was = C16(c, camera + guest.placeY);
        const int16_t step = StepTo(static_cast<int16_t>(was), g.cameraY);
        SetC16(c, camera + guest.placeY, static_cast<uint16_t>(was + step));
        SetC16(c, camera + guest.stepY, static_cast<uint16_t>(step));
        if (step) {
            SetC8(c, camera + guest.wayY, step > 0 ? guest.wayUp : guest.wayDown);
        }
        ReturnGuest(c);
        return true;
    }
    // Extents: the players' outer edges across (D0 left, D1 right), as far apart as the camera then puts their middle
    // in the picture's: at the camera wanted; their heights (D0, D1), as high as the camera then follows them to the
    // place wanted
    bool ExtentsAcross(M68K &c) {
        int index;
        if (!Following(c, index)) {
            return false;
        }
        const Guest &guest = Guests()[index];
        const int middle = g.cameraX + guest.middle;
        c.d[0] = (c.d[0] & 0xFFFF0000) | static_cast<uint16_t>(middle - guest.halfApart);
        c.d[1] = (c.d[1] & 0xFFFF0000) | static_cast<uint16_t>(middle + guest.halfApart);
        ReturnGuest(c);
        return true;
    }
    bool ExtentsUp(M68K &c) {
        int index;
        if (!Following(c, index)) {
            return false;
        }
        const Guest &guest = Guests()[index];
        const uint16_t high = static_cast<uint16_t>(g.cameraY + guest.above);
        c.d[0] = (c.d[0] & 0xFFFF0000) | high;
        c.d[1] = (c.d[1] & 0xFFFF0000) | high;
        ReturnGuest(c);
        return true;
    }

    // --- the stage wanted ---

    // The guest (0 the first) and its stage (1 its first) of the stages' count from 1 (false: none)
    bool Which(int stage, int &index, int &own) {
        for (index = 0; index < static_cast<int>(Guests().size()); index++) {
            const int stages = static_cast<int>(Guests()[index].stageNames.size());
            if (stage >= 1 && stage <= stages) {
                own = stage;
                return true;
            }
            stage -= stages;
        }
        return false;
    }

    void (*g_before)(Machine &) = nullptr;
    void (*g_frame)(Machine &) = nullptr;
    void (*g_state)(Archive &) = nullptr;

    bool Shown() {
        return g.guest > 0 && h.shown && h.shown();
    }

    void BeforeFrame(Machine &machine) {
        if (g_before) {
            g_before(machine);
        }
        g_soundBefore = machine.audio.size();
        // The game's stage music is the guest's while a guest's stage is chosen (from before the round, so that the
        // game's never starts)
        std::vector<bool> taken;
        if (g.guest > 0) {
            for (const uint16_t tune : h.stageTunes) {
                if (taken.size() <= tune) {
                    taken.resize(tune + 1u);
                }
                taken[tune] = true;
            }
        }
        machine.soundtrack.Take(std::move(taken));
        if (!Shown()) {
            return;
        }
        // The game draws its fighters and its panel alone, marked, to be laid over the guest's stage (the stage
        // viewer: nothing of its own)
        if (h.viewer && h.viewer()) {
            machine.video.layerMask = 0;
        } else {
            machine.video.layerMask = 1;
            machine.video.hideObject = [](uint32_t tile, uint16_t attributes) { return !h.notTheStage(tile, attributes); };
            machine.video.markObjects = true;
        }
    }

    // The guest's frame, after the game's: its picture under the game's marked objects, its sound with the game's
    void Frame(Machine &machine) {
        if (g_frame) {
            g_frame(machine);
        }
        // (with the feature on, the stages are made ready from the start)
        if (h.feature && FeatureOn(h.feature)) {
            for (int n = 0; n < static_cast<int>(Guests().size()); n++) {
                StartMaking(n);
            }
        }
        int index = -1, stage = 0;
        if (!Which(h.wanted ? h.wanted() : 0, index, stage) || !cps2recomp::Guest(Guests()[index].id)) {
            index = -1;
            stage = 0;
        }
        if (index + 1 != g.guest || stage != g.stage) {
            g = State();
            g.guest = static_cast<int8_t>(index + 1);
            g.stage = static_cast<int8_t>(stage);
            if (index >= 0) {
                ToRound(index, stage);
            }
        }
        if (g.guest == 0) {
            return;
        }
        const Guest &guest = Guests()[g.guest - 1];
        Machine &board = *cps2recomp::Guest(guest.id);
        // Its camera where the game's is (as far between its ends as the game's is between its own), while its stage
        // is shown; its clock held
        const bool shown = Shown();
        g.follow = shown;
        const int x = std::clamp<int>(Ram16(h.cameraX), h.cameraLeft, h.cameraRight);
        g.cameraX = static_cast<int16_t>(guest.cameraLeft + (x - h.cameraLeft) * (guest.cameraRight - guest.cameraLeft) /
                                                                 std::max(1, h.cameraRight - h.cameraLeft));
        g.cameraY = static_cast<int16_t>(static_cast<int16_t>(Ram16(h.cameraY)) - guest.lower);
        SetG8(board, guest.clock, guest.clockFull);
        memset(board.players, 0, sizeof(board.players));
        board.drawing = machine.drawing;
        board.video.hideObject = guest.notTheStage;
        board.video.layerMask = guest.layers;
        board.video.wide.side = machine.video.wide.side;
        board.RunFrame();

        // The picture: the guest's, under the game's marked pixels
        if (shown && machine.drawing && board.PictureWidth() == machine.PictureWidth()) {
            uint32_t *picture = machine.Picture();
            const uint32_t *behind = board.Picture();
            const size_t pixels = static_cast<size_t>(machine.PictureWidth()) * Video::kHeight;
            for (size_t i = 0; i < pixels; i++) {
                picture[i] = picture[i] & 0xFF000000 ? picture[i] & 0xFFFFFF : behind[i];
            }
        }
        // The sound: the guest's music, as loud as the game's would be (its fades), while the game's stage tune is what
        // it plays
        const Soundtrack::State &music = machine.soundtrack.state;
        const bool playing = shown && music.song >= 0 && machine.soundtrack.Taken(static_cast<uint16_t>(music.song));
        const int64_t gain = playing ? static_cast<int64_t>(music.level) * music.allLevel / Soundtrack::kFull : 0;
        const size_t from = std::min(g_soundBefore, machine.audio.size());
        const size_t made = std::min(machine.audio.size() - from, board.audio.size());
        for (size_t i = 0; i < made && gain; i++) {
            const int64_t sample = machine.audio[from + i] + board.audio[i] * gain / Soundtrack::kFull;
            machine.audio[from + i] = static_cast<int16_t>(std::clamp<int64_t>(sample, -32768, 32767));
        }
        board.audio.clear();
    }

    void StagesState(Archive &ar) {
        if (g_state) {
            g_state(ar);
        }
        if (ar.version != 0 && ar.version < 9) { // (a save state from before has none)
            return;
        }
        ar.Mark(0x44535447);
        ar.Pod(g);
        Machine *board = g.guest > 0 && g.guest <= static_cast<int>(Guests().size()) ? cps2recomp::Guest(Guests()[g.guest - 1].id) : nullptr;
        if (ar.Saving()) {
            g_guestState.clear();
            if (board) {
                Archive inner(g_guestState, true);
                board->Serialize(inner);
            }
            ar.Vec(g_guestState);
        } else {
            ar.Vec(g_guestState);
            if (!board) {
                g = State(); // (no set here: the game's own stages)
            } else if (!g_guestState.empty()) {
                Archive inner(g_guestState, false);
                board->Serialize(inner);
            } else {
                ToRound(g.guest - 1, g.stage); // (a peer's state without its board: from its round)
            }
        }
    }
} // namespace

int Stages() {
    int stages = 0;
    for (const Guest &guest : Guests()) {
        stages += static_cast<int>(guest.stageNames.size());
    }
    return stages;
}

const char *StageName(int stage) {
    int index, own;
    return Which(stage, index, own) ? Guests()[index].stageNames[own - 1] : "";
}

bool Available(int stage) {
    int index, own;
    return Which(stage, index, own) && cps2recomp::Guest(Guests()[index].id);
}

void Register(const Host &host, std::vector<Guest> guests) {
    h = host;
    Guests() = std::move(guests);
    for (int n = 0; n < static_cast<int>(Guests().size()) && n < kMostGuests; n++) {
        const Guest &guest = Guests()[n];
        Ready &ready = TheReady(n);
        ready.round.resize(guest.stageNames.size());
        ready.made = std::make_unique<std::atomic<bool>[]>(guest.stageNames.size());
        AddGuestHook(guest.id, guest.stageAt, StageSet);
        const bool steps = guest.camera == Guest::Camera::Steps;
        AddGuestHook(guest.id, guest.across, steps ? StepAcross : ExtentsAcross);
        AddGuestHook(guest.id, guest.up, steps ? StepUp : ExtentsUp);
    }
    if (Guests().size() > kMostGuests) {
        Guests().resize(kMostGuests);
    }
    Game &game = TheGame();
    g_before = game.beforeFrame;
    game.beforeFrame = BeforeFrame;
    g_frame = game.frame;
    game.frame = Frame;
    g_state = game.state;
    game.state = StagesState;
}

} // namespace cps2recomp::guest_stages
