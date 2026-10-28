#ifndef BKBT_H
#define BKBT_H

#include <ultra64.h>
#include "bool.h"

/* BKBT=1 only (BKBT.md). What this ROM shares with the dual-boot image it can be part of, and the hooks
 * the game calls. A ROM started by itself finds none of it and behaves like the plain game.
 *
 * The handoff block mirrors hack/bt/n64/src/dualboot.h in the BKBT repository (dual-boot contract 2):
 * keep the two in step. The Tooie hack and its launcher own the layout. This side uses the four words
 * below and stores into three of them: it reads the token and the payload address and zeroes both, counts
 * the countdown down, and only reads the flags.
 *
 * This side accesses the block through KSEG1 only. The launcher that returns to Tooie writes the block
 * uncached and then flushes the caches: a line of the block left dirty in this game's data cache would be
 * written back over what the launcher stored. No emulator models the caches, so only a console shows a
 * mistake here. */
#define BKBT_HANDOFF ((volatile u32 *) 0xA05FFF00) /* dualboot.h DUALBOOT_HANDOFF, 0x805FFF00 through KSEG1 */

/* Word indices into the block (byte offset / 4), from the table in dualboot.h. */
#define BKBT_HANDOFF_TOKEN     8  /* +0x20: launch token, written on every launch of this game; bkbt_init zeroes it */
#define BKBT_HANDOFF_PAYLOAD   9  /* +0x24: KSEG0 address of the payload header loaded in this launch, 0 if none; bkbt_init zeroes it */
#define BKBT_HANDOFF_COUNTDOWN 10 /* +0x28: main-loop iterations before the game returns by itself, 0 = stay (tests) */
#define BKBT_HANDOFF_FLAGS     11 /* +0x2C: BKBT_FLAG_*, latched by bkbt_init */

/* The launch token is 'B','K','G' and the contract number in the low byte. The number is the linker
 * symbol bkbt_contract (manual_syms.us.v10.txt), its only definition on this side: use it as
 * (u32) bkbt_contract. */
#define BKBT_TOKEN 0x424B4700
extern u8 bkbt_contract[];

/* The payload is not part of this ROM. The launcher loads it into Expansion Pak RAM and passes the
 * address of its header: magic, entry, load address, length in bytes, contract number, one word each.
 * A field never changes meaning under the same magic. */
#define BKBT_PAYLOAD_MAGIC    0x424B5032 /* 'BKP2' */
#define BKBT_PAYLOAD_ENTRY    1          /* word index: returns to Tooie and never comes back */
#define BKBT_PAYLOAD_CONTRACT 4          /* word index */
#define BKBT_PAYLOAD_HEADER_SIZE 0x14
/* Where a header may be: the Expansion Pak's half of RAM, which this game does not use. */
#define BKBT_PAYLOAD_RAM_START 0x80400000
#define BKBT_PAYLOAD_RAM_END   0x80800000

#define BKBT_FLAG_CHORD_ANYWHERE 0x00000001 /* the chord also works outside the front end */

/* The save device of the dual-boot image is 32 KiB of SRAM, shared with the Tooie hack, and this game's
 * save is BKBT_SRAM_BYTES of it (src/core1/eeprom.c), from the linker symbol bkbt_sram_offset on
 * (manual_syms.us.v10.txt). An offset into the SRAM, not an address: use it as (u32) bkbt_sram_offset.
 * The Tooie side's copy is DUALBOOT_BK_SRAM_OFFSET in hack/bt/n64/src/dualboot.h of the BKBT repository. */
extern u8 bkbt_sram_offset[];

/* The chord that asks for the return to Tooie, on pad 0: L and R down, and Start. It cannot be tested as
 * three buttons. With L, R and Start all down an N64 controller recalibrates its stick and reports that
 * instead of Start: in OSContPad.button START_BUTTON reads 0, the reset bit 0x0080 is set (libultra has
 * no name for it) and the stick reads centre. A console does that and so does ares, which emulates the
 * controller; Project64 and mupen64plus do not, and deliver START_BUTTON. Found on ares v148,
 * 2026-10-11, where L_TRIG | R_TRIG | START_BUTTON never matched in either game. So the chord is down
 * when both of BKBT_CHORD_HELD are and at least one of BKBT_CHORD_START is. The Tooie hack reads its
 * chord the same way (hack/bt/n64/src/main.c in the BKBT repository). */
#define BKBT_PAD_RESET   0x0080
#define BKBT_CHORD_HELD  (L_TRIG | R_TRIG)
#define BKBT_CHORD_START (START_BUTTON | BKBT_PAD_RESET)

/* Where the chord is read from: pad 0 as the controller last answered. The record is kept by
 * src/core1/joy.c, which has the reason why the game's own pad state will not do, and it is written in
 * one place, joy_readData, when a read of pad 0 has come back. A demo's recorded input never gets here.
 *
 * samples counts those reads since boot. While it is 0 the pad has not been read and button means
 * nothing: the chord is then neither down nor released, which matters because the chord counts only
 * after it has been seen released (bkbt_frame). The game reads the pad once per retrace (two reads per
 * drawn frame at 30 frames a second, measured 2026-10-12 on mupen64plus with a controller plugged in),
 * so the count wraps after two years and three months without a reset. The chord is then deaf until
 * the next read, a sixtieth of a second later.
 *
 * The layout is fixed for the tools, which find the record through bkbt_ptr_pad (BKBT.md, "Exports for
 * tools"): samples at +0, button at +4, 8 bytes with the padding. A tool that plays the pad on a game
 * without a controller stores button and a samples that is not 0. */
typedef struct {
    u32 samples; /* +0 */
    u16 button;  /* +4: OSContPad.button */
} BkbtPad;

/* The settings of the generated game (hack feature 2). The client writes them and this game only reads
 * them: one writer. The client writes every other field first and the seed last, so a seed that is not 0
 * means the rest is complete; it starts from seed 0 to change them in a running game. The Archipelago
 * world never generates a seed of 0. Every boot starts them at 0 (they are initialised data), so after a
 * reset or a launch from Tooie the client sees seed 0 and writes them again. */
typedef struct {
    u32 seed;       /* +0x00: the generated game's seed; written last; 0 = not written since this boot */
    u8 skip_intro;  /* +0x04: 1 = a new game skips the intro, 0 = it plays it */
    u8 padding[3];  /* +0x05: 0. Named, so that the size the u32 rounds the struct up to is visible */
} BkbtApSettings;   /* 8 bytes */

extern BkbtApSettings bkbt_ap_settings;

void bkbt_init(void);              /* src/core1/code_0.c: first thing in core1_init */
bool bkbt_launched(void);          /* src/core1/code_0.c: TRUE when the Tooie launcher started this game */
void bkbt_frame(void);             /* src/core1/code_0.c: once per mainLoop iteration */
bool bkbt_padButton(u16 *button);  /* src/core1/joy.c: FALSE while the pad has not been read since boot */
const BkbtApSettings *bkbt_apSettings(void); /* src/core1/code_0.c: the settings to play by, the client's or the defaults */
bool bkbt_apSettingsCommitted(void);         /* src/core1/code_0.c: TRUE once the client has written the settings since boot */

/* The items the player has received (hack feature 3: the moves since layout 0.3.0, the empty honeycombs
 * since 0.5.0, the jiggies since 0.6.0, the Mumbo tokens since 0.7.0, the note doors since 0.8.0). The client writes them and this
 * game only reads them: one writer. They are totals, written again and again, so a repeated write gives
 * nothing new. The seed commits these as well: the client writes the settings and the items first and
 * the seed last, and starts from seed 0 to change them in a running game. Every boot starts them at 0,
 * like the settings. A client writes a field only to a ROM whose anchor has the version that brought it:
 * in a 0.4.0 ROM the byte after the moves is the first byte of bkbt_ap_locations, and in a 0.5.0 ROM the
 * jiggies' byte is padding, as the tokens' byte is in a 0.6.0 ROM. */
#define BKBT_AP_MOVE_COUNT 20 /* the values of enum ability_e, 0..0x13 (include/core2/abilityprogress.h) */
#define BKBT_AP_HONEYCOMB_MAX 24 /* empty honeycombs in the game, enum honeycomb_e 1..0x18 (include/enums.h) */
#define BKBT_AP_JIGGY_MAX 100 /* jiggies in the game, enum jiggy_e 1..0x64 (include/enums.h) */
#define BKBT_AP_TOKEN_MAX 115 /* Mumbo tokens in the game, enum mumbotoken_e 1..0x73 (include/enums.h) */
/* The Lair's note doors, the door actor's selector 1..12 (50 .. 882 notes; FILEPROG_39 + selector is the
 * door's open flag, src/lair/lairspawnqueue.c). A door received opens when Banjo next comes near it,
 * instead of at a note total; the total reaching a door's cost is a location (below). */
#define BKBT_AP_NOTE_DOORS 12
#define BKBT_AP_NOTE_DOOR_SLOTS 13 /* indexed by the door's selector, 1..12; [0] is never set */

typedef struct {
    u8 moves[BKBT_AP_MOVE_COUNT]; /* +0x00: indexed by enum ability_e; not 0 = received */
    u8 honeycombs;                /* +0x14: empty honeycombs received; more than BKBT_AP_HONEYCOMB_MAX reads as that (since 0.5.0) */
    u8 jiggies;                   /* +0x15: jiggies received; more than BKBT_AP_JIGGY_MAX reads as that (since 0.6.0) */
    u8 tokens;                    /* +0x16: Mumbo tokens received; more than BKBT_AP_TOKEN_MAX reads as that (since 0.7.0) */
    u8 padding;                   /* +0x17: 0. Left free, so that the note doors start on a word */
    u8 note_doors[BKBT_AP_NOTE_DOOR_SLOTS]; /* +0x18: indexed by the note door's selector; not 0 = received (since 0.8.0) */
    u8 padding2[3];               /* +0x25: 0. Named, so that the room left for the next field is visible */
} BkbtApItems;                    /* 40 bytes since 0.8.0 (20 in 0.3.0 and 0.4.0, 24 from 0.5.0 to 0.7.0) */

extern BkbtApItems bkbt_ap_items;

/* The locations the player has checked (hack feature 4: the molehills since layout 0.4.0, the empty
 * honeycombs since 0.5.0, the jiggies since 0.6.0, the Mumbo tokens since 0.7.0, the note doors'
 * thresholds since 0.8.0). The game writes them and the client only reads them:
 * one writer. The client sends a check for every byte that is not 0 and that it has not sent yet; the
 * server ignores a check it already has. A byte can go back to 0 when another game is chosen, so a
 * client only ever adds checks.
 *
 * The molehills: a molehill sets its move's byte when the player first talks to it, and reads it back
 * for the first-visit text or the refresher. Saved with the game (SaveData.move_locations,
 * include/save.h), so the save also keeps checks made while no client was reading: loaded when A on the
 * file select chooses the game (never while browsing) and again after the mid-session reloads, zeroed by
 * clearScoreStates.
 *
 * The honeycombs, the jiggies and the Mumbo tokens: copies of the game's own records of the empty
 * honeycombs, the jiggies and the tokens picked up (src/core2/honeycombscore.c, src/core2/jiggyscore.c,
 * src/core2/mumboscore.c, which the game saves itself), made on every main-loop iteration while the
 * chosen game is in play (bkbt_file_chosen), so never from a file the file select is only showing.
 *
 * The note doors' thresholds: not 0 once the total of the worlds' best note counts
 * (itemscore_noteScores_getTotal, src/core2/code_BEF20.c, the total retail's doors read) has reached the
 * door's cost (bkbt_note_door_costs), whether or not Banjo is near the door. Worked out on every
 * main-loop iteration while bkbt_file_chosen, like the copies above; nothing to save, since the game
 * saves the best counts. */
#define BKBT_AP_HONEYCOMB_SLOTS 0x19 /* indexed by enum honeycomb_e, 1..0x18; [0] is never set (HONEYCOMB_COUNT in src/core2/honeycombscore.c) */
#define BKBT_AP_JIGGY_SLOTS 0x65 /* indexed by enum jiggy_e, 1..0x64; [0] is never set (include/enums.h) */
#define BKBT_AP_TOKEN_SLOTS 0x74 /* indexed by enum mumbotoken_e, 1..0x73; [0] is never set (include/enums.h) */

typedef struct {
    u8 moves[BKBT_AP_MOVE_COUNT];   /* +0x00: indexed by enum ability_e, the move a molehill teaches; not 0 = checked */
    u8 honeycombs[BKBT_AP_HONEYCOMB_SLOTS]; /* +0x14: indexed by enum honeycomb_e; not 0 = picked up (since 0.5.0) */
    u8 padding[3];                  /* +0x2D: 0 */
    u8 jiggies[BKBT_AP_JIGGY_SLOTS]; /* +0x30: indexed by enum jiggy_e; not 0 = picked up (since 0.6.0) */
    u8 padding2[3];                 /* +0x95: 0 */
    u8 tokens[BKBT_AP_TOKEN_SLOTS]; /* +0x98: indexed by enum mumbotoken_e; not 0 = picked up (since 0.7.0) */
    u8 note_doors[BKBT_AP_NOTE_DOOR_SLOTS]; /* +0x10C: indexed by the note door's selector; not 0 = its threshold reached (since 0.8.0) */
    u8 padding3[3];                 /* +0x119: 0 */
} BkbtApLocations;                  /* 284 bytes since 0.8.0 (48 in 0.5.0, 152 in 0.6.0, 268 in 0.7.0) */

extern BkbtApLocations bkbt_ap_locations;

/* TRUE while the game's score state (the honeycombs picked up, among others) is that of the game chosen
 * with A on the file select. Set where A chooses a game and after the mid-session reloads
 * (bkbt_gameFile_loadAP), cleared by clearScoreStates, which runs before the file select shows a file
 * and before a demo. Only the game and its pickups change the score state in between. */
extern bool bkbt_file_chosen;

/* src/core1/code_0.c: the number of empty honeycombs the game's own counts stand for, for the
 * rebuild of the counts (func_8034789C, src/core2/code_BEF20.c) while bkbt_file_chosen. */
s32 bkbt_apHoneycombsForHealth(void);

/* src/core1/code_0.c: the number of jiggies received that the game has given the player so far. The
 * game's own held count (ITEM_26_JIGGY_TOTAL) is saved and spent on the Lair's puzzles, so unlike the
 * honeycombs it cannot be rebuilt from the total received; instead this is saved beside it
 * (SaveData.jiggies_given, include/save.h), and the game gives what the client's total is above it.
 * Loaded with the chosen game (bkbt_saveData_loadAP), zeroed by clearScoreStates. */
extern s32 bkbt_jiggies_given;

/* src/core1/code_0.c: the number of Mumbo tokens received that the game has given the player so far,
 * for the same reason as the jiggies: the game's held count (ITEM_1C_MUMBO_TOKEN) is saved and spent at
 * Mumbo's. Saved beside it (SaveData.tokens_given, include/save.h), loaded with the chosen game
 * (bkbt_saveData_loadAP), zeroed by clearScoreStates. */
extern s32 bkbt_tokens_given;

/* src/core1/code_0.c: the notes each note door costs, by selector - 1: retail's costs
 * (note_door_cost_data, src/lair/lairspawnqueue.c:481), copied into core1, which unlike the Lair's
 * overlay is always in RAM. In the hack build only the thresholds read them: the doors open on their
 * items. */
extern s16 bkbt_note_door_costs[BKBT_AP_NOTE_DOORS];

/* src/core1/code_0.c: TRUE when the client has sent note door `door` (its selector, 1..12) and items
 * apply; the door actor opens on this instead of on a note total (src/lair/lairspawnqueue.c). */
bool bkbt_apNoteDoorReceived(s32 door);

/* The Archipelago anchor (docs/ROADMAP.md of the BKBT repository, hack feature 1). Everything the client
 * reads or writes moves with every build, so the client starts from the one word that does not: physical
 * 0x400000, the start of the Expansion Pak's half of RAM, which this game does not use. It holds the
 * KSEG0 address of bkbt_ap_anchor while this game runs. Before that it holds 0 (the Tooie launcher
 * clears it) or 0xFFFFFFFF (the IPL3's RAM test, in a ROM run by itself): neither is an address in RAM,
 * and a client takes either as "not running". The Tooie hack publishes its own anchor in the same word.
 * Stored through KSEG1, like the handoff block, so that no cached copy can be written back over it later. */
#define BKBT_AP_ANCHOR ((volatile u32 *) 0xA0400000) /* physical 0x400000 through KSEG1 */

#define BKBT_AP_MAGIC   0x424B4150 /* 'BKAP' */
#define BKBT_AP_GAME_BK 1          /* the Tooie hack would be 2 */
/* The version of this layout, not of the Archipelago world. Raise it when the layout changes. */
#define BKBT_AP_VERSION_MAJOR 0
#define BKBT_AP_VERSION_MINOR 8
#define BKBT_AP_VERSION_PATCH 0

/* What the anchor points to. A client checks magic, game_id and the version before it trusts anything
 * else, and reads nothing at or past size: later features append fields, so an older client still
 * reads a newer struct and a newer client sees from size what an older ROM has. magic comes first so
 * that a Tooie client, which reads +0 as its version, refuses this struct before it follows a pointer.
 * Big-endian, like everything the client reads. */
typedef struct {
    u32 magic;                  /* +0x00: BKBT_AP_MAGIC */
    u8 game_id;                 /* +0x04: BKBT_AP_GAME_BK */
    u8 padding;                 /* +0x05: 0. Named so that the gap the u16 below needs is visible */
    u16 size;                   /* +0x06: sizeof(BkbtApAnchor) */
    u16 version_major;          /* +0x08 */
    u8 version_minor;           /* +0x0A */
    u8 version_patch;           /* +0x0B */
    BkbtApSettings *settings;   /* +0x0C: &bkbt_ap_settings, which the client writes (since 0.2.0) */
    BkbtApItems *items;         /* +0x10: &bkbt_ap_items, which the client writes (since 0.3.0; 24 bytes since 0.5.0, 40 since 0.8.0) */
    BkbtApLocations *locations; /* +0x14: &bkbt_ap_locations, which the game writes and the client reads (since 0.4.0; 48 bytes since 0.5.0, 152 since 0.6.0, 268 since 0.7.0, 284 since 0.8.0) */
} BkbtApAnchor;                 /* 24 bytes */

extern BkbtApAnchor bkbt_ap_anchor;

/* The seed saved with the game the file select is showing (saveData_load sets it while browsing,
 * clearScoreStates zeroes it), for the file select's check against the committed seed. Not for saving. */
extern u32 bkbt_file_seed;

/* The seed of the game being played: set when A on the file select starts or loads a game (to the
 * committed seed, which the check there has just matched with the file's, or a new game takes), and
 * saved with the game by saveData_create. Nothing else writes it, so a client that changes its seed
 * during play, the mid-session reloads (Bottles' Bonus, the parade, the ending) and clearScoreStates
 * cannot change the seed a save is bound to. */
extern u32 bkbt_playing_seed;

#endif
