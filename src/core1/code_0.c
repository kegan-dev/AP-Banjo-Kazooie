#include <ultra64.h>
#include <PRinternal/macros.h>
#include <PRinternal/viint.h>
#include "core1/core1.h"
#include "functions.h"
#include "variables.h"
#include "version.h"
#if BKBT
#include "bkbt.h"
#include "core2/abilityprogress.h"
#endif

s32 D_80275610 = 0; // always set to 0
s32 D_80275614 = 0;
s32 gGlobalTimer = 0;

/**
 * An unused Konami-esque button combo.
 *
 * Not used for a crash debugger like I initially thought,
 * despite a similar one being in Tooie.
 *
 * It was actually for a security code of some kind.
 */
u32 sKonamiCode_8027561C[] =
{
    JOY_BUTTON_D_UP,
    JOY_BUTTON_C_UP,
    JOY_BUTTON_D_DOWN,
    JOY_BUTTON_C_DOWN,
    JOY_BUTTON_D_LEFT,
    JOY_BUTTON_C_LEFT,
    JOY_BUTTON_D_RIGHT,
    JOY_BUTTON_C_RIGHT,
    JOY_BUTTON_A,
    JOY_BUTTON_B,
    JOY_BUTTON_Z,
    JOY_BUTTON_START,
    JOY_BUTTON_nil
};

s32 D_80275650 = VER_SELECT(0xAD019D3C, 0xA371A8F3, 0, 0); //SM_DATA_CRC2
s32 D_80275654 = VER_SELECT(0xD381B72F, 0xD0709154, 0, 0); //MM_DATA_CRC2

/**
 * "HjunkDire" -- a random, unique, build ID code.
 * 
 * It needed to be a string that could be easily searchable in a ROM,
 * so that its build date and who it was sent to could be found.
 * 
 * The numbers were random - they were only required to be unique and
 * differentiable from prior builds.
 * 
 * The reason for the naming though is still not clear - it's likely
 * obscure and randomised also, to disguise its true purpose in the ROM.
 * 
 * Some builds were sent to the playtester Huw Ward.
 */
char gBuildIdentifier[] = VER_SELECT(
    "HjunkDire:218755", /* Build date: 20 May 1998 */
    "HjunkDire:300875", /* Build date: 15 June 1998 */
    "HjunkDire:746580", /* Build date: 14 July 1998 (listed as "Japanese version" for some reason) */
    "HjunkDire:309178"  /* Build date: unknown */
    /* Debug ramdumps: HjunkDire:584817 - Build date: 18 Nov 1997 ("Shoshinkai Demo") */
);

u32 D_8027A130; // always set to 3
u8 pad_8027A138[0x400];
STACK(sMainThreadStack, 0x1800);
OSThread sMainThread;
s32 gBootMap;
bool sDisableInput;
u64 sDebugVar_8027BEF0; // never used

extern u8 core1_BSS_START[];
extern u8 core1_BSS_END[];

void core1_main(void *arg) { 
    bzero(core1_BSS_START, core1_BSS_END - core1_BSS_START);
    osWritebackDCacheAll();
    sns_find_and_parse_payload();
    osInitialize();
    initThread_create();
}

void func_8023DA74(void) {
    func_8033BD6C();
    func_80255198(); //heap_flush_free_queue
}

void func_8023DA9C(s32 next_state) {
    core1_15B30_sendMesg3ToRenderThread();
    viMgr_clearFramebuffers();

    if (D_8027A130 == 4) {
        func_802E3580();
    }

    if (D_8027A130 == 3) {
        game_free();
    }

    func_8023DA74();
    D_8027A130 = next_state;
    
    if (D_8027A130 == 3) {
        game_init(gBootMap);
    }

    if (D_8027A130 == 4) {
        dummy_func_802E35D0();
    }

    ucode_stub1();
}

s32 globalTimer_getTimeMasked(s32 mask) {
    return gGlobalTimer & mask;
}

s32 globalTimer_getTime(void) {
    return gGlobalTimer;
}

void globalTimer_reset(void) {
    gGlobalTimer = 0;
}

enum map_e getSpecialBootMap(void) {
    return DEBUG_use_special_bootmap() ? MAP_80_GL_FF_ENTRANCE : MAP_91_FILE_SELECT;
}

enum map_e getDefaultBootMap(void) {
    return MAP_1F_CS_START_RAREWARE;
}

void func_8023DBAC(void) {
    setBootMap(getDefaultBootMap());
    func_8023DFF0(3);
}

void func_8023DBDC(void) {
    setBootMap(getSpecialBootMap());
    func_8023DFF0(3);
}

#if BKBT
/* The way back to Tooie in a dual-boot image (dual-boot contract 2; layout and the reason for the
 * uncached access in include/bkbt.h). The Tooie hack's launcher announces every start of this game with
 * a token in the handoff block and passes the address of the payload it has just loaded. The payload is
 * not part of this ROM and holds one thing, the return launcher. This file decides when to call it.
 *
 * Trust comes from the token alone, and the token is taken away the moment it is read. A ROM started by
 * itself, a reset of this game and RAM left over from an earlier session all find no token, so nothing
 * that happens to lie in Expansion Pak RAM is ever called, however much it looks like a payload.
 *
 * The statics are initialised data on purpose: they hold a value from the moment core1 is loaded, with
 * nothing depending on who clears which bss. */
static bool sBkbtLaunched = FALSE;            /* this game was started by the launcher: bkbt_launched() */
static u32 sBkbtFlags = 0;                    /* BKBT_FLAG_*, latched at boot */
static void (*sBkbtReturnEntry)(void) = NULL; /* the payload's entry: returns to Tooie, never to its caller; call it through bkbt_return */
static bool sBkbtChordArmed = FALSE;          /* the pad has been read with the chord not down since boot */

/* The Archipelago settings (include/bkbt.h): the client writes them, the game only reads them. All 0, so
 * at every boot they read as not written yet; the anchor points here from the first instruction. */
BkbtApSettings bkbt_ap_settings = {
    0,
    0,
    {0, 0, 0}
};

/* TRUE once the client has written the settings since this boot: it writes the seed last, and a seed is
 * never 0 (include/bkbt.h). The only place that rule is written down. */
bool bkbt_apSettingsCommitted(void) {
    return bkbt_ap_settings.seed != 0;
}

/* What the game plays by while no client has committed: the hack's own behaviour before settings existed.
 * Only bkbt_apSettings hands it out. */
static const BkbtApSettings sBkbtApDefaults = {
    0,
    TRUE,
    {0, 0, 0}
};

/* The settings the game plays by: the client's once committed, the defaults until then. Game code reads
 * settings only through here, so that no reader sees a half-written struct or takes its zeros for values. */
const BkbtApSettings *bkbt_apSettings(void) {
    if (bkbt_apSettingsCommitted()) {
        return &bkbt_ap_settings;
    }

    return &sBkbtApDefaults;
}

/* The Archipelago items (include/bkbt.h): written by the client, read by the game, all 0 at every boot. */
BkbtApItems bkbt_ap_items = { { 0 } };

/* mainLoop iterations on which bkbt_frame copied the items from the client. For the
 * tools that test a ROM: a test that expects items not to be copied proves nothing unless the game is
 * shown to have applied items. The one static here that is not initialised: a tool reaches it through
 * the pointer below and checks that against this file's .bss (BKBT.md, "Exports for tools"). core1_main
 * clears it at every boot. */
static u32 sBkbtApItemPasses;
u32 *bkbt_ptr_ap_item_passes = &sBkbtApItemPasses;

static bool bkbt_apItemsAllowed(void) {
    enum map_e map;
    s32 gameMode;

    map = gsworld_getMap();
    gameMode = getGameMode();
    return bkbt_apSettingsCommitted() &&
        map != MAP_1E_CS_START_NINTENDO &&
        map != MAP_1F_CS_START_RAREWARE &&
        map != MAP_91_FILE_SELECT &&
        map != MAP_8E_GL_FURNACE_FUN &&
        gameMode == GAME_MODE_3_NORMAL;
}

static void bkbt_apApplyItems(void) {
    u8 moveIx;

    if (!bkbt_apItemsAllowed()) {
        return;
    }

    for (moveIx = 0; moveIx < BKBT_AP_MOVE_COUNT; moveIx++) {
        ability_setLearned(moveIx, bkbt_ap_items.moves[moveIx] != 0);
    }

    sBkbtApItemPasses++;
}

/* The Archipelago anchor (include/bkbt.h). Initialised data, so it is right from the first instruction:
 * bkbt_init only has to publish its address. A global, so that the map carries it for the tools. */
BkbtApAnchor bkbt_ap_anchor = {
    BKBT_AP_MAGIC,
    BKBT_AP_GAME_BK,
    0,
    sizeof(BkbtApAnchor),
    BKBT_AP_VERSION_MAJOR,
    BKBT_AP_VERSION_MINOR,
    BKBT_AP_VERSION_PATCH,
    &bkbt_ap_settings,
    &bkbt_ap_items
};

/* mainLoop iterations on which bkbt_frame found the chord down, whether or not it went anywhere. For the
 * tools that test a ROM: a test that expects the chord to do nothing proves nothing unless the game is
 * shown to have seen the chord. The one static here that is not initialised: a tool reaches it through
 * the pointer below and checks that against this file's .bss (BKBT.md, "Exports for tools"). core1_main
 * clears it at every boot. */
static u32 sBkbtChordSeen;
u32 *bkbt_ptr_chord_seen = &sBkbtChordSeen;

/* Called before anything else in core1_init, so that the token is gone before the game runs. */
void bkbt_init(void) {
    volatile u32 *handoff = BKBT_HANDOFF;
    u32 token;
    u32 payload;
    u32 *header;

    /* The block is above 4 MiB: without the Expansion Pak there is nothing there to read or to zero.
     * osMemSize is RDRAM 0x318, written by the IPL3 (lib/ultralib/src/os/parameters.s:13). */
    if (osMemSize < 0x800000) {
        return;
    }

    token = handoff[BKBT_HANDOFF_TOKEN];
    payload = handoff[BKBT_HANDOFF_PAYLOAD];
    handoff[BKBT_HANDOFF_TOKEN] = 0;
    handoff[BKBT_HANDOFF_PAYLOAD] = 0;
    *BKBT_AP_ANCHOR = (u32) &bkbt_ap_anchor;

    if (token != (BKBT_TOKEN | (u32) bkbt_contract)) {
        return; /* standalone */
    }
    sBkbtLaunched = TRUE;
    sBkbtFlags = handoff[BKBT_HANDOFF_FLAGS];

    /* A launcher that loaded no payload passes 0. The header has to lie inside the Expansion Pak's half
     * of RAM as a whole, on a word boundary, and belong to this contract. */
    if ((payload & 3) || payload < BKBT_PAYLOAD_RAM_START || payload > BKBT_PAYLOAD_RAM_END - BKBT_PAYLOAD_HEADER_SIZE) {
        return;
    }
    header = (u32 *) payload;
    if (header[0] == BKBT_PAYLOAD_MAGIC && header[BKBT_PAYLOAD_CONTRACT] == (u32) bkbt_contract) {
        sBkbtReturnEntry = (void (*)(void)) header[BKBT_PAYLOAD_ENTRY];
    }
}

/* TRUE when the Tooie launcher started this game, that is, when this ROM runs inside the dual-boot image.
 * The save code picks its device by it (src/core1/eeprom.c): the image has SRAM and no EEPROM. Good from
 * bkbt_init on, which runs before anything else in the game. */
bool bkbt_launched(void) {
    return sBkbtLaunched;
}

/* Where the chord may send the player back. The front end: the opening logos, the concert and the file
 * select (game mode 3, like a level), and the attract demos the title plays over (game mode 7). Not the
 * other modes that run from recorded input, 8, 9 and 10: Bottles' bonus, the last entry of the character
 * parade and the Stop 'n' Swop picture start inside a played file (src/core2/code_C4320.c:93-127). The
 * limit is this function's alone: the return works from anywhere, which is what the flag is for. */
static bool bkbt_chordAllowed(void) {
    enum map_e map;

    if (sBkbtFlags & BKBT_FLAG_CHORD_ANYWHERE) {
        return TRUE;
    }
    map = gsworld_getMap();
    return map == MAP_1E_CS_START_NINTENDO
        || map == MAP_1F_CS_START_RAREWARE
        || map == MAP_91_FILE_SELECT
        || getGameMode() == GAME_MODE_7_ATTRACT_DEMO;
}

/* The way back to Tooie. Whatever has to be undone before this game stops goes here, ahead of the call,
 * which never returns. Today that is the anchor: from here until Tooie's boot publishes its own, the word
 * would point into this game's RAM while no game is running, and a client writing through it then would
 * write into memory the return launcher and Tooie are about to reuse. Tooie's launcher withdraws its
 * anchor the same way before it starts this game (hack/bt/n64/asm/dualboot.asm). */
static void bkbt_return(void) {
    *BKBT_AP_ANCHOR = 0;
    sBkbtReturnEntry();
}

/* Called once per mainLoop iteration, after the game has taken its pad state and before the game update.
 * That is not once per drawn frame: two iterations run on the frame of a map change. */
void bkbt_frame(void) {
    volatile u32 *handoff = BKBT_HANDOFF;
    u32 countdown;
    u16 button;
    bool down;

    /* Sync the moves with the AP client every frame */
    bkbt_apApplyItems();

    /* The chord: L + R + Start on pad 0, as the controller last answered (bkbt_padButton in joy.c, not
     * the game's pad state, which a demo overwrites). Start arrives as START_BUTTON or as the
     * controller's reset bit, depending on what delivers the pad (include/bkbt.h); either counts.
     *
     * The chord has to be seen not down once before it counts: a player still holding it from Tooie must
     * not bounce straight back. Only a read of the pad can show that. Until there has been one the chord
     * is neither down nor released, however many iterations go by.
     *
     * This part needs nothing from the handoff block and runs in a ROM started by itself too, where it
     * leads nowhere: the counter is what tells a tool that the game saw the chord it was given. */
    down = FALSE;
    if (bkbt_padButton(&button)) {
        if ((button & BKBT_CHORD_HELD) == BKBT_CHORD_HELD && (button & BKBT_CHORD_START) != 0) {
            down = TRUE;
            sBkbtChordSeen++;
        } else {
            sBkbtChordArmed = TRUE;
        }
    }

    /* Without an entry the block is not looked at: there may be no RAM where it is. */
    if (sBkbtReturnEntry == NULL) {
        return;
    }

    /* Tests: the Tooie hack leaves a number of iterations after which the game goes back by itself.
     * The count is stored before the call, which does not come back: the word has to read 0, stay, once
     * it has run out, whatever looks at it next. */
    countdown = handoff[BKBT_HANDOFF_COUNTDOWN];
    if (countdown != 0) {
        countdown--;
        handoff[BKBT_HANDOFF_COUNTDOWN] = countdown;
        if (countdown == 0) {
            bkbt_return();
        }
    }

    /* Outside the modes that play recorded input, joy_update blanks pad 0's buttons for the game while L
     * and R are held (src/core1/joy.c:254-280), so the chord does not also pause or start a file. */
    if (down && sBkbtChordArmed && bkbt_chordAllowed()) {
        bkbt_return();
    }
}
#endif

void core1_init(void) {
#if BKBT
    bkbt_init();
#endif
#if VERSION == VERSION_PAL
     osTvType = OS_TV_TYPE_PAL;
#endif
    ucode_load();
    setBootMap(getDefaultBootMap());
    rarezip_init(); //initialize decompressor's huft table
    viMgr_init();
    overlayManager_loadCore2();
    sDebugVar_8027BEF0 = sMainThreadStack[0];
    heap_init();
    core1_15B30_init();
    core1_1D590_func_8025AFB0();
    allocUnusedBlock();
    assetCache_init();
    joy_thread_init();
    baMotor_init();
    audioManager_init();
    graphicsCache_init();
    ml_init();
    gctransition_reset();
    D_8027A130 = 0;
    gGlobalTimer = 0;
    func_8023DA9C(3);
}

void globalTimer_incTimer(void) {
    gGlobalTimer++;
}

void globalTimer_decTimer(void) {
    gGlobalTimer--;
}

void mainLoop(void) {
    s32 x, y;
    s32 r, g, b, a;
    u16 tmp;
    u16 rgba;
    s32 offset;

    if ((globalTimer_getTime() & 0x7F) == 0x11) {
        sns_write_payload_over_heap();
    }

    func_8023DA74();

    if ((D_8027A130 != 3) || (getGameMode() != GAME_MODE_4_PAUSED)) {
        globalTimer_incTimer();
    }
    
    if (!sDisableInput) {
        joy_update();
    }

    sDisableInput = FALSE;

#if BKBT
    bkbt_frame();
#endif
    baMotor_80250C08();

#if ANTI_TAMPER
    if (!mapSpecificFlags_validateCRC1()) {
        eeprom_writeBlocks(0, 0, (void *) PHYS_TO_K0(0x00397AD0), EEPROM_MAXBLOCKS);
    }
#endif

    /* D_8027A130 seems to be always 3 during the game, it is only ever changed when calling
     * func_8023DA9C, which only happens in core1_init and after this switch statement, when the
     * following if statement is true (which is never be the case, see below).
     */
    switch (D_8027A130) {
        case 4:
            func_802E35D8();
            break;

        case 3:
            func_80255524();
            func_80255ACC();
            spawnQueue_func_802C3A18();
            if (game_update()) {
                game_draw(FALSE);
            }
            spawnQueue_flush();
            break;
    }

    /* This if block is never executed, because D_80275610 seems to be 0 forever.
     * D_80275610 is only changed in func_8023DFF0, which is only called in a few places, that
     * never execute:
     * - func_8023DBAC and func_8023DBDC: Never gets called
     * - func_802E35D8: Only would be called in the above switch block if D_8027A130 was 4 (never happens, see above)
     * - game_update: When sGameState.transition was 2 or 3, which is never the case
     */
    if (D_80275610) {
        func_8023DA9C(D_80275610 - 1);
        D_80275610 = 0;
    }

    if (!volatileflag_func_8032056C() || !levelSpecificFlags_validateCRC1() || !volatileflag_stub1()) {
        //render weird CRC failure image
        for (y = 30; y < gFramebufferHeight - 30; y++) {
            for (x = 20; x < 235; x++) {
                tmp = ((globalTimer_getTime() << 3) + x * x + y * y);

                r = _SHIFTL(x >> 3, 11, 5);
                g = _SHIFTL(y >> 3, 6, 5);
                b = _SHIFTL(tmp >> 3, 1, 5);
                a = 1;

                rgba = b | r | g | a;
                offset = ((gFramebufferWidth - 0xFF) / 2) + x + (y * gFramebufferWidth);

                gFramebuffers[0][offset] = (s32) rgba;
                gFramebuffers[1][offset] = (s32) rgba;
            }
        }
    }
}

void mainThread_entry(void *arg) { 
    core1_init();
    sns_write_payload_over_heap();

    while (1) {
        mainLoop();
    }
}

// Calling with 3 causes the game to soft reboot, every other value causes a black screen and hang
void func_8023DFF0(s32 arg0) {
    D_80275610 = arg0 + 1;
}

s32 func_8023E000(void) {
    return D_8027A130;
}

void setBootMap(enum map_e map_id) {
    gBootMap = map_id;
}

void mainThread_create(void) {
    osCreateThread(&sMainThread, MAINTHREAD_ID, mainThread_entry, NULL, STACK_START(sMainThreadStack), MAINTHREAD_PRI);
}

OSThread *mainThread_get(void) {
    return &sMainThread;
}

void disableInput_set(void) {
    sDisableInput = TRUE;
}
