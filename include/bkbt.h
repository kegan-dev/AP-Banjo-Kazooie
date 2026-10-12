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

void bkbt_init(void);              /* src/core1/code_0.c: first thing in core1_init */
void bkbt_frame(void);             /* src/core1/code_0.c: once per mainLoop iteration */
bool bkbt_padButton(u16 *button);  /* src/core1/joy.c: FALSE while the pad has not been read since boot */

#endif
