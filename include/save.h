#ifndef __SAVE_H__
#define __SAVE_H__
#if BKBT
#include "bkbt.h"
#endif

typedef struct{
    u8 magic;
    u8 slotIndex;
    u8 data[0x70];
#if BKBT
    /* Retail's padding, never written by retail: the jiggies and the Mumbo tokens received that this game
     * had given when it was saved (bkbt_jiggies_given, bkbt_tokens_given), each saved in the same record
     * as the held count it went into. The last of retail's padding */
    u8 jiggies_given;
    u8 tokens_given;
#else
    u8 padding[0x2];
#endif
#if BKBT
    /* The seed the game is bound to and its checked locations (bkbt_ap_locations), saved with it */
    u32 seed;                               /* +0x74 */
    u8 move_locations[BKBT_AP_MOVE_COUNT];  /* +0x78 */
    /* The Jinjos picked up (bkbt_jinjos_checked), one mask per level, since the Jinjos' build: the
     * record grew from 0x90 to 0xA0 bytes for them */
    u8 jinjos_checked[BKBT_AP_JINJO_LEVELS]; /* +0x8C */
    /* The notes picked up (bkbt_notes_collected), a bit each, since the notes' build: the record grew from
     * 0xA0 to 0x120 bytes for them */
    u8 notes_collected[BKBT_AP_NOTE_BYTES]; /* +0x96 */
    u8 pad;                                 /* +0x113: 0 */
    /* The link to the game's Tooie file (bkbt_playing_link; the hub door, contract 4), since the links'
     * build: spare bytes before it, so the record stays 0x120 bytes and earlier saves read 0, no link */
    u32 link;                               /* +0x114 */
    u8 spare[4];                            /* +0x118: 0. Named, so that the room left for the hack is visible */
#endif
    u32 checksum;                           /* the last word: +0x11C in the hack build, +0x74 in retail */
}SaveData;

typedef struct {
    u32 snsItems;
    u8 padding[0x18];
    u32 checksum;
}GlobalData;

#if BKBT
void bkbt_saveData_loadAP(SaveData*);
void bkbt_gameFile_loadAP(s32);
#endif
#endif
