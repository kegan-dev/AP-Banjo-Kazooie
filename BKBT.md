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
6. **The save device** (2026-10-14; SRAM always since 2026-10-26, item 13): the game keeps its save in
   32 KiB of SRAM from `0x7000` on, by itself and in the dual-boot image, never in an EEPROM; the exported
   ROM's header declares SRAM 256k ("The save device" below). `src/core1/eeprom.c`,
   `include/core1/eeprom.h`, the record lengths in `src/core2/code_B5040.c` and `src/core2/code_B5E00.c`,
   and the linker symbol `bkbt_sram_offset` with its assertions in `manual_syms.us.v10.txt`. From
   2026-10-14 to 2026-10-26 only inside the image, chosen by `bkbt_launched()` in `src/core1/code_0.c`,
   with retail's 512 bytes; by itself the ROM kept its EEPROM.
7. **No new-game intro** (2026-10-14, at the owner's request): a new game starts in Spiral Mountain at the
   exit from Banjo's house, where the intro cutscenes end, instead of playing them. One call in
   `src/core2/ch/gameSelect.c`, in every hack build, by itself and in the dual-boot image ("The new-game
   intro" below). Since 2026-10-18 it is the setting `skip_intro`, on by default (item 10).
8. **Start skips the opening logos from the first boot** (2026-10-15): retail ignores Start on the logos
   after power-on and takes it only from the concert on. One initial value in `src/core2/code_9B180.c`,
   in every hack build ("The opening logos" below).
9. **The Archipelago anchor** (2026-10-17; `docs/ROADMAP.md` of the BKBT repository, hack feature 1):
   while the game runs, the word at physical `0x400000` holds the address of `bkbt_ap_anchor`, a 12-byte
   struct (16 since item 10, 20 since item 11, 24 since item 12) by which an Archipelago client finds the
   game and its layout, by itself and in the dual-boot image. `include/bkbt.h`, `src/core1/code_0.c` ("The
   Archipelago anchor" below).
10. **The Archipelago settings, with the file-select refusal** (2026-10-18; `docs/ROADMAP.md` of the BKBT
    repository, hack feature 2): `bkbt_ap_settings`, 8 bytes that the client writes and the game only
    reads, found through the anchor's new `+0x0C` (layout 0.2.0). The seed is written last and commits
    them (and, since item 11, the items). Until a client has, A on a file of the file select gets the
    buzzer; until 2026-10-26 R held with it started the game with the defaults (item 13 removed that).
    The first setting is the intro skip of item 7. `include/bkbt.h`, `src/core1/code_0.c`,
    `src/core2/ch/gameSelect.c` ("The Archipelago settings" below).
11. **The Archipelago items, version 1: moves** (2026-10-18; `docs/ROADMAP.md` of the BKBT repository,
    hack feature 3): `bkbt_ap_items`, 20 bytes, one per move, that the client writes and the game only
    reads, found through the anchor's new `+0x10` (layout 0.3.0). While the seed is committed and a file
    is played, `bkbt_frame` makes the game's 20 learned-move bits the mirror of them on every main-loop
    iteration: a move sent is learned, a move not sent is unlearned. Its known issue, the jump lesson
    in Spiral Mountain softlocking under a client, is resolved by item 12. `include/bkbt.h`,
    `src/core1/code_0.c` ("The Archipelago items" below).
12. **Molehills as Archipelago locations** (2026-10-22; `docs/ROADMAP.md` of the BKBT repository, hack
    feature 4, first part): `bkbt_ap_locations`, 20 bytes, one per move, that the game writes and the
    client only reads, found through the anchor's new `+0x14` (layout 0.4.0). A molehill's first talk
    sets its move's byte instead of teaching the move, which comes only as an item, and the byte, not
    the move, picks the lesson text or the refresher. Spiral Mountain's tutorials are gone and its
    bridge is always open. No client gate of its own. In RAM only until item 13 saved them.
    `include/bkbt.h`, `src/core1/code_0.c`, `src/core2/ch/mole.c`, `src/SM/ch/smbottles.c`,
    `src/SM/ch/attacktutorial.c`, `src/SM/smspawnqueue.c`, `src/core2/code_DC4B0.c` ("Molehills as
    Archipelago locations" below).
13. **The save bound to its seed, with the molehill locations saved** (2026-10-26; `docs/ROADMAP.md` of
    the BKBT repository, the feature after hack feature 4's molehills): every game record carries the seed
    it is played with and the 20 location bytes (`SaveData` `0x90` bytes, retail `0x78`), in SRAM always,
    the ROM by itself too (item 6). A on the file select starts or loads a game only while a client has
    committed its settings, and loads a file only when it is empty or was saved with the committed seed;
    the R escape of item 10 is gone. The A that starts a game latches its seed for saving and loads its
    locations. `include/save.h`, `include/bkbt.h`, `include/core1/eeprom.h`, `src/core1/code_0.c`,
    `src/core1/eeprom.c`, `src/core2/ch/gameSelect.c`, `src/core2/code_956B0.c`,
    `src/core2/code_B5040.c`, `src/core2/code_B5E00.c`, `src/core2/ch/bottlesbonuscursor.c`,
    `src/core2/gc/parade.c`, `src/core2/code_C4320.c` ("The save device", "The Archipelago settings",
    "Molehills as Archipelago locations" below).
14. **Empty honeycombs as Archipelago items and locations** (2026-10-26; `docs/ROADMAP.md` of the BKBT
    repository, hack features 3 and 4): picking up an empty honeycomb is a check (the game's own bit, which
    it saves itself) and no longer counts; the game publishes those bits as 25 bytes behind the molehill
    bytes of `bkbt_ap_locations`, and its honeycomb pieces and health total follow the total of
    `bkbt_ap_items.honeycombs` the client writes, raised one piece at a time with the HUD (layout 0.5.0;
    the anchor itself is unchanged). `include/bkbt.h`, `src/core1/code_0.c`, `src/core2/ba/marker.c`,
    `src/core2/code_BEF20.c`, `src/core2/code_B5E00.c`, `src/core2/code_956B0.c` ("Empty honeycombs as
    Archipelago items and locations" below).
15. **Jiggies as Archipelago items and locations** (2026-10-27; `docs/ROADMAP.md` of the BKBT repository,
    hack features 3 and 4): picking up a jiggy is a check (the game's own bit, which it saves itself) and no
    longer adds to the jiggies held; the game publishes those bits as 101 bytes behind the honeycomb bytes
    of `bkbt_ap_locations`, and gives the jiggies of `bkbt_ap_items.jiggies` the client writes that are
    above the jiggies it has given, a count it saves in the game record beside the jiggies held (layout
    0.6.0; the anchor itself is unchanged). The Lair's puzzles spend the jiggies held as in retail. The
    level's found count no longer shows its counter: the held-jiggy counter is the only jiggy count on the
    screen. `include/bkbt.h`, `include/save.h`, `src/core1/code_0.c`, `src/core2/ba/marker.c`,
    `src/core2/code_B5040.c`, `src/core2/code_BEF20.c`, `src/core2/code_956B0.c` ("Jiggies as Archipelago
    items and locations" below).
16. **Mumbo tokens as Archipelago items and locations** (2026-10-27; `docs/ROADMAP.md` of the BKBT
    repository, hack features 3 and 4): picking up a token is a check (the game's own bit, which it saves
    itself) and no longer adds to the tokens held, so no counter shows at the pickup; the game publishes
    those bits as 116 bytes behind the jiggy bytes of `bkbt_ap_locations`, and gives the tokens of
    `bkbt_ap_items.tokens` the client writes that are above the tokens it has given, a count it saves in
    the game record beside the tokens held, with the token counter (layout 0.7.0; the anchor itself is
    unchanged). Mumbo charges the tokens held as in retail. The give-and-take-back is one function for the
    jiggies and the tokens (`bkbt_apGiveHeld`). `include/bkbt.h`, `include/save.h`, `src/core1/code_0.c`,
    `src/core2/ch/mumbotoken.c`, `src/core2/code_B5040.c`, `src/core2/code_956B0.c` ("Mumbo tokens as
    Archipelago items and locations" below).

## The frame hook

With `BKBT=1` the game can hand the console back to Banjo-Tooie when it runs inside the dual-boot image of
the BKBT repository. This is dual-boot contract **2**. The same ROM runs by itself: there it finds nothing
of what follows and is the plain game.

Two functions in `src/core1/code_0.c` do it, with the layout in `include/bkbt.h`:

- `bkbt_init()` is the first statement of `core1_init`. It decides once per boot whether a return exists.
- `bkbt_frame()` is called by `mainLoop` on every iteration, after the optional `joy_update` and before
  `baMotor_80250C08`. It decides when to return. Since 2026-10-18 its first statement applies the
  Archipelago items ("The Archipelago items" below), which has nothing to do with the return.

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
  `bkbt_launched()`; the save code picked its device by it until 2026-10-26, and nothing calls it now: the
  BKBT repository's `tools/bk_fork_info.py` takes the symbol as a mark of a hack build), latches
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

It does two things for the return (since 2026-10-18 its first statement applies the Archipelago items,
"The Archipelago items", which is not part of this contract): on every iteration it looks at the chord,
and when there is a return entry it decides whether to call it. Without an entry nothing is called and
the handoff block is not even read.

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
With `BKBT=1` the game saves to 32 KiB of SRAM at cart address `0x08000000`, by itself and in the
dual-boot image, never to an EEPROM. The image is one cartridge with that SRAM and no EEPROM, and the
Tooie hack keeps its save there from offset 0; this game's part starts at `bkbt_sram_offset` (`0x7000`).
By itself the ROM has the same layout, and its header declares the SRAM, so that an emulator or a
flashcart gives it one. So since 2026-10-26 (item 13 above); from 2026-10-14 until then only inside the
image, chosen by `bkbt_launched()` and with retail's 512 bytes, while the ROM by itself kept its EEPROM.
The full description, with the SRAM map and what was run where, is in `hack/DUALBOOT.md` of the BKBT
repository, "The save device".

- **The layout** (`include/save.h:7-24`). The four game records (`SaveData`) one behind the other, then
  the global record (`GlobalData`, `0x20` bytes). A game record is retail's `+0x00..+0x73` (magic,
  `slotIndex`, the game's data, `padding[2]`, which the hack build uses since 2026-10-27 for the jiggies
  given and the Mumbo tokens given, "Jiggies as Archipelago items and locations", "Mumbo tokens as
  Archipelago items and locations"), then under `#if BKBT` `u32 seed` at `+0x74` and
  `u8 move_locations[BKBT_AP_MOVE_COUNT]` at `+0x78..+0x8B` ("The Archipelago settings", "Molehills as
  Archipelago locations"), then the checksum at `+0x8C`: `0x90` bytes, retail `0x78`. In the SRAM: the
  records at `0x7000`, `0x7090`, `0x7120`, `0x71B0`, the global record at `0x7240`, this game's part
  `0x7000..0x725F`, `BKBT_SRAM_BYTES` (`src/core1/eeprom.c:31`). No magic or version of its own: a save of
  a build with `0x78`-byte records fails the checksum at the new places and shows as empty files.
- **Where.** `src/core1/eeprom.c`, under `#if BKBT`: retail's `eeprom_writeBlocks` and `eeprom_readBlocks`
  are the `#else` (`:153-178`) and not in the hack build, and `bkbt_write` and `bkbt_read` (`:139-152`,
  declared in `include/core1/eeprom.h:4-12`) take their place, with `offset` and `count` in bytes from the
  start of record `file`, the global record as file 0 at offset `4 * sizeof(SaveData)`. Both call
  `bkbt_sram(direction, offset, buffer, size)` (`:72-137`), bytes from `bkbt_sram_offset`. Their callers
  in `src/core2/code_B5040.c` pass `sizeof(SaveData)` and `sizeof(GlobalData)` where retail passed 15 and
  4 blocks (`savedata_8033CA2C` `:321-341`, `savedata_8033CA9C` `:343-358`, `savedata_8033CC98`
  `:425-437`, `savedata_8033CE40` `:453-466`); the wipe of a file's old copy stays 8 bytes
  (`savedata_8033CCD0`, `:439-451`). Two lengths retail wrote as numbers follow `sizeof(SaveData)` too:
  the record check, `savedata_verify` (`:331-335`, retail `0x78`), and the spare-file copy in
  `gameFile_8033CFD4` (`src/core2/code_B5E00.c:87-93`, retail `0xF*8`). With retail's copy length every
  file lost its seed and locations when the file select was left, since `gameSelect_free` saves all three
  games that way (`src/core2/ch/gameSelect.c:315-340`; found by the review of 2026-10-26).
- **The header.** The BKBT repository's `hack/build_bk_rom.sh` runs its `tools/ed64romconfig.py -w
  sram256k` on the exported `bk-fork.z64` only (the tree's ROM and the matching build stay as `make` left
  them), before `tools/bk_fork_info.py`, so `bk-fork.json` carries the patched ROM's sha1. Bytes
  `0x3B..0x3F` go from `NBKE` `00` to `NEDE` `30`: game code `ED`, save type 3, SRAM 256k (the EverDrive's
  homebrew header, the same tool and setting as the Tooie hack's Makefile). They lie outside the range the
  boot checksum covers (`0x1000..0x101000`). mupen64plus logs "Save type: 2" with the header and "Save
  type: 0" without it, and its SRAM answers either way, so a missing header does not show there; the BKBT
  repository's tools refuse a build with `0x90`-byte records whose ROM does not declare SRAM
  (`tools/bk_eeprom.py`, `layout_of`).
- **The offset** is the linker symbol `bkbt_sram_offset = 0x7000;` in `manual_syms.us.v10.txt`, used as
  `(u32) bkbt_sram_offset` (`include/bkbt.h`), so that the map carries it for the tools. `0x7000` is the
  border the two games share: below it the SRAM is the Tooie hack's. The link stops if the offset is
  below `0x7000`, not a multiple of 8, or leaves fewer than 512 bytes in the 32 KiB (the assertion still
  says `0x200`, not `BKBT_SRAM_BYTES`; at `0x7000` either fits). The Tooie side's copy is
  `DUALBOOT_BK_SRAM_OFFSET` in `hack/bt/n64/src/dualboot.h` of the BKBT repository, guarded by a
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
  `sBkbtSramRaw` (`eeprom.c:36`) and `BKBT_SRAM_BYTES` (`0x260`, 38 whole cache lines) long, never
  through the caller's buffer, which shares cache lines with other variables. The buffer is written back
  before every transfer and invalidated after a read. Every transfer the game asks for has an SRAM offset
  and a length that are multiples of 8 (`0x90`, `0x20` and the 8-byte wipe).
- **The answers.** 0 for success; -1 (`CONT_RANGE_ERROR`) for a range outside `BKBT_SRAM_BYTES`, with
  nothing transferred (`offset < 0 || size <= 0 || size > BKBT_SRAM_BYTES - offset`, `:81`; the constant
  is cast to `s32`, so that the test stays signed: with `sizeof`'s unsigned type it wrapped, found by the
  review of 2026-10-26); 8 (`CONT_NO_RESPONSE_ERROR`) for a write that does not read back as written.
  Every write is read back, into the buffer filled with `0xFF`, because the game wipes a file's old copy
  only when the write of the new one returned 0 (`gameFile_8033CFD4`, `src/core2/code_B5E00.c:97-115`). A
  read fills the buffer with `0xFF` first as well, what a blank EEPROM holds.
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
  2026-10-14 was checked in the disassembly of `eeprom.c.o`; the build of 2026-10-26 was not.
- **Never outside `BKBT_SRAM_BYTES`.** Below them is the Tooie hack's save.
- **The `EDMA` types.** The plain DMA types would go through `osRomBase` and read this game's own ROM.
- **Lengths from `sizeof`.** Every length of a record follows `SaveData` and `GlobalData`; a number left
  over from retail cuts the record short. The BKBT repository's tools take the record size from the
  build (the spacing of `gameFile_saveData`) and refuse one they do not know.
- **The header on the export.** Without it, an emulator or flashcart that goes by the header gives the
  ROM by itself an EEPROM, and nothing it saves is kept (read, not tried; mupen64plus does not show it).

What it costs in the build of 2026-10-26 (`eeprom.c.o`: `.text` `0x3E0` bytes, `.bss` `0x330` bytes, from
the map): RAM headroom grew by 2,448 bytes, lair ending at `0x80395170` instead of `0x80395B00`, because
libultra's EEPROM functions are no longer linked (the map has none: nothing in the hack build calls one),
and the ROM room by the image builder's count grew by 1,104 bytes, to 23,728 ("Rules for new code"). In
the build of 2026-10-14 the save device had cost 1,728 bytes of RAM headroom and 640 bytes of ROM room.

Checked on mupen64plus with the BKBT repository's save check (`tools/dualboot_save_check.py`), on
2026-10-14: in the image, a planted save, a second boot from the file that run left (recompiler and
pure interpreter), no file at all, a hostile file, a played save and the range test; and the ROM by
itself, which wrote its EEPROM and no SRAM. On 2026-10-26, with the SRAM always and the stand-in client
the file select now needs: the autotest image from a planted game (12 of 12 lines), the ROM by itself (11
of 11, a 32 KiB `.sra` and no `.eep`) and the ROM by itself with a played save (`--play`, 16 of 16, the
record carrying the client's seed). The owner's reports from trying it by hand are in `hack/DUALBOOT.md`
of the BKBT repository, "Status". Not checked: a console, power lost during a save.

## The new-game intro

With `BKBT=1`, a new game on an empty game file no longer plays the intro, the cutscenes of Gruntilda
and Tooty that come before the game. The game starts where they end, in Spiral Mountain at the exit from
Banjo's house. The owner asked for it on 2026-10-14; until 2026-10-18 there was no switch for it. Since
then it is the Archipelago setting `skip_intro` ("The Archipelago settings" below), on in the hack's
defaults: every hack build skips, by itself and in the dual-boot image, unless a client has written
`skip_intro` 0, and the matching build and the retail ROM play the intro. The boot sequence before the
file select (logos, title screen, attract demo) is not changed by it ("The opening logos" below is a
separate change).

- **Where.** `gameSelect_update` in `src/core2/ch/gameSelect.c`, the new-game branch
  (`!gameFile_isNotEmpty`), under `#if BKBT` (`:497-515`): with `bkbt_apSettings()->skip_intro` set, one
  `timedFunc_set_3` that asks `transitionToMap` for `MAP_1_SM_SPIRAL_MOUNTAIN` at
  `WARP_SM_12_FROM_HOUSE`; with it 0, retail's call, which asks for `MAP_85_CS_SPIRAL_MOUNTAIN_3`, the
  first intro map. The `#else` is retail's call alone. The existing-game branch below it is not touched.
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
- **Turning it off.** A client writes `skip_intro` 0 and then the seed; a new game then asks for map
  `0x85` as retail does (since 2026-10-18; the settings check, "The Archipelago settings" below;
  `docs/ROADMAP.md` of the BKBT repository, hack feature 8). Until 2026-10-26 a game started without a
  client (R + A) took the default and skipped; since then no game starts without one.

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
`bkbt_ap_anchor`, a struct that says which game runs and which layout it has (12 bytes, 16 since the
settings of 2026-10-18, "The Archipelago settings" below, 20 since the items of the same day, "The
Archipelago items", and 24 since the molehills of 2026-10-22, "Molehills as Archipelago locations"; the
same 24 bytes in layout 0.5.0, the honeycombs of 2026-10-26, 0.6.0, the jiggies of 2026-10-27, and
0.7.0, the Mumbo tokens of the same day, which grew the structs it points to). An
Archipelago
client starts from that word: everything else it will read moves with every build, and the word does
not. The Tooie hack publishes its own anchor in the same word, so a client sees which of the two games
runs. There is no switch: every hack build, by itself and in the dual-boot image; the matching build has
none of it. Done on 2026-10-17 (`docs/ROADMAP.md` of the BKBT repository, hack feature 1).

- **The layout** (`include/bkbt.h:180-214`), big-endian like everything the client reads, version 0.7.0
  since the Mumbo tokens of 2026-10-27 (0.2.0 with the settings of 2026-10-18, 0.3.0 with the items of
  that day, 0.4.0 with the molehills of 2026-10-22, 0.5.0 with the honeycombs of 2026-10-26, 0.6.0 with the
  jiggies of 2026-10-27):

  | Offset | Field | Value |
  |---|---|---|
  | `+0x00` | `u32 magic` | `0x424B4150`, `'BKAP'` (`BKBT_AP_MAGIC`) |
  | `+0x04` | `u8 game_id` | 1, Banjo-Kazooie (`BKBT_AP_GAME_BK`; the Tooie hack would be 2) |
  | `+0x05` | `u8 padding` | 0 |
  | `+0x06` | `u16 size` | `sizeof(BkbtApAnchor)`, 24 (12 in 0.1.0, 16 in 0.2.0, 20 in 0.3.0, 24 from 0.4.0 on) |
  | `+0x08` | `u16 version_major` | 0 |
  | `+0x0A` | `u8 version_minor` | 7 (1 in 0.1.0, 2 in 0.2.0, 3 in 0.3.0, 4 in 0.4.0, 5 in 0.5.0, 6 in 0.6.0) |
  | `+0x0B` | `u8 version_patch` | 0 |
  | `+0x0C` | `BkbtApSettings *settings` | `&bkbt_ap_settings`, which the client writes (since 0.2.0; "The Archipelago settings") |
  | `+0x10` | `BkbtApItems *items` | `&bkbt_ap_items`, which the client writes (since 0.3.0; "The Archipelago items") |
  | `+0x14` | `BkbtApLocations *locations` | `&bkbt_ap_locations`, which the game writes and the client reads (since 0.4.0; "Molehills as Archipelago locations") |

  The version is that of this layout, not of the Archipelago world. A client checks magic, `game_id` and
  version before it trusts anything else, and reads nothing at or past `size`. The version also says the
  layout of what the pointers point to: in 0.5.0 the items grew from 20 to 24 bytes and the locations
  from 20 to 48, in 0.6.0 the locations to 152 and in 0.7.0 to 268 (the items took a byte of their
  padding each time), behind an unchanged anchor. Later features append
  their fields behind the 12 bytes of 0.1.0 (the settings pointer of hack feature 2 came first, in
  0.2.0, the items pointer of 3 second, in 0.3.0, and the locations pointer of 4 third, in 0.4.0; then
  traps, the rest of 4's flags, the dialog queue of 5, the exit map of 6, death link of 7, a destination
  for Phase 2), so an older client still reads a newer struct and a newer client sees from `size` what
  an older ROM has.
- **Why the magic comes first.** A Tooie client reads `+0` of whatever the word points to as its version,
  a `u16` major, and stops before it follows a pointer when the major is neither 0 nor its own
  (`Archipelago-BanjoTooie/worlds/banjo_tooie/BTClient.py:87-95`, `:1395-1401` of the BKBT repository).
  Its signature test wants the word and the twelve words at `+0x04..+0x30` to be RDRAM addresses
  (`validate_bt_signature`, `:97-124`). This struct fails that test (`+0x04` is not a pointer) and reads
  as version 16971.65.80, so a stock Tooie client attached to the image stops with a version error when
  BK starts and writes nothing (read in the client; the anchor check imitates both tests). A version at
  `+0`, as the earlier sketch of the layout had it, would read as major 0, on which that client does not
  stop.
- **Where.** `src/core1/code_0.c`: the global `bkbt_ap_anchor` (`:239-252` in the molehills' build of
  2026-10-22), initialised data in core1's `.data` that no code fills. `bkbt_init` stores its address,
  `*BKBT_AP_ANCHOR = (u32) &bkbt_ap_anchor;` (`:279`), after the `osMemSize` test and before the test of
  the launch token, so the anchor is published on every boot, in a ROM run by itself (the standalone BK
  world needs it) and in the image alike. `bkbt_return` (`:323-331`) stores 0 there and then calls the
  payload's entry; both triggers in
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
  `bkbt_return`, `0x80275C74` with the settings of 2026-10-18, `0x80275DAC` with the items of that day,
  `0x80275DC0` with the molehills of 2026-10-22, `0x80275248` with the save in SRAM of 2026-10-26, whose
  core1 code is smaller without libultra's EEPROM functions). The BKBT repository's
  `tools/bk_fork_info.py` writes it into `bk-fork.json` and refuses a ROM whose core1 data does not read
  `'BKAP'` and `game_id` 1 at that symbol.
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

## The Archipelago settings

With `BKBT=1` the game plays by settings that an Archipelago client writes: `bkbt_ap_settings`, an
8-byte struct that the anchor points to from layout 0.2.0 on. This first version holds the seed of the
generated game and one setting, `skip_intro`. Until a client has written them the file select refuses
A, so that no game is started or loaded by the wrong settings, and since 2026-10-26 it also refuses a
file saved with another seed (below); there is no escape any more (until then R held with A started a
game with the hack's defaults). There is no switch: every hack build, by itself and in the dual-boot
image; the matching build has none of it. Done on 2026-10-18 (`docs/ROADMAP.md` of the BKBT repository,
hack feature 2); the refusal with an escape, and the seed written last as the word that commits the
rest, are the owner's decisions of 2026-10-17; the seed bound to the save, and no escape, the owner's of
2026-10-22 and 2026-10-26.

- **The layout** (`include/bkbt.h:83-92`), big-endian, behind the anchor's `+0x0C` (`:158`):

  | Offset | Field | Value |
  |---|---|---|
  | `+0x00` | `u32 seed` | the generated game's seed; written last; 0 = not written since this boot |
  | `+0x04` | `u8 skip_intro` | 1: a new game skips the intro, 0: it plays it ("The new-game intro") |
  | `+0x05` | `u8 padding[3]` | 0 |

  With the pointer the anchor grew to 16 bytes and its version to 0.2.0 (`BKBT_AP_VERSION_MINOR`,
  `:142`, 3 since the items, 4 since the locations): a ROM whose anchor says 0.1.0 or size 12 has no
  settings.
- **Where.** `src/core1/code_0.c` (lines of the build of 2026-10-26): the global `bkbt_ap_settings`
  (`:164-170`), initialised data in core1's `.data`, all 0; `bkbt_apSettingsCommitted()` (`:175-179`),
  `seed != 0`, the only place that rule is written; `sBkbtApDefaults` (`:181-187`), `{0, TRUE, {0, 0,
  0}}`, the hack's behaviour from before settings existed; `bkbt_apSettings()` (`:189-197`), which returns
  `&bkbt_ap_settings` once committed and the defaults until then. The anchor's initialiser holds
  `&bkbt_ap_settings` (`:252`), which the linker fills in. Game code reads settings only through
  `bkbt_apSettings`, so that no reader sees a half-written struct or takes its zeros for values (the file
  select's seed test reads `bkbt_ap_settings.seed` directly, after `bkbt_apSettingsCommitted()`).
  Declarations: `include/bkbt.h:94`, `:100-101`.
- **Every boot starts at 0.** core1's `.data` is loaded again at every boot, a reset and each launch
  from Tooie included, so after either the settings read 0 and the client has to write them again.
- **The protocol, for the client** (none exists yet). One writer per region: the client writes the
  settings, the game only reads them. The client writes every other field first and the seed last; to
  change the settings in a running game it writes seed 0 first, then the fields, then the seed. The
  Archipelago world must never generate seed 0 (the Tooie world draws `randint(12212, 9090763)`,
  `Archipelago-BanjoTooie/worlds/banjo_tooie/__init__.py:1121` of the BKBT repository). The client
  notices a boot by comparing the game's seed with its own, as the Tooie client does every 0.2 s
  (`BTClient.py:1404-1418`), and then writes again. The Tooie client writes its seed first
  (`client/game.py:169-171`), which leaves a moment in which the seed says written and the rest is not;
  the order here avoids it. The client should read the anchor word and the magic again right before it
  writes, as the Tooie client looks its pointer up again for every write (`game.py:111-130`).
- **The refusal.** `gameSelect_update` in `src/core2/ch/gameSelect.c`, state `GAME_SELECT_IDLE`, under
  `#if BKBT` (`:549-556`): a branch ahead of retail's A branch, taken when A is pressed (`face_buttons`
  1) and either `!bkbt_apSettingsCommitted()` or the game is not empty and `bkbt_file_seed` is not
  `bkbt_ap_settings.seed` (the next item), plays `COMUSIC_2C_BUZZER` at 22000, retail's buzzer for Z on
  an empty file, and does nothing else. Retail's lines are unchanged. It covers loading an existing file
  and starting a new game alike; Z (erase) is not gated. `bkbt.h` is included under `#if BKBT` at the top
  (`:15-17`). No text is shown in this version (the Tooie hack shows some): a player hears only the
  buzzer.
- **The seed bound to the save** (2026-10-26). Every game record carries the seed it was played with
  (`SaveData.seed`, `include/save.h:12-16`; "The save device"). Two globals in
  `src/core1/code_0.c:172-173` (`include/bkbt.h:165-174`), both 0 at boot: `bkbt_file_seed`, the seed of
  the game the file select shows, set by `saveData_load` from the record
  (`src/core2/code_B5040.c:382-385`) whenever the file select loads a hovered game that is not empty
  (`setGameInformationZoombox`, `gameSelect.c:228-238`) and zeroed by `clearScoreStates`
  (`src/core2/code_956B0.c:1496-1503`), for the refusal only; and `bkbt_playing_seed`, set to the
  committed seed by the A that starts or loads a game (`gameSelect.c:558-564`, after the refusal has
  matched the file's seed, or for a new game) and written into every record `saveData_create` makes
  (`code_B5040.c:415-421`). Nothing else writes the playing seed, so a client that changes its seed
  during play, the reloads of Bottles' Bonus, the parade and the ending, and `clearScoreStates` cannot
  change the seed a save is bound to; until the review of 2026-10-26 found it, the save took whatever
  seed was committed when it was made. A file of another seed, or a game record with seed 0 (this build
  makes none; the BKBT repository's seed check plants one), is refused while a client is attached. A seed
  stays committed across Save and Quit: only a boot resets the settings.
- **No escape** (since 2026-10-26, the owner's decision). Until then R held with A fell through to
  retail's branch and started the game with the defaults, for playing without a client, and under a
  client it also passed the seed test (found by the review of 2026-10-26). R had been chosen because it
  is free on the file select, because R alone does not trigger the blanking that L + R does
  (`src/core1/joy.c:254-278`), and because Z opens the erase prompt. Hand tests now need a client: the
  BKBT repository's stand-in `tools/bk_hand_client.py`.
- **The first setting.** In the new-game branch (`:497-515`), `if (bkbt_apSettings()->skip_intro)`
  chooses between `MAP_1_SM_SPIRAL_MOUNTAIN` at `WARP_SM_12_FROM_HOUSE` and retail's
  `MAP_85_CS_SPIRAL_MOUNTAIN_3` ("The new-game intro").
- **A client that commits again during play** switches `bkbt_apSettings` to its new values for every
  later read. That changes nothing for `skip_intro`, which is read once, at the start, nor for the seed
  a save is bound to; a setting read during play will have to say what it means. (Until 2026-10-26 this
  was the case of a client that came after an R start.)
- **Not `volatile`.** The game reads the settings through a function call at the moment it acts on
  them, and relies on IDO keeping no copy across such a call (read, not tested). A client that wrote by
  DMA would also meet the cache on a console; untried.
- **What it costs.** 208 bytes of RAM headroom: lair ends at `0x80395960` instead of `0x80395890` (the
  map). The image builder counts 22,800 bytes of ROM room before the last 128 KiB, 22,992 before ("Rules
  for new code"). In the settings' build of 2026-10-18 `bkbt_ap_settings` is at `0x80275C6C` and the
  anchor at `0x80275C74`; both move with every build (`0x80275D8C` and `0x80275DAC` in the items' build
  of that day, `0x80275D8C` and `0x80275DC0` in the molehills' build of 2026-10-22).

Checked on mupen64plus with the BKBT repository's settings check (`tools/bk_settings_check.py`, a stand-in
client that finds the settings through the anchor and writes `skip_intro` and then the seed), on
2026-10-18, recompiler unless said: without a client, `--expect refused` passed on the autotest image (A
at BK+388, +629 and +870, nothing asked for); a client from boot that wrote `skip_intro` 0 got the
intro on the autotest image; on the ROM by itself a client that wrote only after a refused A
(`--client after-a`) got the intro on the next A, and without a client R with A (`--escape`) started a
game that skipped the intro, on the pure interpreter. While the feature was built (the owner's runs): R
with A asked for map `0x01` exit `0x12` at BK+420, a client's `skip_intro` 0 for map `0x85` at BK+420,
entered at +448; with a reset at BK+200 the settings read 0 at +201, the client wrote them again at
+204 and the intro followed (+564, +592); on the soak3 image (`--frames 1700 --expect boots`) the
settings read 0 at each of three launches (BK+0, +541, +1082) and the client wrote once per launch (+0,
+546, +1086). On the way: with the header changed alone the ROM stayed `d5685068...`; with the struct
and the pointer, the anchor check of layout 0.1.0 failed ("size 16 version 0.2.0 (want ... 12,
0.1.0)"), and a client's `skip_intro` 0 still got the skip, because nothing read the settings; with the
game reading the struct directly a new game without a client played the intro, as the owner predicted
(zero is not a default); with the refusal and before the escape the new-game check as it was failed (no
request after its A). The anchor check of layout 0.2.0 (8 lines, the `+0x0C` pointer among them), the
round trips, the anti-tamper, save, logo and new-game checks (the save and new-game checks now hold R
with A) pass on the same build, and its matching build is the retail ROM (`hack/DUALBOOT.md` of the BKBT
repository, "Checked"). By hand, the owner reported on 2026-10-18 that on Project64 3.0.1 and ares v148,
in the normal image of this build, A alone on a file got the buzzer, R + A on an empty file started a
new game with the intro skipped, R + A on an existing file loaded it, A alone still buzzed after a
switch to Tooie and back, and the chord both ways and the logo skip still worked. Values from a client
were not tried by hand: there is none, and it would take a memory editor. Not checked: a console; a
real client or EmuLoader; the buzzer, which no tool listens for.

The seed bound to the save and the escape gone, 2026-10-26: checked on mupen64plus (recompiler) with the
BKBT repository's seed check (`tools/bk_seed_check.py`: it plants records with chosen seeds and location
bytes, plays the stand-in client, moves the cursor, presses A and judges `bkbt_file_seed`, the location
bytes and the record saved), all passing, on the fork's ROM by itself unless said: a file of the client's
seed loaded; a file of another seed, a record with seed 0, and any A without a client refused; a new
game, saved, reset and loaded again, bound to the client's seed; a loaded game with locations through a
save and a reload; another seed's file browsed (its locations not published) and then game 1 of the
client's seed chosen, through a save and a reload; an erased game started anew, through a save and a
reload; game 2 through a save and a reload; and on the autotest image a file of another seed refused.
The settings check without a client (`--expect refused`; `--escape` now expects that too) and a client's
`skip_intro` 0 on the autotest image passed. By hand, the owner reported on 2026-10-26 that on ares v148
and Project64 3.0.1, with `tools/bk_hand_client.py`, a game of another seed would not load, a new game
started with a seed committed, and a game of the same seed loaded (`hack/DUALBOOT.md` of the BKBT
repository, "Status"). Not shown: the three mid-session reloads (the harness cannot reach them), a
console.

## The Archipelago items

With `BKBT=1` the game gives the player what an Archipelago client says was received: `bkbt_ap_items`,
a 20-byte struct that the anchor points to from layout 0.3.0 on. This first version holds the moves
only, one byte per move; since layout 0.5.0 (2026-10-26) a byte behind them holds the empty honeycombs
received, which have a section of their own ("Empty honeycombs as Archipelago items and locations"), and
since layout 0.6.0 (2026-10-27) the next byte holds the jiggies received ("Jiggies as Archipelago items and
locations"), and since layout 0.7.0 (the same day) the next the Mumbo tokens received ("Mumbo tokens as
Archipelago items and locations"); the other counted kinds (notes, jinjos) and traps are to come
(`docs/ROADMAP.md` of the BKBT repository, hack feature 3). The client writes totals, as the Tooie
client writes counts of what was received (`docs/upstream-banjo-tooie.md` of the BKBT repository), and
the game makes its learned moves the mirror of them: while the seed is committed and a file is played,
on every main-loop iteration, a move sent is learned and a move not sent is unlearned. There is no
switch: every hack build, by itself and in the dual-boot image; the matching build has none of it. Done
on 2026-10-18 (hack feature 3, version 1); the mirrored totals, one byte per move indexed by the game's
own ability numbers, and a client that always sends moves 3 and `0x13`, are the owner's decisions of
that day. Its known issue, the Spiral Mountain molehill that teaches the jumps softlocking with a client
attached, is resolved since 2026-10-22 by hack feature 4's molehills (below; "Molehills as Archipelago
locations").

- **The layout** (`include/bkbt.h:103-114`), behind the anchor's `+0x10` (`:159`):

  | Offset | Field | Value |
  |---|---|---|
  | `+0x00` | `u8 moves[20]` | indexed by `enum ability_e` (`include/core2/abilityprogress.h:23-44`: 0 barge to `0x13` the first note door); not 0 = received, 0 = not |

  `BKBT_AP_MOVE_COUNT` is 20 (`:108`); only `u8`, so no padding. With the pointer the anchor grew to 20
  bytes and its version to 0.3.0 (`BKBT_AP_VERSION_MINOR`, `:142`, 4 since the locations): a ROM whose
  anchor says 0.2.0 or size 16 has no items.
- **Where.** `src/core1/code_0.c`: the global `bkbt_ap_items` (`:196-197`), initialised data in core1's
  `.data`, all 0; `bkbt_apItemsAllowed()` (`:211-223`), the gate (below); `bkbt_apApplyItems()`
  (`:225-237`), which returns unless the gate allows and otherwise calls `ability_setLearned(i,
  moves[i] != 0)` for each i below `BKBT_AP_MOVE_COUNT` and counts the pass. It is the first statement
  of `bkbt_frame` (`:341-342`), so it runs once per `mainLoop` iteration, ahead of the chord.
  `core2/abilityprogress.h` is included under `#if BKBT` at the top (`:8-11`). The anchor's initialiser
  holds `&bkbt_ap_items` (`:250`). Declaration: `include/bkbt.h:114`. (Lines of the molehills' build of
  2026-10-22.)
- **The mirror.** `ability_setLearned` (`src/core2/abilityprogress.c:114-120`) sets or clears one bit of
  `learnedAbilities` and does nothing else: no dialog, no sound, no flag. So a received move is there
  without Bottles' lesson, and a move the client has not sent is unlearned on the next iteration,
  whoever taught it. Bits 20 to 31 of the word are not touched: nothing reads them, and in the hack build
  only writers of the whole word reach them (the attract demos set every bit,
  `src/core2/code_C4320.c:210`; Furnace Fun keeps a copy, clears the word on its board and puts the copy
  back, `src/lair/code_5ED0.c:660`, `:956`, `:1086`). What the game has been given stays in its save:
  the learned word is saved with each game (`ability_getSizeAndPtr`, `abilityprogress.c:131-134`; at
  `0x6A` of the record laid out by `savedata_init`, `src/core2/code_B5040.c:95-131`).
- **The gate** (`bkbt_apItemsAllowed`). The items are applied only while the settings are committed
  (`bkbt_apSettingsCommitted()`, the seed not 0), the map is none of `MAP_1E_CS_START_NINTENDO` (the
  concert), `MAP_1F_CS_START_RAREWARE` (the logos), `MAP_91_FILE_SELECT` and `MAP_8E_GL_FURNACE_FUN`,
  and the game mode is `GAME_MODE_3_NORMAL`. So not in the front end, not in the attract demos (mode 7),
  in pause (4) or in the other modes that play recorded input (8, 9, 10), and not on the Furnace Fun
  board, where the mirror would give back the moves the board takes away (read, not run). Without the
  seed test a save loaded with R and no client lost every move, and saved that; without the map and
  mode tests the items were applied on the logos, the title, the attract demos, the file select and in
  pause (the owner's runs, below).
- **What taking a move back does to a move in use** (read). Almost every move is checked only when it
  starts (`src/core2/bainput.c:8-66`; in `src/core2/`: `bs/stand.c:70`, `bs/walk.c:99-102`,
  `bs/crouch.c:183`, `bs/bFly.c:299`, `bs/swim.c:247`, `ba/marker.c:735-805`), so one taken back while
  in use finishes. Two are checked on every frame: the high jump (`bs/jump.c:102`: a high jump taken
  back is cut short) and climbing (`climb.c:56-61`, `bs/climb.c:94-126`: the player lets go). Run: claw
  swipe taken back at control+50, in a swipe that had started at +42; the swipe ended at +81, and B at
  +100 no longer swiped. No path to a crash was found.
- **The protocol, for the client.** The seed commits the items as it commits the settings: **the client
  writes the settings and the items first and the seed last, and to change either in a running game it
  writes seed 0 first, then the fields, then the seed.** The game cannot see the order, so the client's
  order is the only defence: with the seed first, a client that arrives while a file is played makes the
  game commit to moves all 0 and unlearn what it had received until the moves arrive (the items check's
  `--bad-order`: claw swipe unlearned at BK+505..+510, learned again at +511). Every boot starts the
  items at 0 (initialised data), a reset and each launch from Tooie included, and the client writes them
  again with the settings. The client sends every move the player should have, the ones the world does
  not shuffle included: the mirror unlearns every move it does not send.
- **Moves 3 and `0x13`: the client always sends them.** The game mirrors all 20 bits, and these two are
  sent always, like every move the world does not shuffle (the owner's decision of 2026-10-18). Camera
  control (3) is Bottles' first molehill's move (`src/SM/ch/smbottles.c:69`) and read by nothing
  (`can_control_camera`, `src/core2/playerutils.c:31-32`, has no caller). The first note door (`0x13`) is
  checked by every note door (`src/lair/lairspawnqueue.c:971`) and is a molehill's move
  (`src/core2/ch/mole.c:70`): not sent, no note door opens. Since 2026-10-22 neither molehill teaches
  its move ("Molehills as Archipelago locations"), so the client is the only way to either. Shuffling
  either later needs
  logic work; BK_Randomizer shuffles neither (its molehill shuffle covers nine moves,
  `docs/upstream-bk-randomizer.md:75-77` of the BKBT repository).
- **Without a client** no game starts or loads since 2026-10-26: A is refused ("The Archipelago
  settings"). Until then R held with A started one with the seed 0, nothing was applied and the save's
  moves stayed (in this build the molehills taught as in retail; from 2026-10-22 they taught nothing,
  "Molehills as Archipelago locations"), and a client that committed later, during play, made the moves
  the mirror from then on. That client cannot exist any more; the items check's `--client control` is now
  a client that commits no move at first and sends them at control+20, the way a running game receives
  them.
- **Resolved on 2026-10-22: the molehills under a client** (the known issue of this build, until hack
  feature 4). A molehill whose move the client had not sent taught it (`player_unlockAbility`), and on
  the next iteration the mirror unlearned it, so every talk played the lesson for a new move, not the
  refresher, with what that lesson hands out (`src/core2/ch/mole.c:238-264`,
  `src/SM/ch/smbottles.c:304-328` in this build; read). Found by the owner's hand test of 2026-10-18: the
  Spiral Mountain molehill that teaches the jumps, where Bottles asks for a tall jump, softlocked. The
  lesson taught the high jump, the next iteration the mirror unlearned it (the stand-in client sent only
  3, 4 and `0x13`), and because the high jump is checked on every frame (`bs/jump.c:102`) only the short
  jump happened, so the lesson could not be completed. Camera control's lesson was tried too and was
  harmless; the other molehills were not tried. The fix is hack feature 4's first part: the molehills
  check locations instead of teaching, and the jump tutorial is no longer spawned ("Molehills as
  Archipelago locations").
- **Resolved on 2026-10-26: a save of another seed.** Until the seed was bound to the save, a save of
  another seed, or one played without a client, loaded while this slot's client ran, got this slot's
  moves mirrored into it, and its next save kept them. Now the file select refuses such a file ("The
  Archipelago settings").
- **A map-flag hazard** (read, not run). The first high jump, flap and flip (`ability_setUsedWithDialog`,
  `src/core2/abilityprogress.c:25-38`) set Spiral Mountain's map flags 8, 9 and `0xA` without looking at
  the map. In retail those moves are taught in Spiral Mountain and first used there; one received
  elsewhere can be first used elsewhere, and then sets id 8, 9 or `0xA` in that map's flags. What those
  ids mean in the other maps was not looked at.
- **For the tools: the pass counter.** `sBkbtApItemPasses` (`:203-209`) counts the iterations on which
  the items were applied, exported as `bkbt_ptr_ap_item_passes` ("Exports for tools"): a test that
  expects the items not to be applied somewhere proves nothing unless the game is shown to apply them
  elsewhere. The items check judges by it that no pass falls on the logos, the concert, the file select,
  a mode other than 3 or a seed of 0.
- **What it costs.** 320 bytes of RAM headroom: lair ends at `0x80395AA0` instead of `0x80395960` (the
  map). The image builder counts 22,592 bytes of ROM room before the last 128 KiB, 22,800 before ("Rules
  for new code"). In the items' build of 2026-10-18 `bkbt_ap_items` is at `0x80275D94`,
  `bkbt_ptr_ap_item_passes` at `0x80275DA8` and the anchor at `0x80275DAC`; all move with every build
  (`0x80275D94`, `0x80275DBC` and `0x80275DC0` in the molehills' build of 2026-10-22).

Checked on mupen64plus with the BKBT repository's items check (`tools/bk_items_check.py`, a stand-in
client that finds the settings and the items through the anchor and writes `skip_intro`, the moves and
then the seed, again after every boot), on 2026-10-18. On this build: a client from boot that sent claw
swipe, on the ROM by itself on the pure interpreter (`--expect granted`: learned at control+10, B
swiped, the saved record had it), and on the soak3 image (`--frames 1700 --expect boots`: the items read
0 at every launch and the client wrote them again). While the feature was built (the owner's runs,
recompiler, the ROM by itself unless said): with the struct and the pointer alone the anchor check of
layout 0.2.0 failed ("size 20 version 0.3.0"), and `granted` and `cleared` failed while `kept` passed,
because nothing read the items; with the mirror and no gate `granted` and `cleared` passed, `kept`
failed (learned `0x00001010` became 0 after the load, and was saved) and a client that never wrote the
seed (`--no-seed`) got its move at BK+449; with the seed test everything passed: claw swipe granted; a
save with claw swipe and roll and a client that sent claw swipe lost roll, and the record kept claw
swipe only (`cleared`); the same save without a client, R with A, kept both (`kept`); a client that came
during play in the right order granted within 2 frames of its commit with nothing lost on the way, and
in the wrong order showed the window above; with the pass counter the gate run failed, with passes on
102 frames of the logos, 2,535 of the title and the attract demos (mode 7), 161 of the file select and
97 of pause (mode 4); with the map and mode tests (the last step, with this build's addresses) it passed
through 2,500 frames of attract demos and the pause menu with 377 passes, every one while a file was
played; saved, reset and loaded with the same client kept claw swipe; claw swipe taken back during play
(`--unreceive 4`) was unlearned within 2 frames and the record kept roll only; and in a test image of
the build claw swipe was granted with the record in the SRAM, a new emulator from that save with the
same client kept it, and one with a client that sent nothing cleared it.
The anchor check of layout 0.3.0 (9 lines, the `+0x10` pointer among them; also on the soak3 image), the
round trips, the anti-tamper, save, logo, new-game and settings checks pass on the same build, and its
matching build is the retail ROM (`hack/DUALBOOT.md` of the BKBT repository, "Checked"). By hand, the
owner reported on 2026-10-18 that on Project64 3.0.1 and ares v148, in the normal image of this build,
without a client everything behaved as retail does (the molehills taught); that the BKBT repository's
stand-in client `tools/bk_hand_client.py`, sending moves 3, 4 and `0x13`, attached and wrote on both
emulators, the first time a client wrote into this game there; that the attract demos did not take its
moves; that loading a game set the learned moves to exactly what it sent; and that the jump molehill
softlocked, as described above. Not checked: Furnace Fun (read, not run), Bottles' bonus game (mode 8),
the map-flag hazard, a console (the game reads what the client writes through KSEG0), and a real
client: none exists for this game yet. On the build of 2026-10-26 (the seed bound to the save) the items
check passed `--expect granted`, `kept` (now a client run: a planted save of the client's seed with
learned `0x1010`, `--moves 4,12`), `granted` with `--reload`, and `boots` on the soak3 image; without a
client it expects every A refused (`--expect refused`, which replaces `not-granted` and the R + A runs).

## Molehills as Archipelago locations

With `BKBT=1` Bottles' molehills are Archipelago locations: a molehill no longer teaches its move, it
checks its location, and the move comes only as an item from the client ("The Archipelago items"). The
game records the checks in `bkbt_ap_locations`, a 20-byte struct that the anchor points to from layout
0.4.0 on, one byte per move, which the game writes and the client only reads. A molehill picks its
first-visit text or its refresher by that byte, not by whether the move is known. This is the first part
of hack feature 4 (`docs/ROADMAP.md` of the BKBT repository); the empty honeycombs followed on
2026-10-26, as 25 bytes behind these 20 ("Empty honeycombs as Archipelago items and locations"), the
jiggies on 2026-10-27, as 101 bytes behind those ("Jiggies as Archipelago items and locations"), and
tokens, the note doors, Cheato and Brentilda are to come. There is no switch and no client gate of
its own: every hack build, by itself and in the dual-boot image (until 2026-10-26 also with R + A
without a client; since then no game starts without one); the matching build has none of it. Done on
2026-10-22; one byte per move indexed like the items, the game as the only writer, and no gate are the
owner's decisions of that day. It resolves the known issue of the items: the Spiral Mountain molehill
that teaches the jumps no longer softlocks under a client. Since 2026-10-26 the locations are saved with
the game (below).

- **The layout** (`include/bkbt.h:116-127`), behind the anchor's `+0x14` (`:160`):

  | Offset | Field | Value |
  |---|---|---|
  | `+0x00` | `u8 moves[20]` | indexed by `enum ability_e`, the move a molehill teaches; not 0 = that molehill's location is checked, 0 = not |

  `BKBT_AP_MOVE_COUNT` (`:108`), as for the items; only `u8`, so no padding. With the pointer the anchor
  grew to 24 bytes and its version to 0.4.0 (`BKBT_AP_VERSION_MINOR`, `:142`): a ROM whose anchor says
  0.3.0 or size 20 has no locations. The two Spiral Mountain molehills that teach three moves set three
  bytes each (below).
- **Where.** `src/core1/code_0.c`: the global `bkbt_ap_locations` (`:199-201`), initialised data in
  core1's `.data`, all 0; the anchor's initialiser ends with `&bkbt_ap_locations` (`:251`).
  Declaration: `include/bkbt.h:127`. Its writers and readers are the two molehill actors,
  `src/core2/ch/mole.c` and `src/SM/ch/smbottles.c`, each with `bkbt.h` included under `#if BKBT` at the
  top (`mole.c:5-7`, `smbottles.c:7-9`).
- **The molehills of the levels and the Lair** (`mole.c`, ids 9..`0x12`, the table `moleTable`,
  `:60-71`: `0xA` eggs, `0xB` beak buster and `0xC` talon trot in Mumbo's Mountain, `0xD` shock jump and
  `0xE` flight in Treasure Trove Cove, `0xF` Wonderwing in Clanker's Cavern, `0x10` wading boots in
  Bubblegloop Swamp, 9 beak bomb in Freezeezy Peak, `0x11` turbo trainers in Gobi's Valley, the levels
  as `chmole_learnedAllLevelAbilities` has them, `:76-97`; `0x12` the first note door, in the Lair).
  `chmole_learnAbility` (`:246-253`) and `chmole_startingDialog` (`:336-343`) test the molehill's byte
  where retail tests its move: not 0, the refresher; 0, the lesson text, and the first talk sets the byte
  where retail calls `player_unlockAbility` (`:262-269`). The rest of the first talk is retail's
  (`:270-281`: a sound or the note display, the dialog and its learn actions, below). The note-door mole
  starts talking by itself, without B, while its byte is 0 (`:422-432`; retail: while its move is not
  known). Keyed on the move, as first written, its lesson (`0xF64`) and refresher (`0xF65`) re-opened
  every ~310 frames without a client and Banjo could not leave: the locations check found the loop, and
  the owner keyed the trigger on the byte.
- **Spiral Mountain** (`smbottles.c`, ids 2..7, the table `chSmBottlesDialogTable`, `:67-76`: 2 camera
  control, 3 dive, 4 the attacks, 5 beak barge, 6 the jumps, 7 climb). `__chSmBottles_getRefresherDialog`
  tests the byte (`:311-319`) and sets it where retail teaches (`:340-348`), both skipped for ability -1,
  which the intro (1) and bridge (8) Bottles have in the table: the index would fall outside the array.
  Bottles pops out of a molehill whose byte is 0 and stays in the hill at one already checked (`sp34`,
  `:580-587`; retail: whether the move is known). Keyed on the move, as first written, Bottles stayed in
  the hill where the client had sent the move (read), and camera control's lesson came 3 frames after B;
  since, 50 frames after B, with Bottles popping out. The attack molehill (4) sets the bytes of claw swipe, roll
  and rat-a-tat rap (4, `0xC`, `0xB`) and the jump molehill (6) those of flap flip, the high jump and
  feathery flap (8, `0xA`, 7), and both then play their refresher (`:410-419`, `:429-437`), on the first
  visit as on every later one: nothing tells a fresh check there.
- **The tutorials are gone.** Spiral Mountain's map flags 1 (talked to Bottles), 3 (all moves learned),
  2, `0xC` and `0xF` are set whenever Bottles' actor initialises (`smbottles.c:519-540`; retail sets them
  once the first or all nine moves are known), so Bottles' introduction and his offer to skip the
  tutorial do not run. Should the offer run, its answer shows `ASSET_E07` and never skips (`:686-699`,
  commented as unreachable; `FILEPROG_DB_SKIPPED_TUTORIAL` is still set from the answer, `:685`, outside
  the `#if`). The jump tutorial is not in Spiral Mountain's spawn list any more
  (`src/SM/smspawnqueue.c:140-147`): that is what removes the softlock. The attack tutorial goes from its
  first state straight to its completed one, which spawns the Colliwobble at once
  (`src/SM/ch/attacktutorial.c:159-165`): its honeycomb can be collected without the lesson, and killing
  it takes an attack move, which the world's logic has to require.
- **The bridge is always open.** The broken bridge (`func_80363500`, `src/core2/code_DC4B0.c:34-44`)
  takes retail's path for a player who knows all nine Spiral Mountain moves (`func_80363440`, then
  despawn) on its first frame, whatever is learned: the world's logic must not assume the nine-move gate.
  Not changed: a loaded game goes to the Lair only with all nine Spiral Mountain moves learned and
  progress flag `0xBD` (`chmole_learnedAllSpiralMountainAbilities`, `mole.c:520-532`, in
  `src/core2/ch/gameSelect.c:526-530`), otherwise to Spiral Mountain at the exit from Banjo's house.
- **No client gate of its own.** From 2026-10-22 to 2026-10-26 R + A without a client played the same
  way: the molehills checked their locations and taught nothing, and no move arrived, because the items
  apply only with a committed seed (the owner's decision of 2026-10-22: a test path with Archipelago
  behaviour). Since 2026-10-26 the file select starts no game without a client ("The Archipelago
  settings"). "Without a client, play as retail" no longer holds for the molehills.
- **Saved with the game** (2026-10-26). Each game record carries the 20 bytes (`SaveData.move_locations`,
  `include/save.h:12-16`, at `+0x78`; "The save device"), and `saveData_create` copies
  `bkbt_ap_locations` into it at every save (`src/core2/code_B5040.c:415-421`). The file select does not
  load them while a game is only hovered (`saveData_load` sets `bkbt_file_seed` and nothing of the
  locations): the A that starts or loads a game does, through `bkbt_gameFile_loadAP`
  (`src/core2/code_B5E00.c:128-136`, `bkbt_saveData_loadAP` in `code_B5040.c:388-398`, prototypes in
  `include/save.h:26-29`), called at `src/core2/ch/gameSelect.c:563`, so a client sees a file's locations
  only once the file is chosen. `clearScoreStates` zeroes them (`src/core2/code_956B0.c:1496-1503`): at
  boot, on entering the file select and at every hover there, at an attract demo, and at Bottles' Bonus,
  the character parade and the ending, which then reload the game from its record in RAM
  (`gameFile_load`) and, since 2026-10-26, its locations too
  (`src/core2/ch/bottlesbonuscursor.c:199-201`, `src/core2/gc/parade.c:156-158`,
  `src/core2/code_C4320.c:274-276`; until the review of 2026-10-26 found it they stayed 0 after those).
  Erase (Z) and the sandcastle cheat clear the whole record, seed and locations included
  (`savedata_clear` over `sizeof(SaveData)`, `code_B5040.c:468-473`, through `gameFile_clear` from
  `gameSelect.c:310-313` and `src/TTC/ma/castle.c:1035`).
- **The protocol, for the client** (none exists yet). One writer: the game writes the locations, the
  client only reads them, and the server only reads what the client sends. The client sends a check for
  every byte that is not 0 and that it has not sent yet; the server ignores a check it already has.
  Nothing commits them. **Since 2026-10-26 a byte can go back to 0**: the bytes read 0 in the front end
  and while a reload is under way, and when another file is chosen they are that file's. So a 0 never
  means "unchecked" to the client: it only ever adds checks. Within a played file the game does not clear
  a byte (the locations check judges that). The save buffers checks made while no client is connected,
  and they are sent once the file is played with one.
- **After a reset or a launch from Tooie**, a molehill whose check was saved shows its refresher; one
  checked since the last save shows its first-visit text again and sets its byte again, which reaches the
  server as a repeat. Until 2026-10-26 they were in RAM only, and every boot started them at 0. A save
  that lacks checks the server has (a lost save) shows first-visit texts again; the server ignores the
  repeats: cosmetic.
- **Open: checks made after the last save, at the three reloads.** Bottles' Bonus, the parade and the
  ending reload the record in RAM, which holds the last save. If the game does not save before them,
  checks made since that save drop out of RAM and out of the next save (a connected client has sent them
  by then, and the molehill would show its first-visit text again). Whether it saves first was read, not
  settled, and the harness cannot reach these paths.
- **Lesson rewards repeat.** What a lesson hands out comes with every first-visit text
  (`chmole_additionalAbilityLearnActions`, `mole.c:204-240`): the egg molehill gives 50 eggs (`:224-226`)
  with eggs not learned (0 to 50, measured), and again whenever its byte is 0 again (a reset and reload
  without a save since the talk); the flight and Wonderwing lessons give 25 red and 5 gold feathers the
  same way (`:227-232`; read, not run).
- **A save that skipped Bottles' tutorial.** A retail save made after the skip has
  `FILEPROG_DB_SKIPPED_TUTORIAL` set, and there a checked Spiral Mountain molehill shows Bottles'
  refuse-help texts (`ASSET_E0A`..`E0E`) instead of its refresher (`smbottles.c:320-328`; read, not run).
  Since 2026-10-26 neither a retail save nor one of an earlier hack build loads at all ("The save
  device"), so only a planted record can carry the flag.
- **For the tools.** `bkbt_ap_locations` is a global, so the map carries it (`0x80275DA8` in the
  molehills' build of 2026-10-22; the anchor moved to `0x80275DC0` and `bkbt_ptr_ap_item_passes` to
  `0x80275DBC`, `bkbt_ap_items` stayed at `0x80275D94`); all move with every build. The anchor check
  compares the anchor's `+0x14` with it; the locations check reads the bytes through the anchor.
- **What it costs.** 96 bytes of RAM headroom: lair ends at `0x80395B00` instead of `0x80395AA0` (the
  map). ROM room by the image builder's count (taken from the built images): 22,624 bytes before the last
  128 KiB, 32 more than the items' 22,592; the code is stored compressed ("Rules for new code").

Checked on mupen64plus with the BKBT repository's locations check (`tools/bk_locations_check.py`: it
loads a planted game, puts Banjo next to one molehill by writing the player's position, presses B,
answers the dialogs with A and watches the dialog, the learned moves and the locations, with the stand-in
client of the items check or with R + A), on 2026-10-22, recompiler unless said, all passing on this
build: the note-door mole in the Lair (`0x12`) without a client (`nomoves`) and with one (`located`);
Spiral Mountain 6 `located` and `nomoves`, 2 `sent` (camera control's lesson `0xDF4` 50 frames after B,
Bottles popping out), 3, 4, 5 and 7 `located`, 4 and 6 `sent`, 6 after a reload `forgotten`; Mumbo's
Mountain `0xA` `located` and `sent` (moves 3, 4, 6 and `0x13`), `0xB` and `0xC` `located`, `0xA` without a
client after a reload `forgotten`; on the autotest image Spiral Mountain 6 `located` and Mumbo's
Mountain `0xA` `nomoves`; Spiral Mountain 6 on the pure interpreter. A deliberately wrong expectation
(`0xA` `sent` with eggs not sent) failed, on two lines. On the build before the owner's two fixes
(`f2041451...`) the same tool passed 19 runs and failed the note-door mole without a client, with the
loop described above (Banjo moved 0 units). Measured: the egg molehill's lesson gives 50 eggs with eggs
not learned, and again after a reset and reload. The anchor check of layout 0.4.0 (10 lines on the ROM by
itself, the `+0x14` pointer among them; also on the soak3 image), the round trips, the anti-tamper, save,
logo, new-game, settings and items checks pass on the same build, and its matching build is the retail
ROM (`hack/DUALBOOT.md` of the BKBT repository, "Checked"). By hand, the owner reported on 2026-10-22
that on Project64 3.0.1 and ares v148, in the normal image of this build, with the stand-in client
`tools/bk_hand_client.py --moves 3,4,0x13` of the BKBT repository, which now reads the locations and sends
the move of every one checked: at an unchecked molehill Bottles popped out with the lesson text, the
client printed the check and the move arrived; the same molehill again gave the refresher with Bottles in
the hill; the jump and attack molehills gave their refresher and then their three moves arrived; after a
reset and reload the molehills showed the lesson again; and without a client (R + A) no move arrived,
nothing froze, the molehills showed the lesson text and then the refresher, and the note-door mole let
Banjo go. Before the two fixes the owner had played through Mumbo's Mountain by hand with the feature.
Not checked: the molehills of the later levels (9 and `0xD`..`0x11`: Treasure Trove Cove, Clanker's
Cavern, Bubblegloop Swamp, Freezeezy Peak, Gobi's Valley), a console, and a real client: none exists for
this game yet.

The locations saved, 2026-10-26: on mupen64plus (recompiler) the locations check passed `--expect
remembered` on Mumbo's Mountain `0xA` (`--save --reload`: the byte in the record saved after the talks,
in RAM and on disk, with the client's seed; after the reset 0 until A, the saved bytes from A on; talk 3
the refresher), and `located` on Spiral Mountain 6, the note-door mole `0x12` and, on the autotest image,
Spiral Mountain 6. Its runs without a client (`--client none`, `--expect nomoves`) now stop with a
message, since no game starts without one. The seed check showed the locations of a file only after A,
browsing publishing nothing ("The Archipelago settings"). Not shown: the three mid-session reloads (the
harness cannot reach them).

## Empty honeycombs as Archipelago items and locations

With `BKBT=1` the 24 empty honeycombs are Archipelago locations and the empty honeycomb is an
Archipelago item. Picking one up checks its location and no longer counts toward health; the game's
pieces towards the next unit of health and its health total follow the total the client says was
received. There is no switch: every hack build, by itself and in the dual-boot image; the matching build
has none of it. Done on 2026-10-26 (`docs/ROADMAP.md` of the BKBT repository, hack features 3 and 4),
built by Claude with the owner reviewing. The owner's decisions of that day: a received honeycomb shows
with the game's own HUD and upgrade; the checks are bytes in the locations struct, not the game's own
bits; the pause menu keeps counting the honeycombs picked up.

- **How retail does it** (read from the decomp). One bit per honeycomb in `sHoneycombScore[3]`
  (`src/core2/honeycombscore.c:12`; honeycomb n, `enum honeycomb_e` 1..`0x18`, is bit `n & 7` of byte
  `(n - 1) / 8`), set only by the pickup (`src/core2/ba/marker.c:578`), saved in each game record at
  `+0x0F..+0x11` (`src/core2/code_B5040.c:116-126`), and read by every honeycomb and by the switches and
  characters that spawn the five that are not placed (`code_42CB0.c:105-112`, the GV and RBB switches,
  Gobi, the SM Colliwobble and Quarries) and by the pause menu. The pieces (`ITEM_13_EMPTY_HONEYCOMB`)
  and the health total (`ITEM_15_HEALTH_TOTAL`) are not saved: `func_8034789C`
  (`src/core2/code_BEF20.c:530`) rebuilds them from the bits as `total % 6` and `5 + MIN(3, total / 6)`,
  at every map load (`gc/section.c:286`) and every load of a save (`func_80347958`, which also fills the
  health). The pickup adds a piece and, with the sixth, blocks the pause menu (`gcpausemenu_80314AC8(0)`);
  the HUD (`fx/honeycarrierscore.c:179-255`) trades six pieces for a unit of health and lifts the block.
- **The layout** (`include/bkbt.h:103-141`; layout 0.5.0, `BKBT_AP_VERSION_MINOR` 5): the items gained
  `+0x14 u8 honeycombs`, the total received, read as at most `BKBT_AP_HONEYCOMB_MAX` (24), and 3 bytes of
  named padding (24 bytes); the locations gained `+0x14 u8 honeycombs[BKBT_AP_HONEYCOMB_SLOTS]`,
  `BKBT_AP_HONEYCOMB_SLOTS` `0x19`, indexed by `enum honeycomb_e` like the game's `HONEYCOMB_COUNT` (byte
  `[0]` is never set), not 0 = picked up, and padding (48 bytes). A client writes the total only to a ROM
  whose anchor says 0.5.0 or later: in a 0.4.0 ROM the byte after the moves is `bkbt_ap_locations.moves[0]`.
- **Which state is the chosen game's: `bkbt_file_chosen`** (`src/core1/code_0.c:174`, declared in
  `include/bkbt.h:149`). The score state holds the chosen game only between A on the file select and the
  next `clearScoreStates`: hovering a file clears it and loads that file (`src/core2/ch/gameSelect.c:228-238`),
  and demos and Bottles' Bonus clear it too. The flag is set in `bkbt_gameFile_loadAP`
  (`src/core2/code_B5E00.c:142`), which runs at A and after the mid-session reloads, each time right
  after the game's own load, and cleared in `clearScoreStates` (`src/core2/code_956B0.c:1504`). Only the
  pickups change the bits in between.
- **Locations.** Under `#if BKBT` the pickup (`marker.c:576-593`) no longer adds the piece nor blocks
  the pause; it still sets the bit, plays the jingle, shows the first-time text, sparkles and despawns.
  The bit is the check, so the spawn gates, the pause menu and the save need no change. `bkbt_apHoneycombs`
  (`code_0.c:281`), called from `bkbt_frame` after the moves (`:428`), copies the 24 bits into the
  locations on every main-loop iteration while `bkbt_file_chosen`; `clearScoreStates` zeroes the copy
  with the molehill bytes. The game's record stays the only source, so a load, a reload and a pickup
  cannot disagree with the copy, and a file the file select only shows is never published.
- **Items.** `sBkbtApHoneycombsGiven` (`code_0.c:221`) is how many honeycombs the game's counts stand
  for; RAM only, since the client sends its total again after every boot. Under `#if BKBT`
  `func_8034789C` takes its total from `bkbt_apHoneycombsForHealth` (`code_0.c:270`; `code_BEF20.c:534-539`):
  while the file is chosen and the seed committed, the received total, taken as given at once and
  quietly, so a load never replays the HUD for honeycombs given before; while the file is not chosen,
  retail's count of the bits, which keeps demos as they were recorded (`func_802E4A08()` cannot tell a
  demo there: a demo rebuilds before its game mode is set, `demo.c:74`, `code_5C870.c:264-273`); while
  the seed is not committed, what was given. `bkbt_apHoneycombs` gives one piece per iteration while the
  total is higher, as the pickup did (`item_inc`, which shows the HUD, and with the sixth the pause
  block), under the items' gate (`bkbt_apItemsAllowed`) and only while Banjo is in the map and placed
  (`func_8028F070`) and not dying; it waits while 6 pieces wait for the HUD. A lower total than given is
  applied quietly: the rebuild, then an adjustment of the pieces by 0, which clamps the health without a
  HUD or the death latch an adjustment of the health could set. `bkbt_gameFile_loadAP` sets the flag and
  calls `func_80347958`, so a game starts with the health of what was received, filled.
- **For the tools.** `bkbt_ptr_ap_honeycombs_given` (`code_0.c:222`) exports the number given ("Exports
  for tools" in "Rules for new code"): the counts read 0 pieces and a health total of 8 both for 18 and
  for 24, so only the number shows that the last six arrived. The BKBT repository's honeycomb check
  (`tools/bk_honeycomb_check.py`) reads it and, from the map, `D_80385F30` (the item counts),
  `sHoneycombScore`, `D_80383084` (the pause block counter) and `D_803815C0` (the HUD's trade state),
  all globals; the anchor check wants 0.5.0 and room for the two structs in the map; the stand-in client,
  the seed check and the hand client write or read the honeycomb bytes from 0.5.0 on.
- **What it costs.** 480 bytes of RAM headroom (lair ends at `0x80395350`, `lair_VRAM_END`) and 192 of
  ROM room (23,536 bytes before the last 128 KiB, by the image builder's count).
- **Known limits.** A map load during the HUD's upgrade (1.5 s) can leave the health total one too high
  (capped at 8) and the pause menu refused until the next map load: the upgrade's timed functions
  survive a map load (`timedfuncqueue.c:280-299`) while the map load resets the block counter; retail has
  the same race. A honeycomb picked up since the last save respawns after a reset, and its check is made
  again (the server ignores it). The first-time text ("six give health") now means six received, and the
  pause totals count the honeycombs picked up, not those received. Whether Bottles' Bonus and the ending
  save before they reload is still open (the parade does: `func_8030AFD8(1)` in `gc/parade.c`).

Checked on mupen64plus on 2026-10-26 with the BKBT repository's honeycomb check, the seed check and the
anchor check (`hack/DUALBOOT.md` of the BKBT repository, "Checked"): the counts follow the client's total
and not the planted bits (six honeycombs planted, total 0: health 5/5); totals of 13 and 200 (read as
24); a total raised from 0 to 7 given one piece per iteration, the pause menu blocked while six wait,
one trade by the HUD, health 6/6, the same after a map change (also on the pure interpreter, and from
17 to 30, read as 24, with two trades); a total lowered from 13 to 4, quietly; pickups of the SM stump,
waterfall and tree honeycombs and both of Mumbo's Mountain's, which set the bit and the byte and count
nothing until the client's total rises; a pickup saved, reset and loaded again, published from A and not
there to pick up again; and, in the seed check, files with honeycombs browsed and refused without
publishing them. By hand: the owner tried the normal image on Project64 3.0.1 and ares v148 on
2026-10-26 with the BKBT repository's hand client (`hack/DUALBOOT.md` there, "Status"): pickups that count
nothing until the client sends them, the upgrade at six, the pause menu, a save and a load, a lower total,
the SM Colliwobble's and Quarries' honeycombs and the attract demos. Not checked: the three honeycombs
spawned by events outside Spiral Mountain, the ROM by itself by hand, a console, any client but the
stand-ins.

## Jiggies as Archipelago items and locations

With `BKBT=1` the 100 jiggies are Archipelago locations and the jiggy is an Archipelago item. Picking one
up checks its location and no longer adds to the jiggies held; the game gives the jiggies the client says
were received, and the Lair's puzzles spend them as in retail. There is no switch: every hack build, by
itself and in the dual-boot image; the matching build has none of it. Done on 2026-10-27
(`docs/ROADMAP.md` of the BKBT repository, hack features 3 and 4), built by Claude with the owner
reviewing. The owner's decisions of that day: the checks are bytes in the locations struct, as for the
honeycombs; a received jiggy shows with the game's own held-jiggy counter; the puzzle costs stay retail's
for now.

- **How retail does it** (read from the decomp; `docs/bk-decomp-notes.md` of the BKBT repository,
  "Jiggies"). One bit per jiggy in `jiggyscore` (`src/core2/jiggyscore.c:11-14`; jiggy n, `enum jiggy_e`
  1..`0x64` in `include/enums.h:1514-1616`, is bit `n & 7` of byte `(n - 1) / 8`), set only by the pickup
  (`src/core2/ba/marker.c:548`) and saved in each game record at `+0x02..+0x0E`; every jiggy despawns at
  init when its bit is set (`src/core2/ch/jiggy.c:147-148`), so the 55 spawned by events need nothing
  either. Three counts: `ITEM_E_JIGGY`, the jiggies found in the level, rebuilt from the bits at every level
  load and counted up by the jig dance with its HUD (`src/core2/bs/jig.c:68-72`); `ITEM_26_JIGGY_TOTAL`,
  the jiggies held, which the pickup raises by 1 without a HUD (`marker.c:549`), the puzzles spend and
  give back (`src/lair/jigsawpicture.c:362-415`), and the game **saves** (saved items byte 4, record
  `+0x69`, `src/core2/code_BEF20.c:489-499`, restored at `:570-577`); and `ITEM_2B_UNKNOWN`, a copy of it
  for the podium's counter, which `item_adjustByDiff` moves by every diff asked for `ITEM_26`, even one its
  clamp at 0 refuses (`code_BEF20.c:80`, `:149-151`). The sandcastle cheats of Treasure Trove Cove fill
  the puzzles of CC to CCW without spending (`src/TTC/ma/castle.c:954-1016`), so the jiggies held are not
  the jiggies received minus the pieces placed.
- **The layout** (`include/bkbt.h:103-168`; layout 0.6.0, `BKBT_AP_VERSION_MINOR` 6): the items keep 24
  bytes, `+0x15 u8 jiggies` taken from their padding (the total received, read as at most
  `BKBT_AP_JIGGY_MAX`, 100); the locations append `+0x30 u8 jiggies[BKBT_AP_JIGGY_SLOTS]` (`0x65`, indexed
  by `enum jiggy_e`, `[0]` never set, not 0 = picked up) and 3 bytes of padding behind the honeycombs'
  padding, which stays at `+0x2D`: 152 bytes. A client writes the total only to a ROM whose anchor says
  0.6.0 or later: in a 0.5.0 ROM `+0x15` is padding.
- **The jiggies given, saved.** The honeycomb counts are rebuilt at every load, so the hack rebuilds them
  from the client's total; the jiggies held are saved and spent, so the hack keeps `bkbt_jiggies_given`
  (`src/core1/code_0.c`, a global, declared in `include/bkbt.h`), the received jiggies it has given, and
  gives only what the total is above it. It is saved in the record's first padding byte, `+0x72`
  (`SaveData.jiggies_given`, `include/save.h`, under `#if BKBT`; the matching build keeps `padding[2]`,
  and the record stays `0x90` bytes): written by `saveData_create` (`src/core2/code_B5040.c`) in the same
  call that saves the jiggies held, read back by `bkbt_saveData_loadAP` right after the game's own load
  (at A and after the three mid-session reloads), zeroed by `clearScoreStates`. So a reset or a reload
  always brings back a matching pair, and nothing recomputes the jiggies held: puzzles and cheats behave
  as in retail.
- **Locations.** Under `#if BKBT` the pickup (`marker.c`, the jiggy case) no longer raises the jiggies
  held; the bit, the BGS timer stop, the first-jiggy texts, the Jinjo fanfare and the dance stay.
  `bkbt_apJiggies` (`code_0.c`), called from `bkbt_frame` after the honeycombs, copies the 100 bits into
  the locations on every main-loop iteration while `bkbt_file_chosen`; `clearScoreStates` zeroes the copy.
- **Items.** `bkbt_apJiggies` then, under the items' gate and while Banjo is in the map, placed and not
  dying: a total above the jiggies given goes into `ITEM_26` in one `item_adjustByDiff` call with the HUD,
  which counts up by itself (retail's refills add their whole amount in one call); before it, when the
  held-jiggy counter is not on the screen, its shown number (`itemPrintValues`, `src/core2/code_73640.c:88`)
  is set to the jiggies held, so that it counts up from there after a puzzle spent jiggies or a lower total
  took some; while a puzzle podium's counter (`ITEM_2B`'s) is up, without the HUD, the podium's counter
  showing the rise. A total below the jiggies given takes back quietly what it can from the jiggies held,
  never from a puzzle, and lowers the jiggies given only by what it took: the rest stays given, so a total
  that dips and comes back gives nothing twice.
- **One jiggy counter on the screen.** The level's found count (`ITEM_E_JIGGY`) counts the checks made in
  the level, not jiggies the player has. In the owner's first hand test (Project64 3.0.1, 2026-10-27) its
  counter came up after the dance, beside the held-jiggy counter that showed the jiggy the client sent, and
  confused. Under `#if BKBT`, `item_adjustByDiff` (`src/core2/code_BEF20.c`) never shows it, whoever asks:
  retail asks from the jig (`src/core2/bs/jig.c:70`, `:102`) and from the pickups that skip it, in
  flight, swimming or mid-jump (`src/core2/code_14420.c:935`, `:946`), and nothing else prints it (the pause menu shows the jiggies held
  and hides it, `src/core2/gc/pauseMenu.c:133`, `:145`). It still counts, because a level's tenth jiggy
  reads it for the note door's jig and fanfare (`bs/jig.c:38`, `code_14420.c:908`).
- **For the tools.** `bkbt_jiggies_given` is a global, so it needs no exported pointer. The BKBT
  repository's jiggy check (`tools/bk_jiggy_check.py`) reads it and, from the map, `D_80385F30` (the item
  counts), `jiggyscore`, `gFileProgressFlags` (+8, the puzzles' pieces placed), `itemPrintValues` and
  `D_803692F8` (the counters on the screen, for the state of the level count's counter), all globals; the anchor check wants 0.6.0 and room for the 152 bytes in the map; the stand-in client, the
  seed check and the hand client write or read the jiggy bytes from 0.6.0 on.
- **What it costs.** 576 bytes of RAM headroom (lair ends at `0x80395590`, `lair_VRAM_END`) and 160 of ROM
  room (23,376 bytes before the last 128 KiB, by the image builder's count).
- **Known limits.** A save of a build before the jiggies holds the jiggies picked up and counted and none
  given, so the client's total lands on top of them. A total lowered below what the puzzles hold leaves
  the jiggies held at 0 and the rest given. A puzzle the sandcastle filled cost nothing and C-down on it
  gives jiggies, as in retail. A jiggy picked up and a total given since the last save are picked up and
  given again after a reset. The 100-jiggy ending and the pause totals count the jiggies picked up. A
  held-jiggy counter already on the screen when Banjo reaches a podium can overlap the podium's counter
  until it hides (3 s).

Checked on mupen64plus on 2026-10-27 with the BKBT repository's jiggy check, the seed check and the
anchor check (`hack/DUALBOOT.md` of the BKBT repository, "Checked"): see there for the list. The owner
tried it by hand the same day on Project64 3.0.1 and ares v148 with the BKBT repository's hand client, in
the dual-boot image, a jiggy spawned by an event, Treasure Trove Cove's podium and C-down included
(`hack/DUALBOOT.md` of the BKBT repository, "Status"). Not checked: a console, any client but the
stand-ins, the ROM by itself by hand.

## Mumbo tokens as Archipelago items and locations

With `BKBT=1` the 115 Mumbo tokens are Archipelago locations and the token is an Archipelago item.
Picking one up checks its location and no longer adds to the tokens held; the game gives the tokens the
client says were received, with the token counter, and Mumbo charges them as in retail. There is no
switch: every hack build, by itself and in the dual-boot image; the matching build has none of it. Done
on 2026-10-27 (`docs/ROADMAP.md` of the BKBT repository, hack features 3 and 4), built by Claude with the
owner reviewing, the same way as the jiggies. The owner's decisions of that day: Mumbo's prices stay
retail's for now; a pickup shows no counter, which shows only when the client sends a token (as the
owner chose for the jiggies after their first hand test).

- **How retail does it** (read from the decomp; `docs/bk-decomp-notes.md` of the BKBT repository,
  "Mumbo tokens"). One bit per token in `sMumboTokenScore` (`src/core2/mumboscore.c:5-11`, 16 bytes;
  token n, `enum mumbotoken_e` 1..`0x73` in `include/enums.h`, is bit `n & 7` of byte `(n - 1) / 8`), set
  only by the pickup (`src/core2/ch/mumbotoken.c:30`) and saved in each game record at `+0x12..+0x21`;
  every token despawns at init when its bit is set (`:77-83`). A token's id comes from the flag node
  nearest to where it spawns, the node's value less 199 (`:45-60`), placed or dropped by an enemy alike.
  Two counts: `ITEM_1C_MUMBO_TOKEN`, the tokens held, which the pickup raises by 1 with its counter
  (`item_inc`, `:39`, retail's line under `#else` in the fork), Mumbo charges (`src/core2/ch/mumbo.c:165`, `:178`, `:353`: termite 5, croc 10,
  walrus 15, pumpkin 20, bee 25, `:77-91`, once per transformation, after which `FILEPROG_90..94` say it
  is paid for, `:395`) and the game **saves** (saved items byte 0, record `+0x65`,
  `src/core2/code_BEF20.c:501`, restored at `:580`); and `ITEM_25_MUMBO_TOKEN_TOTAL`, a copy of it for the
  pause menu, which every `item_adjustByDiff` call sets to it (`code_BEF20.c:96`). Mumbo keeps the token
  counter on the screen while he is awake (`mumbo.c:222-227`). The sandcastle's banned cheat sets the
  tokens held to 99 (`src/TTC/ma/castle.c:1019`) and the attract demo to 25 (`src/core2/code_C4320.c:209`).
- **The layout** (`include/bkbt.h:103-178`; layout 0.7.0, `BKBT_AP_VERSION_MINOR` 7): the items keep 24
  bytes, `+0x16 u8 tokens` taken from their padding (the total received, read as at most
  `BKBT_AP_TOKEN_MAX`, 115), one byte of padding left at `+0x17`; the locations append `+0x98 u8
  tokens[BKBT_AP_TOKEN_SLOTS]` (`0x74`, indexed by `enum mumbotoken_e`, `[0]` never set, not 0 = picked
  up) behind the jiggies' padding: 268 bytes. A client writes the total only to a ROM whose anchor says
  0.7.0 or later: in a 0.6.0 ROM `+0x16` is padding.
- **The tokens given, saved**, for the jiggies' reason: the tokens held are saved and spent.
  `bkbt_tokens_given` (`src/core1/code_0.c`, declared in `include/bkbt.h`) is saved in the record's last
  padding byte, `+0x73` (`SaveData.tokens_given`, `include/save.h`), by `saveData_create` in the same call
  that saves the tokens held, read back by `bkbt_saveData_loadAP`, zeroed by `clearScoreStates`. The
  record has no padding left: the next count of this kind needs a bigger record, which the tools would
  have to learn (`tools/bk_eeprom.py` of the BKBT repository knows `0x90` and `0x78` only).
- **Locations.** Under `#if BKBT` the pickup (`src/core2/ch/mumbotoken.c`, `chMumboToken_collect`) no
  longer raises the tokens held, so no counter shows there; the bit, the sound, the first token's text
  and the sparkle stay. `bkbt_apTokens` (`code_0.c`), called from `bkbt_frame` after the jiggies, copies
  the 115 bits into the locations on every main-loop iteration while `bkbt_file_chosen`.
- **Items.** `bkbt_apTokens` then, under the items' gate and while Banjo is in the map, placed and not
  dying, calls `bkbt_apGiveHeld` (`code_0.c`), the jiggies' rule made one function for both: a total above
  the tokens given goes into `ITEM_1C` in one `item_adjustByDiff` call with the counter, after the
  counter's shown number is set to the tokens held when it is not on the screen; a total below takes back
  quietly what it can from the tokens held, never what Mumbo took, and lowers the tokens given only by
  that. Near Mumbo, whose own call keeps the counter up, the counter counts up as it is.
- **For the tools.** `bkbt_tokens_given` is a global. The BKBT repository's token check
  (`tools/bk_token_check.py`) reads it and, from the map, `D_80385F30`, `sMumboTokenScore`,
  `gFileProgressFlags` (+8, Mumbo's paid flags), `itemPrintValues` and `D_803692F8` (the token counter's
  state); the anchor check wants 0.7.0 and room for the 268 bytes in the map; the stand-in client, the
  seed check and the hand client write or read the token bytes from 0.7.0 on.
- **What it costs.** 416 bytes of RAM headroom (lair ends at `0x80395730`, `lair_VRAM_END`) and 272 of ROM
  room (23,104 bytes before the last 128 KiB, by the image builder's count).
- **Known limits.** A save of a build before the tokens holds the tokens picked up and counted and none
  given, so the client's total lands on top of them. A total lowered below what was spent leaves the
  tokens held at 0 and the rest given. A token picked up and a total given since the last save are picked
  up and given again after a reset. The pause totals count the tokens picked up.

Checked on mupen64plus on 2026-10-27 with the BKBT repository's token check, the seed check and the
anchor check (`hack/DUALBOOT.md` of the BKBT repository, "Checked"): see there for the list. The owner
tried it by hand the same day on Project64 3.0.1 and ares v148 with the BKBT repository's hand client, in
the dual-boot image (`hack/DUALBOOT.md` of the BKBT repository, "Status"). Not checked: a console, any
client but the stand-ins, the tokens dropped by enemies, a transformation other than the termite, the
ROM by itself by hand.

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
  build, 68,336 with the dual-boot hook of contract 2, 68,224 with the pad record of 2026-10-12, 66,496
  with the save device of 2026-10-14, and the same with the new-game intro of that day and the logo
  skip of 2026-10-15, 66,416 with the Archipelago anchor of 2026-10-17, 66,208 with the Archipelago
  settings of 2026-10-18, 65,888 with the Archipelago items of that day, 65,792 with the molehills as
  locations of 2026-10-22, 68,240 with the save in SRAM of 2026-10-26, which no longer links
  libultra's EEPROM functions, 67,760 with the honeycombs of that day, 67,184 with the jiggies of
  2026-10-27 and 66,768 with the Mumbo tokens of that day: the largest, lair, ends at
  `0x80395840` in the maps of 2026-10-14 and 2026-10-15, at `0x80395890` in that of 2026-10-17, at
  `0x80395960` in that of the settings, at `0x80395AA0` in that of the items, at `0x80395B00` in that of
  the molehills, at `0x80395170` in that of the save in SRAM, at `0x80395350` in that of the
  honeycombs, at `0x80395590` in that of the jiggies and at `0x80395730` in that of the tokens, read as
  `lair_VRAM_END`). Every level overlay is
  loaded at the end of core2
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
  is read from) in `src/core1/joy.c`, `bkbt_ptr_chord_seen` (`sBkbtChordSeen`, the chord counter) and,
  since 2026-10-18, `bkbt_ptr_ap_item_passes` (`sBkbtApItemPasses`, the count of iterations that applied
  the Archipelago items, "The Archipelago items") and, since 2026-10-26, `bkbt_ptr_ap_honeycombs_given`
  (`sBkbtApHoneycombsGiven`, the empty honeycombs the game's counts stand for, "Empty honeycombs as
  Archipelago items and locations") in `src/core1/code_0.c`, `bkbt_ptr_ucode`
  (`sUcodeData`, the rspboot buffer) in `src/core1/ucode.c`. The statics they point to are
  uninitialised, so each lies in its file's `.bss`, and core1 clears its bss at every boot. The items
  check takes `bkbt_ptr_ap_item_passes` from the map, the honeycomb check `bkbt_ptr_ap_honeycombs_given`;
  `tools/bk_fork_info.py` exports neither. The
  game's own pad arrays (`sInputs`, `sInputsPrimary`) are not exported any more: the chord is not read
  from them, and a tool that wrote them would not reach it. The map being played
  and the game mode need no export: `sGsWorldData` (+4) and `sGameState` (+4) are globals. Neither does
  what the save check reads: `gSaveData`, `gameFile_saveData`, `gameFile_GameIdToFileIdMap`,
  `D_80383F04` (the spare file) and the function `savedata_8033CC98` are globals too, and so is the
  Archipelago anchor, `bkbt_ap_anchor` ("The Archipelago anchor"), its settings, `bkbt_ap_settings`
  ("The Archipelago settings"; the anchor check compares the anchor's `+0x0C` with it), its items,
  `bkbt_ap_items` ("The Archipelago items"; the anchor check compares the anchor's `+0x10` with it), and,
  since 2026-10-22, its locations, `bkbt_ap_locations` ("Molehills as Archipelago locations"; the anchor
  check compares the anchor's `+0x14` with it), and the game's own `learnedAbilities`, which the items
  and locations checks read, and `g_Dialog`, which the locations check reads. Since 2026-10-26 two more
  globals of `src/core1/code_0.c`: `bkbt_file_seed`, which the seed check reads from the map (and goes
  without when a build lacks it), and `bkbt_playing_seed`, which no tool reads (the checks judge the seed
  in the saved record); the seed check also reads `gameNumber`. Since the honeycombs of that day
  `bkbt_file_chosen`, which no tool reads, and the game's own `sHoneycombScore`, `D_80385F30` (the item
  counts), `D_80383084` (the pause block counter) and `D_803815C0` (the HUD's trade state), which the
  honeycomb check reads (the seed check writes `sHoneycombScore` to stand in for a pickup). Since the
  jiggies of 2026-10-27 `bkbt_jiggies_given` and the game's own `jiggyscore`, `gFileProgressFlags` and
  `itemPrintValues`, which the jiggy check reads (the seed check writes `jiggyscore` to stand in for a
  pickup). Since the Mumbo tokens of that day `bkbt_tokens_given` and the game's own `sMumboTokenScore`
  and `D_803692F8` (the counters on the screen), which the token check reads (the seed check writes
  `sMumboTokenScore` to stand in for a pickup). The tools take the size of a game record
  from the spacing of `gameFile_saveData[4]` and the `gameFile_GameIdToFileIdMap` behind it, in
  `bk-fork.json` or an image's info block, and refuse a size they do not know (`tools/bk_eeprom.py` of
  the BKBT repository). One static is used without an export: the locations check
  writes the player's position, `sPlayerPosition` in `src/core2/playerposition.c`, which has no symbol;
  it takes that file's `.bss` from the map and checks at run time that it holds the position (plausible
  floats, and Banjo moved by the game after the tool's write), and stops as a harness problem otherwise.
  Two of those names
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
    device reaches the SRAM with a handle, through the game's own PI manager; by itself the ROM has the
    same SRAM, which its exported header declares (until 2026-10-26 it kept its EEPROM there).
  - Nothing may grow the ROM past 16 MiB or into its last 128 KiB (`0xFE0000` and up), which have to stay
    fill: at 64 MiB that is the top of the image, where some flashcarts keep save data. The image builder
    refuses a ROM that uses them. That leaves little: in the hack build of 2026-10-14 the last compressed
    overlay ends at cart `0xFDA6B0`, 22,864 bytes below that line (23,504 in the build of 2026-10-12,
    before the save device; 22,992 by the image builder's count in the build of 2026-10-17, with the
    Archipelago anchor; 22,800 in that of 2026-10-18, with the Archipelago settings; 22,592 in that with
    the Archipelago items, later that day; 22,624 in that of 2026-10-22, with the molehills as
    locations; 23,728 in that of 2026-10-26, with the save in SRAM, whose fill starts at cart
    `0xFDA350`; 23,536 in that with the honeycombs, later that day, fill from `0xFDA410`; 23,376 in that
    with the jiggies of 2026-10-27, fill from `0xFDA4B0`; 23,104 in that with the Mumbo tokens of that
    day, fill from `0xFDA5C0`).
