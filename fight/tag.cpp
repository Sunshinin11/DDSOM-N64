// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Tag-team play for four people (tag.h).
#include "tag.h"

#include "alternative.h"
#include "game.h"

#include <algorithm>

namespace cps2recomp::tag {

namespace {
    Description g_d;
    std::vector<HudLine> g_labels; // (on the HUD now)

    // The HUD's lines with the labels in place of the ones there
    void Label(std::vector<HudLine> labels) {
        std::vector<HudLine> lines;
        for (const HudLine &line : Hud()) {
            if (std::find(g_labels.begin(), g_labels.end(), line) == g_labels.end()) {
                lines.push_back(line);
            }
        }
        lines.insert(lines.end(), labels.begin(), labels.end());
        g_labels = std::move(labels);
        SetHud(std::move(lines));
    }

    // Whether the controls are routed now
    bool Routing() {
        return Chosen() != kOff && !(g_d.otherMode && g_d.otherMode());
    }

    // The person whose controls go to a side's panel now (-1: the side's first person, with the second's Start and
    // coins), and the partner's off point while a round is fought (-1: none)
    void Controller(int side, int &person, int &partner) {
        person = partner = -1;
        const int pick = g_d.picking ? g_d.picking(side) : -1;
        if (pick >= 0) {
            person = Person(side, pick == 1 ? 1 : 0);
            return;
        }
        const int point = g_d.onPoint ? g_d.onPoint(side) : -1;
        if (point == 0 || point == 1) {
            person = Person(side, point);
            if (g_d.fighting && g_d.fighting()) {
                partner = Person(side, 1 - point);
            }
        }
    }
} // namespace

Mode Chosen() {
    const int value = FeatureValue(kFeature);
    return value == kCoop ? kCoop : value == kVersus ? kVersus : kOff;
}

int Person(int side, int pick) {
    switch (Chosen()) {
    case kCoop:
        return side == 0 ? pick : -1;
    case kVersus:
        return side + 2 * pick;
    default:
        return pick == 0 ? side : -1;
    }
}

void BeforeFrame(Machine &machine) {
    uint16_t held[Machine::kMaxPlayers];
    std::copy(std::begin(machine.players), std::end(machine.players), held);
    // (players 3 and 4 are on the lines of players 1's and 2's kicks: never on the board here)
    machine.players[2] = machine.players[3] = 0;
    if (!Routing()) {
        return;
    }
    for (int side = 0; side < 2; side++) {
        const int first = Person(side, 0), second = Person(side, 1);
        if (first < 0) {
            machine.players[side] = 0; // (the computer's side: nobody comes in)
            continue;
        }
        int person, partner;
        Controller(side, person, partner);
        uint16_t panel;
        if (person < 0) {
            panel = static_cast<uint16_t>(held[first] | (held[second] & (kStart | kCoin)));
        } else {
            const int other = person == first ? second : first;
            panel = static_cast<uint16_t>(held[person] | (held[other] & kCoin));
            if (partner >= 0 && g_d.tagButtons && (held[partner] & g_d.tagButtons) == g_d.tagButtons) {
                panel |= g_d.tagButtons;
            }
        }
        machine.players[side] = panel;
    }
}

void Frame(Machine &) {
    if (!Routing()) {
        if (!g_labels.empty()) {
            Label({});
        }
        return;
    }
    std::vector<HudLine> lines;
    float left = g_d.labelLeft, right = g_d.labelRight;
    if (g_d.labelEnds) {
        g_d.labelEnds(left, right);
    }
    for (int side = 0; side < 2; side++) {
        const int point = g_d.onPoint ? g_d.onPoint(side) : -1;
        const int person = point == 0 || point == 1 ? Person(side, point) : -1;
        if (person >= 0) {
            const std::string text = "P" + std::to_string(person + 1);
            lines.push_back({side == 0 ? left : right, g_d.labelY, side == 0 ? -1 : 1, text});
        }
    }
    Label(std::move(lines));
}

void Register(const Description &description) {
    g_d = description;
    TheGame().features.push_back({kFeature, g_d.page ? g_d.page : modes::kModes, "Tag team",
                                  g_d.help ? g_d.help :
                                  "Each character of a team played by a person of its own. Co-op: players 1 and 2 are one team "
                                  "against the computer. 2 vs 2: players 1 and 3 against players 2 and 4. Each chooses one "
                                  "character (the first person first); in a fight the panel is whoever's character is on point, "
                                  "and the partner tags in with HP+HK. The labels under the bars say who is on point.",
                                  0, {"Off", "Co-op vs. computer", "2 vs 2"}});
}

} // namespace cps2recomp::tag
