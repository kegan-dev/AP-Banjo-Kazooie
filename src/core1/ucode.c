#include <ultra64.h>
#include <PR/rcp.h>
#include "core1/core1.h"

#define IPL3FONT_ROM_ADDR 0x0B70
#define UCODE_SIZE 256

static u8 sUcodeData[UCODE_SIZE];
#if ANTI_TAMPER
static s32 sIMEMdword;
static s32 sDMEMdword;
static s32 sStatus;
#endif

#if BKBT
/* For the tools that test a ROM (BKBT.md, "Exports for tools"): sUcodeData has no symbol. A tool finds
 * this pointer in the map, reads it from RAM and compares the buffer with the ROM's IPL3 at 0xB70. */
u8 *bkbt_ptr_ucode = sUcodeData;
#endif

void ucode_load(void) {
#if ANTI_TAMPER
    sDMEMdword = IO_READ(SP_DMEM_START) ^ 0xFFFFFFFF;
    sStatus = sDMEMdword ? 0x01 : 0x00;

    sIMEMdword = IO_READ(SP_IMEM_START) ^ 0x000017D7;
    sStatus |= sIMEMdword ? 0x02 : 0x00;

    if (sStatus == 0) {
        parallel_readDMA(sUcodeData, PHYS_TO_K1(PI_DOM1_ADDR2 + IPL3FONT_ROM_ADDR), UCODE_SIZE);
    }
#else
    parallel_readDMA(sUcodeData, PHYS_TO_K1(PI_DOM1_ADDR2 + IPL3FONT_ROM_ADDR), UCODE_SIZE);
#endif
}

void ucode_stub1(void) {}

void ucode_stub2(void) {
    osPiReadIo(0, NULL);
}

s32 ucode_stub3(void) {
    return 0;
}

void ucode_getPtrAndSize(void **ptr, u32 *size) {
    *ptr = sUcodeData;
    *size = UCODE_SIZE;
}
