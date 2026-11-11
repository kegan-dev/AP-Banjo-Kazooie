#ifndef BKBT_H
#define BKBT_H

#include <ultra64.h>
#include "bool.h"

/* BKBT=1 only (BKBT.md). What this ROM shares with the dual-boot image it can be part of, and the hooks
 * the game calls. A ROM started by itself finds none of it and behaves like the plain game.
 *
 * The handoff block mirrors hack/bt/n64/src/dualboot.h in the BKBT repository (dual-boot contract 4):
 * keep the two in step. The Tooie hack and its launcher own the layout. This side uses the ten words
 * below: it reads the token, the payload address, the destination and the links and zeroes them, counts
 * the countdown down, only reads the flags, and writes the destination and the link of its return.
 *
 * This side accesses the block through KSEG1 only. The launcher that returns to Tooie writes the block
 * uncached and then flushes the caches: a line of the block left dirty in this game's data cache would be
 * written back over what the launcher stored. No emulator models the caches, so only a console shows a
 * mistake here. */
#define BKBT_HANDOFF ((volatile u32 *) 0xA05FFF00) /* dualboot.h DUALBOOT_HANDOFF, 0x805FFF00 through KSEG1 */

/* Word indices into the block (byte offset / 4), from the table in dualboot.h. */
#define BKBT_HANDOFF_DEST_MAP  6  /* +0x18: BKBT_DEST_TAG | the map to arrive at in the game started, 0 = none (contract 3) */
#define BKBT_HANDOFF_DEST_EXIT 7  /* +0x1C: the destination's entrance; both written on every launch, zeroed by the game started */
#define BKBT_HANDOFF_TOKEN     8  /* +0x20: launch token, written on every launch of this game; bkbt_init zeroes it */
#define BKBT_HANDOFF_PAYLOAD   9  /* +0x24: KSEG0 address of the payload header loaded in this launch, 0 if none; bkbt_init zeroes it */
#define BKBT_HANDOFF_COUNTDOWN 10 /* +0x28: main-loop iterations before the game returns by itself, 0 = stay (tests) */
#define BKBT_HANDOFF_FLAGS     11 /* +0x2C: BKBT_FLAG_*, latched by bkbt_init */
#define BKBT_HANDOFF_LINK      12 /* +0x30: with a destination, the link of the Tooie file at the door (from Tooie) or of the save going back (to it); 0 = none (contract 4) */
#define BKBT_HANDOFF_FILE_LINKS 13 /* +0x34..+0x3C: from Tooie with a destination, the links its three files hold, 0 = none; to it 0. Like the destination: written on every launch, zeroed by the game started */

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
#define BKBT_FLAG_DOOR_TEST      0x00000002 /* tests: the countdown counts in the Lair's lobby, and then takes its exit */

/* The doors (contract 4; BKBT.md, "The hub door" and "The doors between the games"). Tooie's Spiral Mountain
 * entrance to its ruined Lair starts this game with the destination BKBT_DEST_TAG | MAP_69_GL_MM_LOBBY,
 * WARP_GL_MM_LOBBY_12_ENTRANCE and the link of the Tooie file at the door: the game boots into the file select,
 * which chooses the save of that link out of sight and starts it in the Lair. Since contract 5 any destination in
 * this game's maps: a Tooie world entrance that leads into a world of this game, or the way out of a Tooie world
 * behind one of the Lair's doors, starts the chosen game there (bkbt_doorWarp). In a game started so, the Lair's
 * way out to Spiral Mountain saves and returns to Tooie with the destination Tooie's Spiral Mountain at the way
 * out of its ruined Lair, in Tooie's numbering (the Tooie side's DUALBOOT_DOOR_* in dualboot.h), and the save's
 * link; and the exit map's entries for Tooie (BKBT_AP_GAME_TOOIE) save and return to theirs. */
#define BKBT_DEST_TAG 0x44530000          /* 'DS' in the high half; the map in the low */
#define BKBT_DOOR_TOOIE_MAP  0x0AF        /* Tooie's BT_MAP_SM */
#define BKBT_DOOR_TOOIE_EXIT 2
bool bkbt_doorArrival(void);  /* this boot came through a door and its game has not started yet */
void bkbt_doorArrived(void);  /* the file select started it */
/* The file select starts a door's game with this (timedFunc_set_2): the hub door's destination as Spiral Mountain's
 * entrance into the Lair goes (warp_lairEnterLairFromSMLevel: the Lair's cutscene, or its skip), any other straight
 * to its map and exit. src/core1/code_0.c */
void bkbt_doorWarp(s32 arg0, s32 arg1);
/* game_changeMap, once a map change's fade has played: the Lair's way out to Spiral Mountain in a game the hub
 * door started saves and goes back to Tooie, and then does not return. src/core1/code_0.c */
void bkbt_doorMapChange(s32 map, s32 exit);
/* The game to play at a door arrival, by the owner's order (2026-11-02): the save of the Tooie file's link;
 * else an unlinked save of the committed seed, which the link adopts; else an empty file; else a save whose
 * link no Tooie file holds any more, erased here for a new game. -1 when none is left (the player chooses).
 * Only with the client's seed committed. src/core1/code_0.c */
s32 bkbt_doorChoose(void);
u32 bkbt_doorLink(void);      /* the Tooie file's link this arrival came with, 0 for none */
/* The curtain (src/core1/code_0.c): from the hub door into this game until its destination's map, and from
 * the Lair's way out until the return, the screen is black (the file select's and the transitions'
 * osViBlack(0) wait for it) and the sound silent (every audio buffer is zeroed before the audio interface
 * plays it, bkbt_silence in src/core1/code_1D00.c's audio frame). */
bool bkbt_curtainUp(void);
void bkbt_curtainLift(void);  /* the player has to choose: the screen and the sound come back at once */
void bkbt_viBlack(bool black); /* the game's own osViBlack calls (the file select's, the transitions'), held black behind the curtain */
void bkbt_silence(void *buffer, u32 bytes);

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
    u8 notesanity;  /* +0x05: 1 = the notes are Archipelago items: a note picked up is only a check, and the
                     * game's note totals are the Notes received (since 0.11.0; latched when a game is chosen) */
    u8 skip_lair_cutscene; /* +0x06: 1 = the first entry into the Lair goes straight to the lobby, 0 = it plays
                     * Gruntilda's machine-room cutscene (since 0.12.0; read live, as skip_intro) */
    u8 padding[1];  /* +0x07: 0. Named, so that the size the u32 rounds the struct up to is visible */
} BkbtApSettings;   /* 8 bytes */

extern BkbtApSettings bkbt_ap_settings;

void bkbt_init(void);              /* src/core1/code_0.c: first thing in core1_init */
bool bkbt_launched(void);          /* src/core1/code_0.c: TRUE when the Tooie launcher started this game */
void bkbt_frame(void);             /* src/core1/code_0.c: once per mainLoop iteration */
bool bkbt_padButton(u16 *button);  /* src/core1/joy.c: FALSE while the pad has not been read since boot */
const BkbtApSettings *bkbt_apSettings(void); /* src/core1/code_0.c: the settings to play by, the client's or the defaults */
bool bkbt_apSettingsCommitted(void);         /* src/core1/code_0.c: TRUE once the client has written the settings since boot */

/* The items the player has received (hack feature 3: the moves since layout 0.3.0, the empty honeycombs
 * since 0.5.0, the jiggies since 0.6.0, the Mumbo tokens since 0.7.0, the note doors since 0.8.0, the
 * Jinjos since 0.10.0, the Notes since 0.11.0). The client writes them and this
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
/* The Jinjos, one item and one location per level and colour (since 0.10.0). A Jinjo is known only by
 * its colour and the level it is in (src/core2/ch/jinjo.c): the colour is its marker id less
 * MARKER_5A_JINJO_BLUE (include/enums.h), which is also its bit in the game's mask of the level's Jinjos,
 * ITEM_12_JINJOS (blue 0x01, green 0x02, orange 0x04, pink 0x08, yellow 0x10: jinjo.c shifts 1 by the
 * marker id + 6, which the CPU takes mod 32). Levels 1..0xA by enum level_e (include/enums.h): the nine
 * levels with Jinjos and the Lair (6), whose row is never set. */
#define BKBT_AP_JINJO_COLOURS 5
#define BKBT_AP_JINJO_LEVELS 10    /* enum level_e 1..0xA, by level - 1 */
#define BKBT_AP_JINJO_SLOTS 50     /* BKBT_AP_JINJO_INDEX(level, colour), 0..49; 25..29 (the Lair) never set */
#define BKBT_AP_JINJO_INDEX(level, colour) (((level) - 1) * BKBT_AP_JINJO_COLOURS + (colour))
#define BKBT_AP_JINJO_ALL 0x1F     /* the five colours' bits: the level's Jinjo jiggy */
/* The notes (since 0.11.0): 100 in each of the nine levels, numbered BKBT_AP_NOTE_INDEX(level, n), n 0..99
 * within the level: the level's maps by map id, each map's note sprites in the order the game reads them
 * from its setup (src/core2/code_A5BC0.c; the table in src/core1/code_0.c), then the five a hut drops in
 * Mumbo's Mountain and Bubblegloop Swamp (n 95..99, BKBT_AP_HUT_NOTE_FIRST). 0..999, the Lair's 500..599
 * never set. One bit each, byte index >> 3, bit index & 7, in the save and in the locations alike. */
#define BKBT_AP_NOTES_PER_LEVEL 100
#define BKBT_AP_NOTE_SLOTS 1000    /* BKBT_AP_NOTE_INDEX(level, n), levels 1..0xA */
#define BKBT_AP_NOTE_BYTES 125     /* BKBT_AP_NOTE_SLOTS bits */
#define BKBT_AP_NOTE_INDEX(level, n) (((level) - 1) * BKBT_AP_NOTES_PER_LEVEL + (n))
#define BKBT_AP_HUT_NOTE_FIRST 95  /* n of the first of a hut's five notes, in levels 1 and 4 */
#define BKBT_AP_NOTE_MAX 900       /* Notes received: more reads as this */

typedef struct {
    u8 moves[BKBT_AP_MOVE_COUNT]; /* +0x00: indexed by enum ability_e; not 0 = received */
    u8 honeycombs;                /* +0x14: empty honeycombs received; more than BKBT_AP_HONEYCOMB_MAX reads as that (since 0.5.0) */
    u8 jiggies;                   /* +0x15: jiggies received; more than BKBT_AP_JIGGY_MAX reads as that (since 0.6.0) */
    u8 tokens;                    /* +0x16: Mumbo tokens received; more than BKBT_AP_TOKEN_MAX reads as that (since 0.7.0) */
    u8 padding;                   /* +0x17: 0. Left free, so that the note doors start on a word */
    u8 note_doors[BKBT_AP_NOTE_DOOR_SLOTS]; /* +0x18: indexed by the note door's selector; not 0 = received (since 0.8.0) */
    u8 padding2[3];               /* +0x25: 0. Named, so that the room left for the next field is visible */
    u8 jinjos[BKBT_AP_JINJO_SLOTS]; /* +0x28: indexed by BKBT_AP_JINJO_INDEX; not 0 = received (since 0.10.0) */
    u16 notes;                    /* +0x5A: Notes received; more than BKBT_AP_NOTE_MAX reads as that (since 0.11.0;
                                   * the game reads it only with notesanity) */
} BkbtApItems;                    /* 92 bytes since 0.10.0 (20 in 0.3.0 and 0.4.0, 24 from 0.5.0 to 0.7.0, 40 in 0.8.0 and 0.9.0) */

extern BkbtApItems bkbt_ap_items;

/* The locations the player has checked (hack feature 4: the molehills since layout 0.4.0, the empty
 * honeycombs since 0.5.0, the jiggies since 0.6.0, the Mumbo tokens since 0.7.0, the note doors'
 * thresholds since 0.8.0, Gruntilda beaten since 0.9.0). The game writes them and the client only reads them:
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
 * saves the best counts.
 *
 * Gruntilda beaten, the Archipelago goal (since 0.9.0): a copy of the game's own saved flag
 * FILEPROG_FC_DEFEAT_GRUNTY, which the final battle sets (src/fight/chfinalboss.c), made on every
 * main-loop iteration while bkbt_file_chosen, like the copies above.
 *
 * The Jinjos (since 0.10.0): a Jinjo picked up sets its byte instead of the level's mask, so it gives
 * nothing by itself; its item comes from the client like any other. The game keeps no record of the
 * Jinjos (its mask lasts one level visit), so the hack keeps one, bkbt_jinjos_checked, saved with the
 * game (SaveData.jinjos_checked, include/save.h) like the molehills, and a checked Jinjo no longer
 * spawns. Published from it on every main-loop iteration while bkbt_file_chosen.
 *
 * The notes (since 0.11.0): a bit per note number (BKBT_AP_NOTE_INDEX), set when the note is picked up.
 * The game keeps the notes of one level visit only, so the hack keeps every note picked up,
 * bkbt_notes_collected, saved with the game (SaveData.notes_collected), and a note kept no longer
 * appears; with or without notesanity. Published from it on every main-loop iteration while
 * bkbt_file_chosen. With notesanity the note doors' thresholds above count the Notes received. */
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
    u8 gruntilda_beaten;            /* +0x119: not 0 = FILEPROG_FC_DEFEAT_GRUNTY set, the goal (since 0.9.0; padding before) */
    u8 padding3[2];                 /* +0x11A: 0 */
    u8 jinjos[BKBT_AP_JINJO_SLOTS]; /* +0x11C: indexed by BKBT_AP_JINJO_INDEX; not 0 = picked up (since 0.10.0) */
    u8 padding4[2];                 /* +0x14E: 0 */
    u8 notes[BKBT_AP_NOTE_BYTES];   /* +0x150: a bit per BKBT_AP_NOTE_INDEX; set = picked up (since 0.11.0) */
    u8 padding5[3];                 /* +0x1CD: 0 */
} BkbtApLocations;                  /* 464 bytes since 0.11.0 (48 in 0.5.0, 152 in 0.6.0, 268 in 0.7.0, 284 in 0.8.0 and 0.9.0, 336 in 0.10.0) */

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

/* src/core1/code_0.c: the Jinjos picked up in the chosen game, one mask of the colours' bits per level
 * (by level - 1, BKBT_AP_JINJO_LEVELS). The game keeps no such record, so the hack does: saved with the
 * game (SaveData.jinjos_checked, include/save.h), loaded with the chosen game (bkbt_saveData_loadAP),
 * zeroed by clearScoreStates. A Jinjo whose bit is set does not spawn (src/core2/ch/jinjo.c). */
extern u8 bkbt_jinjos_checked[BKBT_AP_JINJO_LEVELS];

/* src/core1/code_0.c: the Jinjos, for src/core2/ch/jinjo.c and the level and world updates.
 * bkbt_apJinjosActive: TRUE while the chosen game is in play in a level with Jinjos; the Jinjos are
 * Archipelago's then, and retail's otherwise (demos). bkbt_apJinjoChecked/bkbt_apJinjoCheck: read and set
 * the Jinjo of `colour` (0..4) in `level`. bkbt_apJinjosLevelEntry: sets the level's mask to the Jinjos
 * received, quietly, after level_load has zeroed it (src/core2/code_9A740.c). bkbt_apJinjoJiggy: spawns
 * the level's Jinjo jiggy on Banjo once its five Jinjos are received (src/core2/code_ABC00.c). */
bool bkbt_apJinjosActive(s32 level);
bool bkbt_apJinjoChecked(s32 level, s32 colour);
void bkbt_apJinjoCheck(s32 level, s32 colour);
void bkbt_apJinjosLevelEntry(s32 level);
void bkbt_apJinjoJiggy(void);

/* src/core1/code_0.c: TRUE for the level's Jinjo jiggy while bkbt_apJinjosActive, when bkbt_apJinjoJiggy is
 * its only spawner. src/core2/code_ABC00.c asks as a jiggy spawns, and src/core2/code_41FB0.c holds this one
 * where it appears, collectable at once (bkbt_bundle_holdCollectable): Banjo takes it there. */
bool bkbt_apIsJinjoJiggy(s32 jiggy);
struct actor_s;
void bkbt_bundle_holdCollectable(struct actor_s *actor);

/* src/core1/code_0.c: the notes picked up in the chosen game, a bit per BKBT_AP_NOTE_INDEX. The game keeps
 * those of one level visit only, so the hack does: saved with the game (SaveData.notes_collected,
 * include/save.h), loaded with the chosen game (bkbt_saveData_loadAP), zeroed by clearScoreStates. A note
 * whose bit is set does not appear (bkbt_notesSetupNote). */
extern u8 bkbt_notes_collected[BKBT_AP_NOTE_BYTES];

/* src/core1/code_0.c: the notes, for the setup read (src/core2/gccube.c, src/core2/code_A5BC0.c), the
 * pickups (src/core2/ba/marker.c), the huts' drop (src/core2/code_41FB0.c), the level entry
 * (src/core2/code_9A740.c), the load (src/core2/code_B5E00.c) and the counts (src/core2/code_BEF20.c).
 * bkbt_notesSetupBegin/bkbt_notesSetupNote/bkbt_notesSetupEnd: number the map's notes as its setup is read;
 * TRUE for a note the chosen game has kept, which is to stay hidden. bkbt_apNotePicked (a placed note, by
 * position) and bkbt_apHutNotePicked: keep the note; TRUE when the game is to count it as retail does.
 * bkbt_apHutNotesLeft: how many of a hut's `count` notes to drop. bkbt_apNotesLevelEntry: the level's count
 * starts at its notes kept. bkbt_apNotesLoaded: after the chosen game is loaded. bkbt_apNotesTotal: the
 * note total the game shows and the thresholds read. bkbt_apNotesKept: TRUE while notes are kept, when the
 * game's "a new best" texts mean nothing. */
void bkbt_notesSetupBegin(void);
bool bkbt_notesSetupNote(s16 position[3]);
void bkbt_notesSetupEnd(void);
bool bkbt_apNotePicked(s16 position[3]);
bool bkbt_apHutNotePicked(void);
s32 bkbt_apHutNotesLeft(s32 count);
void bkbt_apNotesLevelEntry(s32 level);
void bkbt_apNotesLoaded(void);
s32 bkbt_apNotesTotal(s32 total);
bool bkbt_apNotesKept(void);

/* The exit map (hack feature 6, since 0.13.0): where a map change leads. The client writes it and this game only
 * reads it, as the items. Entry by entry from the first, until one whose on_map is 0: a map change that the game
 * asks for while Banjo is in on_map, to og_map at og_exit, goes to to_map at to_exit instead (bkbt_mapChange, from
 * game_changeMap). Every boot starts it at 0, an empty table, which plays as retail; the client writes it before
 * the seed, as the items, so a seed that is not 0 means it is complete. to_game is the game the destination is in:
 * this one (BKBT_AP_GAME_BK), or Tooie (BKBT_AP_GAME_TOOIE, since 0.14.0: in a game a door started, the change saves
 * and returns to Tooie at to_map/to_exit, in Tooie's numbering, the doors between the games; in any other game the
 * entry is passed over, as if it were not there). The client writes only the doors that do not lead where they do
 * in retail (worlds/banjo_kazooie/Entrances.py and worlds/banjo_kazooie_tooie/Entrances.py of the BKBT repository):
 * BK's nine world entrances in the Lair, each with its way out, which leads back to the door that was used, in
 * whichever game it is. */
#define BKBT_AP_EXIT_SLOTS 32
#define BKBT_AP_GAME_TOOIE 2        /* a to_game in Tooie, taken only in a game a door started */
typedef struct {
    u16 on_map;     /* +0x00: the map Banjo is in when the game asks (gsworld_getMap()); 0 ends the table */
    u16 og_map;     /* +0x02: the map the game asks for */
    u8 og_exit;     /* +0x04: the exit it asks for */
    u8 to_game;     /* +0x05: BKBT_AP_GAME_BK or BKBT_AP_GAME_TOOIE; anything else is passed over */
    u16 to_map;     /* +0x06: in to_game's numbering, so a u16 for Tooie's maps; this game's fit a byte */
    u8 to_exit;     /* +0x08 */
    u8 padding[3];  /* +0x09: 0 */
} BkbtApExit;       /* 12 bytes */
typedef struct {
    BkbtApExit entries[BKBT_AP_EXIT_SLOTS]; /* +0x000 */
    u32 end;        /* +0x180: 0, never written: ends a full table, and its u32 puts the struct on a word boundary,
                     * as the client's reads want (IDO has no aligned attribute) */
} BkbtApExits;      /* 388 bytes */

extern BkbtApExits bkbt_ap_exits;

/* src/core1/code_0.c. bkbt_mapChange: game_changeMap (src/core2/code_5C870.c) asks it about every map change, with
 * the change's game mode and level reset; TRUE when the exit map sent it elsewhere, *map and *exit then the new
 * destination. It also turns Banjo back into himself on his way between the Lair and a world. bkbt_quickTransform:
 * Banjo's update (func_80295C14, src/core2/bsmethods.c) asks it right after the buttons are read: D-pad left in
 * the Lair, near the door of a world with a Mumbo form paid for, transforms him (and back), and since 0.15.0
 * anywhere in that world too (each counts a tag, bkbt_ap_link). */
bool bkbt_mapChange(s32 mode, s32 reset_level, s32 *map, s32 *exit);
void bkbt_quickTransform(void);

/* The link (since 0.15.0: hack feature 7, death link, with the traps and tag link): what passes between this game
 * and the other games of the multiworld while it is played. Unlike the blocks above, both sides write it, each its
 * own fields: the client its requests (written directly, not through the seed's commit: a death must not take the
 * items away for a frame), the game what it did. Every boot starts it at 0, and the game acts on it only while the
 * seed is committed and a game is chosen (bkbt_apLink).
 * - Deaths: a death the client requests (deaths_requested above deaths_applied) kills Banjo as soon as he can die,
 *   and the game counts his own deaths (deaths), not those a request caused, for the client to send.
 * - Tags (tag link, as Tooie's: its D-pad left swap): a tag the client requests turns Banjo into his world's Mumbo
 *   form, or back, in a world only (elsewhere it waits); D-pad left that changes his form counts one (tags).
 * - Traps: the client writes the totals of each kind given to this game, and the game plays them one at a time
 *   while the counts it has played, saved with the game (bkbt_traps_applied, SaveData.traps_applied), are below. The
 *   kinds are Tooie's, in its order (pc.traps of the Tooie hack, shared/include/ap_memory/items.h). */
#define BKBT_AP_TRAP_KINDS 5
#define BKBT_AP_TRAP_TRIP 0         /* knocked back, without damage */
#define BKBT_AP_TRAP_SLIP 1         /* five seconds of sliding */
#define BKBT_AP_TRAP_TRANSFORM 2    /* Mumbo's transformation, back into the form he has */
#define BKBT_AP_TRAP_SQUISH 3       /* a face-plant, without damage */
#define BKBT_AP_TRAP_TIP 4          /* one of Bottles' refreshers */
typedef struct {
    u8 deaths_requested;                  /* +0x00: client: deaths to apply this boot, in all */
    u8 tags_requested;                    /* +0x01: client: tags to apply this boot, in all */
    u8 traps[BKBT_AP_TRAP_KINDS];         /* +0x02: client: the traps of each kind given to this game, in all */
    u8 padding;                           /* +0x07: 0 */
    u8 deaths;                            /* +0x08: game: Banjo's own deaths this boot */
    u8 deaths_applied;                    /* +0x09: game: requested deaths applied this boot */
    u8 tags;                              /* +0x0A: game: forms changed with D-pad left this boot */
    u8 tags_applied;                      /* +0x0B: game: requested tags applied (or spent) this boot */
    u8 traps_applied[BKBT_AP_TRAP_KINDS]; /* +0x0C: game: the traps of each kind played, saved with the game */
    u8 padding2[3];                       /* +0x11: 0 */
} BkbtApLink;                             /* 20 bytes */

extern BkbtApLink bkbt_ap_link;

/* The traps of each kind the chosen game has played (src/core1/code_0.c): loaded and saved with it
 * (SaveData.traps_applied), zeroed by clearScoreStates, published in bkbt_ap_link. */
extern u8 bkbt_traps_applied[BKBT_AP_TRAP_KINDS];

/* src/core1/code_0.c. bkbt_apDied: the end of every death (func_8029B890, src/core2/code_14420.c), once. bkbt_apLink:
 * the end of the world update (func_80332E08, src/core2/code_ABC00.c), never while paused: deaths, tags and traps
 * applied. */
void bkbt_apDied(void);
void bkbt_apLink(void);

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
#define BKBT_AP_VERSION_MINOR 15
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
    BkbtApItems *items;         /* +0x10: &bkbt_ap_items, which the client writes (since 0.3.0; 24 bytes since 0.5.0, 40 since 0.8.0, 92 since 0.10.0) */
    BkbtApLocations *locations; /* +0x14: &bkbt_ap_locations, which the game writes and the client reads (since 0.4.0; 48 bytes since 0.5.0, 152 since 0.6.0, 268 since 0.7.0, 284 since 0.8.0, 336 since 0.10.0) */
    BkbtApExits *exits;         /* +0x18: &bkbt_ap_exits, which the client writes (since 0.13.0) */
    BkbtApLink *link;           /* +0x1C: &bkbt_ap_link, which both write, each its own fields (since 0.15.0) */
} BkbtApAnchor;                 /* 32 bytes since 0.15.0 (28 from 0.13.0) */

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

/* The link of the game being played to its Tooie file (the hub door, contract 4): loaded with the save it
 * was chosen from (bkbt_saveData_loadAP; 0 for a new game or an unlinked save), set to the Tooie file's link
 * when a hub door arrival chooses the game, saved with it by saveData_create (SaveData.link) and handed back
 * to Tooie with the return through the door. */
extern u32 bkbt_playing_link;

#endif
