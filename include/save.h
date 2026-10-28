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
     * as the held count it went into. The last of the padding: another such count needs a bigger record */
    u8 jiggies_given;
    u8 tokens_given;
#else
    u8 padding[0x2];
#endif
#if BKBT
    /* The seed the game is bound to and its checked locations (bkbt_ap_locations), saved with it */
    u32 seed;
    u8 move_locations[BKBT_AP_MOVE_COUNT];
#endif
    u32 checksum;
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
