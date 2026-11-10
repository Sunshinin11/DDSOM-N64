# Fighting games: the shared training parts

`fight/` holds what a training mode needs that does not depend on the game (`fight/fight.h`); a game's sources
say, from its own memory, what the players are doing, and these keep count and say what to show (the HUD's lines
and boxes, `runtime/game.h`).

| Piece | What it does | What the game tells it |
| --- | --- | --- |
| `Frames` | Startup, active and recovery of each move; the advantage after a hit or a guarded one; a meter of the last 60 steps | Each player once a step (a `StepView`: who is in charge of it, whether it can act, is frozen by a hit, attacking, in the air, has an attack box out), and each hit landing or guarded (`Contact`) |
| `MoveLog` | Every move's numbers as lines of `frame_data.csv` beside settings.ini (character, input, air, which move, startup, active, recovery, advantage) | Each player once a step after `Frames::Step`, and a function that tells a move's start (asked only as one begins) |
| `InputHistory`, `Notation` | What each player holds and for how long, the stick as a keypad number (from the player's side, if the game says which way it faces) | What the game read off the panel each frame |
| `Combos` | The last hit, the combo (hits landing before the player is free again), what of it comes back by itself, the round's best | Each hit's damage, and whether the player was still reeling |
| `Dummy`, `RegisterDummy` | Player 2 stands, crouches, jumps, records and replays (three slots, or any at random), answers with a recording, guards (always, after a hit, at random) high or low; the game's own actions while guarding, while held, getting up | Each frame a `DummyView` (fighting, what it is doing, which side, whether an attack is near enough to guard and must be guarded standing, whether it is knocked down); the game's own actions as `DummyOptions` |
| `StageBox`, `ToPicture` | A box in the stage drawn on the picture through the camera, coloured by its kind | The boxes, read the way the game's own hit tests read them |

The features the dummy registers have the same names in every game (`dummy_stand`, `dummy_guard`, ...), so a
player's `settings.ini` reads the same across the ports.

`runtime/spare.h` holds the machines a game's modes keep as save states and run beside the port's (a stage that
scrolls into the next, the next fight made ready while the last one ends): `RunSpare`, `Promote`, `Take`, `Give`,
and `SetSpareKeeping` for what a game keeps of its own around a spare's frame.

## The Alternative modes page and the training session

Two modules hold what every fighting game's "Alternative modes" page and training mode had as copies of one another
(Vampire Savior's, Street Fighter Alpha 2's): a game fills in a description and registers it, and keeps only its own
facts and hooks.

- `fight/alternative.h` (`modes::`): the page itself and its training, attract mode and stage viewer pages; attract mode
  on a loop (the game says where its loop begins and ends and steers its parts with `GoTo`: `attractStep`); the stage
  viewer and its infinite stage on a spare machine; and what the modes share: getting a machine to a round (`ToRound`),
  a sound asked for (`PlaySound`), the match and the camera a mode wants (`Wanted`, `CameraAt`), what is kept around a
  spare's frame (the game adds its own: `keep`, `fresh`, `putBack`).
- `fight/session.h` (`session::`): the training session. Getting there from switching on, player 1's controls choosing
  for both, the round kept as its fighting begins for "Start over" (the middle or a corner, sides swapped), the stage,
  no clock and no knock-outs (`NoClock`, `NoKnockOut`, for the game's own clock and damage hooks), and the aids of
  `fight.h` put together on the HUD. The game tells it, from its own hooks, each player's step (`Step`, a `StepView`),
  a hit landing (`Contact`) and a hit's damage (`Hit`); the description says where the game keeps the rest (players,
  health, the clock, the camera and its copies, the stage's ends, the panel's words, the boxes, the guard test) and
  what the game adds of its own (its idea of fighting, a refill's conditions, a player's readout, the HUD's places, the
  dummy's extras, its own features beside the toolkit's).

Vampire Savior (and the games built from its sources), Street Fighter Alpha 2, Super Gem Fighter Mini Mix and Street
Fighter Alpha use both; moving the first two onto them changed none of their pictures, frame hashes, frame data logs or
feature lists.

## The Marvel games' engine

`fight/marvel.h` (`marvel::`): Capcom's Marvel games on the board share one engine (X-Men vs. Street Fighter was made
from Marvel Super Heroes: `tools/portmap.py` places much of the one's code in the other), with their players' records,
cameras, boxes and animations at other places and offsets in each. What their sources would each write the same is
here, from a description of where a game keeps it (`marvel::Engine`, filled in by the game's `src/engine.cpp`): the
session's view of a player at its step (frame data, a hit landing) and the boxes as the game's hit tests read them,
the ring out's own movements, widescreen and the invisible walls (the camera kept between its ends and a player kept
inside the picture, with the wide picture's margin), and the model viewer (a round reached unseen, the second player
out of the way, the camera after the first, the character's animations as the game keeps them, a frame copied into
the record by the game's own layout). A game gives the places of the hooks these make as `HookAt(0x...)` in its own
sources, so that the translator finds them. The modes' and the session's descriptions stay in the game's sources
and name these functions. X-Men vs. Street Fighter and Marvel Super Heroes use it; moving the first onto it changed
none of its hash logs (the ladder, training, widescreen, the walls, the model viewer, the fighting modes, attract
mode, the stage viewer) or its feature list.

## Tag team for four people

`fight/tag.h` (`tag::`): in a tag-team game, each character of a team played by a person of its own ("Co-op vs.
computer": players 1 and 2 against the computer; "2 vs 2": 1 and 3 against 2 and 4), with no patched ROM. The board
reads only players 1 and 2, so it is an input router run first in the game's `beforeFrame`: choosing, a side's panel
is the person whose pick it is; in a match, the person whose character is on point, by the game's own byte for it, with
the partner's tag buttons (HP+HK) passed while a round is fought; elsewhere the first person's, with the partner's
Start and coins. Players 3 and 4 never reach the board (a six-button game reads the kicks on their lines). Its `Frame`
puts a P1-P4 label under each side's bars. A game describes where the point character and the choosing are
(`tag::Description`), and when a mode of its own works the controls instead. X-Men vs. Street Fighter is the first.

## The fighting modes

`fight/battle.h`: survival, the endless battle, the ring out and the team battle, for a game's "Alternative modes"
page. A game fills
in a `battle::Description` (its result byte, camera, players' and thrown things' records, health, characters and
stages, and hooks for its own: getting a machine to a round, a sound, what is wanted of the next match, which
objects are the stage's, the panel's and the fighters') and calls `battle::Register`; the menu's pages, commands and
features are the same in every game.

The team battle (`Description::teams`; the Vampire Savior games first) is survival on both sides: each knock-down
ends a match, as in Vampire Savior, which has no rounds, and the next is made ready on the spare machine with the
loser's next fighter and the winner's health put back. A game can give each fighter one health bar, whatever its
board says (`Description::oneBarEach`, run before each frame of the team battle's machines: Vampire Savior's puts
its "damage gauges" setting at one), so that a team of two has the bars one fighter usually has. With `battle::RegisterTeamTag` the tag-team router below follows
whose fighter is on, and puts its labels beside the battle's own lines.

Its tag style (`Description::canTag`, `tagIn`, `meter`) swaps a side's fighter in the middle of a match: the toolkit
keeps each fighter's health and ceiling while it rests (the ceiling's health coming back to it), swaps on Tag (`kTag`,
a gamepad's left trigger while the game runs, never on the board's lines) or Start (or the partner's Tag, through the
router: `tagButtons` is kTag there) when the game says the fighter can act, swaps in the
next when the bar on has emptied (the game's sources ask `TeamHoldsKnockOut` where health is taken, so that it stops at
nothing while a partner is left), puts the health and the gauges' bytes back after the game's frame has set the
newcomer up, and decides a time over by the teams' health (`TeamTimeOverResult`). With `offStage` (and `speedX`,
`speedY`, `floor`) a tag moves as in the Marvel games, worked by the toolkit through the side's controls and speeds:
the one on jumps back (the game's jump) and is sped off the picture, let out of it and not followed by the camera;
the newcomer, set up off the picture at that end, jumps in (its speed across stretched to land by the other player)
and presses a jump attack on the way down, the game's own, which can hit. A fighter whose bar emptied is knocked down
by the game (its sources make the hit a knock-down that lands lying) and lies two seconds, not to be hit
(`untouchable`, asked every frame) and the game not playing it, and the next one jumps in. The one swapped is not
drawn until it is off the picture (the game sets it up where a round begins), by its colour sets (`playerSets`), and
the ones leaving and coming are moved by place, not by their speed (a character's jump may be a move of its own). How a fighter is swapped is the
game's: in Vampire Savior, its own setting up of a player run again with the new character, after the objects the
outgoing one made are given back to the game's pools (`src/livetag.cpp`). What another game needs for it, and how Vampire Savior's
facts were found: [TAG_TEAM.md](TAG_TEAM.md).

The ring out needs two things of the game's player code, at each player's step of a fight (`RingOutMove`):

- **Flung out** (`kFlungOut`, then `kFalling`): the one knocked past where the game would keep it in
  (`RingOutLetsOut`, asked where the game keeps the players in) is put in the game's knock-down in the air, with
  the game's own animation of being thrown back, speeds up and away, and from then on only flies and animates
  (the game's routine for it not run), so it falls through the floor and out of the picture.
- **Dropping in** (`kDropsIn`): the winner jumps off the old stage; at the top of its jump its record there is handed
  to the next match's machine, where the game puts it in the air as it was (its frame, speeds, steps), at the same
  place in the picture, a picture's height above the floor. That machine's camera is held up by as much
  (`CameraWanted::beyondEnds`) and comes down as the winner falls, the old stage's picture going up and the next
  one's coming in below it, the fighters over both, until the game's own jump lands it.

**The computer's play of the modes.** The game's computer player fights every fight as a fight: it knows nothing of a
tag (the toolkit's, not the game's), nor that in a ring out only the stage's ends count. A side the game plays is
coached in these, as a person would play them (`battle.cpp`, the coach). In the tag style the coach says when the side
tags and whom it tags in: when the one on is low and a partner has more, or a partner who has rested has much more;
best while the other fighter is down (reeling 20 frames or more), far off or in a tag of its own; not soon after its
last tag, and never just out of a hit. In a ring out it takes the side's controls for a moment, the game's play of it
off (as the endless battle's person does), and gives them back once the fighter can act again: out from near its own
end (the losers' last free moment, in Alpha 2's: half within 30 pixels of their end, nine in ten within 145) with a
throw over its head or a jump over the other, a walk out of the end's way; with the other's back to its end, a throw
that sends it on or a sweep; and walks and jumps to the better side. Never into an attack coming (the game's own guard
test, `Description::attackComing`): the game's play guards it. And a game's sources ask `RingOutMayWalkBack` where the
computer takes the way it walks (Alpha 2's Walking rewrite, Vampire Savior's hook on its walk), so that a coached
fighter does not walk back towards the end it is near. A game whose computer player is readable code with tactics of
the port's for the modes (`Description::computerPlaysModes`: the Vampire Savior games, Alpha 2 and Alpha 3, each in its
`src/computer.cpp`) plays them itself instead, its own guard on all the while: the coach stands aside, and the
computer is told the mode it is in, how far its back and the other's are from their ends, when its fighter wants to
tag and how well it plays (`ComputerModeOf`, `ComputerBack`, `ComputerTagWanted`, `ComputerSkill`), and asks for a
tag, taken once its fighter can (`ComputerAsksTag`); `RingOutMayWalkBack` is still asked. The difficulty (the team
battle's own, or the game's:
`difficultyNow`) says how soon it sees a chance and how often it takes one. A game gives `canAct` (or `canTag`) for any
of it; for tests, `CPS2RECOMP_BATTLE_CPU=1` has the game play side 1 too (and begins a mode again when it is over),
`CPS2RECOMP_COACH` and `CPS2RECOMP_COACH_PARTS` choose what is coached, and `CPS2RECOMP_BATTLE_LOG` writes each tag,
plan and ring out to stderr.

**The Multi game page's endless battle.** The collection's launcher has a page that plays several games one after
another (recomp-common's `Collection::multi`; `launcher/src/description.cpp` makes its Endless battle from the games
whose `game.cfg` says `multi = endless`). It starts each game with `--multi endless:shuffle:SECONDS` or
`endless:scroll:SPEED` (`runtime/game.h`, `MultiGame`): the battle begins the endless battle at the game's first frame,
on stages that fade or scroll, with every stage in turn (shuffled for the fade, in order for the scroll) and none
twice; once the last has been fought on for its time, or scrolled across, the picture and sound fade out and the
game ends the program with exit code 3 (`kNextGame`, `EndProgram`), which the launcher takes as the next game's
turn. A start or a next match that never comes to its round ends it the same way, so that one game's trouble does not
stop the run. The fighters are as the game's own Endless battle page has them. The player can ask for the next game (N, a
gamepad's left shoulder: `AskNextGame`): the battle starts the same fade-out at once, and if the game is not far
enough on for that (still starting), the port ends it itself 150 ticks of the screen later (stopped too). F or the left trigger held runs four
frames a picture, unheard (`Port::hurry`).

Every match and stage machine after the first is begun from a machine kept at the character choice with both players
in (the first spare that came there), so the next one is ready in about 500 frames, not 2,000 from switching on.

Street Fighter Alpha 2 makes them in its PlayerFighting rewrite; the Vampire Savior games in their player step hook
(02244C); Super Gem Fighter Mini Mix and Street Fighter Alpha in theirs (00C642; 01CE48 and 021DF4).

The endless battle and the ring out lay the fighters' machine's objects over a stage another machine shows. A game whose
panel (the bars, names and clock) is a scroll screen and not objects says which (`panelLayers`, Video::layerMask's bits):
the fighters' machine then draws that screen too and marks its pixels with the objects' (Video::markLayers), and the
stage's machines leave it out. Street Fighter Alpha's panel is its first scroll screen.

## Custom colours and the colour editor

`fight/colours.h` gives a game the players' own colours for its characters and an editor to make them, as
Vampire Savior had them first. A game registers a `colours::Description` (in its `RegisterGame`) and calls
`colours::BeforeFrame` before its viewer's own work each frame; its model viewer calls `EditorBeforeFrame`
(after its round is on, before it shows its animation), `EditorFrame` (after its picture is made),
`EditorShift` (the character drawn that far left) and `EditorAnimation` (the choice to use for "Animation"
while the editor is on), and `EndEditor` when its own Start is chosen.

| The game tells | What for |
| --- | --- |
| `characters` (number, name, the name abroad), `setNames`, `palettesInSet`, `lineNames` | The files' names and sections, the editor's rows |
| `setAddress`, `ranges` | Where a set is in the ROM image (`Program`), and every place of the characters' colours (sets and effects) |
| `romFile`, `romFileAt`, `romFileSize`, `same`, `romFileSwapped`, `romFileWhose` | The ROM file PalMod changes: only its words inside `ranges` are taken (the rest are counted and left), and the file is refused unless `same` is the game's |
| `featureDescription`, `originalHelp` | The feature's words in the menu, and the remark at the head of each file "Write the game's colours" writes |
| `showSet` | The editor's preview: player 1 made to use the set, and where its palettes are in the buffer the board shows from (each frame the set is written there, the brightness kept) |
| `startViewer`, `viewerOn`, `viewerCharacter`, `characterNumber`, `animationStep`, `animationNames`, `backgrounds` | The model viewer the editor works in; it uses the viewer's `model_character`, `model_background` and `model_size` choices |
| `animationForLine` (optional) | The animation that best shows a line of the set, for the Line row to go to (0 standing, -1 none, -2 not known yet: asked again the next frame). Vampire Savior finds them by showing every frame of every animation on a spare machine, a few a frame, and counting the objects in each of the player's palettes |

The files, the same in every game: `palettes/<Character>.txt` (a `[SET]` section a set, up to a line a palette
of 16 `#RRGGBB` or `#RGB` colours, `-` for the game's own; a `#` with no hex digit after it starts a remark),
`palettes/original/` (the game's own, written by the command), `palettes/library/<Character>/<Name N>.txt` (one
set a file). The editor saves a character's file 45 frames after the last change and when it ends, with only
the sets and lines that are not the game's.
