# Every game's songs

The names of the files [a custom soundtrack](SOUNDTRACK.md) plays in place of particular songs: name a file as the
first column (`ryu.flac`, `player_select.ogg`) or the number, the second, which is hexadecimal (`12.mp3`,
`0x12.mp3`), with `.ogg`, `.flac`, `.mp3` or `.wav`. A song played once (a fanfare, the versus screen) is played once
whatever the file. Each game also writes this list, as `songs.txt`, into its soundtrack folder, with the music it has
no name for yet by number.

<!-- Made by tools/songs.py from each game's --list-songs: change games/<id>/src/music.cpp, not this. -->

- [Alien vs. Predator](#alien-vs-predator) (23)
- [Cadillacs and Dinosaurs](#cadillacs-and-dinosaurs) (28)
- [Darkstalkers](#darkstalkers) (22)
- [Marvel Super Heroes](#marvel-super-heroes) (22)
- [Marvel Super Heroes vs. Street Fighter](#marvel-super-heroes-vs-street-fighter) (29)
- [Marvel vs. Capcom](#marvel-vs-capcom) (28)
- [Night Warriors](#night-warriors) (24)
- [Street Fighter Alpha](#street-fighter-alpha) (37)
- [Street Fighter Alpha 2](#street-fighter-alpha-2) (59)
- [Street Fighter Alpha 3](#street-fighter-alpha-3) (44)
- [Street Fighter Zero 2 Alpha](#street-fighter-zero-2-alpha) (59)
- [Super Gem Fighter Mini Mix](#super-gem-fighter-mini-mix) (21)
- [Super Puzzle Fighter II Turbo](#super-puzzle-fighter-ii-turbo) (24)
- [Super Street Fighter II Turbo](#super-street-fighter-ii-turbo) (42)
- [The Punisher](#the-punisher) (22)
- [Vampire Hunter 2](#vampire-hunter-2) (61)
- [Vampire Savior](#vampire-savior) (78)
- [Vampire Savior 2](#vampire-savior-2) (48)
- [Vampire Savior Definitive](#vampire-savior-definitive) (78)
- [X-Men vs. Street Fighter](#x-men-vs-street-fighter) (27)
- [X-Men: Children of the Atom](#x-men-children-of-the-atom) (18)

## Alien vs. Predator

| File name | Number | What it is |
|---|---|---|
| `story` | `1A` | The story (attract mode, and before a game) |
| `player_select` | `05` | Player select |
| `round_1` | `07` | Round 1: City of Despair (and round 7) |
| `round_2_start` | `16` | Round 2's title and first words |
| `round_2` | `06` | Round 2: War in the Underpass |
| `round_2_later` | `09` | Round 2, further on (and round 4) |
| `round_3_start` | `15` | Before round 3 (and round 5): the radio's words |
| `round_3` | `08` | Round 3 (and round 5) |
| `round_4` | `0A` | Round 4: the hive |
| `round_6_start` | `17` | Round 6's title: Nightmare in the Lab |
| `round_6` | `0B` | Round 6 |
| `special_stage` | `1C` | Special stage |
| `round_7_start` | `18` | Round 7's title: Hunt's End |
| `boss_appears` | `1F` | A boss appears (its name shown) |
| `boss` | `0F` | Boss fight (Chrysalis, round 1; Stalker, round 3) |
| `boss_2` | `10` | Boss fight (Razor Claws, round 2) |
| `boss_3` | `11` | Boss fight (round 5's drop ship) |
| `results` | `04` | A round's results (played once) |
| `continue` | `01` | Continue? |
| `qsound` | `19` | The QSound notice (played once) |
| `capcom` | `20` | Capcom's notice (played once) |
| `fanfare_1b` | `1B` | A long piece the board plays once (where is not known yet: an ending?) (played once) |
| `fanfare_03` | `03` | A short fanfare (where it plays is not known yet) (played once) |

## Cadillacs and Dinosaurs

| File name | Number | What it is |
|---|---|---|
| `story` | `24` | Attract mode: the story, first part |
| `story_2` | `25` | Attract mode: the story, second part (played once) |
| `episode` | `00` | An episode's title and map |
| `episode_1_start` | `01` | Episode 1's opening words |
| `rooftop` | `02` | Episode 1: the top of the building |
| `mansion` | `03` | Episode 1: the mansion (and episode 2's sawmill, episode 6's start) |
| `street` | `04` | Episode 1: 47th Street (and episode 7) |
| `jungle` | `05` | Episode 2: the jungle (and episode 6) |
| `swamp` | `06` | Episode 2: the mud swamp |
| `waste_land` | `07` | Episode 3: the waste land |
| `harbour` | `09` | Episode 4's start (and episode 8's underground) |
| `indoors` | `0A` | Episode 4: indoors |
| `episode_4_later` | `0B` | Episode 4, further on |
| `grassland` | `17` | Episode 5: the grassland |
| `laboratory` | `0C` | Episode 8: the laboratory |
| `boss_words` | `20` | A boss's words before its fight |
| `boss` | `0E` | Boss fight (Vice T., episode 1) |
| `boss_2` | `0F` | Boss fight (heard in episode 2) |
| `boss_3` | `10` | Boss fight (Hogg, episode 3) |
| `boss_4` | `0D` | Boss fight (heard against Blade) |
| `results` | `18` | An episode's results (played once) |
| `drive_on` | `13` | On to the next episode (and attract mode's start) |
| `continue` | `1B` | Continue? |
| `fanfare_14` | `14` | A short fanfare (where it plays is not known yet) (played once) |
| `fanfare_1d` | `1D` | A short fanfare (where it plays is not known yet) (played once) |
| `fanfare_1e` | `1E` | A short fanfare (where it plays is not known yet) (played once) |
| `fanfare_22` | `22` | A piece the board plays once (where is not known yet) (played once) |
| `fanfare_26` | `26` | A short fanfare (where it plays is not known yet) (played once) |

## Darkstalkers

| File name | Number | What it is |
|---|---|---|
| `demitri` | `08` | Demitri's stage |
| `jon_talbain` | `09` | Jon Talbain's stage |
| `victor` | `0A` | Victor's stage |
| `lord_raptor` | `0B` | Lord Raptor's stage |
| `morrigan` | `0C` | Morrigan's stage |
| `anakaris` | `0D` | Anakaris's stage |
| `felicia` | `0E` | Felicia's stage |
| `bishamon` | `0F` | Bishamon's stage |
| `rikuo` | `10` | Rikuo's stage |
| `sasquatch` | `11` | Sasquatch's stage |
| `huitzil` | `12` | Huitzil's stage |
| `pyron` | `13` | Pyron's stage |
| `introduction` | `00` | The introduction and the title |
| `qsound` | `2B` | The QSound notice (played once) |
| `select` | `01` | Character select |
| `map` | `03` | The map, before the next opponent (played once) |
| `versus` | `02` | Versus (played once) |
| `winner` | `15` | Winner (played once) |
| `ranking` | `05` | The ranking (played once) |
| `ending_start` | `21` | After the last fight, before the ending |
| `ending` | `2A` | Ending (Jon Talbain's) |
| `after_ending` | `07` | After the ending |

## Marvel Super Heroes

| File name | Number | What it is |
|---|---|---|
| `spiderman` | `09` | Spider-Man's theme |
| `captain_america` | `0A` | Captain America's theme |
| `hulk` | `0B` | Hulk's theme |
| `iron_man` | `0C` | Iron Man's theme |
| `wolverine` | `0D` | Wolverine's theme |
| `psylocke` | `0E` | Psylocke's theme |
| `blackheart` | `0F` | Blackheart's theme |
| `shuma_gorath` | `10` | Shuma-Gorath's theme |
| `juggernaut` | `11` | Juggernaut's theme |
| `magneto` | `12` | Magneto's theme |
| `doom` | `13` | Dr. Doom's theme |
| `thanos` | `14` | Thanos's theme |
| `qsound` | `2F` | The QSound logo (played once) |
| `rankings` | `07` | The rankings |
| `opening` | `01` | The opening |
| `player_select` | `02` | Player select |
| `next_opponent` | `08` | The next opponent (played once) |
| `versus` | `04` | Before each fight (played once) |
| `winner` | `1E` | Winner |
| `before_thanos` | `2B` | Before Thanos |
| `ending` | `2E` | Ending |
| `game_over` | `06` | Game over (played once) |

## Marvel Super Heroes vs. Street Fighter

| File name | Number | What it is |
|---|---|---|
| `apocalypse` | `0C` | Apocalypse's theme |
| `cyclops` | `0D` | Cyclops's theme |
| `captain_america` | `0E` | Captain America's theme |
| `hulk` | `0F` | Hulk's theme |
| `wolverine` | `10` | Wolverine's theme |
| `shuma_gorath` | `11` | Shuma-Gorath's theme |
| `omega_red` | `12` | Omega Red's theme |
| `spiderman` | `13` | Spider-Man's theme |
| `blackheart` | `14` | Blackheart's theme |
| `ryu` | `15` | Ryu's theme |
| `ken` | `16` | Ken's theme |
| `chunli` | `17` | Chun-Li's theme |
| `dhalsim` | `18` | Dhalsim's theme |
| `zangief` | `19` | Zangief's theme |
| `bison` | `1A` | M. Bison's theme |
| `akuma` | `1B` | Akuma's theme |
| `sakura` | `1C` | Sakura's theme |
| `dan` | `1E` | Dan's theme |
| `cyber_akuma` | `1F` | Cyber-Akuma's theme |
| `qsound` | `07` | The QSound logo (played once) |
| `opening` | `01` | The opening |
| `player_select` | `02` | Player select |
| `next_stage` | `04` | Before each stage |
| `winner` | `05` | Winner (played once) |
| `warning` | `0B` | Before the last fights |
| `last_win` | `32` | After the last fight (played once) |
| `ending` | `24` | Ending |
| `staff_roll` | `20` | Staff roll |
| `game_over` | `0A` | Game over |

## Marvel vs. Capcom

| File name | Number | What it is |
|---|---|---|
| `onslaught` | `0C` | Onslaught's theme |
| `war_machine` | `0D` | War Machine's theme |
| `captain_america` | `0E` | Captain America's theme |
| `hulk` | `0F` | Hulk's theme |
| `wolverine` | `10` | Wolverine's theme |
| `gambit` | `11` | Gambit's theme |
| `venom` | `12` | Venom's theme |
| `spiderman` | `13` | Spider-Man's theme |
| `roll` | `14` | Roll's theme |
| `ryu` | `15` | Ryu's theme |
| `captain_commando` | `16` | Captain Commando's theme |
| `chunli` | `17` | Chun-Li's theme |
| `jin` | `18` | Jin's theme |
| `zangief` | `19` | Zangief's theme |
| `strider` | `1A` | Strider Hiryu's theme |
| `morrigan` | `1B` | Morrigan's theme |
| `megaman` | `1C` | Mega Man's theme |
| `qsound` | `07` | The QSound logo (played once) |
| `opening` | `01` | The opening |
| `player_select` | `02` | Player select |
| `next_stage` | `04` | Before each stage |
| `winner` | `05` | Winner (played once) |
| `warning` | `0B` | Before Onslaught |
| `last_win` | `2E` | After Onslaught |
| `ending` | `20` | Ending |
| `staff_roll` | `1D` | Staff roll |
| `game_over` | `0A` | Game over |
| `rankings` | `2F` | The rankings |

## Night Warriors

| File name | Number | What it is |
|---|---|---|
| `demitri` | `08` | Demitri's stage |
| `jon_talbain` | `09` | Jon Talbain's stage |
| `victor` | `0A` | Victor's stage |
| `lord_raptor` | `0B` | Lord Raptor's stage |
| `morrigan` | `0C` | Morrigan's stage |
| `anakaris` | `0D` | Anakaris's stage |
| `felicia` | `0E` | Felicia's stage |
| `bishamon` | `0F` | Bishamon's stage |
| `rikuo` | `10` | Rikuo's stage |
| `sasquatch` | `11` | Sasquatch's stage |
| `huitzil` | `12` | Huitzil's stage |
| `pyron` | `13` | Pyron's stage |
| `hsien_ko` | `3EC` | Hsien-Ko's stage |
| `donovan` | `3ED` | Donovan's stage |
| `qsound` | `2B` | The QSound notice (played once) |
| `introduction` | `01` | The introduction and the title |
| `ranking` | `05` | The Best 5 ranking (played once) |
| `select` | `03` | Character select |
| `next_opponent` | `2F` | The next opponent chosen (played once) |
| `versus` | `02` | Versus (played once) |
| `winner` | `17` | Winner (played once) |
| `ending_start` | `23` | After the last fight, before the ending |
| `ending` | `3F5` | Ending (Lord Raptor's) |
| `staff_roll` | `2A` | Staff roll |

## Street Fighter Alpha

| File name | Number | What it is |
|---|---|---|
| `ryu` | `01` | Ryu's theme |
| `ken` | `02` | Ken's theme |
| `chunli` | `03` | Chun-Li's theme |
| `sagat` | `04` | Sagat's theme |
| `sodom` | `05` | Sodom's theme |
| `adon` | `06` | Adon's theme |
| `birdie` | `07` | Birdie's theme |
| `guy` | `08` | Guy's theme |
| `charlie` | `09` | Charlie's theme |
| `rose` | `0A` | Rose's theme |
| `akuma` | `0B` | Akuma's theme |
| `bison` | `0C` | M. Bison's theme |
| `dan` | `32` | Dan's theme |
| `qsound` | `22` | The QSound notice (played once) |
| `attract` | `0D` | The introduction (and its demonstration fights) (played once) |
| `player_select` | `0E` | Player select |
| `versus` | `0F` | Versus screen (played once) |
| `winner` | `10` | Winner (the winner's quote) (played once) |
| `challenger` | `12` | Here comes a new challenger (played once) |
| `continue` | `11` | Continue? |
| `game_over` | `13` | Game over (played once) |
| `ranking` | `24` | Score ranking (played once) |
| `ryu_ending` | `15` | Ryu's ending |
| `ken_ending` | `16` | Ken's ending |
| `chunli_ending` | `17` | Chun-Li's ending (and its staff roll) (played once) |
| `sagat_ending` | `18` | Sagat's ending |
| `sodom_ending` | `19` | Sodom's ending |
| `adon_ending` | `1A` | Adon's ending |
| `birdie_ending` | `1B` | Birdie's ending |
| `guy_ending` | `1C` | Guy's ending |
| `charlie_ending` | `1D` | Charlie's ending |
| `rose_ending` | `1E` | Rose's ending |
| `akuma_ending` | `1F` | Akuma's ending |
| `bison_ending` | `20` | M. Bison's ending |
| `dan_ending` | `33` | Dan's ending |
| `staff_roll_a` | `21` | Staff roll (after Ryu's, Akuma's, Charlie's, Guy's, Rose's, M. Bison's and Sagat's endings) |
| `staff_roll_b` | `31` | Staff roll (after Ken's, Adon's, Sodom's, Birdie's and Dan's endings) |

## Street Fighter Alpha 2

| File name | Number | What it is |
|---|---|---|
| `ryu` | `01` | Ryu's theme |
| `ken` | `02` | Ken's theme |
| `chunli` | `03` | Chun-Li's theme |
| `sagat` | `04` | Sagat's theme |
| `sodom` | `05` | Sodom's theme |
| `adon` | `06` | Adon's theme |
| `birdie` | `07` | Birdie's theme |
| `guy` | `08` | Guy's theme |
| `charlie` | `09` | Charlie's theme |
| `rose` | `0A` | Rose's theme |
| `akuma` | `0B` | Akuma's theme |
| `bison` | `0C` | M. Bison's theme |
| `dan` | `0D` | Dan's theme |
| `dhalsim` | `0E` | Dhalsim's theme |
| `zangief` | `0F` | Zangief's theme |
| `gen` | `10` | Gen's theme |
| `rolento` | `11` | Rolento's theme |
| `sakura` | `12` | Sakura's theme |
| `title` | `3E` | Title (with the QSound logo) (played once) |
| `attract` | `13` | The attract mode's introduction of the fighters (played once) |
| `player_select` | `14` | Player select |
| `versus` | `15` | Versus screen (played once) |
| `winner` | `16` | Winner (the winner's quote) (played once) |
| `continue` | `17` | Continue? |
| `challenger` | `18` | Here comes a new challenger (played once) |
| `game_over` | `19` | Game over (played once) |
| `fanfare_1a` | `1A` | A short fanfare (where it plays is not known yet) (played once) |
| `ranking` | `1B` | Score ranking (played once) |
| `rival` | `34` | Before the last fight (the rival's words) |
| `sagat_final` | `35` | Sagat's last fight |
| `ryu_ending` | `1C` | Ryu's ending (played once) |
| `ryu_ending_2` | `1D` | Ryu's ending, second part |
| `ken_ending` | `1E` | Ken's ending |
| `chunli_ending` | `1F` | Chun-Li's ending |
| `sagat_ending` | `3A` | Sagat's ending |
| `sagat_ending_2` | `20` | Sagat's ending, second part (played once) |
| `sodom_ending` | `21` | Sodom's ending |
| `adon_ending` | `22` | Adon's ending |
| `adon_ending_2` | `23` | Adon's ending, second part |
| `birdie_ending` | `24` | Birdie's ending |
| `guy_ending` | `25` | Guy's ending (played once) |
| `charlie_ending` | `26` | Charlie's ending |
| `charlie_ending_2` | `36` | Charlie's ending, second part |
| `rose_ending` | `27` | Rose's ending (played once) |
| `akuma_ending` | `28` | Akuma's ending |
| `akuma_ending_2` | `37` | Akuma's ending, second part |
| `bison_ending` | `29` | M. Bison's ending |
| `dan_ending` | `2A` | Dan's ending (played once) |
| `dhalsim_ending` | `2B` | Dhalsim's ending |
| `zangief_ending` | `2C` | Zangief's ending |
| `zangief_ending_2` | `38` | Zangief's ending, second part |
| `gen_ending` | `2D` | Gen's ending |
| `gen_ending_2` | `2E` | Gen's ending, second part |
| `rolento_ending` | `2F` | Rolento's ending |
| `sakura_ending` | `30` | Sakura's ending |
| `sakura_ending_2` | `31` | Sakura's ending, second part |
| `staff_roll_a` | `32` | Staff roll (after Ken's, Sodom's and Guy's endings) |
| `staff_roll_b` | `33` | Staff roll (after Chun-Li's, Charlie's, Adon's, Rose's, M. Bison's, Sagat's, Rolento's and Gen's) |
| `staff_roll_c` | `39` | Staff roll (after Ryu's, Birdie's, Dan's, Sakura's, Dhalsim's and Zangief's) |

## Street Fighter Alpha 3

| File name | Number | What it is |
|---|---|---|
| `ryu` | `09` | Ryu's theme |
| `ken` | `0A` | Ken's theme |
| `akuma` | `0B` | Akuma's theme |
| `charlie` | `0C` | Charlie's theme |
| `chunli` | `0D` | Chun-Li's theme |
| `adon` | `0E` | Adon's theme |
| `sodom` | `0F` | Sodom's theme |
| `guy` | `10` | Guy's theme |
| `birdie` | `11` | Birdie's theme |
| `rose` | `12` | Rose's theme |
| `bison` | `13` | M. Bison's theme |
| `sagat` | `14` | Sagat's theme |
| `dan` | `15` | Dan's theme |
| `sakura` | `16` | Sakura's theme |
| `rolento` | `17` | Rolento's theme |
| `dhalsim` | `18` | Dhalsim's theme |
| `zangief` | `19` | Zangief's theme |
| `gen` | `1A` | Gen's theme |
| `balrog` | `1E` | Balrog's theme |
| `cammy` | `1F` | Cammy's theme |
| `honda` | `21` | E. Honda's theme |
| `blanka` | `22` | Blanka's theme |
| `mika` | `23` | R. Mika's theme |
| `cody` | `24` | Cody's theme |
| `vega` | `25` | Vega's theme |
| `karin` | `26` | Karin's theme |
| `juli_juni` | `27` | Juli's and Juni's theme |
| `sodom_other` | `1D` | Sodom's theme, another version (003A54: for one of his kinds) |
| `rival_final_stage` | `29` | The rival's fight on the final stage (the ninth) |
| `ninth_akuma_stage` | `1B` | The ninth fight when it is on Akuma's stage |
| `tenth_ryu` | `28` | The tenth fight when it is against Ryu |
| `before_final` | `2C` | Before the final fight (M. Bison's words) |
| `final` | `20` | The final fight (M. Bison) |
| `notices` | `2F` | The notices at the start of the show (played once) |
| `attract` | `01` | The attract mode's introduction, to the title (played once) |
| `player_select` | `02` | Player select |
| `introduction` | `08` | A character's introduction before a match |
| `rival` | `2B` | A rival's words before a match |
| `winner` | `04` | Winner (the winner's quote) |
| `challenger` | `03` | Here comes a new challenger (played once) |
| `continue` | `05` | Continue? |
| `game_over` | `06` | Game over (played once) |
| `ranking` | `07` | Score ranking |
| `ending` | `2D` | The ending and the staff roll |

## Street Fighter Zero 2 Alpha

The same songs as [Street Fighter Alpha 2](#street-fighter-alpha-2).

## Super Gem Fighter Mini Mix

| File name | Number | What it is |
|---|---|---|
| `felicia` | `01` | Felicia's stage (the shop) |
| `ibuki` | `02` | Ibuki's stage (the big screen) |
| `hsienko` | `03` | Hsien-Ko's stage (the snowy village) |
| `chunli_sakura` | `04` | Chun-Li's and Sakura's stage (the elephant) |
| `morrigan_dan` | `05` | Morrigan's and Dan's stage (Chinatown) |
| `ryu` | `06` | Ryu's stage (the forge) |
| `tessa_akuma` | `07` | Tessa's and Akuma's stage (the castle) |
| `ken_zangief` | `08` | Ken's and Zangief's stage (the hall) |
| `qsound` | `2E` | The QSound logo (played once) |
| `capcom` | `09` | The maker's logo (played once) |
| `attract` | `0A` | The attract mode's introduction |
| `player_select` | `0B` | Player select |
| `story` | `12` | The story before the first fight |
| `versus` | `0C` | Versus (the next opponent, one player) |
| `versus_two` | `0F` | Versus (two players) |
| `winner` | `0E` | Winner (played once) |
| `last_fight` | `10` | Before the last fight |
| `ending` | `22` | Ending |
| `staff_roll` | `2D` | Staff roll |
| `game_over` | `1E` | Game over (the name entry) |
| `ranking` | `1F` | Score ranking |

## Super Puzzle Fighter II Turbo

| File name | Number | What it is |
|---|---|---|
| `morrigan` | `01` | Morrigan's theme |
| `chunli` | `02` | Chun-Li's theme |
| `ryu` | `03` | Ryu's theme |
| `ken` | `04` | Ken's theme |
| `hsienko` | `05` | Hsien-Ko's theme |
| `donovan` | `06` | Donovan's theme |
| `felicia` | `07` | Felicia's theme |
| `sakura` | `08` | Sakura's theme |
| `devilot` | `09` | Devilot's theme |
| `akuma` | `0A` | Akuma's theme |
| `dan` | `0B` | Dan's theme |
| `qsound` | `1E` | The QSound logo (played once) |
| `title` | `0C` | Title (played once) |
| `dojo` | `0D` | The Karate Puzzle Dojo (how to play) |
| `select` | `0E` | Mode and character select |
| `logo` | `0F` | The game's logo before each match (played once) |
| `winner` | `10` | Winner (played once) |
| `intermission` | `11` | Intermission |
| `continue` | `12` | Continue |
| `challenger` | `13` | Here comes a new challenger (played once) |
| `game_over` | `14` | Game over (played once) |
| `ranking` | `15` | Score ranking (played once) |
| `best_player` | `16` | Score ranking: the best player (played once) |
| `staff_roll` | `17` | Ending and staff roll |

## Super Street Fighter II Turbo

| File name | Number | What it is |
|---|---|---|
| `ryu` | `02` | Ryu's stage |
| `honda` | `03` | E. Honda's stage |
| `blanka` | `05` | Blanka's stage |
| `guile` | `07` | Guile's stage |
| `ken` | `01` | Ken's stage |
| `chunli` | `04` | Chun-Li's stage |
| `zangief` | `06` | Zangief's stage |
| `dhalsim` | `08` | Dhalsim's stage |
| `bison` | `0C` | M. Bison's stage |
| `sagat` | `0B` | Sagat's stage |
| `balrog` | `09` | Balrog's stage |
| `vega` | `0A` | Vega's stage |
| `cammy` | `0E` | Cammy's stage |
| `thawk` | `0F` | T. Hawk's stage |
| `feilong` | `0D` | Fei Long's stage |
| `deejay` | `10` | Dee Jay's stage |
| `ryu_later` | `12` | Ryu's stage, faster (a fighter's health low) |
| `honda_later` | `13` | E. Honda's stage, faster (a fighter's health low) |
| `blanka_later` | `15` | Blanka's stage, faster (a fighter's health low) |
| `guile_later` | `17` | Guile's stage, faster (a fighter's health low) |
| `ken_later` | `11` | Ken's stage, faster (a fighter's health low) |
| `chunli_later` | `14` | Chun-Li's stage, faster (a fighter's health low) |
| `zangief_later` | `16` | Zangief's stage, faster (a fighter's health low) |
| `dhalsim_later` | `18` | Dhalsim's stage, faster (a fighter's health low) |
| `bison_later` | `1C` | M. Bison's stage, faster (a fighter's health low) |
| `sagat_later` | `1B` | Sagat's stage, faster (a fighter's health low) |
| `balrog_later` | `19` | Balrog's stage, faster (a fighter's health low) |
| `vega_later` | `1A` | Vega's stage, faster (a fighter's health low) |
| `cammy_later` | `1E` | Cammy's stage, faster (a fighter's health low) |
| `thawk_later` | `1F` | T. Hawk's stage, faster (a fighter's health low) |
| `feilong_later` | `1D` | Fei Long's stage, faster (a fighter's health low) |
| `deejay_later` | `20` | Dee Jay's stage, faster (a fighter's health low) |
| `final_fight` | `D5` | The final fight |
| `final_fight_later` | `D6` | The final fight, faster (a fighter's health low) |
| `title` | `D4` | The introduction and the title (played once) |
| `qsound` | `3D` | The QSound notice (played once) |
| `player_select` | `D3` | Player select |
| `versus` | `35` | Versus screen (played once) |
| `winner` | `36` | Winner (the winner's quote) (played once) |
| `ranking` | `3A` | Score ranking (played once) |
| `ryu_ending` | `23` | Ryu's ending |
| `staff_roll` | `3C` | Staff roll |

## The Punisher

| File name | Number | What it is |
|---|---|---|
| `story` | `04` | Attract mode: the story (and stage 5) |
| `players` | `A1` | After Start: the players' screen (the Punisher and Nick Fury) |
| `stage_1` | `00` | Stage 1: Back to the War (and stage 5's river) |
| `stage_2_title` | `08` | Stage 2's title |
| `stage_2` | `01` | Stage 2: Raid on Pantaberde |
| `stage_3_title` | `09` | Stage 3's title |
| `stage_3` | `02` | Stage 3: Waterfront Warfare |
| `bonus` | `1A` | The bonus round (breaking barrels, before stage 4) |
| `stage_4_title` | `0A` | Stage 4's title |
| `stage_4` | `03` | Stage 4: Death on Rails |
| `stage_5_title` | `0B` | Stage 5's title |
| `stage_6_title` | `0C` | Stage 6's title |
| `stage_6` | `05` | Stage 6: Final Punishment |
| `stage_6_hall` | `A4` | Stage 6: the hall, further on |
| `stage_6_machines` | `06` | Stage 6: the machine room, further on |
| `boss_entrance` | `1B` | A boss's entrance (stage 1) |
| `boss_entrance_2` | `18` | A boss's entrance (stage 2) |
| `boss` | `0D` | Boss fight (stages 1 and 3) |
| `boss_2` | `0E` | Boss fight (stages 2 and 4) |
| `stage_clear` | `10` | A stage cleared (played once) |
| `continue` | `A2` | Continue? |
| `game_over` | `13` | Game over (played once) |

## Vampire Hunter 2

| File name | Number | What it is |
|---|---|---|
| `opening` | `01` | The opening (played once) |
| `character_select` | `02` | Character select |
| `next` | `03` | Between a win and the next versus screen |
| `versus` | `05` | Versus screen (played once) |
| `continue` | `06` | Continue? |
| `game_over` | `07` | Game over (played once) |
| `ranking` | `08` | Score ranking (played once) |
| `demitri_theme` | `0A` | Demitri's theme |
| `gallon_theme` | `0B` | Gallon's theme |
| `victor_theme` | `0C` | Victor's theme |
| `zabel_theme` | `0D` | Zabel's theme |
| `morrigan_theme` | `0E` | Morrigan's theme |
| `anakaris_theme` | `0F` | Anakaris's theme |
| `felicia_theme` | `10` | Felicia's theme |
| `bishamon_theme` | `11` | Bishamon's theme |
| `aulbath_theme` | `12` | Aulbath's theme |
| `sasquatch_theme` | `13` | Sasquatch's theme |
| `leilei_theme` | `16` | Lei-Lei's theme |
| `phobos_theme` | `19` | Phobos's theme |
| `pyron_theme` | `1A` | Pyron's theme |
| `donovan_theme` | `1C` | Donovan's theme |
| `rival` | `3B` | Before the last fight (the rival) |
| `pyron_appears` | `EA` | Pyron appears (played once) |
| `ending` | `3D` | The ending |
| `ending_2` | `3E` | The ending, on (one way) |
| `ending_3` | `3F` | The ending, on (the other way) |
| `demitri_win` | `23` | Demitri wins |
| `gallon_win` | `24` | Gallon wins |
| `victor_win` | `25` | Victor wins |
| `zabel_win` | `26` | Zabel wins |
| `morrigan_win` | `27` | Morrigan wins |
| `anakaris_win` | `28` | Anakaris wins |
| `felicia_win` | `29` | Felicia wins |
| `bishamon_win` | `2A` | Bishamon wins |
| `aulbath_win` | `2B` | Aulbath wins |
| `sasquatch_win` | `2C` | Sasquatch wins |
| `leilei_win` | `2F` | Lei-Lei wins |
| `phobos_win` | `32` | Phobos wins |
| `pyron_win` | `33` | Pyron wins |
| `donovan_win` | `35` | Donovan wins |
| `music_04` | `04` | Where it plays is not known yet |
| `music_21` | `21` | Where it plays is not known yet |
| `music_36` | `36` | Where it plays is not known yet |
| `music_37` | `37` | Where it plays is not known yet |
| `music_3a` | `3A` | Where it plays is not known yet |
| `music_3c` | `3C` | Where it plays is not known yet |
| `music_40` | `40` | Where it plays is not known yet |
| `music_41` | `41` | Where it plays is not known yet |
| `music_4a` | `4A` | Where it plays is not known yet |
| `music_4b` | `4B` | Where it plays is not known yet |
| `music_4c` | `4C` | Where it plays is not known yet |
| `music_4d` | `4D` | Where it plays is not known yet |
| `music_4e` | `4E` | Where it plays is not known yet |
| `music_4f` | `4F` | Where it plays is not known yet |
| `music_50` | `50` | Where it plays is not known yet |
| `music_51` | `51` | Where it plays is not known yet |
| `music_52` | `52` | Where it plays is not known yet |
| `music_53` | `53` | Where it plays is not known yet |
| `music_59` | `59` | Where it plays is not known yet |
| `music_5a` | `5A` | Where it plays is not known yet |
| `music_d0` | `D0` | Where it plays is not known yet |

## Vampire Savior

| File name | Number | What it is |
|---|---|---|
| `feast_of_the_damned` | `09` | Feast of the Damned |
| `concrete_cave` | `0A` | Concrete Cave |
| `tower_of_arrogance` | `0B` | Tower of Arrogance |
| `red_thirst` | `0C` | Red Thirst |
| `deserted_chateau` | `0D` | Deserted Chateau |
| `abaraya` | `0E` | Abaraya |
| `vanity_paradise` | `0F` | Vanity Paradise |
| `war_agony` | `10` | War Agony |
| `forever_torment` | `11` | Forever Torment |
| `green_scream` | `12` | Green Scream |
| `iron_horse_iron_terror` | `13` | Iron Horse, Iron Terror |
| `fetus_of_god` | `14` | Fetus of God |
| `qsound` | `55` | The QSound logo (played once) |
| `capcom` | `57` | The Capcom logo (played once) |
| `opening` | `01` | The opening (played once) |
| `character_select` | `02` | Character select |
| `next` | `03` | Between a win and the next versus screen |
| `versus` | `05` | Versus screen (played once) |
| `rival` | `47` | Before the last fight (the rival) |
| `jedah_appears` | `E2` | Jedah appears (played once) |
| `continue` | `06` | Continue? |
| `game_over` | `07` | Game over (played once) |
| `ranking` | `08` | Score ranking (played once) |
| `staff_roll` | `45` | Staff roll |
| `bulleta_win` | `15` | Bulleta wins |
| `demitri_win` | `16` | Demitri wins |
| `gallon_win` | `17` | Gallon wins |
| `victor_win` | `18` | Victor wins |
| `zabel_win` | `19` | Zabel wins |
| `morrigan_win` | `1A` | Morrigan wins |
| `anakaris_win` | `1B` | Anakaris wins |
| `felicia_win` | `1C` | Felicia wins |
| `bishamon_win` | `1D` | Bishamon wins |
| `aulbath_win` | `1E` | Aulbath wins |
| `sasquatch_win` | `1F` | Sasquatch wins |
| `qbee_win` | `21` | Q-Bee wins |
| `leilei_win` | `22` | Lei-Lei wins |
| `lilith_win` | `23` | Lilith wins |
| `jedah_win` | `24` | Jedah wins |
| `bulleta_ending` | `25` | Bulleta's ending |
| `demitri_ending` | `26` | Demitri's ending (played once) |
| `gallon_ending` | `27` | Gallon's ending (played once) |
| `victor_ending` | `28` | Victor's ending |
| `zabel_ending` | `29` | Zabel's ending |
| `morrigan_ending` | `2A` | Morrigan's ending |
| `anakaris_ending` | `2B` | Anakaris's ending |
| `felicia_ending` | `2C` | Felicia's ending |
| `bishamon_ending` | `2D` | Bishamon's ending |
| `aulbath_ending` | `2E` | Aulbath's ending |
| `sasquatch_ending` | `2F` | Sasquatch's ending |
| `qbee_ending` | `31` | Q-Bee's ending |
| `leilei_ending` | `32` | Lei-Lei's ending |
| `lilith_ending` | `33` | Lilith's ending |
| `jedah_ending` | `34` | Jedah's ending |
| `music_04` | `04` | Where it plays is not known yet (played once) |
| `music_35` | `35` | Where it plays is not known yet |
| `music_36` | `36` | Where it plays is not known yet |
| `music_37` | `37` | Where it plays is not known yet |
| `music_38` | `38` | Where it plays is not known yet |
| `music_39` | `39` | Where it plays is not known yet |
| `music_3a` | `3A` | Where it plays is not known yet |
| `music_3b` | `3B` | Where it plays is not known yet |
| `music_3c` | `3C` | Where it plays is not known yet |
| `music_3d` | `3D` | Where it plays is not known yet |
| `music_3e` | `3E` | Where it plays is not known yet |
| `music_3f` | `3F` | Where it plays is not known yet |
| `music_42` | `42` | Where it plays is not known yet |
| `music_43` | `43` | Where it plays is not known yet |
| `music_44` | `44` | Where it plays is not known yet |
| `music_46` | `46` | Where it plays is not known yet |
| `music_4e` | `4E` | Where it plays is not known yet |
| `music_4f` | `4F` | Where it plays is not known yet |
| `music_50` | `50` | Where it plays is not known yet |
| `music_51` | `51` | Where it plays is not known yet |
| `music_52` | `52` | Where it plays is not known yet |
| `music_53` | `53` | Where it plays is not known yet |
| `music_54` | `54` | Where it plays is not known yet (played once) |
| `music_478` | `478` | Where it plays is not known yet (played once) |

## Vampire Savior 2

| File name | Number | What it is |
|---|---|---|
| `opening` | `01` | The opening (played once) |
| `character_select` | `02` | Character select |
| `next` | `03` | Between a win and the next versus screen |
| `versus` | `05` | Versus screen (played once) |
| `continue` | `06` | Continue? |
| `game_over` | `07` | Game over (played once) |
| `ranking` | `08` | Score ranking (played once) |
| `feast_of_the_damned` | `09` | Feast of the Damned |
| `concrete_cave` | `0A` | Concrete Cave |
| `tower_of_arrogance` | `0B` | Tower of Arrogance |
| `red_thirst` | `0C` | Red Thirst |
| `deserted_chateau` | `0D` | Deserted Chateau |
| `abaraya` | `0E` | Abaraya |
| `vanity_paradise` | `0F` | Vanity Paradise |
| `war_agony` | `10` | War Agony |
| `forever_torment` | `11` | Forever Torment |
| `green_scream` | `12` | Green Scream |
| `iron_horse_iron_terror` | `13` | Iron Horse, Iron Terror |
| `fetus_of_god` | `14` | Fetus of God |
| `feast_of_the_damned_empty` | `15` | Feast of the Damned, empty |
| `rival` | `30` | Before the last fight (the rival) |
| `jedah_appears` | `E2` | Jedah appears (played once) |
| `ending` | `38` | The ending |
| `ending_2` | `39` | The ending, on (one way) |
| `ending_3` | `3A` | The ending, on (the other way) |
| `bulleta_win` | `17` | Bulleta wins |
| `demitri_win` | `18` | Demitri wins |
| `victor_win` | `1A` | Victor wins |
| `zabel_win` | `1B` | Zabel wins |
| `morrigan_win` | `1C` | Morrigan wins |
| `anakaris_win` | `1D` | Anakaris wins |
| `felicia_win` | `1E` | Felicia wins |
| `bishamon_win` | `1F` | Bishamon wins |
| `qbee_win` | `23` | Q-Bee wins |
| `leilei_win` | `24` | Lei-Lei wins |
| `lilith_win` | `25` | Lilith wins |
| `jedah_win` | `26` | Jedah wins |
| `phobos_win` | `27` | Phobos wins |
| `pyron_win` | `28` | Pyron wins |
| `donovan_win` | `2A` | Donovan wins |
| `music_04` | `04` | Where it plays is not known yet |
| `music_16` | `16` | Where it plays is not known yet |
| `music_2b` | `2B` | Where it plays is not known yet |
| `music_2c` | `2C` | Where it plays is not known yet |
| `music_2f` | `2F` | Where it plays is not known yet |
| `music_31` | `31` | Where it plays is not known yet |
| `music_3b` | `3B` | Where it plays is not known yet |
| `music_3c` | `3C` | Where it plays is not known yet |

## Vampire Savior Definitive

The same songs as [Vampire Savior](#vampire-savior).

## X-Men vs. Street Fighter

| File name | Number | What it is |
|---|---|---|
| `wolverine` | `0B` | Wolverine's theme |
| `cyclops` | `0C` | Cyclops's theme |
| `storm` | `0D` | Storm's theme |
| `rogue` | `0E` | Rogue's theme |
| `gambit` | `0F` | Gambit's theme |
| `sabretooth` | `10` | Sabretooth's theme |
| `juggernaut` | `11` | Juggernaut's theme |
| `magneto` | `12` | Magneto's theme |
| `apocalypse` | `13` | Apocalypse's theme |
| `ryu` | `14` | Ryu's theme |
| `ken` | `15` | Ken's theme |
| `chunli` | `16` | Chun-Li's theme |
| `dhalsim` | `17` | Dhalsim's theme |
| `zangief` | `18` | Zangief's theme |
| `bison` | `19` | M. Bison's theme |
| `akuma` | `1A` | Akuma's theme |
| `charlie` | `1B` | Charlie's theme |
| `cammy` | `1C` | Cammy's theme |
| `qsound` | `07` | The QSound logo (played once) |
| `opening` | `01` | The opening |
| `player_select` | `02` | Player select |
| `next_stage` | `04` | Before each stage |
| `winner` | `05` | Winner (played once) |
| `warning` | `2B` | Before Apocalypse |
| `ending` | `24` | Ending |
| `staff_roll` | `1D` | Staff roll |
| `game_over` | `0A` | Game over |

## X-Men: Children of the Atom

| File name | Number | What it is |
|---|---|---|
| `wolverine` | `07` | Wolverine's theme |
| `psylocke` | `08` | Psylocke's theme |
| `colossus` | `09` | Colossus's theme |
| `cyclops` | `0A` | Cyclops's theme |
| `storm` | `0B` | Storm's theme |
| `iceman` | `0C` | Iceman's theme |
| `spiral` | `0D` | Spiral's theme |
| `silver_samurai` | `0E` | Silver Samurai's theme |
| `omega_red` | `0F` | Omega Red's theme |
| `sentinel` | `10` | Sentinel's theme |
| `juggernaut` | `11` | Juggernaut's theme |
| `magneto` | `12` | Magneto's theme |
| `winner` | `05` | Winner (played once) |
| `magneto_beaten` | `02` | Magneto beaten |
| `ending` | `17` | Ending |
| `ending_2` | `18` | Ending, continued |
| `staff_roll` | `19` | Staff roll |
| `game_over` | `04` | Game over (played once) |
