# The tag team battle in another game

How Vampire Savior got the "Tag team battle" mode (Alternative modes), and what another fighting game needs to have it.
Written as the handoff for Street Fighter Alpha 2, the next one; the same holds for any game on `fight/battle.h`.

## What the mode is

Two teams of two or three, chosen on its page (each fighter, or Random), on one stage, against the CPU or player 2;
with "Tag team" on the page, co-op (players 1 and 2 against the CPU's team) or 2 vs 2 for four players. Two styles:

- **Knock-out** (KOF-like): one fighter of each team at a time; the one knocked down is out and the next of that team
  comes on with a full bar; the winner stays on with the health it had left. Made from the toolkit's survival: each
  knock-down ends a match, and the next match is made ready on the spare machine (`runtime/spare.h`).
- **Tag** (Marvel-like): each fighter keeps a bar of its own; the left trigger (or Start) tags the next one in whenever
  the one on can act: it calls the next one in, jumps back and flies off the picture, and the next one jumps in from
  that side with a jump attack that can hit. A bar emptied is a knock-down (the game's own) that lies two seconds, not to
  be hit, and the next one jumps in. Resting fighters get back the health that comes back by itself; the super gauges
  carry across a tag; a time over goes to the team with more health in all. The match is over when a team has nobody
  left.

Common to both: one health bar each (or as the board gives), no entrances, a CPU difficulty of its own, every copy of a
character in the match in a colour set of its own, the others' health listed under the bars, the result's line.

## What is shared and what is the game's

**Shared, done** (the toolkit: nothing to write for a new game):

- `fight/battle.cpp`: the mode, both styles, the page's rows (`team_*`, kept as each game's own: `OwnFeatures`), the
  teams and their colours, the knock-out style's matches, the tag style's state machine (`TagBeforeFrame`: still,
  leaving, down, coming), the health and gauges kept by the side, the health coming back while resting, the time over
  by the teams' health, the HUD lines, the newcomer hidden until placed (`playerSets`), the game's play of a side
  switched off while its tag moves.
- `fight/tag.cpp`: the router for co-op and 2 vs 2 (`battle::RegisterTeamTag`), the P1-P4 labels.
- `machine/machine.h` `kTag` and `port/frontend_sdl.cpp`: the left trigger as Tag while the game runs.
- `docs/FIGHT.md` has the toolkit's side in prose.

**The game's own** (`battle::Description` fields; Vampire Savior's in `games/vampire_savior/src/battle.cpp`,
`livetag.cpp`, `fight.cpp`, `frames.cpp`). Each is a fact of the game's program that has to be found in it:

| Field | What it is | Vampire Savior's |
| --- | --- | --- |
| `teams`, `teamLinesY` | the mode on the menu; where the line of a team's others goes | true; 0.19 (under the bars' bats) |
| `RegisterTeamTag(y)` | the router and where its labels go | 0.25 |
| `oneBarEach` | the board's health setting put at one bar, before each frame | the damage gauges byte A5+A4 = 0 |
| `noEntrances` | both players begin a match ready | the "no entrance" bits A5+102 (by +381) |
| `difficulty` | the CPU's difficulty 0-7 | A5+A0 and the copy a game takes, A5+BE |
| `canTag` | the side's fighter can be swapped now | a round fought, not paused (A5+8A), `CanAct`, no hit stop |
| `tagIn(side, character, colours)` | the fighter becomes that character, in that colour set, where it stands | `TagIn`, below |
| `taggedIn` | the game has set the newcomer up | the record's step (+04) off 0 |
| `meter`, `meterSize` | the gauges' bytes, kept by the side across a swap | +109, 3 bytes |
| `offStage(side, off)` | let out of the picture's edges, the camera not following | +134 = 0x80 (the game's own byte for it; the hit test skips it too) |
| `speedX`, `speedY`, `floor` | the speeds (longs, 65536 a pixel a step) and the floor's height | +40, +44, 0x28 |
| `untouchable(side)` | a knocked-out fighter not hit (asked every frame it lies) | +147 = 10, the game's own time of not being hit; and once it lies, its steps not run (`TagStep`) |
| `tagCall(character)` | a sound a character makes as it tags out | a table of its entrance's sounds |
| `downSound` | the announcer's sound at a knock-out | 0x0540 |

And three things outside the description:

- **The knock-down.** Where the game takes health off (Vampire Savior's `TakeDamage` rewrite, `fight.cpp`), a hit that
  empties the bar of a fighter with a partner left (`TeamHoldsKnockOut(player)`), and is not guarded, is told to be a
  knock-down landing lying: the reeling's kind (+54 = 04, read where reeling begins, 02383C) and +117 = 1 (at landing,
  024BB8: lie, not get straight up). Health stops at 0 there and in the round's frame (`RoundFighting`), as in training.
- **Time over.** In the round frame's rewrite, after the game's own decision (00A3B2), `TeamTimeOverResult()` (when not
  0) is the result.
- **A hook where the setting up of a player is done** (Vampire Savior's 022354: `TagSetUp`) puts the newcomer back
  where it was swapped in. The game's setting up puts a fighter where a round begins, in the picture, and it would be
  seen there for a frame.

## How Vampire Savior's swap works (`livetag.cpp`), and what to find in Street Fighter Alpha 2

1. **The game sets a fighter up again itself.** A player's routine branches on its step (+04); step 0 (0222C8) clears
   the record from +80 to +37F (place +10/+14 and the character's bytes from +380 kept), sets health, armour, palette
   slot and first animation, and calls the character's own setting up (a table by number, BD0FA). So a swap is: the
   character bytes (+382, +3E0), the colour set (+3AE, and its pointer +3A4, above what is cleared), the "no entrance"
   bit, and the state put at 0; the game does the rest at its next step. **Find Alpha 2's: what runs when a match
   begins for each player (watch the health word being written as a round starts: `--watch`), and whether running it
   again with another character mid-fight makes that character.** The spike's first test (one swap mid-fight, then
   pictures) answers it in an hour.
2. **What the outgoing character made.** Its own setting up took objects from the game's pools (seven free lists from
   A5+7966/79BE, given back by 017028 on); they name their player at +30 or +32, and the record keeps links at +28-+2E.
   Left running with another character in the record they crashed the game (Q-Bee's in nearly every pair). `GiveBack`
   gives every live one (+00 not zero: not +8, which a taken object keeps) back as the game does, and what those made,
   and clears every player's links to them. **Find Alpha 2's pools and owner fields** (the thrown records, kThrown, are
   one; look at the allocator's callers in the characters' setting up).
3. **The colours** go in on the frame after, in C++ as the game's palette copy does (calling the game's routine from
   `beforeFrame` reset the board: the CPU is stopped inside another routine there). Alpha 2's palette copy is named in
   `sfa2.h` (kColourChoice, kColourSet, the buffer at 90C2xx).
4. **Character-specific leftovers** found by the matrix (below): +3C (how a fighter is drawn: Aulbath's setting up sets
   his own; put back to the 0 a match begins with), Aulbath's move bits (A5-2F50). Expect a few of these in any game.
5. **When.** Only a fighter that can act, not frozen by a hit (swapping one knocked about crashed once). The toolkit
   adds: its leaving is called off if it does not jump within 8 frames (a move of its own came out) or it is hit; the
   buttons pressed with the tag are not passed (they made supers).

## Tests that found the faults (do these again)

- **The swap matrix.** A test switch in the game's sources swaps a player at given frames (`VSAV_SWAP=FRAME:PLAYER:CHARACTER,...`,
  `VSAV_SWAP_FREE=1` to wait until it can act). For each character A: a save state a few frames into a versus fight with
  player 1 as A (`VSAV_CHARACTERS=A,-1`, `--state-save`); then for each B: load it, made-up play for both players
  (`--auto SEED:2:2`), player 1 swapped A->B->A->B and player 2 to B and back, 2,400 frames, with `--stats` and
  `--trace 8000-8FFF`. A run passes with no reset (mode word 0 after the fight began), no interpreter fallback, and the
  right character byte after every swap. All pairs of the roster, twice with other swap times. Vampire Savior's went
  from 167 of 210 clean to 210.
- **The knock-out fuzz.** One-hit knock-outs against the CPU's teams of three, every character among them, 24 runs:
  no reset, the swaps made while the knocked-out one lies.
- **The movies.** A 2 vs 2 tag movie and a knock-out style movie (`games/vampire_savior/movies/team_*`, made-up play for
  four players, the run's options and `CPS2RECOMP_BATTLE_SEED=1` in each first line): `make check` both ways and
  `--check-rewrites`. Record them again when the mode's behaviour changes; the versus movie must hash as before.
- **Pictures.** `--shot-every 1` around a tag and a knock-out, in widescreen too: nothing of a newcomer seen before it
  jumps, nothing left standing.
- Sounds: the queue (Vampire Savior's ring at FF0E0E, its end at FF1E0E) traced over each character's entrance; a
  character's call is the first sound of its entrance no other character's has. Not listened to.

## Street Fighter Alpha 2: where to start

- `games/sfa2/src/battle.cpp` fills the description for survival and the rest already: add `d.teams = true`,
  `teamLinesY`, `battle::RegisterTeamTag(...)` and the knock-out style works (Alpha 2 has rounds: the toolkit moves on
  at each round decided, as its survival does). Then the tag style's fields.
- Alpha 2 already has rewrites of `TakeDamage` (0108E2, `damage.cpp`), `RoundFighting` (005820, `fight.cpp`),
  `HitLands` (`hit.cpp`) and the player's own routines (`player.cpp`: `PlayerInControl` 021620, `PlayerReeling`
  022116): the knock-down, the time over and the "lies" hold go in those.
- Names it already has: `kHealth`/`kHealthShown` (+50/+52), `kInAir` (+31), `kComputer` (+A5), `kColourChoice` (+A8),
  `kColourSet` (+0D), `kStun` (+137); its custom combo and stun are more of what a side might keep across a tag
  (`meter`).
- Its roster has six other forms (Shin Gouki and the like); a tag to one of those is a choice of the menu's rows.
- Its tag call: the same trace of each character's entrance or win sounds.
- What Alpha 2 lacks that Vampire Savior had: no "no entrance" bit is known yet, no damage gauges (`oneBarEach` may be
  the health's most, or nothing), and its knock-down rules differ (find the reeling's kinds as above: trace the byte
  the reeling branches on over many hits and see which land the player down).

## Not done

- The CPU's tags are the toolkit's coach's (`fight/battle.cpp`, below FIGHT.md's "The computer's play of the modes"),
  from the game's health and the fighters' states, not a decision in the game's own computer player.
- The one leaving and the one coming are one after the other, not on screen together (one record a side).
