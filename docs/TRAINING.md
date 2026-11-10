# Training mode

Every fighting game has a training mode (the arcade games had none). **Alternative modes** at the top of the in-game
menu has a Training mode page: Start goes straight to the character choice with two players (player 1's controls
choose for both) and into a fight with no clock that nobody loses. The aids below are switches on that page, and on
the launcher's Features page.

What each game has differs a little (throw boxes and Custom Combo time in the Alphas, the Infinity Gems at hand in
Marvel Super Heroes, a dummy that Alpha counters in Alpha 3...); each game's own page lists its own.

| Aid | What it does |
| --- | --- |
| Hit boxes | The boxes the game itself tests: where a player can be hit (blue), attacks (red; orange for things thrown), pushing boxes (green) |
| Throw range | (Alpha 2 and 3) The zone in front of each player that the other's push box has to reach for a throw |
| Frame data and frame meter | Startup, active and recovery of each move, and the advantage after a hit or a guarded hit, counted in the game's own frames; a row of cells for each player, one a frame; optionally every move written to `frame_data.csv` beside the settings |
| Input display | What each player presses and for how long, in numpad notation from the player's side |
| Combo readout | Hits and damage of a combo, and the round's best |
| Stun readout | (the Street Fighter games and Pocket Fighter) How much stun each player has taken, against how much stuns them |
| Fixed damage | (Alpha 2 and 3) No random part in a hit's damage, so a combo always does the same |
| Health and meter refills | Health comes back when a player hasn't been hit for a moment; the super gauge can be kept full |
| The dummy | Stands, crouches or jumps; guards (always, after a hit, or at random; high or low by the game's own rule for each attack); reverses; techs throws and gets up quickly after a knock-down (Alpha 2 and 3, the Vampire Savior games); or is played by the game's own computer player (Alpha 2 and 3). It **records and replays** what you show it in three slots (one, or any at random), and can answer with a recording after guarding or being hit |
| Start over | The round as it began, in the middle or a corner, sides swapped or not (Backspace, or a gamepad's left shoulder) |
| Stage | Any of the game's stages, or the game's own choice |
| Stop, step, slow | Stop the game, go on a frame at a time, or play at a half, a quarter or an eighth of the speed |

Save states (nine slots) work in training as everywhere, as do the cheats (no damage, one-hit knockouts, full gauge,
infinite time).

Super Puzzle Fighter II Turbo has a training mode of its own: an attack readout, a landing guide, an input display, a
dummy that sends counter gems to cancel, and crash gems as you choose.

| | |
| --- | --- |
| ![Training mode in Street Fighter Alpha 2: hit boxes, input display, combo readout and the frame meter](screenshots/sfa2/training.png) | ![Training mode in Vampire Savior](screenshots/vampire_savior/training.png) |
| ![Training mode in Marvel vs. Capcom](screenshots/mvsc/training.png) | ![Training mode in Super Puzzle Fighter II Turbo](screenshots/puzzle_fighter/training.png) |
