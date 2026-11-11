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
/* The hack's save device: 32 KiB of SRAM, by itself as in the dual-boot image (whose one cartridge has no
 * EEPROM); the exported ROM's header declares it (hack/build_bk_rom.sh). The game's records, which retail
 * keeps in a 4 Kbit EEPROM, are bytes of that SRAM from bkbt_sram_offset on (manual_syms.us.v10.txt):
 * the four game records (SaveData, now with the seed and the locations, include/save.h), then the global
 * record (GlobalData), BKBT_SRAM_BYTES in all. Below bkbt_sram_offset the SRAM is the Tooie hack's save.
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
#define BKBT_SRAM_BYTES (s32)(sizeof(SaveData) * 4 + sizeof(GlobalData)) /* 4 game records and the global one; s32, so that the range check below is signed */
/* The link checks that 0x4E0 bytes from bkbt_sram_offset on fit the SRAM (manual_syms.us.v10.txt), and the
 * tools know a record of 0x130 bytes (tools/bk_eeprom.py of the BKBT repository): stop a build whose
 * records add up to anything else, or whose record is not a whole number of EEPROM blocks, the unit of
 * the game's transfers. IDO has no _Static_assert; an array of size -1 does not compile. */
typedef char bkbt_sram_size_check[(sizeof(SaveData) == 0x130 && BKBT_SRAM_BYTES == 0x4E0
                                   && sizeof(SaveData) % EEPROM_BLOCK_SIZE == 0) ? 1 : -1];
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
 * callers' buffers share lines with other variables, so this buffer starts on a
 * line boundary and is a whole number of lines long. NULL until the first
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

/* One read or write of the game's save. `direction` is OS_READ or OS_WRITE; `offset` and `size` are in
 * bytes, `offset` counted from bkbt_sram_offset. Returns what the game's save code expects of retail's
 * EEPROM calls: 0 for success, CONT_RANGE_ERROR (-1) for a range outside the BKBT_SRAM_BYTES (nothing is
 * transferred), CONT_NO_RESPONSE_ERROR (8) for a write that does not read back as written. */
static s32 bkbt_sram(s32 direction, s32 offset, void *buffer, s32 size) {
    s32 i;

    /* Never outside this game's bytes: below them is the Tooie hack's save, and nothing behind them is
     * used. Written so that no sum can overflow. */
    if (offset < 0 || size <= 0 || size > BKBT_SRAM_BYTES - offset) {
        return CONT_RANGE_ERROR;
    }

    if (sBkbtSramBuf == NULL) {
        sBkbtSramHandle.type = DEVICE_TYPE_SRAM;
        sBkbtSramHandle.domain = PI_DOMAIN2;
        sBkbtSramHandle.baseAddress = PHYS_TO_K1(PI_DOM2_ADDR2); /* 0xA8000000 */
        /* The bus timing for the half of the cartridge bus the SRAM is on: a console has to be told how
         * fast it may talk to the chip. Nothing else in the image sets it, so without these values the
         * transfers would run on whatever the console was left with. libultra writes them to the PI at
         * the first transfer through this handle (EPI_SYNC, PRinternal/piint.h), and they stay: after a
         * visit to this game, the Tooie hack's own SRAM transfers run on them too. The values are the
         * ones Ocarina of Time uses for its SRAM. No emulator used so far acts on them, and they have not
         * been tried on a console. */
        sBkbtSramHandle.latency = 5;
        sBkbtSramHandle.pulse = 0x0C;
        sBkbtSramHandle.pageSize = 0x0D;
        sBkbtSramHandle.relDuration = 2;

        osCreateMesgQueue(&sBkbtSramQueue, &sBkbtSramMesg, 1);
        sBkbtSramBuf = (u8 *)(((u32) sBkbtSramRaw + 15) & ~15); /* up to the next multiple of 16 */
    }

    if (direction == OS_WRITE) {
        bcopy(buffer, sBkbtSramBuf, size);
        bkbt_sramTransfer(OS_MESG_TYPE_EDMAWRITE, offset, size);
    }

    /* A read, or the read-back of a write. First the buffer gets what a blank EEPROM holds, 0xFF. If
     * whatever plays the cartridge answers the transfer without delivering bytes, a read then gives the
     * game a file that fails its checksum instead of whatever the last transfer left here, and the
     * read-back of a write cannot match by accident on the bytes just copied in for it. A transfer that is
     * never answered cannot be caught here: osRecvMesg waits for good. */
    for (i = 0; i < size; i++) {
        sBkbtSramBuf[i] = (u8)0xFF;
    }

    bkbt_sramTransfer(OS_MESG_TYPE_EDMAREAD, offset, size);

    if (direction == OS_READ) { /* hand the bytes to the caller */
        bcopy(sBkbtSramBuf, buffer, size);
        return 0;
    }

    /* A write counts only if the SRAM reads back what was written. An EEPROM reports a failed write; SRAM
     * reports nothing, so we look. The game depends on it: gameFile_8033CFD4 (src/core2/code_B5E00.c)
     * writes a file to the spare slot and wipes that file's old copy only when the write returned 0. A
     * write that returned 0 without the bytes being stored would cost the player that file. */
    for (i = 0; i < size; i++) {
        if (((u8 *)buffer)[i] != sBkbtSramBuf[i]) {
            return CONT_NO_RESPONSE_ERROR;
        }
    }

    return 0;
}

/* The hack's replacements for retail's eeprom_writeBlocks and eeprom_readBlocks, in bytes instead of
 * 8-byte blocks: `offset` and `count` are bytes, from the start of record `file` (the global record is
 * file 0, offset 4 * sizeof(SaveData)). */
s32 bkbt_write(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * sizeof(SaveData) + offset;

    return bkbt_sram(OS_WRITE, address, buffer, count);
}

s32 bkbt_read(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * sizeof(SaveData) + offset;

    return bkbt_sram(OS_READ, address, buffer, count);
}
#else

s32 eeprom_writeBlocks(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * GAMEFILE_NUM_BLOCKS + offset;
    s32 ret;


    controller_func_8024F35C(3);
    ret = osEepromLongWrite(si_getReplyQueue(), address, buffer, count * EEPROM_BLOCK_SIZE);
    controller_func_8024F35C(0);

    return ret;
}

s32 eeprom_readBlocks(s32 file, s32 offset, void *buffer, s32 count) {
    s32 address = file * GAMEFILE_NUM_BLOCKS + offset;
    s32 ret;


    controller_func_8024F35C(3);
    ret = osEepromLongRead(si_getReplyQueue(), address, buffer, count * EEPROM_BLOCK_SIZE);
    controller_func_8024F35C(0);

    return ret;
}
#endif
