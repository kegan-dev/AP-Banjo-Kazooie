<p align="center">
  <img src="banner.svg" alt="Banjo-Kazooie AP: the Archipelago hack" width="100%">
</p>

<p align="center">
  <img alt="Archipelago anchor layout 0.16" src="https://img.shields.io/badge/AP%20layout-0.16.0-e5604f">
  <img alt="Dual-boot contract 5" src="https://img.shields.io/badge/dual--boot%20contract-5-4aa8e0">
  <img alt="Status: alpha" src="https://img.shields.io/badge/status-alpha-f2b632">
  <a href="../LICENSE"><img alt="CC0 1.0" src="https://img.shields.io/badge/licence-CC0%201.0-7b68ee"></a>
</p>

**Banjo-Kazooie's [Archipelago](https://archipelago.gg) hack, built from a fork of the
[Banjo-Kazooie decompilation](https://gitlab.com/banjo.decomp/banjo-kazooie).** It's the game behind the
[Banjo-Kazooie world](https://github.com/kegan-dev/Archipelago-BanjoKazooie), and Banjo-Kazooie's half of the
[dual-boot image](https://github.com/kegan-dev/AP-Banjo-Kazooie-Tooie) that the
[Banjo-Kazooie & Tooie world](https://github.com/kegan-dev/Archipelago-BanjoKazooieTooie) plays.

> [!NOTE]
> **Playing?** You don't need this repository: the Banjo-Kazooie client patches your own ROM. This is the hack's
> source. The decompilation's own README is [here](../README.md).

## Branches

| Branch | What |
|---|---|
| `bkbt` (default) | The hack. Every change is under `#if BKBT`, so `make` still builds the retail ROM byte for byte, and `make BKBT=1` builds the hack |
| `master` | The upstream decompilation, unchanged |

## What the hack does

**Archipelago.** While the game runs, physical RAM `0x400000` points to an anchor (`'BKAP'`, layout 0.16.0) with the
addresses of small tables the client reads and writes: the seed's settings, the items received, the checks made,
an exit map, and the link for deaths, tags and traps. The layout is [`include/bkbt.h`](../include/bkbt.h).

- **Items**: Bottles' moves, empty honeycombs, jiggies, Mumbo tokens, note doors, Jinjos, Notes and refills arrive
  from the client. A move is learned without a lesson, a note door opens when Banjo walks up, and a level's five
  Jinjos spawn its jiggy on Banjo.
- **Checks**: a molehill, a jiggy, a token, a honeycomb, a Jinjo or a note picked up is published, and a note door's
  count reached. The game's own flags are the checks wherever it has them.
- **Saves**: in SRAM, bound to the seed they were started with, with the checks made offline and the notes kept.
- **Gameplay**: notes stay collected, infinite lives, quick transform with D-pad left, the Lair's world doors led
  anywhere by the exit map, death link, tag link and five traps.
- **Skips**: the intro, the Lair's first cutscene and the game-over cutscene; Start on the opening logos.

**Dual-boot.** Started by Banjo-Tooie's launcher in the image, the game takes its destination from the handoff block,
picks the file tied to the Tooie save at the door, and arrives behind a black, silent screen. Leaving the Lair (or a
door into a Tooie world, or Save and Quit) saves and goes back to Tooie. The contract between the two games is in the
image's [docs/CONTRACT.md](https://github.com/kegan-dev/AP-Banjo-Kazooie-Tooie/blob/main/docs/CONTRACT.md).

[BKBT.md](../BKBT.md) is the hack's full record: how each feature works, why, and the rules for new code.

## Building

Set up the decompilation as [its README](../README.md) says, with `baserom.us.v10.z64`, then:

```bash
make            # the retail ROM, byte for byte: check this first
make BKBT=1     # the hack: build/us.v10/banjo.us.v10.z64
```

Switching between the two recompiles everything. To play the hack by itself, its header has to declare SRAM, which
the image repository's `hack/build_bk_rom.sh` writes when it exports the build (with the map and the addresses the
tools need). That repository builds everything together: [AP-Banjo-Kazooie-Tooie](https://github.com/kegan-dev/AP-Banjo-Kazooie-Tooie).

## Credits

The [Banjo-Kazooie decompilation](https://gitlab.com/banjo.decomp/banjo-kazooie) project, without which none of this
would exist. [Banjo-Tooie AP](https://github.com/jjjj12212/Archipelago-BanjoTooie) by jjjj12212, g0goTBC and Auztin,
whose design (a ROM hack plus a client writing into RAM) this follows.

## Licence

[CC0 1.0](../LICENSE), like the decompilation, the hack's changes included. Banjo-Kazooie is a trademark of its
owners; this project isn't affiliated with them. No game data is in this repository: the build needs your own ROM.
