#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Writes docs/SONGS.md: every game's songs, the names a custom soundtrack's files take (docs/SOUNDTRACK.md), as each
# built game lists them (--list-songs: the game's own table, games/<id>/src/music.cpp). Run from the repository's top
# after building the games (make games); B=<folder> for builds kept elsewhere. A game not built is left out, and said.
#   tools/songs.py            write docs/SONGS.md
#   tools/songs.py --check    only say whether docs/SONGS.md is as the games list it (exit 1 if not)
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gamecfg  # noqa: E402

OUT = os.path.join(gamecfg.ROOT, "docs", "SONGS.md")
BUILD = os.environ.get("B", os.path.join(gamecfg.ROOT, "build"))

HEAD = """# Every game's songs

The names of the files [a custom soundtrack](SOUNDTRACK.md) plays in place of particular songs: name a file as the
first column (`ryu.flac`, `player_select.ogg`) or the number, the second, which is hexadecimal (`12.mp3`,
`0x12.mp3`), with `.ogg`, `.flac`, `.mp3` or `.wav`. A song played once (a fanfare, the versus screen) is played once
whatever the file. Each game also writes this list, as `songs.txt`, into its soundtrack folder, with the music it has
no name for yet by number.

<!-- Made by tools/songs.py from each game's --list-songs: change games/<id>/src/music.cpp, not this. -->

"""


def anchor(name):
    return "".join(c for c in name.lower().replace(" ", "-") if c.isalnum() or c == "-")


def main():
    check = "--check" in sys.argv[1:]
    games = []
    for game in sorted(os.listdir(os.path.join(gamecfg.ROOT, "games"))):
        if not os.path.exists(os.path.join(gamecfg.ROOT, "games", game, "game.cfg")):
            continue
        cfg = gamecfg.load(game)
        port = os.path.join(BUILD, game, game + (".exe" if os.name == "nt" else ""))
        if not os.path.exists(port):
            print("songs.py: %s not built (%s), left out" % (game, port), file=sys.stderr)
            continue
        listed = subprocess.run([port, "--list-songs"], capture_output=True, text=True)
        if listed.returncode != 0 or not listed.stdout.strip():
            print("songs.py: %s has no --list-songs (build it again), left out" % game, file=sys.stderr)
            continue
        songs = [line.split("\t") for line in listed.stdout.splitlines() if line]
        games.append((cfg.get("name", game), game, songs))
    games.sort(key=lambda g: g[0].lower())

    text = HEAD + "".join("- [%s](#%s) (%d)\n" % (name, anchor(name), len(songs)) for name, _, songs in games)
    shown = {}  # (a list another game has already shown: said so instead of shown again)
    for name, game, songs in games:
        text += "\n## %s\n\n" % name
        same = shown.get(tuple(map(tuple, songs)))
        if same:
            text += "The same songs as [%s](#%s).\n" % (same, anchor(same))
            continue
        shown[tuple(map(tuple, songs))] = name
        text += "| File name | Number | What it is |\n|---|---|---|\n"
        for song in songs:
            what = song[2] + (" (played once)" if len(song) > 3 and song[3] == "once" else "")
            text += "| `%s` | `%s` | %s |\n" % (song[0], song[1], what.replace("|", "\\|"))

    have = open(OUT).read() if os.path.exists(OUT) else ""
    if check:
        if have != text:
            print("songs.py: docs/SONGS.md is not as the games list their songs: run tools/songs.py", file=sys.stderr)
            return 1
        print("songs.py: docs/SONGS.md is as the games list their songs (%d games)" % len(games))
        return 0
    if have != text:
        with open(OUT, "w") as f:
            f.write(text)
    print("songs.py: %d games' songs in docs/SONGS.md" % len(games))
    return 0


if __name__ == "__main__":
    sys.exit(main())
