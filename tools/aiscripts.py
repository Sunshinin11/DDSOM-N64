#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Prints the computer player's scripts as text: the Vampire Savior games', Super Gem Fighter's and Street Fighter
Alpha 1's, 2's and 3's computer is an interpreter running scripts from the ROM, a set for each character
(games/vampire_savior/PROGRESS.md, "The computer player"; games/sfa/PROGRESS.md, games/sfa2/PROGRESS.md and
games/sfa3/PROGRESS.md, "The computer player: scripts").
Read from a build's work/<id>/ (program.lst, program.bin, program.bin.data), run from the repository's top.

  aiscripts.py [GAME]                     every character's scripts (GAME: vampire_savior)
  aiscripts.py GAME --character NAME|N    one character's
  aiscripts.py GAME --check               decode everything and say what did not fit; prints nothing else
  aiscripts.py GAME --against FILE        check a run's --watch of a computer's script pointer (Vampire Savior
                                          +220, Alpha 2 +230: a long) against the instructions found: every
                                          place the game's interpreter kept there must be one of theirs, and
                                          every step from one to the next one the scripts allow
                                          (CPS2RECOMP_WATCH_REPORTS=N for more than the first 2,000 writes)

The instructions' lengths are the game's own: its routine that skips over instructions (an "if" whose test
fails) has a table of them, read here from the program. The instructions are named by the interpreter they
were read from, told apart by where it keeps the computer's part of a player's record: Vampire Savior's (the
banks' places from +210) or Alpha 2's (from +220, its record's fields 10h on from Vampire Savior's; fewer
operations, a second "if" with "else if" and a break out of it, the Alpha counter); Alpha 3's is Alpha 2's with its
own fields and more (the isms, a throw, a custom combo), named by the game, and so is Alpha 1's, Alpha 2's with less
(one "if", fewer waits and tests). An instruction of another kind is printed as its words.

Each character has four banks of scripts: 0 its plans (one chosen by the game), 1 its tactics (one at random
from 32 slots for its plan, in the row of its tier: 0, 20 or 40 by the difficulty), 2 its reactions to an attack or a
projectile (likewise at random), 3 more reactions (one chosen by the game). A script is a word (what happens
when it is broken off) and instructions, two bytes each (an operation and a kind of it) and words after, to
the end (FF). Distances are the gap between the two fighters (Vampire Savior +130, Alpha 2 +218). A move is a
word, bit 0 clear for a normal and set for a special. Vampire Savior's: a normal's stance, punch or kick,
strength, the directions held and a variant, or a special's number and strength. Alpha 2's: three bits held
for a character's command normals (+212, +213, +214), then a normal's stance, punch or kick and strength, a
nibble each, or a special's number, punch or kick and strength.
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.getcwd() if os.path.isdir("games") else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class Program:
    def __init__(self, game):
        work = os.path.join(ROOT, "work", game)
        self.code = open(os.path.join(work, "program.bin"), "rb").read()      # as the 68000 fetches (and PC-relative)
        self.data = open(os.path.join(work, "program.bin.data"), "rb").read()  # as it reads data
        self.lines = []
        for line in open(os.path.join(work, "program.lst")).read().split("\n"):
            m = re.match(r"^[> ]([0-9A-F]{6})  (.*)$", line)
            if m:
                self.lines.append((int(m.group(1), 16), m.group(2)))

    def cw(self, a):  # a signed word of the program's code space
        return struct.unpack(">h", self.code[a:a + 2])[0]

    def cuw(self, a):
        return struct.unpack(">H", self.code[a:a + 2])[0]

    def dw(self, a):  # a word of its data
        return struct.unpack(">H", self.data[a:a + 2])[0]

    def dsw(self, a):  # a signed word of its data (offsets: lea 0(a0,d1.w))
        return struct.unpack(">h", self.data[a:a + 2])[0]

    def dl(self, a):
        return struct.unpack(">I", self.data[a:a + 4])[0]


# --- the instructions' lengths: the game's skipping routine ---

def skip_lengths(p):
    """{(op, kind): operand words} and {test: operand words} for "if", from the routines that skip over
    instructions: move.b (a0)+,d0 / bmi / move.w T(pc,d0.w),d1 / jmp T(pc,d1.w) (the operation's table). A game can
    have more than one (Alpha 2 and 3: one for each kind of if), and they need not know the same operations and tests
    (Alpha 3's): the one that knows the most is taken, and lengths["known by all"] says what every one of them knows."""
    tables = []
    for i, (a, ins) in enumerate(p.lines):
        if ins == "move.b (a0)+,d0" and i + 2 < len(p.lines) and p.lines[i + 1][1].startswith("bmi ") and \
                re.match(r"move\.w \$[0-9A-F]+\(pc,d0\.w\),d1", p.lines[i + 2][1]):
            tables.append(int(re.search(r"\$([0-9A-F]+)", p.lines[i + 2][1]).group(1), 16))
    if not tables:
        sys.exit("aiscripts: the routine that skips over instructions is not in this program's listing")
    found = [skip_lengths_of(p, t) for t in tables]
    found = [x for x in found if x[0].get("ifs")]  # (a skipping routine knows its ifs)
    lengths, tests = max(found, key=lambda x: (len(x[0]), sum(len(t) for t in x[1].values())))
    ops = set.intersection(*({k[0] for k in x[0] if k != "ifs"} for x in found))
    known = set.intersection(*({k for t in x[1].values() for k in t} for x in found))
    lengths["known by all"] = (ops, known)
    return lengths, tests


def skip_lengths_of(p, table):
    def length_at(a):  # a routine that steps over operands: lea N(a0),a0; rts, or move.w (a0)+,d1 for each (Alpha 1's); rts
        if p.cuw(a) == 0x41E8 and p.cuw(a + 4) == 0x4E75:
            return p.cuw(a + 2) // 2
        n = 0
        while p.cuw(a + 2 * n) == 0x3218 and n < 8:
            n += 1
        if p.cuw(a + 2 * n) == 0x4E75:
            return n
        return None

    def sub_table(t, end):  # word offsets from t to routines, up to `end` (where the code goes on)
        out = []
        a = t
        while a < end:
            n = length_at(t + p.cw(a))
            if n is None:
                break
            out.append(n)
            a += 2
        return out

    lengths, tests = {}, {}
    handlers = []
    for op in range(0, 0x40, 2):
        h = table + p.cw(table + op)
        if not (table < h < table + 0x400) or p.cuw(h) != 0x1018:  # move.b (a0)+,d0: the kind
            break
        handlers.append((op, h))
    starts = sorted(h for _, h in handlers)
    for op, h in handlers:
        later = [s for s in starts if s > h]
        if p.cuw(h + 2) == 0x6100:  # bsr: every kind the same
            n = length_at(h + 4 + p.cw(h + 4))
            lengths[(op, None)] = n
        elif p.cuw(h + 2) == 0x6000:  # bra back: no operands
            lengths[(op, None)] = 0
        elif p.cuw(h + 2) == 0x323B and p.cuw(h + 6) == 0x4EBB:  # a table of kinds, jsr
            t = h + 4 + p.cuw(h + 4)
            for k, n in enumerate(sub_table(t, later[0] if later else t + 0x40)):
                lengths[(op, 2 * k)] = n
        elif p.cuw(h + 2) == 0x323B and p.cuw(h + 6) == 0x4EFB:  # "if": its kinds by a jmp table
            t = h + 4 + p.cuw(h + 4)
            kinds = []
            while t + 2 * len(kinds) < t + p.cw(t) and len(kinds) < 8:  # (up to the first kind's code)
                kinds.append(t + p.cw(t + 2 * len(kinds)))
            if len(kinds) not in (3, 5):
                continue  # (not an if's: not a skipping routine's table)
            for k in range(len(kinds)):
                lengths[(op, 2 * k)] = 0  # (the test's own word: below)
            # [addq.b #1,nesting] / move.w (a0)+,d0 / move.w T(pc,d0.w),d1 / jsr (Alpha 2's first "if" skips without
            # counting the nesting: it has a skipping routine of its own)
            ifs = kinds[0]
            tt = ifs + 4 + p.cuw(ifs + 4) if p.cuw(ifs) == 0x3018 else ifs + 8 + p.cuw(ifs + 8)
            tests[op] = {}
            for k, n in enumerate(sub_table(tt, kinds[1] if kinds[1] > tt else tt + 0x80)):
                tests[op][2 * k] = n
            lengths.setdefault("ifs", {})[op] = len(kinds)
    return lengths, tests


# --- the banks of scripts: the routines that start one ---

def bank_tables(p):
    """The four tables of the characters' scripts (by the character), from the routines that start a script of
    each bank: movea.l #TABLE,a0 ... move.l a0,0x210(a6) (0x214, 0x218, 0x21C; Alpha 2's from 0x220). Also where
    the banks' places start in the record (0x210 or 0x220)."""
    found = {}
    for i, (a, ins) in enumerate(p.lines):
        m = re.match(r"move\.l a0,0x(2[12][048C])\(a6\)$", ins)
        if not m:
            continue
        for back in range(1, 32):
            # (Alpha 1's: lea TABLE(pc),a0)
            mm = re.match(r"(?:movea\.l #|lea )\$([0-9A-F]+)(?:\(pc\))?,a0$", p.lines[i - back][1])
            if mm:
                found.setdefault(int(m.group(1), 16), int(mm.group(1), 16))
                break
            if p.lines[i - back][1].startswith("rts"):
                break
    for base in (0x210, 0x220):
        if all(base + 4 * b in found for b in range(4)):
            return {b: found[base + 4 * b] for b in range(4)}, base
    sys.exit("aiscripts: the banks' tables were not all found (%s)" % found)


def character_names(game):
    """The characters' names by number: a family's roster ({0xNN, "Name"} in family.h), or a game's list
    (kCharacterNames[...] = {"Name", ...} in its src/*.h)"""
    names = {}
    folder = os.path.join(ROOT, "games", game)
    family = os.path.join(folder, "family.h")
    if os.path.exists(family):
        for m in re.finditer(r'\{0x([0-9A-Fa-f]+), "([^"]+)"', open(family).read()):
            names.setdefault(int(m.group(1), 16), m.group(2))
    src = os.path.join(folder, "src")
    for f in sorted(os.listdir(src)) if not names and os.path.isdir(src) else []:
        m = re.search(r"kCharacterNames\[[^\]]*\] = \{([^}]*)\}", open(os.path.join(src, f)).read())
        if m:
            for n, name in enumerate(re.findall(r'"([^"]*)"', m.group(1))):
                names[n] = name
    return names


# --- naming (Vampire Savior's instructions) ---

STRENGTH = {0: "light", 2: "medium", 4: "heavy", 6: "strength 6"}


def move(w):
    if w & 1:
        text = "special %d %s" % ((w & 0x1E) >> 1, STRENGTH.get((w >> 4) & 6, "?"))
        variant = (w >> 8) & 0xF
    else:
        # (as the game unpacks it, 02CE44: the stance, punch or kick, the strength, in twos from bit 1)
        stance = {0: "standing", 2: "crouching", 4: "jumping", 6: "stance 6"}[w & 6]
        button = {0: "punch", 2: "kick"}.get((w >> 2) & 6, "button %d" % ((w >> 2) & 6))
        text = "%s %s %s" % (stance, STRENGTH[(w >> 4) & 6], button)
        held = (w >> 8) & 0xF
        if held:
            text += " holding %X" % held
        variant = (w >> 12) & 0xF
    if variant:
        text += ", variant %d" % variant
    return text


def way(w):  # a jump's or dash's word: the way (high byte: 0 forward, 1 back, FF up) and its kind (low)
    return {0: "forward", 1: "back", 0xFF: "straight up"}.get(w >> 8, "way %02X" % (w >> 8))


def with_move(w, m, frames):
    """A jump's or dash's move, by its kind (the way word's low byte, a byte offset into a table: 0 and 2 start
    no timer, 4 starts the move after so many frames; bit 0 is Felicia's alone)"""
    kind = w & 0xFF
    if kind & 0xFE == 4:
        return ": %s after %d frames" % (move(m), frames)
    return "" if not m and not frames else " (kind %d: %s, %d)" % (kind, move(m), frames)


TESTS = {
    0x00: "gap <= {0}", 0x02: "gap >= {0}", 0x04: "one time in 2", 0x06: "a gauge stock",
    0x08: "the other in the air", 0x0A: "the other crouching", 0x0C: "the other lying down",
    0x0E: "the other's projectile out", 0x10: "the other's projectile out ({0})",
    0x12: "the other's projectile out ({0})", 0x14: "an attack coming",
    0x16: "the other jumping back", 0x18: "the other jumping forward", 0x1A: "the other jumping straight up",
    0x1C: "the other open (on the floor, in a move)", 0x1E: "level <= {0}", 0x20: "level >= {0}",
    0x22: "no projectile of mine out", 0x24: "the other's health below {0}", 0x26: "my health above the other's",
    0x28: "the other in the air, falling", 0x2A: "the other in the air, rising", 0x2C: "the other below {0} up",
    0x2E: "the other above {0} up", 0x30: "the other's projectile out", 0x32: "none of the other's projectiles out",
    0x34: "my health below {0}", 0x36: "three times in 4", 0x38: "the other is character {0}",
    0x3A: "I am character {0}", 0x3C: "+39 set", 0x3E: "one time in 16 by the bits {0:04X}",
    0x40: "the other's projectile {0} out", 0x42: "gauge stocks >= {0}",
}


def name(op, kind, args, names):
    a = args + [0] * 5
    n = {
        (0x00, 0x00): "stand %d frames" % a[0],
        (0x00, 0x02): "crouch",
        (0x00, 0x04): "stand up",
        (0x00, 0x06): "wait while the other is in the air",
        (0x00, 0x08): "wait while the other is in the air, above %d" % a[0],
        (0x00, 0x0A): "wait while my projectile %d is out" % a[0],
        (0x00, 0x0C): "(nothing)",
        (0x00, 0x0E): "wait while the other's projectile is out (%d)" % a[0],
        (0x00, 0x10): "think (a time by level and character)",
        (0x00, 0x12): "wait while my projectile is out",
        (0x00, 0x14): "wait until the other is open",
        (0x00, 0x16): "think (a time by level and character), not broken off",
        (0x00, 0x18): "wait a frame",
        (0x00, 0x1A): "wait %d frames, not broken off" % a[0],
        (0x00, 0x1C): "wait until gap <= %d, at most %d frames" % (a[0], a[1]),
        (0x00, 0x1E): "wait until gap >= %d, at most %d frames" % (a[0], a[1]),
        (0x00, 0x20): "wait until the other is open (not thrown)",
        (0x00, 0x22): "wait (+23F, with a gauge stock)",
        (0x00, 0x24): "stand",
        (0x02, 0x00): "walk forward until gap <= %d" % a[0],
        (0x02, 0x02): "walk back until gap >= %d" % a[0],
        (0x02, 0x04): "walk to gap %d" % a[0],
        (0x02, 0x06): "walk forward %d frames" % a[0],
        (0x02, 0x08): "walk back %d frames" % a[0],
        (0x02, 0x0A): "walk forward until gap <= %d, at most %d frames" % (a[0], a[1]),
        (0x02, 0x0C): "walk back until gap >= %d, at most %d frames" % (a[0], a[1]),
        (0x02, 0x0E): "walk to gap %d, at most %d frames" % (a[0], a[1]),
        (0x04, 0x00): "jump %s%s" % (way(a[1]), with_move(a[1], a[0], a[2])),
        (0x04, 0x02): "jump %s (%04X), then jump %s%s" % (way(a[0]), a[1], way(a[3]), with_move(a[3], a[2], a[4])),
        (0x04, 0x04): "jump %s%s (+23C %d)" % (way(a[1]), with_move(a[1], a[0], a[2]), a[3]),
        (0x04, 0x06): "jump %s, the other way%s" % (way(a[1]), with_move(a[1], a[0], a[2])),
        (0x04, 0x08): "jump %s into special 5%s" % (way(a[1]), with_move(a[1], a[0], a[2])),
        (0x04, 0x0A): "jump %s into chain %d%s" % (way(a[1]), a[0], with_move(a[1], 0, a[2]) and " (%d frames)" % a[2]),
        (0x06, 0x00): "attack: %s" % move(a[0]),
        (0x06, 0x02): "attack, held: %s" % move(a[0]),
        (0x08, 0x00): "special move: %s" % move(a[0]),
        (0x0A, 0x00): "gauge move (one stock): %s" % move(a[0]),
        (0x0E, 0x00): "go to a tactic (bank 1), then back here",
        (0x0E, 0x02): "go to a tactic (bank 1) by the table 0BC780's chance, then back here",
        (0x10, 0x00): "guard if an attack is coming (by the character's chance)",
        (0x10, 0x02): "guard if an attack is coming, then: %s" % move(a[0]),
        (0x12, 0x00): "above level %d: let the reactions %04X break in" % (a[0], a[1]),
        (0x12, 0x02): "above level %d: keep the reactions %04X out" % (a[0], a[1]),
        (0x12, 0x04): "can be broken off",
        (0x12, 0x06): "cannot be broken off",
        (0x14, 0x00): "gauge move, kind 12 (one stock): %s" % move(a[0]),
        (0x16, 0x00): "dash %s%s" % (way(a[1]), with_move(a[1], a[0], a[2])),
        (0x18, 0x00): "chain %d" % a[0],
        (0x1A, 0x00): "(stop)",
    }.get((op, kind))
    if n is None:
        n = "op %02X kind %02X %s" % (op, kind, " ".join("%04X" % x for x in args))
    return n


ENDS = {0x00: "end: a tactic (bank 1) at random", 0x02: "end: back to the plan", 0x04: "end: a tactic, or back to the plan, by the table 0BB480's chance"}


# --- naming (Alpha 2's instructions: its interpreter at 0280D4) ---

def alpha_move(w):
    """As the game unpacks it (029BE4): bits 1 to 3 held for a command normal (+212, +213, and +214, which is
    also the way a jump or walk goes), then a normal's stance, punch or kick and strength (+100, +101, +102), a
    nibble each, or a special's number (+133, in the character's own list) and strength"""
    held = "".join(" +%X" % (0x212 + b) for b in range(3) if w & (2 << b))
    if w & 1:  # (the character's own code knows a special's button: the nibble is 0)
        text = "special %d %s" % ((w >> 4) & 0xF, STRENGTH.get((w >> 12) & 0xF, "strength %d" % ((w >> 12) & 0xF)))
        if (w >> 8) & 0xF:
            text += " (+101 %d)" % ((w >> 8) & 0xF)
    else:
        text = "%s %s %s" % ({0: "standing", 2: "crouching", 4: "jumping"}.get((w >> 4) & 0xF, "stance %d" % ((w >> 4) & 0xF)),
                             STRENGTH.get((w >> 12) & 0xF, "strength %d" % ((w >> 12) & 0xF)),
                             {0: "punch", 2: "kick"}.get((w >> 8) & 0xF, "button %d" % ((w >> 8) & 0xF)))
    return text + (", held" + held if held else "")


def alpha_way(w):  # a jump's way word's high byte, as Alpha 2's jump reads it (023344): 0 straight up, 1 forward, FF back
    return {0: "straight up", 1: "forward", 0xFF: "back"}.get(w >> 8, "way %02X" % (w >> 8))


def alpha_with_move(w, m, frames):
    """A jump's move, by the way word's low byte (as Vampire Savior's: 4 starts the move after so many frames)"""
    kind = w & 0xFF
    if kind & 0xFE == 4:
        return ": %s after %d frames" % (alpha_move(m), frames)
    return "" if not m and not frames else " (kind %d: %s, %d)" % (kind, alpha_move(m), frames)


ALPHA_TESTS = dict(TESTS)
ALPHA_TESTS.update({
    0x06: "super gauge >= {0}", 0x0C: "the other knocked down",
    0x0E: "the other's projectile out", 0x10: "the other's projectile within {0}",
    0x12: "no projectile of the other's within {0}", 0x14: "an attack coming",
    0x16: "the other jumping forward", 0x18: "the other jumping straight up", 0x1A: "the other jumping back",
    0x30: "no projectile of the other's out, or one of class 0 or 4 (table 028BCE)",
    0x32: "the other's projectile out, of class 3 or more (table 028BCE)", 0x3C: "+5B set",
})
for _t in (0x3E, 0x40, 0x42):
    del ALPHA_TESTS[_t]


def alpha_name(op, kind, args, names):
    a = args + [0] * 5
    n = {
        (0x00, 0x00): "stand %d frames" % a[0],
        (0x00, 0x02): "crouch",
        (0x00, 0x04): "stand up",
        (0x00, 0x06): "wait while the other is in the air",
        (0x00, 0x08): "wait while the other is in the air, above %d" % a[0],
        (0x00, 0x0A): "if the other is attacking: a normal at its attack (by where it reaches, 027038)",
        (0x00, 0x0C): "stand %d frames, or a normal at the other's attack (027038)" % a[0],
        (0x00, 0x0E): "wait while the other's projectile is out, farther than %d" % a[0],
        (0x00, 0x10): "think (a time by level and character)",
        (0x00, 0x12): "wait while my projectile is out",
        (0x00, 0x14): "wait until the other is open",
        (0x00, 0x16): "think (a time by level and character), not broken off",
        (0x00, 0x18): "wait a frame",
        (0x00, 0x1A): "wait %d frames, not broken off" % a[0],
        (0x00, 0x1C): "wait until gap <= %d, at most %d frames" % (a[0], a[1]),
        (0x00, 0x1E): "wait until gap >= %d, at most %d frames" % (a[0], a[1]),
        (0x00, 0x20): "wait until the other is open (not thrown)",
        (0x00, 0x22): "a normal by the gap and the height between (table 026F1A), if it has one",
        (0x00, 0x24): "stand",
        (0x02, 0x00): "walk forward until gap <= %d" % a[0],
        (0x02, 0x02): "walk back until gap >= %d" % a[0],
        (0x02, 0x04): "walk to gap %d" % a[0],
        (0x02, 0x06): "walk forward %d frames" % a[0],
        (0x02, 0x08): "walk back %d frames" % a[0],
        (0x02, 0x0A): "walk forward until gap <= %d, at most %d frames" % (a[0], a[1]),
        (0x02, 0x0C): "walk back until gap >= %d, at most %d frames" % (a[0], a[1]),
        (0x02, 0x0E): "walk to gap %d, at most %d frames" % (a[0], a[1]),
        (0x04, 0x00): "jump %s%s" % (alpha_way(a[1]), alpha_with_move(a[1], a[0], a[2])),
        (0x04, 0x02): "special 3 (a jump of its own) %s, as %s%s" % (alpha_way(a[1]), alpha_move(a[0]), alpha_with_move(a[1], a[0], a[2])),
        (0x06, 0x00): "attack: %s" % alpha_move(a[0]),
        (0x06, 0x02): "attack, held: %s" % alpha_move(a[0]),
        (0x08, 0x00): "special move: %s" % alpha_move(a[0]),
        (0x0A, 0x00): "super combo: %s" % alpha_move(a[0]),
        (0x0E, 0x00): "go to a tactic (bank 1), then back here",
        (0x0E, 0x02): "go to a tactic (bank 1) by the table 09BE44's chance, then back here",
        (0x10, 0x00): "guard if an attack is coming",
        (0x10, 0x02): "guard if an attack is coming, with an Alpha counter (a level of gauge, the character's chance 09B044)",
        (0x12, 0x00): "above level %d: let the reactions %04X break in" % (a[0], a[1]),
        (0x12, 0x02): "above level %d: keep the reactions %04X out" % (a[0], a[1]),
        (0x12, 0x04): "can be broken off",
        (0x12, 0x06): "cannot be broken off",
    }.get((op, kind))
    if n is None:
        n = "op %02X kind %02X %s" % (op, kind, " ".join("%04X" % x for x in args))
    return n


ALPHA_ENDS = dict(ENDS)
ALPHA_ENDS[0x04] = "end: a tactic, or back to the plan, by the table 09AF44's chance"


# --- reading the scripts ---

# The instructions' names by interpreter (bank_tables: where the banks' places start in the record)
# --- naming (Alpha 3's instructions: its interpreter at 030A80; Alpha 2's but for these) ---

def alpha3_move(w):
    """As the game unpacks it (0328B0): bits 0 to 3 held (+83; bit 0 is also the way, +211); bit 11 set, a special
    (its number from bit 4, +AA; punch or kick, bit 10; strength from bit 13), clear a normal (bits 4 to 7 to +2C0;
    stance from bit 8, punch or kick bit 10, strength from bit 13)"""
    held = " held %X" % (w & 0xF) if w & 0xF else ""
    strength = STRENGTH.get((w >> 12) & 0xE, "strength %d" % ((w >> 12) & 0xE))
    if w & 0x800:
        return "special %d %s%s" % ((w >> 4) & 0x3F, strength, held)
    text = "%s %s %s" % ({0: "standing", 2: "crouching", 4: "jumping"}.get((w >> 7) & 6, "stance %d" % ((w >> 7) & 6)), strength,
                         "kick" if w & 0x400 else "punch")
    if w & 0xF0:
        text += " (+2C0 %X)" % ((w >> 4) & 0xF)
    return text + held


def alpha3_with_move(w, m, frames):
    kind = w & 0xFF
    if kind & 0xFE == 4:
        return ": %s after %d frames" % (alpha3_move(m), frames)
    return "" if not m and not frames else " (kind %d: %s, %d)" % (kind, alpha3_move(m), frames)


ALPHA3_TESTS = dict(ALPHA_TESTS)
ALPHA3_TESTS.update({
    0x06: "super gauge >= {0} (A-ISM; X- and V-ISM, or +123 set: a full gauge)",
    0x3E: "A-ISM", 0x40: "not A-ISM", 0x42: "X-ISM", 0x44: "not X-ISM", 0x46: "V-ISM", 0x48: "not V-ISM",
    0x4A: "+5B is 1", 0x4C: "+B9 set",
})


def alpha3_name(op, kind, args, names):
    a = args + [0] * 5
    n = {
        (0x04, 0x00): "jump %s%s" % (alpha_way(a[1]), alpha3_with_move(a[1], a[0], a[2])),
        (0x04, 0x02): "special 3 (a jump of its own) %s, as %s%s" % (alpha_way(a[1]), alpha3_move(a[0]), alpha3_with_move(a[1], a[0], a[2])),
        (0x04, 0x04): "jump %s, a V-ISM custom combo in the air if it can (half the gauge)%s" % (alpha_way(a[1]), alpha3_with_move(a[1], a[0], a[2])),
        (0x04, 0x06): "jump %s into an air throw (%02X %02X)%s" % (alpha_way(a[1]), a[0] >> 8, a[0] & 0xFF, alpha3_with_move(a[1], 0, a[2])),
        (0x06, 0x00): "attack: %s" % alpha3_move(a[0]),
        (0x06, 0x02): "attack, held: %s" % alpha3_move(a[0]),
        (0x08, 0x00): "special move: %s" % alpha3_move(a[0]),
        (0x0A, 0x00): "super combo: %s (X-ISM: level 3, a full gauge; V-ISM: a frame's wait instead)" % alpha3_move(a[0]),
        (0x0A, 0x02): "custom combo (V-ISM, half the gauge), timers by %d, special %d" % (a[0] >> 8, a[0] & 0xFF),
        (0x0E, 0x02): "go to a tactic (bank 1) by the table 0C965A's chance, then back here",
        (0x10, 0x02): "guard if an attack is coming, with an Alpha counter (a level of gauge, the character's chance 0C845A)",
        (0x16, 0x00): "throw (%02X %02X)" % (a[0] >> 8, a[0] & 0xFF),
    }.get((op, kind))
    return n if n is not None else alpha_name(op, kind, args, names)


ALPHA3_ENDS = dict(ENDS)
ALPHA3_ENDS[0x04] = "end: a tactic, or back to the plan, by the table 0C835A's chance"

# --- naming (Alpha 1's instructions: its interpreter at 022810; Alpha 2's, with fewer of them) ---

ALPHA1_TESTS = {t: n for t, n in ALPHA_TESTS.items() if t <= 0x34}
ALPHA1_TESTS.update({
    0x30: "no projectile of the other's out, or one of class 0 or 4 (table 0231C4)",
    0x32: "the other's projectile out, of class 3 or more (table 0231C4)",
})


def alpha1_name(op, kind, args, names):
    a = args + [0] * 5
    n = {
        (0x00, 0x0A): "if the other is attacking: a normal at its attack (by where it reaches, 02191A)",
        (0x00, 0x0C): "stand %d frames, or a normal at the other's attack (02191A)" % a[0],
        (0x0E, 0x02): "go to a tactic (bank 1) by the table 120498's chance, then back here",
        (0x10, 0x02): "guard if an attack is coming, with an Alpha counter (the character's chance 11FC18)",
    }.get((op, kind))
    return n if n is not None else alpha_name(op, kind, args, names)


ALPHA1_ENDS = dict(ENDS)
ALPHA1_ENDS[0x04] = "end: a tactic, or back to the plan, by the table 11FB18's chance"

STYLES = {
    0x210: (name, TESTS, ENDS),
    0x220: (alpha_name, ALPHA_TESTS, ALPHA_ENDS),
    "alpha3": (alpha3_name, ALPHA3_TESTS, ALPHA3_ENDS),
    "alpha1": (alpha1_name, ALPHA1_TESTS, ALPHA1_ENDS),
}
# Alpha 1's banks' places are Alpha 2's (from +220): told apart by the game
ALPHA1 = ("sfa",)
# Alpha 3's interpreter is Alpha 2's with more (its record's fields elsewhere, the same banks' places): told apart by
# the game. Its first if's skipping routine (032AEE) knows its throw (a word after it) and its tests from 3E (none),
# its second if's only Alpha 2's operations and tests (--check counts those inside such an if: none)
ALPHA3 = ("sfa3",)
ALPHA3_MORE = {(0x16, None): 1}
ALPHA3_MORE_TESTS = {t: 0 for t in range(0x3E, 0x4E, 2)}


class Decoder:
    def __init__(self, p, lengths, tests, names, style=0x210):
        self.p, self.lengths, self.tests, self.names = p, lengths, tests, names
        self.ifs = lengths.get("ifs", {})  # the "if" operations: how many kinds each has
        self.name, self.test_names, self.end_names = STYLES[style]
        self.problems = []
        # (Alpha 3: what not every one of its skipping routines knows, inside an if whose routine does not: a failed if
        # there would step over it)
        self.unskippable = []
        self.known = lengths.get("known by all", (set(), set()))
        self.boundaries = set()
        self.ends = set()
        self.printed = {}  # a script's place: what it was first printed as
        self.following = {}  # an instruction's place: the next one's
        self.firsts = set()  # scripts' first instructions
        self.plans = set()  # the plans' instructions (a plan's place is kept as it runs, and gone back to)
        self.in_plan = False

    def instruction_length(self, op, kind):
        if (op, kind) in self.lengths:
            return self.lengths[(op, kind)]
        return self.lengths.get((op, None))

    def script(self, at, limit, label):
        """The script at `at` (its first word, then its instructions), as lines; `limit`: where the bank's data
        ends (a script that runs on past it is a problem)."""
        p = self.p
        self.firsts.add(at + 2)
        if at in self.printed:
            return ["    the same as %s" % self.printed[at]], at
        self.printed[at] = label
        lines = ["    header %04X" % p.dw(at)]
        a = at + 2
        self.firsts.add(a)
        depth = depth5 = 0  # (depth5: inside the ifs of five kinds)
        previous = None
        while a < limit:
            self.boundaries.add(a)
            if previous is not None:
                self.following[previous] = a
            previous = a
            if self.in_plan:
                self.plans.add(a)
            op, kind = p.data[a], p.data[a + 1]
            if op == 0xFF:
                lines.append("    %06X  [FF %02X] %s%s" % (a, kind, "  " * depth, self.end_names.get(kind, "end, kind %02X" % kind)))
                self.ends.add(a)
                if depth:
                    self.problems.append("%s: ends inside an if (%06X)" % (label, a))
                return lines, a + 2
            if op in self.ifs:
                # An "if" of three kinds: 0 if, 2 else, 4 endif. Of five (Alpha 2's second): 0 if, 2 else if (a
                # test, the same nesting), 4 else, 6 a break out to its endif (a branch done), 8 endif.
                five = self.ifs[op] == 5
                if kind == 0 or (five and kind == 2):
                    test = p.dw(a + 2)
                    n = self.tests[op].get(test)
                    if n is None:
                        self.problems.append("%s: an if of an unknown test %02X (%06X)" % (label, test, a))
                        return lines, a
                    args = [p.dw(a + 4 + 2 * i) for i in range(n)]
                    if depth5 and test not in self.known[1]:
                        self.unskippable.append("%s: test %02X inside an if of operation %02X (%06X)" % (label, test, max(self.ifs), a))
                    text = self.test_names.get(test, "test %02X" % test)
                    text = text.format(*args) if n else text
                    if test in (0x38, 0x3A) and args:
                        text = text.replace("character %d" % args[0], self.names.get(args[0], "character %d" % args[0]))
                    if kind == 0:
                        lines.append("    %06X  [%02X %02X] %sif %s" % (a, op, test, "  " * depth, text))
                        depth += 1
                        depth5 += five
                    else:
                        lines.append("    %06X  [%02X %02X] %selse if %s" % (a, op, test, "  " * max(depth - 1, 0), text))
                    a += 4 + 2 * n
                    continue
                if kind == (4 if five else 2):
                    lines.append("    %06X  [%02X %02X] %selse" % (a, op, kind, "  " * max(depth - 1, 0)))
                    a += 2
                    continue
                if five and kind == 6:
                    lines.append("    %06X  [%02X 06] %s(out to the endif)" % (a, op, "  " * depth))
                    a += 2
                    continue
                if kind == (8 if five else 4):
                    depth = max(depth - 1, 0)
                    depth5 = max(depth5 - five, 0)
                    lines.append("    %06X  [%02X %02X] %sendif" % (a, op, kind, "  " * depth))
                    a += 2
                    continue
            if depth5 and op not in self.known[0]:
                self.unskippable.append("%s: operation %02X inside an if of operation %02X (%06X)" % (label, op, max(self.ifs), a))
            n = self.instruction_length(op, kind)
            if n is None:
                self.problems.append("%s: an instruction not in the game's table, %02X %02X (%06X)" % (label, op, kind, a))
                return lines, a
            args = [p.dw(a + 2 + 2 * i) for i in range(n)]
            lines.append("    %06X  [%02X %02X] %s%s" % (a, op, kind, "  " * depth, self.name(op, kind, args, self.names)))
            a += 2 + 2 * n
        self.problems.append("%s: runs past its bank's data (%06X)" % (label, a))
        return lines, a


def offsets(p, base):
    """A table of signed word offsets from `base` (to scripts before it too), which runs up to where the nearest
    of those after it points"""
    out, end = [], None
    while end is None or 2 * len(out) < end:
        o = p.dsw(base + 2 * len(out))
        if o == 0 or abs(o) >= 0x8000:
            break
        if o > 0:
            end = o if end is None else min(end, o)
        out.append(base + o)
        if len(out) > 256:
            break
    return out


def character(p, d, banks, who, out):
    name = d.names.get(who, "character %02X" % who)
    end = len(p.data)
    tables = [p.dl(banks[b] + 4 * who) for b in range(4)]
    if not all(0x400 <= t < end for t in tables):
        return False
    out.append("== %s (%02X)" % (name, who))
    # bank 0, the plans: by +205 (a byte offset into the word offsets)
    d.in_plan = True
    for k, s in enumerate(offsets(p, tables[0])):
        out.append("  plan %d" % k)
        lines, _ = d.script(s, end, "%s plan %d" % (name, k))
        out.extend(lines)
    d.in_plan = False
    # banks 1 and 2, the tactics (by the plan, +205) and the reactions: for each, 32 slots a tier (three rows), then
    # the scripts' offsets at +60
    for bank, what in ((1, "tactic"), (2, "reaction")):
        for g, sub in enumerate(offsets(p, tables[bank])):
            slots = list(p.data[sub:sub + 0x60])
            for k in range(max(slots) + 1):
                s = sub + p.dsw(sub + 0x60 + 2 * k)
                rows = "/".join("%d" % slots[r * 32:(r + 1) * 32].count(k) for r in range(3))
                out.append("  %s %d.%d (slots of 32 by tier 0/20/40: %s)" % (what, g, k, rows))
                lines, _ = d.script(s, end, "%s %s %d.%d" % (name, what, g, k))
                out.extend(lines)
    # bank 3: by +22B (Alpha 1's: the four answers its Reactions choose from, 023B78; the table runs on into data)
    for k, s in enumerate(offsets(p, tables[3])[:4 if d.name is alpha1_name else None]):
        out.append("  reaction (by +22B) %d" % k)
        lines, _ = d.script(s, end, "%s reaction-b %d" % (name, k))
        out.extend(lines)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("game", nargs="?", default="vampire_savior")
    ap.add_argument("--character")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--against")
    args = ap.parse_args()
    p = Program(args.game)
    lengths, tests = skip_lengths(p)
    banks, style = bank_tables(p)
    place = style + 0x10  # (where the interpreter is, in the record)
    if args.game in ALPHA1:
        style = "alpha1"
    if args.game in ALPHA3:
        style = "alpha3"
        lengths.update(ALPHA3_MORE)
        for t in tests.values():
            t.update(ALPHA3_MORE_TESTS)
    names = character_names(args.game)
    d = Decoder(p, lengths, tests, names, style)
    who = None
    if args.character:
        if re.match(r"^(0x)?[0-9A-Fa-f]+$", args.character):
            who = int(args.character, 16 if args.character.startswith("0x") else 10)
        else:
            found = [n for n, s in names.items() if s.lower() == args.character.lower()]
            if not found:
                sys.exit("aiscripts: no character %r (%s)" % (args.character, ", ".join(sorted(names.values()))))
            who = found[0]
    out = []
    seen = set()
    characters = min(32, (banks[1] - banks[0]) // 4)  # (the tables one after the other, a long a character)
    for n in range(characters):
        if who is not None and n != who:
            continue
        table = p.dl(banks[0] + 4 * n)
        if table in seen and who is None:
            continue  # (a character with another's scripts)
        seen.add(table)
        character(p, d, banks, n, out)
    if args.against:
        # The interpreter keeps in +220 (Alpha 2: +230) the next instruction's place (after one is done) or the place just after
        # an instruction's first word (as it starts one); after an end, just past it. From one kept place to the
        # next it goes on to the following instruction, forward in the same script (an if or else skipping), to
        # the start of a script, or back into the plan where it was (the plan's place is kept as it runs).
        kept = places = 0
        wrong = {}
        moves = {"stayed": 0, "to the next instruction": 0, "forward in the script": 0, "to a script's start": 0,
                 "back to the plan": 0, "elsewhere": 0}
        returns = d.plans
        previous = None
        for line in open(args.against):
            m = re.match(r"watch: ([0-9A-F]{6}) writes ([0-9A-F]{8}) to ([0-9A-F]{6})", line)
            if not m or int(m.group(3), 16) & 0x3FF != place:
                continue
            value = int(m.group(2), 16)
            if not 0x400 <= value < len(p.data):
                previous = None
                continue  # (cleared)
            kept += 1
            here = value if value in d.boundaries else value - 2
            if here in d.boundaries or here in d.ends:
                places += 1
            else:
                wrong[value] = m.group(1)
            if previous is not None:
                # (a place kept can be an instruction's or the word after its first: either reading will do)
                candidates = [c for c in (value, value - 2) if c in d.boundaries]
                if value == previous:
                    moves["stayed"] += 1
                elif any(c == d.following.get(b) for c in candidates for b in previous_here):
                    moves["to the next instruction"] += 1
                elif any(c in d.firsts for c in candidates):
                    moves["to a script's start"] += 1
                elif any(c in returns for c in candidates):
                    moves["back to the plan"] += 1
                else:
                    ahead = set()
                    for b in previous_here:
                        x, steps = b, 0
                        while x is not None and steps < 256:
                            ahead.add(x)
                            x, steps = d.following.get(x), steps + 1
                    moves["forward in the script" if any(c in ahead for c in candidates) else "elsewhere"] += 1
            previous, previous_here = value, [c for c in (value, value - 2) if c in d.boundaries]
        print("aiscripts: %d places the game kept, %d of them the instructions' found here; from one to the next: %s" %
              (kept, places, ", ".join("%s %d" % (k, v) for k, v in moves.items())))
        for value, at in sorted(wrong.items())[:20]:
            print("  %06X (kept at %s) is not" % (value, at))
        return 0 if kept and not wrong and not moves["elsewhere"] else 1
    if args.check:
        ins = len(d.boundaries)
        print("aiscripts: %s: %d instructions in %d characters' scripts; banks at %s; %d kinds of instruction, %d tests" %
              (args.game, ins, len(seen), " ".join("%06X" % banks[b] for b in range(4)),
               len([k for k in lengths if k not in ("ifs", "known by all")]), sum(len(t) for t in tests.values())))
        for problem in d.problems:
            print("  " + problem)
        if d.unskippable:
            print("  %d instructions inside an if whose skipping routine does not know them (the first: %s)" %
                  (len(d.unskippable), d.unskippable[0]))
        return 1 if d.problems else 0
    print("\n".join(out))
    for problem in d.problems:
        print("aiscripts: " + problem, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
