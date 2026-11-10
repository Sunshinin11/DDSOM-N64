# Building from source

Everything here runs from a checkout with your own ROM sets, one folder for each game in `rom/` (ignored by
Git): `rom/sfa2/`, `rom/vampire_savior/`, `rom/puzzle_fighter/`, each with the set's files, or the set's zip
(and its parent set's zip, if your collection keeps the shared files there) with its key file. A link to the
folder you keep a set in does as well. Nothing built from a set is ever committed: see
[CONTRIBUTING.md](../CONTRIBUTING.md). Each game's `games/<id>/game.cfg` lists its files and their checksums;
the build tells you which one is missing or different.

| Game | `GAME=` | ROM set |
| --- | --- | --- |
| [Super Street Fighter II Turbo](../games/ssf2t/README.md) | `ssf2t` | `ssf2xjr1` (Japan 940223) |
| [Street Fighter Alpha](../games/sfa/README.md) | `sfa` | `sfa` (Europe 950727) |
| [Street Fighter Alpha 2](../games/sfa2/README.md) | `sfa2` | `sfa2u` (USA 960430) |
| [Street Fighter Zero 2 Alpha](../games/sfz2al/README.md) | `sfz2al` | `sfz2al` (Asia 960826) |
| [Street Fighter Alpha 3](../games/sfa3/README.md) | `sfa3` | `sfa3u` (USA 980904) |
| [Darkstalkers](../games/dstlk/README.md) | `dstlk` | `dstlk` (Europe 940705) |
| [Night Warriors](../games/nwarr/README.md) | `nwarr` | `nwarr` (Europe 950316) |
| [Vampire Savior](../games/vampire_savior/README.md) | `vampire_savior` | `vsavj` (Japan 970519) |
| [Super Gem Fighter Mini Mix](../games/pocket_fighter/README.md) | `pocket_fighter` | `sgemf` (USA 970904) |
| [X-Men: Children of the Atom](../games/xmcota/README.md) | `xmcota` | `xmcota` (Euro 950331) |
| [Marvel Super Heroes](../games/msh/README.md) | `msh` | `msh` (Europe 951024) |
| [X-Men vs. Street Fighter](../games/xmvsf/README.md) | `xmvsf` | `xmvsf` (Europe 961004) |
| [Marvel Super Heroes vs. Street Fighter](../games/mshvsf/README.md) | `mshvsf` | `mshvsf` (Europe 970625) |
| [Marvel vs. Capcom](../games/mvsc/README.md) | `mvsc` | `mvsc` (Europe 980123) |
| [Super Puzzle Fighter II Turbo](../games/puzzle_fighter/README.md) | `puzzle_fighter` | `spf2t` (Europe 960529) |

## macOS

Apple clang (Command Line Tools), then `brew install cmake ninja sdl3 python`. SDL 3.4.16 or later (`brew upgrade sdl3`):
with 3.4.14 the game's window stays black while it runs and sounds, so CMake asks for 3.4.16.

```sh
make GAME=sfa2              # build/sfa2/sfa2 (twice the first time: the port itself writes the
                            # deciphered program the translator works from); on macOS also the game's
                            # own launcher and a quick app bundle (NO_APP=1: not)
make GAME=sfa2 explore      # once, about ten minutes: finds the game's code by made-up play and translates it
make GAME=sfa2 run          # play (windowed)

make games                  # every game whose ROM set is in rom/<id>/
make collection             # build/collection/: the collection's launcher with every built game beside it
make collection-app         # build/collection/CPS-2 Recomp.app, a quick bundle for your own machine;
                            # a release's app is made by tools/release/macos.sh
```

`make GAME=<id> launcher` and `make GAME=<id> app` still make a launcher for that one game, which is handy
while working on it.

The Display page's shaders need librashader and the presets in the app: `brew install rustup && rustup default
stable` once, and `tools/librashader.sh` (which the app bundles and `tools/release/macos.sh` run) builds
librashader from its source into `build/deps/librashader/` and copies the presets beside it, the first time only.
Without Rust a quick bundle has no shaders (and says so); the release needs them. To try a preset from a build
folder, put `librashader.dylib` beside the game and run it with `--shader FILE.slangp`.

## Linux (and SteamOS)

GCC 13 or later or Clang 16 or later (C++20), CMake 3.28 or later, Ninja, Python 3, zlib's and SDL 3's
development files, and what SDL needs to open a window (the list is in `tools/release/Dockerfile`). Where the
distribution has no SDL 3 package, build it once and point CMake at it:

```sh
cmake -S SDL3-3.4.16 -B sdl-build -DCMAKE_INSTALL_PREFIX=$HOME/sdl3 && cmake --build sdl-build --target install
CMAKE_PREFIX_PATH=$HOME/sdl3 make GAME=sfa2
```

Then the same targets as on macOS (not the app bundles). For the shaders, Rust (rustup.rs; the release image has
it) and `tools/librashader.sh`, which makes `librashader.so` and the presets in `build/deps/librashader-linux-<processor>/`:
put both beside the game.

## Windows

Built by cross-compiling with MinGW-w64 on Linux (`tools/release/windows.sh`), or in an MSYS2 UCRT64 shell with
`mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,python,sdl3,zlib}` and the same targets. Microsoft's compiler has not
been tried. For the shaders, `rustup target add x86_64-pc-windows-gnu` and `tools/librashader.sh --windows`
(`tools/release/windows.sh` runs it), which makes `librashader.dll` and the presets in
`build/deps/librashader-windows-x64/`: put both beside the game.

## The checks

```sh
make check                  # no game needed: the audit, the copy of recomp-common, the stand-in game built and started
make audit                  # nothing from a game, no keys, no personal paths in the tracked files

make GAME=sfa2 check FRAMES=40000 MOVIE=games/sfa2/movies/ladder.txt
                            # translated code equals the interpreter run block by block; two runs alike
make GAME=sfa2 check-blocks     # every translated block beside the interpreter: registers, flags, cycles, writes
make GAME=sfa2 check-rewrites   # every readable rewrite beside the routine it replaces
make GAME=sfa2 check-release FRAMES=40000
                            # the build with no translated code (the interpreters; what CI builds) against the translated one
make GAME=sfa2 check-widescreen # the game's own check of its widescreen (Street Fighter Alpha 2, Vampire Savior)
```

[VERIFYING.md](VERIFYING.md) says what each proves. A run with no window and no sound device, for tests on any
system:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/sfa2/sfa2 --rom rom/sfa2 --headless \
    --frames 40000 --movie games/sfa2/movies/ladder.txt --features free_play,no_damage,one_hit \
    --hash-picture --hash-log pictures.txt --stats
```

`--hash-picture` hashes each frame's picture, which is how builds for different systems are compared: the same
program built for macOS, Linux and Windows must write the same file.

That holds for builds made from the same translated code. Which code gets translated, and where its blocks start,
comes from the entries your own tree has found (`make GAME=<id> explore`, and every `make GAME=<id> run` adds
what play reaches), and the port looks at time and interrupts only where a block starts. So two trees with
different entries are both right and still differ in a few pictures: a tree built from nothing against the tree
the releases are built from gave 21 different pictures in the 40,000 of Street Fighter Alpha 2's ladder movie,
and the same last picture. Compare hash files, and replay input movies, between builds of one tree.

## Release builds

A release is the collection: the launcher and every game's program, for each system. A game's program holds its
translated code, so a release is built where every ROM set is, after `make GAME=<id>` and
`make GAME=<id> explore` for each game (see [RELEASING.md](RELEASING.md)):

```sh
tools/release/macos.sh      # build/release/CPS2Recomp-macOS.zip (Apple silicon and Intel)
docker build --platform linux/amd64 -t cps2recomp-build tools/release
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src cps2recomp-build tools/release/linux.sh
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src cps2recomp-build tools/release/windows.sh
docker build --platform linux/arm64 -t cps2recomp-build-arm64 tools/release
docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src cps2recomp-build-arm64 tools/release/linux.sh
tools/release/all.sh        # all four and SHA256SUMS.txt
tools/release/android.sh    # build/release/CPS2Recomp-android.apk
```

The Android app needs CMake, Ninja, a JDK 17, the Android SDK with platform 34 and NDK 27.2 (with Homebrew:
`brew install openjdk@17 android-commandlinetools`, then `sdkmanager "platforms;android-34" "ndk;27.2.12479018"`),
and Rust for librashader (`rustup`; its `aarch64-linux-android` target is added on its own).
Each game is a library there that the app starts in a process of its own (`android/`), and the launcher is
recomp-common's, a library too. It is signed with your key when `~/.config/cps2recomp/android-signing.env` names one
(`KEYSTORE`, `KEYSTORE_PASSWORD`, `KEY_ALIAS`, `KEY_PASSWORD`), else with Android's debug key, which is fine for your
own phone (`adb install -r build/release/CPS2Recomp-android.apk`).

Each script ends with `tools/release/check_archive.sh`, which fails if an archive holds a game file or a
builder's home path, lacks a game's program or the licences, or has a game program with no translated code in a
release. With `NO_GAME_CODE=1` the scripts build the game programs with no translated code (they run a ROM set in
the port's interpreters, more slowly): that needs no ROM set, and is what CI builds to prove everything compiles
and starts. `GAMES="sfa2 vampire_savior"` builds some of the games only, for a test.
