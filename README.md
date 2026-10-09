[![GitHub Release](https://img.shields.io/github/v/release/TheSuperHackers/GeneralsGameCode?include_prereleases&sort=date&display_name=tag&style=flat&label=Release)](https://github.com/TheSuperHackers/GeneralsGameCode/releases)
![GitHub milestone details](https://img.shields.io/github/milestones/progress-percent/TheSuperHackers/GeneralsGameCode/3)
![GitHub milestone details](https://img.shields.io/github/milestones/progress-percent/TheSuperHackers/GeneralsGameCode/1)
![GitHub milestone details](https://img.shields.io/github/milestones/progress-percent/TheSuperHackers/GeneralsGameCode/4)
![GitHub milestone details](https://img.shields.io/github/milestones/progress-percent/TheSuperHackers/GeneralsGameCode/5)
![GitHub milestone details](https://img.shields.io/github/milestones/progress-percent/TheSuperHackers/GeneralsGameCode/6)

[![GitHub issues by-label](https://img.shields.io/github/issues/TheSuperHackers/GeneralsGameCode/bug?style=flat&label=Bug%20Issues&labelColor=%23c4c4c4&color=%23424242)](https://github.com/TheSuperHackers/GeneralsGameCode/issues?q=label%3ABug)
[![GitHub issues by-label](https://img.shields.io/github/issues/TheSuperHackers/GeneralsGameCode/enhancement?style=flat&label=Enhancement%20Issues&labelColor=%23c4c4c4&color=%23424242)](https://github.com/TheSuperHackers/GeneralsGameCode/issues?q=label%3AEnhancement)
[![GitHub issues by-label](https://img.shields.io/github/issues/TheSuperHackers/GeneralsGameCode/major?style=flat&label=Major%20Issues&labelColor=%23c4c4c4&color=%23424242)](https://github.com/TheSuperHackers/GeneralsGameCode/issues?q=label%3AMajor)
[![GitHub issues by-label](https://img.shields.io/github/issues/TheSuperHackers/GeneralsGameCode/critical?style=flat&label=Critical%20Issues&labelColor=%23c4c4c4&color=%23424242)](https://github.com/TheSuperHackers/GeneralsGameCode/issues?q=label%3ACritical)
[![GitHub issues by-label](https://img.shields.io/github/issues/TheSuperHackers/GeneralsGameCode/blocker?style=flat&label=Blocker%20Issues&labelColor=%23c4c4c4&color=%23424242)](https://github.com/TheSuperHackers/GeneralsGameCode/issues?q=label%3ABlocker)

# Welcome to the Generals Game Code Project

GeneralsGameCode is a community-driven project aimed at fixing and improving the classic RTS game, *Command &
Conquer: Generals* and its expansion *Zero Hour*. This repository contains the source code for both games, with a
primary focus on *Zero Hour*.

Additionally, there is a complementary project repository for fixing and improving game data and assets such as
INI scripts, GUI, AI, maps, models, textures, audio, localization. You can find it
[here](https://github.com/TheSuperHackers/GeneralsGamePatch/) and contribute to it as well.

## About this fork

This is a private fork of [TheSuperHackers/GeneralsGameCode](https://github.com/TheSuperHackers/GeneralsGameCode),
used to play *Zero Hour* over LAN with friends. It follows upstream closely (upstream fixes are merged regularly) and
adds its own fixes, performance work and features on top. Only *Zero Hour* is targeted; base *Generals* is kept
compiling with its behaviour unchanged.

**Every player in a LAN game must run the exact same build of this fork.** The fork changes game logic (for example
pathfinding), so it is not compatible with retail *Zero Hour* 1.04 or with other builds in multiplayer.

### What this fork changes

**Late-game performance** (big LAN games with large armies)
- Faster pathfinding in *Zero Hour*: the improved pathfinding code runs from the first frame, the A* open list is a
  binary heap, the straight-line "beam" each search pushes toward the goal is capped, and the path queue serves twice
  as many cells per frame.
- Path requests that hit a full pathfind queue are retried instead of being dropped, so units no longer freeze
  waiting for a path that never comes.
- Measured on an 8-player map with about 1,150 units: logic time p95 about -50%, mean about -37%, units waiting more
  than 3 seconds for a path about -79%. Tested on a 2-PC LAN game with about 400 units per team: in sync, no issues.
- The exe is large-address-aware (4 GB address space on 64-bit Windows instead of 2 GB).
- The exe asks hybrid-graphics laptops (NVIDIA Optimus, AMD switchable graphics) to use the dedicated GPU.

**LAN play**
- The host refuses players whose exe or INI data CRC differs, so a mismatched build cannot join and desync mid-game.
  The Options menu shows `exe:XXXXXXXX ini:XXXXXXXX`; these must match on every PC.
- New HUD line next to the network latency counter: the slowest player's frame rate and the current game speed
  (in a lockstep game every PC runs at the speed of the slowest one).
- The game speed recovers faster after a slow PC catches up (frame rates averaged over 8 seconds instead of 30).

**Optional build cap** (off by default)

Limits how much each player can own, so very large late-game armies cannot slow the whole LAN game down.

- *Setting it:* the host picks the cap in the LAN lobby with the **Build Cap** box: No limit, 350, 450, 650, 900 or
  1200. Other players see the choice but cannot change it. The choice is kept in the host's `Options.ini`
  (`LoadCap`), which skirmish uses too.
- *Faction weighting:* GLA needs more units than USA or China for the same army power, so the cap is weighted when
  the game starts. The match total (players x chosen cap) is split by weight USA 0.75, China 0.70, GLA 1.00. A
  lobby where everyone plays the same faction keeps the chosen cap. Examples at 350:

  | Lobby | USA | China | GLA |
  |---|---|---|---|
  | USA vs GLA | 300 | | 400 |
  | China vs GLA | | 288 | 412 |
  | 2 USA + 2 China + 2 GLA | 321 | 300 | 429 |

- *What counts (load points):* infantry 1, vehicle 3, aircraft 4, structure 2. Units waiting in a production queue
  count too. An Angry Mob counts 10 from the moment it is queued (its full size) and its members count 0 while the
  mob lives. An INI `LoadPoints` field on an object overrides all of this.
- *What does not count:* things the player cannot control: Supply Drop Zone and paradrop cargo planes,
  general-power aircraft (A-10, B-52, B-3, Spectre, Carpet Bomber, MiG napalm strike), artillery barrage cannons,
  Stinger Site soldiers and the drones that ride along with vehicles (battle, scout, hellfire, repair, spy).
  Sentry and Guardian drones are built like other units and do count.
- *At the cap:* build buttons grey out and production is refused. Units that appear without production still
  arrive and count, even over the cap: paradrops, ambushes, the free supply-center harvester, tunnel defenders,
  rebuilt GLA buildings. A player who ends up over the cap (for example `Cap 362/350`) cannot produce until losses
  bring them back under it.
- *HUD:* `Cap used/cap` in the top-left corner, shown whenever a cap is on (even with the latency counter turned off).

**Superweapons option** (LAN lobby and skirmish)

- The **Superweapons** box replaces the old "Limit Superweapons" check box: **Unlimited**, **Limit 1** (the retail
  limit, one of each type) or **Disabled**.
- *Disabled:* Particle Cannon, Nuclear Missile and SCUD Storm (every general's version) can still be built and
  upgraded, but they never fire, not for players, the AI or map scripts. Their button stays greyed, there are no
  countdown timers and no "superweapon detected/ready" announcements. The limit of one per type does not apply.
- *Fog of war:* a working superweapon normally shows itself to every enemy through the fog. A disabled one does not:
  enemies only see it once they scout it, like any other building.
- The skirmish AI still builds them (wasted money for it).
- Only the host can change it in the LAN lobby; the other players see the choice.

**General's points rate** (LAN lobby, off by default)

- The host picks **Gen. Points** in the LAN lobby: 1x, 0.75x, 0.5x, 0.33x, 0.25x or 0.1x. It slows the general's
  rank (skill) points earned from kills and unit abilities; unit veterancy is unchanged. Points granted by map
  scripts are not scaled. Fractions carry over, so at 0.1x ten kills worth 1 point give 1 point.
- The choice is kept in the host's `Options.ini` (`GeneralPointsRate`, in percent), which skirmish uses too, like the
  build cap. The superweapons choice is kept there as well (`SuperweaponsOff`).

**Display**
- Fullscreen uses a borderless window by default, which avoids the D3D8 device-loss loop when switching windows.
  Start with `-exclusivefullscreen` to get the original exclusive fullscreen mode.

**Bug fixes**
- Crash when a unit fires before it was ever drawn (weapon recoil list out of bounds).
- Memory corruption when a network game resets (per-player frame rate and latency arrays written past their end).

**Benchmark mode** (for development)
- `generalszh.exe -bench <scenario.ini> -benchOut <dir> -setCwd "<game folder>" [-headless]` starts a scripted
  late-game skirmish without menus, plays every slot like a human, writes per-frame timings, pathfinding counters and
  logic CRCs, and quits. `-benchListMaps <file>` lists the multiplayer maps with their player counts.

### Compatibility notes
- Replays recorded with retail *Zero Hour* or with older builds of this fork do not play back correctly (paths differ).
- Save games keep the same format. The build cap, disabled superweapons and the general's points rate are not stored
  in save games, so a loaded skirmish plays without them.
- Every PC also needs the Microsoft Visual C++ 2015-2022 x86 runtime.

### Installing this fork for a LAN game
1. Build `z_generals` (see [Building the Game Yourself](#building-the-game-yourself)), or take the exe from the LAN
   package the host shares.
2. Copy `generalszh.exe` into the *Zero Hour* game folder on every PC (keep a backup of the original).
3. Before playing, check that the `exe:` and `ini:` values in the Options menu are identical on all PCs.

## Project Overview

The game was originally developed using Visual Studio 6 and C++98. We've updated the code to be compatible with Visual
Studio 2022 and C++20.

The initial goal of this project is to fix critical bugs and implement improvements while maintaining compatibility with
the original *Generals* version 1.08 and *Zero Hour* version 1.04. Once we can break retail compatibility, more fixes
and features will be possible to implement.

## Current Focus and Future Plans

Here's an overview of our current focus and future plans

- **Modernizing the Codebase**: Transitioning to modern C++ standards and refactoring old code.
- **Critical Bug Fixes**: Fixing game-breaking issues (e.g., fullscreen crash).
- **Minor Bug Fixes**: Addressing minor bugs (e.g., UI issues, graphical glitches).
- **Cross-Platform Support**: Adding support for more platforms (e.g., Linux, macOS).
- **Engine Improvements**: Enhancing the game engine to improve performance and stability.
- **Client-Side Features**: Enhancing the game's client with features such as an improved replay viewer and UI updates.
- **Multiplayer Improvements**: Implementing a new game server and an upgraded matchmaking lobby.
- **Tooling Improvements**: Developing new or improving existing tools for modding and game development.
- **Community-Driven Improvements**: Once the community grows, we plan to incorporate more features, updates, and
  changes based on player feedback.

## Running the Game

To run *Generals* or *Zero Hour* using this project, you need to have the original *Command & Conquer: Generals and Zero Hour* game
installed. The easiest way to get it is through *Command & Conquer The Ultimate Collection*
on [Steam](https://store.steampowered.com/bundle/39394). Once the game is ready, download the latest version of the
project from [GitHub Releases](https://github.com/TheSuperHackers/GeneralsGameCode/releases), extract the necessary 
files, and follow the instructions in the [Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki).


## Joining the Community

You can chat and discuss the development of the project on our [Discord channel](https://www.community-outpost.com/discord) to get the latest updates,
report bugs, and contribute to the project!

## Building the Game Yourself

We provide support for building the project on Windows and Linux. For detailed build instructions, check the
[Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki/build_guides), which includes guides for VS6, VS2022,
Docker, CLion, and links to forks supporting additional versions.

### Quick Start

**Windows (Visual Studio 2022)**
```bash
cmake --preset win32
cmake --build build/win32 --config Release
```

**Linux (via Docker)**
```bash
./scripts/docker-build.sh              # Build using Docker
./scripts/docker-install.sh --detect # Install to your game
```

### Dependency management

The repository uses a vcpkg manifest (`vcpkg.json`). Dependency versions come from the `builtin-baseline` commit
recorded there, with per-port `overrides` when a specific version is required. Update the baseline to pick up new
versions. GitHub Actions consumes these ports through a vcpkg binary cache backed by a NuGet feed on GitHub
Packages, keyed by vcpkg's own ABI hashes, so the first CI build warms the feed and subsequent builds pull prebuilt
binaries instead of re-compiling everything. Pull requests from forks restore from the feed but cannot write to it.

### Profiling

Tracy profiling is supported in the CMake preset `win32-profile`.
Use `tracy-profiler.exe` from [Tracy v0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1).
If you get an error when using Tracy, try removing `dbghelp.dll` from the game binary directory.

## Contributing

We welcome contributions to the project! If you’re interested in contributing, you need to have knowledge of C++. Join
the developer chat on Discord for more information on how to get started. Please make sure to read our
[Contributing Guidelines](CONTRIBUTING.md) before submitting a pull request. You can also check out 
the [Wiki](https://github.com/TheSuperHackers/GeneralsGameCode/wiki) for more detailed documentation.


## License & Legal Disclaimer

EA has not endorsed and does not support this product. All trademarks are the property of their respective owners.

This project is licensed under the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html), which allows you to
freely modify and distribute the source code under the terms of this license. Please see [LICENSE.md](LICENSE.md) 
for details.
