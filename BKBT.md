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
3. **`BKBT`**, the switch for hack code, and its first user: the way back to Tooie in a dual-boot image
   ("The frame hook" below).
4. **Link-time guards** at the end of `manual_syms.us.v10.txt`, for mistakes that neither the linker nor
   an emulator reports (rules below).
5. **A build that cannot pass off a stale or damaged result**: objects depend on the switches and on the
   headers, the link depends on the symbol files it reads, and the final ROM gets its name only when
   `bk_rom_compress` succeeded and the file is 16 MiB.
6. **The save device** (2026-10-14): inside the dual-boot image the game keeps its 512 bytes of save in
   the image's SRAM, from `0x7000` on, instead of the EEPROM the image does not have; run by itself it
   keeps its EEPROM ("The save device" below). `src/core1/eeprom.c`, the accessor `bkbt_launched()` in
   `src/core1/code_0.c`, and the linker symbol `bkbt_sram_offset` with its assertions in
   `manual_syms.us.v10.txt`.
7. **No new-game intro** (2026-10-14, at the owner's request): a new game starts in Spiral Mountain at the
   exit from Banjo's house, where the intro cutscenes end, instead of playing them. One call in
   `src/core2/ch/gameSelect.c`, in every hack build, by itself and in the dual-boot image ("The new-game
   intro" below).
8. **Start skips the opening logos from the first boot** (2026-10-15): retail ignores Start on the logos
   after power-on and takes it only from the concert on. One initial value in `src/core2/code_9B180.c`,
   in every hack build ("The opening logos" below).
9. **The Archipelago anchor** (2026-10-17; `docs/ROADMAP.md` of the BKBT repository, hack feature 1):
   while the game runs, the word at physical `0x400000` holds the address of `bkbt_ap_anchor`, a 12-byte
   struct by which an Archipelago client finds the game and its layout, by itself and in the dual-boot
   image. `include/bkbt.h`, `src/core1/code_0.c` ("The Archipelago anchor" below).

## The frame hook

With `BKBT=1` the game can hand the console back to Banjo-Tooie when it runs inside the dual-boot image of
the BKBT repository. This is dual-boot contract **2**. The same ROM runs by itself: there it finds nothing
of what follows and is the plain game.

Two functions in `src/core1/code_0.c` do it, with the layout in `include/bkbt.h`:

- `bkbt_init()` is the first statement of `core1_init`. It decides once per boot whether a return exists.
- `bkbt_frame()` is called by `mainLoop` on every iteration, after the optional `joy_update` and before
  `baMotor_80250C08`. It decides when to return.

The return itself is not in this ROM. It is the entry of a payload that the Tooie hack's launcher loads
into Expansion Pak RAM before it starts this game: the return launcher, which contains no address of this
game and so does not depend on this build. One payload serves every build of the same contract.

### The contract number

`bkbt_contract = 2;` in `manual_syms.us.v10.txt` is the only place on this side that says the number. It
is a linker symbol so that the map carries it; C uses it as `(u32) bkbt_contract`. The launcher carries
the same number in a tag, the Tooie hack's C in a tagged record, the payload in its header, and the image
builder of the BKBT repository compares them all. Raise it when the meaning of anything below changes.

### The handoff block

At `0x805FFF00`, in Expansion Pak RAM, which this game does not use. The Tooie hack owns the layout
(`hack/bt/n64/src/dualboot.h` in the BKBT repository; `include/bkbt.h` mirrors what this side uses, and
the two have to be kept in step). This side uses four words:

| Offset | Meaning | Written by | This side |
|---|---|---|---|
| `+0x20` | launch token, `0x424B4700 \| contract` (`'B','K','G',2`), written on every launch of this game | Tooie's launcher | `bkbt_init` reads it and zeroes it |
| `+0x24` | KSEG0 address of the header of the payload loaded in this launch, 0 if none | Tooie's launcher | `bkbt_init` reads it and zeroes it |
| `+0x28` | test countdown: `mainLoop` iterations before the game returns by itself, 0 = stay | the Tooie hack | `bkbt_frame` counts it down |
| `+0x2C` | flags. Bit 0: the chord also works outside the front end | the Tooie hack | `bkbt_init` latches it |

**This side accesses the block through KSEG1 only** (`0xA05FFF00`, `volatile`). The return launcher
writes the block uncached and then flushes the caches, so a line of the block that this game had left
dirty in its data cache would be written back over what the launcher stored. No emulator models the
caches: a cached access here passes every test and fails on a console.

### `bkbt_init`

- Without the Expansion Pak (`osMemSize < 0x800000`) it returns before it touches anything: the block is
  above 4 MiB.
- Otherwise it reads `+0x20` and `+0x24` and zeroes both, whatever they held, and publishes the
  Archipelago anchor, token or not ("The Archipelago anchor" below).
- A wrong token means standalone, and nothing more happens. The token is what the trust rests on. A ROM
  started by itself, a reset of this game and RAM left behind by an earlier session all find no token,
  because every start consumes it. A payload that merely lies in RAM is never called, however valid its
  header looks.
- With the right token it remembers that the launcher started the game (`sBkbtLaunched`, read through
  `bkbt_launched()`, by which the save code picks its device: "The save device" below), latches
  the flags, and accepts the payload if `+0x24` is a word-aligned KSEG0 address whose 20-byte header lies
  inside `0x80400000..0x80800000`, with magic `'BKP2'` and this ROM's contract number. Then, and only
  then, there is a return entry.

The payload header, one word each:

| Offset | Content |
|---|---|
| `+0x00` | `'BKP2'` |
| `+0x04` | entry: returns to Tooie and never to its caller |
| `+0x08` | load address (this side does not use it: the launcher passes the address) |
| `+0x0C` | length in bytes |
| `+0x10` | contract number |

A header field never changes its meaning under the same magic.

### `bkbt_frame`

It does two things: on every iteration it looks at the chord, and when there is a return entry it decides
whether to call it. Without an entry nothing is called and the handoff block is not even read.

- **Countdown.** If `+0x28` is not zero it is decremented and stored, and when it has reached zero the
  entry is called. It is stored before the call, so the word reads 0 from then on.
- **Chord.** L + R + Start on pad 0 calls the entry if the chord is armed (below) and the policy allows
  it: flag bit 0, or map `0x1F`, `0x1E` or `0x91` (the opening logos, the concert, the file select), or
  game mode 7 (an attract demo). The policy is one function, `bkbt_chordAllowed`; the return itself
  works from anywhere.
- **The chord is read as L and R down, and Start or the controller's reset bit** (`0x0080` of
  `OSContPad.button`). With L, R and Start all down an N64 controller recalibrates its stick and reports
  that instead of Start: `START_BUTTON` reads 0 and the reset bit is set. A console and ares do that,
  Project64 and mupen64plus deliver `START_BUTTON`. Found on ares v148 on 2026-10-11, where a test for
  the three buttons never matched (the controller mapping there was checked and is right). The two
  encodings are `0x1030` and `0x00B0`; `include/bkbt.h` has the masks.
- **The chord is read from the pad record, not from the game's pad state.** `src/core1/joy.c` keeps pad
  0 as the controller last answered: `BkbtPad` in `include/bkbt.h`, `{ u32 samples; u16 button; }`. It is
  written in one place, `joy_readData`, when `osContGetReadData` has delivered pad 0 (the game found a
  controller at boot, and pad 0's `errno` in that read is 0): `button` is the pad's button word and
  `samples` counts those reads since boot. `bkbt_frame` gets it through `bkbt_padButton`. The game's own
  `sInputs[0]` and `sInputsPrimary` cannot serve. In the modes that play recorded input a demo's word
  sits in `sInputs[0]` until the next read of the pad comes back, and an iteration that starts before
  then copies it into `sInputsPrimary`. A chord held all along then read as released for one iteration,
  and the game went back to Tooie by itself (mupen64plus with a controller plugged in, 2026-10-12, 75
  frames into the first attract demo).
- **The chord is edge-armed.** It counts only once a read of the pad has shown it not down since boot, so
  a player who still holds it from Tooie does not bounce straight back. While `samples` is 0 the pad has
  not been read: the chord is then neither down nor released, nothing arms and nothing is called,
  however many iterations go by. (Before the record there was a rule that left the first two iterations
  out, an iteration count standing in for "the pad has been read".)
- **The chord counter.** `bkbt_frame` counts the iterations on which it found the chord down, whether
  the entry was called or not, and also when there is no entry. It is there for the tools: a test that
  expects the chord to do nothing proves nothing unless the game is shown to have seen the chord.

What the entry can rely on, and what a change to these functions has to keep:

- It is a plain o32 call with no arguments, on the main thread with interrupts on. The entry expects
  nothing in any register and does not come back.
- Both triggers call it through `bkbt_return`, which first withdraws the Archipelago anchor (stores 0
  at physical `0x400000`). Whatever else has to be undone before the game stops goes there too.
- `bkbt_frame` runs once per `mainLoop` iteration. That is not once per drawn frame (two iterations run
  on the frame of a map change), and `gGlobalTimer` does not count iterations (the game resets it when an
  attract demo starts and freezes it in pause). The countdown counts iterations.
- Outside the modes that play recorded input, `joy_update` blanks pad 0's buttons for the game while L
  and R are held, so the chord does not also pause the game or start a file.
- Two threads use the pad record without a lock, and the order of the accesses is what makes that
  safe: the pad thread stores `button` and then counts the sample, `bkbt_padButton` reads `samples` and
  then `button`. The pad thread can interrupt the main thread between the two reads, never the other way
  round. The comment at `sBkbtPad` in `src/core1/joy.c` has the reasoning; the record is `volatile` so
  that the compiler keeps the order.

For a tool that plays the pad: the record is found through `bkbt_ptr_pad` ("Exports for tools" below),
`samples` at `+0` (32 bits) and `button` at `+4` (16 bits). A game without a controller never writes
it, so what a tool stores there stays. The pad counts as read once `samples` is not 0, and a chord has to
be seen released before it works: store a `button` without the chord and a `samples` of 1 or more, let an
iteration pass, then store the chord. A chord that is in the record from the first sample on is a chord
held since boot and does nothing until it is released. With a controller attached the game overwrites
`button` at every retrace, and a tool has to press the emulator's buttons instead. The counter, through
`bkbt_ptr_chord_seen`, goes up by one for every iteration that saw the chord.

By hand, the chord was seen to work in the dual-boot image with this build on Project64 3.0.1 and on
ares v148 (the owner, 2026-10-12; `hack/DUALBOOT.md` of the BKBT repository, "Status"). Everything
else here was checked on mupen64plus only (recompiler and pure interpreter), in two ways. On a plain ROM with a stub in
place of the payload: a tool writes the stub and the handoff words into RAM and presses reset, which
keeps RDRAM (the real payload cannot be used that way, it would run this ROM's IPL3 as Tooie's). And
inside the dual-boot image with the real launcher and the real payload; what was run there is listed in
`hack/DUALBOOT.md` of the BKBT repository, "Checked". The pad rule above was checked on the plain ROM on
2026-10-12, in two ways again. With no controller and a tool writing the record: a chord word without a
sample neither arms nor fires; a chord that is there before the first iteration (a writer thread; the
counter read 1 after one iteration) or from the first frame never fires in 2160 frames, through the
start of the first attract demo, while the counter goes up on every frame; released and pressed it
fires; both encodings; the front-end limit and the flag that lifts it; the countdown; no token, nothing.
And with mupen64plus's input plugin and a controller plugged in, the kind of test in which `joy_readData`
itself writes the record: the record follows the pad within a frame, with two reads per drawn frame; a
chord held from before the boot never fires in 2400 frames, through the first attract demo and into the
file select that the held Start leads to, although `sInputsPrimary` read `0x0000` on one frame of the
demo; released and pressed it fires; a tap of three frames fires. That controller's own buttons deliver
`0x1030` only. The image was then run with a controller as well (`hack/DUALBOOT.md` of the BKBT
repository, "Checked"), and there the check tool answers the emulator's polls of the pad itself, so that
`0x00B0` went through `joy_readData` too: tapped, and held from before BK's boot past the first
attract demo, on the recompiler and the pure interpreter. With `0x00B0` held the demo plays on, because
that word has no Start in it. L + R alone, held for 8 frames before the first chord of a stay, moved
neither the counter nor the game. Not checked: any other emulator, and a console, where the cache rule
above and the order of the two threads' accesses are the parts that matter. Without the Expansion Pak
the ROM was only booted in the emulator (`osMemSize` `0x400000`, nothing called); that the size test
comes before any access above 4 MiB is shown by the disassembly of `bkbt_init`, not by a run.

## The save device

The dual-boot image is one cartridge with one save device, 32 KiB of SRAM at cart address `0x08000000`,
and no EEPROM. The Tooie hack keeps its save there from offset 0. With `BKBT=1`, when the launcher
started this game, the game's 512 bytes of "EEPROM" are 512 bytes of that SRAM instead: block n is the
8 bytes at `bkbt_sram_offset + 8 * n`. Run by itself the ROM never goes there and keeps its EEPROM. The
full description, with the SRAM map and what was run where, is in `hack/DUALBOOT.md` of the BKBT
repository, "The save device".

- **Where.** `src/core1/eeprom.c`, under `#if BKBT`. `eeprom_writeBlocks` and `eeprom_readBlocks` ask
  `bkbt_launched()` (`src/core1/code_0.c`) on every call: TRUE, the launcher's token was there at boot,
  sends them to `bkbt_sramBlocks`; FALSE leaves them exactly as they were. No caller changed.
- **The offset** is the linker symbol `bkbt_sram_offset = 0x7000;` in `manual_syms.us.v10.txt`, used as
  `(u32) bkbt_sram_offset` (`include/bkbt.h`), so that the map carries it for the tools. `0x7000` is the
  border the two games share: below it the SRAM is the Tooie hack's. The link stops if the offset is
  below `0x7000`, not a multiple of 8, or leaves fewer than 512 bytes in the 32 KiB. The Tooie side's
  copy is `DUALBOOT_BK_SRAM_OFFSET` in `hack/bt/n64/src/dualboot.h` of the BKBT repository, guarded by a
  `_Static_assert` in its `src/save.c`; nothing compares the two, so change both. The BKBT repository's
  image builder refuses a build whose `bkbt_sram_offset` is not the border.
- **The route.** The game's own `osPiStartDma` cannot reach the SRAM: it ORs `osRomBase` into every
  address (`lib/ultralib/src/io/pirawdma.c:40`), and in the image that is `0xB3000000`, where this ROM
  is. A handle has a base of its own (`epirawdma.c:40`). So every transfer is a request (`OSIoMesg`) of
  type `OS_MESG_TYPE_EDMAREAD` or `OS_MESG_TYPE_EDMAWRITE` to the PI manager, with an SRAM handle of its
  own (`DEVICE_TYPE_SRAM`, `PI_DOMAIN2`, base `0xA8000000`). It is built by hand: `osEPiStartDma`, which
  builds the same request (`epidma.c:46-58`), is not among the libultra files this ROM links, and both
  builds share that list (`decompressed.us.v10.yaml`).
- **The buffer and the cache.** Every transfer goes through `sBkbtSramBuf`, on a 16-byte boundary inside
  `sBkbtSramRaw` and 32 whole cache lines long, never through the caller's buffer: the game's save
  records are `0x78` bytes apart and share cache lines with other variables. The buffer is written back
  before every transfer and invalidated after a read.
- **The answers.** 0 for success; -1 (`CONT_RANGE_ERROR`) for a range outside the 64 blocks, with nothing
  transferred; 8 (`CONT_NO_RESPONSE_ERROR`) for a write that does not read back as written. Every write
  is read back, into the buffer filled with `0xFF`, because the game wipes a file's old copy only when
  the write of the new one returned 0 (`gameFile_8033CFD4`, `src/core2/code_B5E00.c:88-103`). A read
  fills the buffer with `0xFF` first as well, what a blank EEPROM holds.
- **The bus timing.** The handle carries latency 5, pulse `0x0C`, page size `0x0D` and release 2, the
  values Ocarina of Time uses for its SRAM. libultra writes them to the PI's domain 2 registers at the
  first transfer through the handle (`EPI_SYNC`, `PRinternal/piint.h`), and they stay: after a visit to
  this game the Tooie hack's own SRAM transfers run on them too. No emulator used so far acts on them,
  and they have not been tried on a console.

What a change to it has to keep:

- **Main thread only.** One request, one queue and one buffer serve all transfers, and each call waits
  until its transfer is over. Every caller of the two functions is on the main thread, and `bkbt_frame`
  calls the return entry from there, so no save transfer is in flight when the console goes back to
  Tooie.
- **Never a caller's buffer as the DMA's RAM side, and the cache calls around every transfer.** No
  emulator models the cache: a mistake here passes every test and fails only on a console. The build of
  2026-10-14 was checked in the disassembly of `eeprom.c.o`.
- **Never outside the 64 blocks.** Below them is the Tooie hack's save.
- **The `EDMA` types.** The plain DMA types would go through `osRomBase` and read this game's own ROM.
- **The choice by `bkbt_launched()` alone.** A ROM run by itself must never write an SRAM it knows
  nothing about.

What it cost in the build of 2026-10-14 (`eeprom.c.o`: `.text` `0x4F0` bytes, `.bss` `0x2D0` bytes, from
the map): 1,728 bytes of RAM headroom and 640 bytes of ROM room ("Rules for new code").

Checked on mupen64plus with the BKBT repository's save check (`tools/dualboot_save_check.py`), on
2026-10-14: in the image, a planted save, a second boot from the file that run left (recompiler and
pure interpreter), no file at all, a hostile file, a played save and the range test; and the ROM by
itself, which wrote its EEPROM and no SRAM. The owner's report from trying it by hand is in
`hack/DUALBOOT.md` of the BKBT repository, "Status". Not checked: a console.

## The new-game intro

With `BKBT=1`, A on an empty game file no longer plays the intro, the cutscenes of Gruntilda and Tooty
that come before the game. The game starts where they end, in Spiral Mountain at the exit from Banjo's
house. There is no switch for it: every hack build skips, by itself and in the dual-boot image, and the
matching build and the retail ROM play the intro. The owner asked for it on 2026-10-14. The boot sequence
before the file select (logos, title screen, attract demo) is not changed by it ("The opening logos"
below is a separate change).

- **Where.** `gameSelect_update` in `src/core2/ch/gameSelect.c`, the new-game branch
  (`!gameFile_isNotEmpty`), under `#if BKBT`: one `timedFunc_set_3` that asks `transitionToMap` for
  `MAP_1_SM_SPIRAL_MOUNTAIN` at `WARP_SM_12_FROM_HOUSE`. The `#else` is retail's call, which asks for
  `MAP_85_CS_SPIRAL_MOUNTAIN_3`, the first intro map. The existing-game branch below it is not touched.
- **Why there.** Each intro map hands on to the next through `cutscenetrigger_update`
  (`src/core2/code_956B0.c:84-92`), and the last one, `MAP_89_CS_INTRO_BANJOS_HOUSE_2`, goes to
  `MAP_1_SM_SPIRAL_MOUNTAIN` at `WARP_SM_12_FROM_HOUSE` (`:89`). A loaded game that has not reached the
  Lair goes to the same place (`warp_smExitBanjosHouse`, `gameSelect.c:521`, `code_956B0.c:1331-1333`).
- **Why it is safe.** The intro saves nothing: a map change saves the game only when the level left is
  1..0xC or the map is the game-over room (`gameSelect_saveAndExit`, `gameSelect.c:727-740`), and every
  intro map is `LEVEL_D_CUTSCENE` (`src/core2/gc/section.c`). It also sets nothing a new game keeps: at
  the first frame Banjo can move, the BKBT repository's new-game check finds the game's state equal to
  that of a game that played the whole intro in every region it compares but one (the next item), and the
  first record the game saves equal byte for byte.
- **The one difference: the health pop-up.** The file select (map `0x91`) and Spiral Mountain are both
  `LEVEL_B_SPIRAL_MOUNTAIN` (`gc/section.c`), and `game_loadMap` reloads the level only when the level
  changes (`src/core2/code_5C870.c:91-101`). The intro goes from level B to D and back, so its arrival in
  Spiral Mountain reloads the level, and the reload arms the latch `D_80385FE0` (`itemscore_levelReset`,
  `src/core2/code_BEF20.c:221`, then `:242-275`), which shows the honeycomb health display once the
  screen transition is over. The skip stays in level B, as a loaded game in Spiral Mountain does, and the
  latch stays 0. It is never saved. The same latch charges a life lost in a death (`D_80385FE4`), which a
  new game has cleared (`code_BEF20.c:196`).
- **What it costs.** Nothing: the linker map of the build equals that of the build before it on every
  line that does not mention `.mdebug`, so no address moved, and the RAM headroom and the ROM room are
  what they were ("Rules for new code"). No new global, no new file.
- **Turning it off.** If Archipelago should ever want the intro back, a setting would be read at this
  same line (`docs/ROADMAP.md` of the BKBT repository, hack feature 8). Nothing reads one now.

Checked on mupen64plus with the BKBT repository's new-game check (`tools/bk_newgame_check.py`), on
2026-10-14, in the dual-boot image (recompiler and pure interpreter) and on the ROM by itself: a new game
from an empty file asks for map `0x01` exit `0x12`, enters no intro map, and Banjo can move 91 frames of
the emulator after A, where the build before this one, which plays the intro, took 5,043. Compared with a
run of that build: 10 frames into control 21 of the 22 regions the tool compares are equal (the flags,
scores, items and abilities, the stored and the level state, the game files and the global record in RAM,
the selected game, map and exit; the tool lists them), the 22nd is the latch (1 after the intro, 0 after
the skip), and the first saved record is identical. A loaded game goes where it always went (on the ROM
by itself): to Spiral Mountain, and to the Lair when its record has all nine moves of Spiral Mountain and
progress flag `0xBD`. The save check, the round trips and the anti-tamper checks pass on the same build,
and its matching build is the retail ROM (`hack/DUALBOOT.md` of the BKBT repository, "Checked"). By hand,
the owner reported on 2026-10-14 that on Project64 3.0.1 and ares v148 it "all held": a new game on an
empty file starting outside Banjo's house within seconds, a save made after it kept across closing the
emulator, a new game on the second file (the gaming chair, where the music is handled differently), and
an existing file loading where it always did. Not checked: a console; and whether the honeycombs pop up
on arrival was neither looked at in a run nor part of the report.

## The opening logos

With `BKBT=1`, Start on the opening logos (the N64 "N", then Rareware) goes to the title screen from the
first boot on. In retail it does nothing there after power-on and works only from the concert that
follows; after Save and Quit it works on the logos at once. The hack build now behaves as retail does
after that first visit. There is no switch: every hack build, by itself and in the dual-boot image; the
matching build and the retail ROM keep the wait. Start goes to the title, never straight to the file
select, in both. Done on 2026-10-15.

- **Where.** `src/core2/code_9B180.c:41-52`, under `#if BKBT`: `u8 D_8036DE78 = 0;`, where the `#else`
  keeps retail's `= 1`.
- **How it works** (read in the source). Both logos are map `0x1F` (`MAP_1F_CS_START_RAREWARE`); map
  `0x1E` (`MAP_1E_CS_START_NINTENDO`, a misleading name) is the concert. The table `D_8036DE00`
  (`code_9B180.c:32-39`) keys handlers by prop id, and prop `0x18` is in maps `0x1E` and `0x1F` only (the
  map setups decompressed with the BKBT repository's `tools/bk_rom.py`). Its per-frame handler
  `func_80322318` calls `func_8031D0C0` (`src/core2/code_956B0.c:250-257`), which on Start fades the music
  and calls `func_8034B968` (`src/core2/code_C4320.c:234`); that asks for entry 0 of the front-end table
  (`code_C4320.c:20-32`), map `0x02`, demo `0x5B`, mode 7: the title. Its load handler `func_803222F4`
  (`code_9B180.c:111-116`) clears `D_8036DE78` if it is set and then zeroes the entry's `unk4`, which
  makes the per-frame loop `func_80322490` (`:159-168`) skip the entry for that visit. So in retail the
  first map with the prop after power-on, the logos, ignores Start; the concert and every later visit
  (Save and Quit goes to map `0x1F`, `src/core2/code_14420.c:291`) take it. core2's `.data` is unpacked
  from the ROM at every boot (`src/core1/code_0.c:295`, `src/core1/overlaymanager.c:115`), a launch from
  Tooie included, so the flag was 1 at every launch.
- **Why it is safe.** Starting at 0 is the state retail is in after the first visit. Nothing else
  references `D_8036DE78` (grep of the decomp, its asm and its symbol files). Prop `0x18A`
  (`func_80322298`, `func_803222BC`) shares the flag but is in none of the 129 map setups (a static scan,
  not watched at run time).
- **What it costs.** One byte of core2's `.data`. No section changed size and no address in the map
  moved (compiled alone with IDO: `BKBT=0` gives identical sections, `BKBT=1` changes only `.data` byte
  `0x78`). Because core2 is stored compressed, the ROM differs from the build before in 26,135 bytes: the
  header checksum (`0x10..0x17`), core2's checksum slot in the boot code (`0x5E83..0x5E87`) and core2's
  compressed block from `0xF36666`.
- **The chord.** While L and R are held the game blanks every other button (`src/core1/joy.c:254-278`),
  so after a launch with the chord Start skips the logos only once the chord has been let go (seen on
  mupen64plus with that state simulated in RAM, not on this build). Below global timer 2
  `controller_getStartButtonSafe` returns 0 (`joy.c:163-169`; read, not run).

Checked on mupen64plus with the BKBT repository's logo check (`tools/bk_logo_check.py`), on 2026-10-15:
`--expect skip` passed on the ROM by itself (recompiler and pure interpreter) and in the dual-boot image:
Start at BK frame 60 asked for the title at +62, the title (map `0x02`, mode 7) came at +123, the concert
was never entered. On the build before (`7a5333cb...`) `--expect wait` passed (the logos asked for the
concert by themselves at +419; Start in the concert asked for the title within 2 frames) and `--expect
skip` failed. The new-game check, the save check, the round trips and the anti-tamper checks pass on the
same build, and its matching build is the retail ROM (`hack/DUALBOOT.md` of the BKBT repository,
"Checked"). By hand, the owner reported on 2026-10-15 that on Project64 3.0.1 and ares v148 Start on the
"N" went to the title after a launch from Tooie, still did after Save and Quit, and that left alone the
logos, concert and title played in order. Not checked: a console; picture and sound were not judged by
the tools.

## The Archipelago anchor

With `BKBT=1` and the Expansion Pak, the word at physical `0x400000` holds the KSEG0 address of
`bkbt_ap_anchor`, a 12-byte struct that says which game runs and which layout it has. An Archipelago
client starts from that word: everything else it will read moves with every build, and the word does
not. The Tooie hack publishes its own anchor in the same word, so a client sees which of the two games
runs. There is no switch: every hack build, by itself and in the dual-boot image; the matching build has
none of it. Done on 2026-10-17 (`docs/ROADMAP.md` of the BKBT repository, hack feature 1).

- **The layout** (`include/bkbt.h:88-119`), big-endian like everything the client reads:

  | Offset | Field | Value |
  |---|---|---|
  | `+0x00` | `u32 magic` | `0x424B4150`, `'BKAP'` (`BKBT_AP_MAGIC`) |
  | `+0x04` | `u8 game_id` | 1, Banjo-Kazooie (`BKBT_AP_GAME_BK`; the Tooie hack would be 2) |
  | `+0x05` | `u8 padding` | 0 |
  | `+0x06` | `u16 size` | `sizeof(BkbtApAnchor)`, 12 |
  | `+0x08` | `u16 version_major` | 0 |
  | `+0x0A` | `u8 version_minor` | 1 |
  | `+0x0B` | `u8 version_patch` | 0 |

  The version is that of this layout, not of the Archipelago world. A client checks magic, `game_id` and
  version before it trusts anything else, and reads nothing at or past `size`. Later features append
  their fields behind the 12 bytes (the settings of hack feature 2, the item queue and traps of 3, the
  flags of 4, the dialog queue of 5, the exit map of 6, death link of 7, a destination for Phase 2), so
  an older client still reads a newer struct and a newer client sees from `size` what an older ROM has.
- **Why the magic comes first.** A Tooie client reads `+0` of whatever the word points to as its version,
  a `u16` major, and stops before it follows a pointer when the major is neither 0 nor its own
  (`Archipelago-BanjoTooie/worlds/banjo_tooie/BTClient.py:87-95`, `:1395-1401` of the BKBT repository).
  Its signature test wants the word and the twelve words at `+0x04..+0x30` to be RDRAM addresses
  (`validate_bt_signature`, `:97-124`). This struct fails that test (`+0x04` is not a pointer) and reads
  as version 16971.65.80, so a stock Tooie client attached to the image stops with a version error when
  BK starts and writes nothing (read in the client; the anchor check imitates both tests). A version at
  `+0`, as the earlier sketch of the layout had it, would read as major 0, on which that client does not
  stop.
- **Where.** `src/core1/code_0.c`: the global `bkbt_ap_anchor` (`:163-173`), initialised data in core1's
  `.data` that no code fills. `bkbt_init` stores its address, `*BKBT_AP_ANCHOR = (u32) &bkbt_ap_anchor;`
  (`:200`), after the `osMemSize` test and before the test of the launch token, so the anchor is
  published on every boot, in a ROM run by itself (the standalone BK world needs it) and in the image
  alike. `bkbt_return` (`:244-252`) stores 0 there and then calls the payload's entry; both triggers in
  `bkbt_frame` go through it ("The frame hook").
- **Through KSEG1** (`BKBT_AP_ANCHOR` is `(volatile u32 *) 0xA0400000`), for the handoff block's reason:
  a cached copy of the line written back later would land on what the next game stored there. No
  emulator models the caches; only a console shows a mistake here.
- **Before it is published** the word reads `0xFFFFFFFF` in a ROM run by itself (the IPL3's RAM sizing
  leaves that at `0x400000`, `0x400004`, `0x600000` and `0x600004`, and the game never uses the upper
  4 MiB: its heap is a fixed array, `src/core1/memory.c:19-21`, `:189`) and 0 in the image (the Tooie
  launcher withdraws Tooie's anchor, `hack/bt/n64/asm/dualboot.asm:312-318` of the BKBT repository).
  Neither is an RDRAM address, and a client takes either as "not running".
- **Withdrawn before the return.** From `bkbt_return` until Tooie's boot publishes its own, the word is 0.
  Left in place it would point into this game's RAM while the return launcher and Tooie reuse it. Tooie's
  old struct (at `0x8041DD78` in the normal Tooie build) is still there after a launch and still passes
  the Tooie signature test, so the word, not the struct, is what keeps a Tooie client from writing into
  this game.
- **Without the Expansion Pak** nothing is published: `bkbt_init` has returned before.
- **For the tools.** `bkbt_ap_anchor` is a global, so the map carries it; it moves with every build
  (`0x80275BEC` with the struct alone, `0x80275C0C` once `bkbt_init` published it, `0x80275C2C` with
  `bkbt_return`). The BKBT repository's `tools/bk_fork_info.py` writes it into `bk-fork.json` and refuses
  a ROM whose core1 data does not read `'BKAP'` and `game_id` 1 at that symbol.
- **What it costs.** 80 bytes of RAM headroom: lair, the largest level overlay, ends at `0x80395890`
  instead of `0x80395840` (the map). The image builder counts 22,992 bytes of ROM room before the last
  128 KiB, 22,864 before ("Rules for new code").

Checked on mupen64plus with the BKBT repository's anchor check (`tools/bk_anchor_check.py`), on
2026-10-17, recompiler: `--expect anchor` passed on the ROM by itself (4000 frames, through maps `0x1F`,
`0x1E`, `0x02` and `0x22`; the struct read `424b41500100000c00000100`), on the autotest image (1500
frames) and with `--reset-at 1000` (published again after the reset); and on a test image of the same
build with the soak3 variant's Tooie hack (the owner's run, 1700 frames): published at frames 241, 782
and 1323, 0 on BK's last frame before each return (540, 1081, 1622) and Tooie's `0x8041DD78` one frame
later. A scratch build of the same design passed on the pure interpreter while the feature was prepared.
With the struct in the build and nothing publishing it, the check failed on the ROM by itself with the
word at `0xFFFFFFFF` (the owner's run). The round-trip check as it was failed on the published build,
because it wanted the word 0 while BK runs, and passes with its new rule; the round trips, the
anti-tamper, save, logo and new-game checks pass on the same build, and its matching build is the retail
ROM (`hack/DUALBOOT.md` of the BKBT repository, "Checked"). By hand, the owner reported on 2026-10-17
that on Project64 3.0.1 and ares v148 the chord both ways, a save and a load in Banjo-Kazooie and the logo
skip still work in the normal image of this build; the anchor cannot be seen by a player and was not read
there. Not checked: a console, where the KSEG1 store matters; any client (none exists for BK yet, and
EmuLoader has not attached by this word); any emulator but mupen64plus for the anchor itself.

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
- **core1 + core2 + the largest level overlay have about 66 KB to grow** (68,864 bytes in the first hack
  build, 68,336 with the dual-boot hook of contract 2, 68,224 with the pad record of 2026-10-12, 66,496
  with the save device of 2026-10-14, and the same with the new-game intro of that day and the logo
  skip of 2026-10-15, 66,416 with the Archipelago anchor of 2026-10-17: the largest, lair, ends at
  `0x80395840` in the maps of 2026-10-14 and 2026-10-15 and at `0x80395890` in that of 2026-10-17, read
  as `lair_VRAM_END`). Every level overlay is loaded at the end of core2
  and has to end below `0x803A5C00`. The link stops with a `BKBT:` message past that.
- **No RAM address of the game is a constant.** Everything moves with every build. Take addresses from
  `build/us.v10/banjo.us.v10.map`.
- **Exports for tools.** A tool that tests a ROM finds a global in the map. A `static` has no symbol
  (IDO emits none), so a static that a tool needs gets an exported pointer: `<type> *bkbt_ptr_<name> =
  <the static>;` under `#if BKBT`, in the file that owns the static and below it. The tool takes the
  pointer variable's address from the map, reads the pointer from RAM and checks that it lies inside
  that file's `.bss` range in the map. The pointer is initialised data, never bss: it has to hold the
  address from the moment the segment is loaded, and it must not become an uninitialised global ahead of
  a checksum slot (above). Only in core1 and core2, which are always in RAM; the level overlays share one
  address and are there one at a time. Today: `bkbt_ptr_pad` (`sBkbtPad`, the pad record that the chord
  is read from) in `src/core1/joy.c`, `bkbt_ptr_chord_seen` (`sBkbtChordSeen`, the chord counter) in
  `src/core1/code_0.c`, `bkbt_ptr_ucode` (`sUcodeData`, the rspboot buffer) in `src/core1/ucode.c`. The
  statics they point to are uninitialised, so each lies in its file's `.bss`, and core1 clears its bss
  at every boot. The game's own pad arrays (`sInputs`, `sInputsPrimary`) are not exported any more:
  the chord is not read from them, and a tool that wrote them would not reach it. The map being played
  and the game mode need no export: `sGsWorldData` (+4) and `sGameState` (+4) are globals. Neither does
  what the save check reads: `gSaveData`, `gameFile_saveData`, `gameFile_GameIdToFileIdMap`,
  `D_80383F04` (the spare file) and the function `savedata_8033CC98` are globals too, and so is the
  Archipelago anchor, `bkbt_ap_anchor` ("The Archipelago anchor"). Two of those names
  are ones the decomp generated; if upstream renames them, `tools/bk_fork_info.py` of the BKBT
  repository has to follow. The BKBT repository's new-game check reads more globals, by name from the
  map; several of those names were generated by the decomp too (its docstring lists them), and it stops
  and names one that is missing.
- **The ROM is exactly 16 MiB, by itself and in the dual-boot image**, where it sits unchanged at cart
  `0x3000000`. Nothing relocates it: the launcher there sets `osRomBase` (RDRAM `0x308`), and libultra's
  two raw PI functions OR that word into every address they are given
  (`lib/ultralib/src/io/pirawdma.c:40`, `pirawread.c:63`). Three things follow.
  - Hack code reads the cart the way the game does, by an offset into this ROM, through
    `parallel_readDMA`, `osPiStartDma` or `osPiReadIo`. It never passes an absolute cart address of the
    image to `parallel_readDMA` or to an `osPiRaw*` function: the OR would turn it into another address.
    The handle-based `osEPi*` functions and `osCartRomInit` have a base of their own and are not for this
    ROM's data. The one use of a handle is the save device, for the SRAM, which is not this ROM: a
    request with an SRAM handle, built by hand and sent to the PI manager, because `osEPiStartDma` is not
    linked ("The save device").
  - What the image holds outside this ROM (the payload slot at `0x2200000`, the info block behind it, the
    space up to `0x3000000`) cannot be reached through the game's PI functions. Code that needs it brings
    a PI routine of its own, and has to cope with a standalone ROM, where none of it exists. The save
    device does both: it reaches the SRAM with a handle, through the game's own PI manager, and by
    itself the ROM keeps its EEPROM.
  - Nothing may grow the ROM past 16 MiB or into its last 128 KiB (`0xFE0000` and up), which have to stay
    fill: at 64 MiB that is the top of the image, where some flashcarts keep save data. The image builder
    refuses a ROM that uses them. That leaves little: in the hack build of 2026-10-14 the last compressed
    overlay ends at cart `0xFDA6B0`, 22,864 bytes below that line (23,504 in the build of 2026-10-12,
    before the save device; 22,992 by the image builder's count in the build of 2026-10-17, with the
    Archipelago anchor).
