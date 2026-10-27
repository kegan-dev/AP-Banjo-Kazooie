#ifndef BANJO_KAZOOIE_CORE1_EEPROM_H
#define BANJO_KAZOOIE_CORE1_EEPROM_H

#if BKBT
/* The hack uses its own read and write functions that use SRAM. Eliminates the
 * notion of EEPROM blocks */
s32 bkbt_write(s32 file, s32 offset, void *buffer, s32 count);
s32 bkbt_read(s32 file, s32 offset, void *buffer, s32 count);
#else
s32 eeprom_writeBlocks(s32 file, s32 offset, void *buffer, s32 count);
s32 eeprom_readBlocks(s32 file, s32 offset, void *buffer, s32 count);
#endif

#endif
