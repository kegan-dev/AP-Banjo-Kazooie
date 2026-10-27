#ifndef __SAVE_H__
#define __SAVE_H__
#if BKBT
#include "bkbt.h"
#endif

typedef struct{
    u8 magic;
    u8 slotIndex;
    u8 data[0x70];
    u8 padding[0x2];
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
