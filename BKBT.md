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
either byte-neutral at retail layout or behind a switch. Run it after every merge from upstream. With the
default switches `make` fails when the ROM differs.

Going from one build to the other recompiles every C file (about two minutes with `-j4`). The objects
depend on the switches through a stamp file, `build/us.v10/flags.*.stamp`, and on every header, so a
changed switch or an edited header cannot leave stale objects behind. One thing the stamp cannot see:
objects compiled by a commit from before it existed (upstream `master`, or `bkbt` before the BKBT switch
was added). After building such a commit in this tree, delete `build/us.v10/flags.*.stamp` before
building `bkbt` again.

The build needs Linux and the retail ROM as `baserom.us.v10.z64` (see `README.md`). From the BKBT
repository on Windows: `bash hack/build_bk_rom.sh` (the hack) or `bash hack/build_bk_rom.sh matching`.
That script also checks what `make` cannot: a failed link leaves the files of the previous build in
place, so judge a build by `make`'s exit code or by the script, never by the ROM being there.

## What the branch changes

1. **The table for map `0x22`** in `src/core2/code_C9F00.c` names two Clanker's Cavern functions by their
   own symbols instead of symbols of other overlays that share their address (upstream issue 14). No byte
   changes at retail layout; without it, moving code in CC, GV or CCW hangs the game.
2. **The rest of the anti-tamper is under `ANTI_TAMPER`.** Upstream's switch leaves nine places compiled
   in: the cold-boot checks in `ucode_load`, `anctrl_setDuration` and `gWorldExitPad_update`, the four
   validators of the checksum words kept beside the volatile flags and the level-specific flags, and the
   two EEPROM overwrites that follow a failed validation. A build with the switch off and those left in
   derails without a cold boot's leftovers in low RAM, and wipes the save when the checksum words are
   disturbed. Five users of the validators stay compiled (the picture `mainLoop` draws on a failed
   validation, and penalties in pause, sound, score and the map model). They can do nothing while the
   validators return `TRUE`, so a validator must not be brought back.
3. **`BKBT`**, the switch for hack code, and its first user, the frame hook (below).
4. **Link-time guards** at the end of `manual_syms.us.v10.txt`, for mistakes that neither the linker nor
   an emulator reports (rules below).
5. **A build that cannot pass off a stale or damaged result**: objects depend on the switches and on the
   headers, the link depends on the symbol files it reads, and the final ROM gets its name only when
   `bk_rom_compress` succeeded and the file is 16 MiB.

## The frame hook

With `BKBT=1`, `mainLoop` (`src/core1/code_0.c`) looks at `bkbt_payload` (`0x80500000`, set in
`manual_syms.us.v10.txt`) on every iteration, before the game update. If the word there is `'BKPL'` it
calls the address in the next word. The payload is not part of this ROM: the dual-boot image's launcher
loads it into Expansion Pak RAM.

- It is a plain o32 call with no arguments. The handler has to preserve `s0`-`s7`, `sp`, `ra` and
  `f20`-`f31`; `mainLoop` keeps live values in `s0`-`s2` across the call. Everything else is free,
  `HI`/`LO` and `f0`-`f19` included. About 6 KB of the main thread's stack are available.
- It is called once per `mainLoop` iteration. That is not once per drawn frame (two iterations run on
  the frame of a map change), and `gGlobalTimer` does not count iterations (the game resets it when an
  attract demo starts and freezes it in pause). A handler that needs a counter keeps its own.
- Only the magic is checked. A payload left in RAM by an earlier session, or one assembled for another
  build of this ROM (it carries RAM addresses of the build it was made for), is called just the same.
  This has to be closed before a ROM from this branch goes into an image.

## Rules for new code

- **Hack code goes under `#if BKBT`**, so the matching build stays matching.
- **No uninitialised global in a file whose `.bss` is linked ahead of `checksums.c.o`.** The loader writes
  four checksum words at the start of each overlay's bss, over whatever is there. In core2 that file is
  `anctrl.c`; the level overlays have up to twenty. Give the variable an initial value, or put it in a
  new file appended to the overlay's list in `decompressed.us.v10.yaml` (not in the cutscenes overlay,
  where an appended file lands ahead of the checksums). The link stops with a `BKBT:` message if this goes
  wrong.
- **No uninitialised global in `src/boot`.** The entry stub clears a fixed number of bytes of the boot
  segment's bss. The link stops with a `BKBT:` message.
- **core1 + core2 + the largest level overlay have about 68 KB to grow** (68,864 bytes in the first hack
  build). Every level overlay is loaded at the end of core2 and has to end below `0x803A5C00`. The link
  stops with a `BKBT:` message past that.
- **No RAM address of the game is a constant.** Everything moves with every build. Take addresses from
  `build/us.v10/banjo.us.v10.map`.
