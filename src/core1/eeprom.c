#include <ultra64.h>
#include "core1/core1.h"
#include "save.h"
#if BKBT
#include <PRinternal/piint.h> /* osPiGetCmdQueue: libultra keeps it out of <ultra64.h> */
#include "bkbt.h"
#endif

#define ROUND_UP_DIVIDE(a, b) (((a) + (b) - 1) / (b))

// The round up divide is not technically needed, but will come in handy for modding
#define GAMEFILE_NUM_BLOCKS ROUND_UP_DIVIDE(sizeof(SaveData), EEPROM_BLOCK_SIZE)

#if BKBT
/* The save device of the dual-boot image. The image is one cartridge with one save device, 32 KiB of SRAM,
 * and no EEPROM. When the launcher started this game (bkbt_launched), the game's 512 bytes of "EEPROM"
 * are 512 bytes of that SRAM instead, from bkbt_sram_offset on (manual_syms.us.v10.txt): block n is the
 * 8 bytes at bkbt_sram_offset + 8 * n. Below that offset the SRAM is the Tooie hack's save. A ROM started
 * by itself never comes here and keeps its EEPROM. One game file is GAMEFILE_NUM_BLOCKS (15) blocks.
 *
 * The bytes travel as a request (an OSIoMesg) to libultra's PI manager, the thread that does all of this
 * game's cartridge transfers, with an SRAM handle of our own in it. Two reasons for doing it by hand:
 * the game's own call (osPiStartDma) ORs osRomBase into every address (lib/ultralib/src/io/pirawdma.c:40),
 * and in the image osRomBase is 0xB3000000, where this game's ROM is, so it cannot reach the SRAM; a
 * handle uses its own base instead (epirawdma.c:40). And osEPiStartDma, which builds this same request
 * (epidma.c:46-58), is not among the libultra files this ROM links; both builds share that list
 * (decompressed.us.v10.yaml), so adding it would change the matching build too.
 *
 * Main thread only, like every caller of the two functions at the end of this file: one request, one
 * queue and one buffer serve all transfers, and each call waits until its transfer is done. */
#define BKBT_SRAM_BYTES (EEPROM_MAXBLOCKS * EEPROM_BLOCK_SIZE) /* 512: the whole EEPROM */
static OSPiHandle sBkbtSramHandle; /* which device: the SRAM */
static OSIoMesg sBkbtSramIoMesg;   /* the request sent to the PI manager */
static OSMesgQueue sBkbtSramQueue; /* where the manager replies */
static OSMesg sBkbtSramMesg;       /* that queue's one slot */
static u8 sBkbtSramRaw[BKBT_SRAM_BYTES + 16]; /* 16 spare bytes: sBkbtSramBuf is rounded up inside it */

/* Every transfer goes through this buffer, never through the caller's. A DMA moves bytes between the
 * cartridge and RAM without passing through the CPU's data cache, so before every transfer the cache is
 * written back (RAM then holds what the CPU stored, and no dirty line can later land on top of what the
 * DMA brought), and after a read it is invalidated (the CPU then fetches what the DMA brought). Both work
 * on whole 16-byte lines, and a line shared with another variable would take that variable along. The
 * callers' buffers share lines with other variables (the game's save records are 0x78 bytes apart), so
 * this buffer starts on a line boundary and is a whole number of lines long. NULL until the first
 * transfer since boot (core1_main clears core1's bss).
 *
 * No emulator models the cache: a mistake here passes every test and fails only on a console. */
static u8 *sBkbtSramBuf;

/* One transfer of `size` bytes between the start of sBkbtSramBuf and the SRAM, `byteOffset` bytes after
 * bkbt_sram_offset. `type` is OS_MESG_TYPE_EDMAREAD (SRAM to buffer) or OS_MESG_TYPE_EDMAWRITE (buffer to
 * SRAM): the manager uses the handle only for these E types (devmgr.c:84-91); the plain DMA types would
 * go through osRomBase and reach this game's own ROM. Returns when the transfer is over. */
static void bkbt_sramTransfer(s32 type, s32 byteOffset, s32 size) {
    osWritebackDCache(sBkbtSramBuf, BKBT_SRAM_BYTES);

    sBkbtSramIoMesg.hdr.type = type; /* Which kind of transfer. */
    sBkbtSramIoMesg.hdr.retQueue = &sBkbtSramQueue; /* Where the manager sends its reply. */
    sBkbtSramIoMesg.dramAddr = sBkbtSramBuf; /* Where bytes go/are in memory. */
    sBkbtSramIoMesg.devAddr = (u32)bkbt_sram_offset + byteOffset; /* Where on the device, from its start. */
    sBkbtSramIoMesg.size = size; /* How many bytes. */
    sBkbtSramIoMesg.piHandle = &sBkbtSramHandle; /* Which device. */

    osSendMesg(osPiGetCmdQueue(), (OSMesg) &sBkbtSramIoMesg, OS_MESG_BLOCK);
    osRecvMesg(&sBkbtSramQueue, NULL, OS_MESG_BLOCK);

    if (type == OS_MESG_TYPE_EDMAREAD) {
        osInvalDCache(sBkbtSramBuf, BKBT_SRAM_BYTES);
    }
}

/* The SRAM side of eeprom_readBlocks and eeprom_writeBlocks. `direction` is OS_READ or OS_WRITE; `address`
 * and `count` are in 8-byte EEPROM blocks, counted from the start of this game's 512 bytes. Returns what
 * the game expects of the EEPROM calls: 0 for success, CONT_RANGE_ERROR (-1) for a range outside the 64
 * blocks (nothing is transferred), CONT_NO_RESPONSE_ERROR (8) for a write that does not read back as
 * written. */
static s32 bkbt_sramBlocks(s32 direction, s32 address, void *buffer, s32 count) {
    s32 i;
    s32 size = count * EEPROM_BLOCK_SIZE;
    s32 byteOffset = address * EEPROM_BLOCK_SIZE;

    /* Never outside this game's 64 blocks: below them is the Tooie hack's save, and nothing behind them is
     * used. Written so that no sum can overflow. */
    if (address < 0 || count <= 0 || count > EEPROM_MAXBLOCKS - address) {
        return CONT_RANGE_ERROR;
    }

    if (sBkbtSramBuf == NULL) {
        sBkbtSramHandle.type = DEVICE_TYPE_SRAM;
        sBkbtSramHandle.domain = PI_DOMAIN2;
        sBkbtSramHandle.baseAddress = PHYS_TO_K1(PI_DOM2_ADDR2); /* 0xA8000000 */

        osCreateMesgQueue(&sBkbtSramQueue, &sBkbtSramMesg, 1);
        sBkbtSramBuf = (u8 *)(((u32) sBkbtSramRaw + 15) & ~15); /* up to the next multiple of 16 */
    }

    if (direction == OS_WRITE) {
        bcopy(buffer, sBkbtSramBuf, size);
        bkbt_sramTransfer(OS_MESG_TYPE_EDMAWRITE, byteOffset, size);
    }

    /* A read, or the read-back of a write. First the buffer gets what a blank EEPROM holds, 0xFF. If
     * whatever plays the cartridge answers the transfer without delivering bytes, a read then gives the
     * game a file that fails its checksum instead of whatever the last transfer left here, and the
     * read-back of a write cannot match by accident on the bytes just copied in for it. A transfer that is
     * never answered cannot be caught here: osRecvMesg waits for good. */
    for (i = 0; i < size; i++) {
        sBkbtSramBuf[i] = (u8)0xFF;
    }

    bkbt_sramTransfer(OS_MESG_TYPE_EDMAREAD, byteOffset, size);

    if (direction == OS_READ) { /* hand the bytes to the caller */
        bcopy(sBkbtSramBuf, buffer, size);
        return 0;
    }

    /* A write counts only if the SRAM reads back what was written. An EEPROM reports a failed write; SRAM
     * reports nothing, so we look. The game depends on it: gameFile_8033CFD4 (src/core2/code_B5E00.c:88-103)
     * writes a file to the spare slot and wipes that file's old copy only when the write returned 0. A
     * write that returned 0 without the bytes being stored would cost the player that file. */
    for (i = 0; i < size; i++) {
        if (((u8 *)buffer)[i] != sBkbtSramBuf[i]) {
            return CONT_NO_RESPONSE_ERROR;
        }
    }

    return 0;
}
#endif

s32 eeprom_writeBlocks(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * GAMEFILE_NUM_BLOCKS + offset;
    s32 ret;

#if BKBT
    if (bkbt_launched()) {
        return bkbt_sramBlocks(OS_WRITE, address, buffer, count);
    }
#endif

    controller_func_8024F35C(3);
    ret = osEepromLongWrite(si_getReplyQueue(), address, buffer, count * EEPROM_BLOCK_SIZE);
    controller_func_8024F35C(0);

    return ret;
}

s32 eeprom_readBlocks(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * GAMEFILE_NUM_BLOCKS + offset;
    s32 ret;

#if BKBT
    if (bkbt_launched()) {
        return bkbt_sramBlocks(OS_READ, address, buffer, count);
    }
#endif

    controller_func_8024F35C(3);
    ret = osEepromLongRead(si_getReplyQueue(), address, buffer, count * EEPROM_BLOCK_SIZE);
    controller_func_8024F35C(0);

    return ret;
}
