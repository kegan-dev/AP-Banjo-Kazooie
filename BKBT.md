# The `bkbt` branch

This repository is a private fork of the Banjo-Kazooie decompilation
([banjo.decomp/banjo-kazooie](https://gitlab.com/banjo.decomp/banjo-kazooie)). `master` is upstream,
untouched. `bkbt` is the base of the Archipelago ROM hack for Banjo-Kazooie. The project it belongs to is
the BKBT repository, which holds the documentation (`hack/bk/README.md` there), the build wrapper and the
tools that test a ROM.

## Two builds from one tree

| | `make` | `make BKBT=1` |
|---|---|---|
| Result | byte-identical to the retail ROM; `make` prints `OK` | the hack |
| Anti-tamper | compiled in | compiled out (`ANTI_TAMPER=0 ANTI_PIRACY=0` are implied, and required) |
| Code under `#if BKBT` | absent | present |

The matching build is the check that this branch has not changed the game by accident: every change is
either byte-neutral at retail layout or behind a switch. Run it after every merge from upstream.

Going from one build to the other recompiles every C file (about three minutes). The objects depend on
the switches through a stamp file, `build/us.v10/flags.*.stamp`, so a changed switch cannot leave stale
objects behind.

The build needs Linux and the retail ROM as `baserom.us.v10.z64` (see `README.md`). From the BKBT
repository on Windows: `bash hack/build_bk_rom.sh` (the hack) or `bash hack/build_bk_rom.sh matching`.

## What the branch changes

1. **The table for map `0x22`** in `src/core2/code_C9F00.c` names two Clanker's Cavern functions by their
   own symbols instead of symbols of other overlays that share their address (upstream issue 14). No byte
   changes at retail layout; without it, moving code in CC, GV or CCW hangs the game.
2. **The rest of the anti-tamper is under `ANTI_TAMPER`.** Upstream's switch leaves nine places compiled
   in: the cold-boot checks in `ucode_load`, `anctrl_setDuration` and `gWorldExitPad_update`, the four
   validators of the checksum words kept beside the volatile flags and the level-specific flags, and the
   two EEPROM overwrites that follow a failed validation. A build with the switch off and those left in
   derails without a cold boot's leftovers in low RAM, and wipes the save when the checksum words are
   disturbed.
3. **`BKBT`**, the switch for hack code, and its first user: `mainLoop` calls the handler of a payload in
   Expansion Pak RAM once per iteration when the payload is there (`bkbt_payload` in
   `manual_syms.us.v10.txt`).
4. **Link-time guards** at the end of `manual_syms.us.v10.txt`, for two mistakes that neither the linker
   nor an emulator reports (next section).
5. **The final ROM is checked**: `make` fails when `bk_rom_compress` fails or the ROM is not 16 MiB,
   instead of leaving a damaged file that the next `make` accepts.

## Rules for new code

- **Hack code goes under `#if BKBT`**, so the matching build stays matching.
- **No uninitialised global in a file whose `.bss` is linked ahead of `checksums.c.o`.** The loader writes
  four checksum words at the start of each overlay's bss, over whatever is there. In core2 that file is
  `anctrl.c`; the level overlays have up to twenty. Give the variable an initial value, or put it in a
  new file appended to the overlay's list in `decompressed.us.v10.yaml` (not in the cutscenes overlay,
  where an appended file lands ahead of the checksums). The link stops with a `BKBT:` message if this goes
  wrong.
- **core1 + core2 + the largest level overlay have about 68 KB to grow** (68,864 bytes in the first hack
  build). Every level overlay is loaded at the end of core2 and has to end below `0x803A5C00`. The link
  stops with a `BKBT:` message past that.
- **No RAM address of the game is a constant.** Everything moves with every build. Take addresses from
  `build/us.v10/banjo.us.v10.map`.
