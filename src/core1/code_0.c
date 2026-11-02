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
static bool sBkbtDoorArrival = FALSE;         /* the hub door brought this boot, and its game has not started yet */
static bool sBkbtDoorSession = FALSE;         /* the hub door brought this boot: the Lair's exit leads to Tooie */
static bool sBkbtDoorLeaving = FALSE;         /* the Lair's exit was taken: save and return on the next iteration */

/* The Archipelago settings (include/bkbt.h): the client writes them, the game only reads them. All 0, so
 * at every boot they read as not written yet; the anchor points here from the first instruction. */
BkbtApSettings bkbt_ap_settings = {
    0,
    0,
    0,
    {0, 0}
};

u32 bkbt_file_seed = 0;
u32 bkbt_playing_seed = 0;
bool bkbt_file_chosen = FALSE;

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
    FALSE,
    {0, 0}
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

/* The Archipelago locations (include/bkbt.h): written by the game (the molehills, src/core2/ch/mole.c and
 * src/SM/ch/smbottles.c; the honeycombs, the jiggies, the Mumbo tokens and the note doors' thresholds,
 * bkbt_apHoneycombs, bkbt_apJiggies, bkbt_apTokens and bkbt_apNoteDoors below), read by the client, all
 * 0 at every boot. */
BkbtApLocations bkbt_ap_locations = { { 0 } };

/* mainLoop iterations on which bkbt_frame copied the items from the client. For the
 * tools that test a ROM: a test that expects items not to be copied proves nothing unless the game is
 * shown to have applied items. Not initialised, unlike the statics above: a tool reaches it through
 * the pointer below and checks that against this file's .bss (BKBT.md, "Exports for tools"). core1_main
 * clears it at every boot. */
static u32 sBkbtApItemPasses;
u32 *bkbt_ptr_ap_item_passes = &sBkbtApItemPasses;

/* The empty honeycombs the game has given while the chosen game is in play: the number its own counts
 * stand for, ITEM_13_EMPTY_HONEYCOMB (pieces towards the next unit of health, total mod 6) and
 * ITEM_15_HEALTH_TOTAL. Not saved: the client sends its total again after every boot, and the counts
 * are rebuilt from it at every load. Exported like sBkbtApItemPasses, because a tool cannot read it
 * from the counts: they read 0 pieces and a health total of 8 both for 18 and for 24. */
static s32 sBkbtApHoneycombsGiven;
s32 *bkbt_ptr_ap_honeycombs_given = &sBkbtApHoneycombsGiven;

/* The jiggies received that the game has given (include/bkbt.h). Saved with the game, unlike the counter
 * above, because what it went into is saved too. A global, so that the save code sees it and the map
 * carries it for the tools. */
s32 bkbt_jiggies_given = 0;

/* The Mumbo tokens received that the game has given (include/bkbt.h), saved with the game for the same
 * reason. */
s32 bkbt_tokens_given = 0;

/* The Jinjos picked up in the chosen game, a mask per level (include/bkbt.h), saved with the game.
 * Initialised data, a global for the save code and the map. */
u8 bkbt_jinjos_checked[BKBT_AP_JINJO_LEVELS] = {0};

/* The notes each note door costs (include/bkbt.h): retail's note_door_cost_data, which is in the Lair's
 * overlay and so in RAM only while Banjo is in the Lair. Initialised data, a global for the map. */
s16 bkbt_note_door_costs[BKBT_AP_NOTE_DOORS] = {50, 180, 260, 350, 450, 640, 765, 810, 828, 846, 864, 882};

/* core2 functions used below that no header declares. */
bool honeycombscore_get(enum honeycomb_e indx);                /* src/core2/honeycombscore.c */
s32 honeycombscore_get_total(void);                            /* src/core2/honeycombscore.c */
void item_inc(enum item_e item);                               /* src/core2/code_BEF20.c: +1, with the HUD */
s32 item_getCount(enum item_e item);                           /* src/core2/code_BEF20.c */
void item_adjustByDiffWithoutHud(enum item_e item, s32 diff);  /* src/core2/code_BEF20.c */
void func_8034789C(void);         /* src/core2/code_BEF20.c: rebuilds the honeycomb pieces and the health total */
void gcpausemenu_80314AC8(int arg0); /* src/core2/gc/pauseMenu.c: 0 adds a block on the pause menu, 1 lifts one */
bool func_8028F070(void);         /* src/core2/code_7060.c: Banjo is in the map and placed (player_draw draws him) */
bool player_isDead(void);         /* src/core2/code_7060.c */
u32 jiggyscore_isCollected(enum jiggy_e jiggy_id);             /* src/core2/jiggyscore.c */
bool mumboscore_get(enum mumbotoken_e indx);                   /* src/core2/mumboscore.c */
s32 item_adjustByDiff(enum item_e item, s32 diff, s32 no_hud); /* src/core2/code_BEF20.c */
bool func_802FADD4(enum item_e item_id); /* src/core2/code_73640.c: the item's HUD counter is showing or coming in */
s32 itemscore_noteScores_getTotal(void); /* src/core2/code_BEF20.c: the sum of the worlds' best note counts */
extern f32 itemPrintValues[];            /* src/core2/code_73640.c: the number each HUD counter shows */
enum level_e level_get(void);            /* src/core2/code_9A740.c: the level Banjo is in */
int jiggyscore_isSpawned(enum jiggy_e jiggy_id); /* src/core2/jiggyscore.c: spawned this level visit, or collected */
bool player_isStable(void);              /* src/core2/playerutils.c: Banjo stands on the ground */
int gcdialog_hasCurrentTextId(void);     /* src/core2/gc/dialog.c: a text is on the screen */
int gctransition_done(void);             /* src/core2/gc/transition.c: no map transition is running */
bool func_802E4A08(void);                /* src/core2/code_5C870.c: the game plays back recorded input (demos) */
extern u8 sItemscoreNoteScores[];        /* src/core2/code_BEF20.c: each level's note count, by enum level_e */

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

/* The number of empty honeycombs the game rebuilds its counts from (func_8034789C, src/core2/code_BEF20.c)
 * at every map load and every load of a save. Retail counts the honeycombs picked up. While the chosen
 * game is in play those are locations, and the counts follow the honeycombs received: all of them,
 * quietly, so a load never replays the HUD for what was given before. The rest of the time (boot, the
 * file select, demos, the moments between clearScoreStates and the reload of a game) it is retail's
 * count, which is what a demo was recorded with. While the seed is not committed the client is changing
 * its items, and what was given stays. */
s32 bkbt_apHoneycombsForHealth(void) {
    if (!bkbt_file_chosen) {
        return honeycombscore_get_total();
    }
    if (bkbt_apSettingsCommitted()) {
        sBkbtApHoneycombsGiven = MIN(bkbt_ap_items.honeycombs, BKBT_AP_HONEYCOMB_MAX);
    }
    return sBkbtApHoneycombsGiven;
}

/* The empty honeycombs (include/bkbt.h), on every mainLoop iteration while the chosen game is in play. */
static void bkbt_apHoneycombs(void) {
    s32 id;
    s32 received;

    if (!bkbt_file_chosen) {
        return;
    }

    /* The locations: a copy of the game's own record of the honeycombs picked up. That record stays the
     * one the honeycombs, the switches and characters that spawn them, the pause menu and the save read. */
    for (id = 1; id < BKBT_AP_HONEYCOMB_SLOTS; id++) {
        bkbt_ap_locations.honeycombs[id] = honeycombscore_get(id);
    }

    /* The items: one honeycomb per iteration, given the way a pickup gives it in retail
     * (src/core2/ba/marker.c), so the HUD shows it and trades six in for a unit of health. Only while
     * Banjo is about, as for a pickup: not in a cutscene map, where nothing would show, nor while he dies,
     * when the upgrade's refill would come too late. */
    if (!bkbt_apItemsAllowed() || !func_8028F070() || player_isDead()) {
        return;
    }
    /* Six pieces wait for the HUD to trade them in (src/core2/fx/honeycarrierscore.c), which also lifts
     * the pause block set with the sixth. Nothing changes until it has. */
    if (item_getCount(ITEM_13_EMPTY_HONEYCOMB) >= 6) {
        return;
    }
    received = MIN(bkbt_ap_items.honeycombs, BKBT_AP_HONEYCOMB_MAX);
    if (received > sBkbtApHoneycombsGiven) {
        sBkbtApHoneycombsGiven++;
        item_inc(ITEM_13_EMPTY_HONEYCOMB);
        if (item_getCount(ITEM_13_EMPTY_HONEYCOMB) >= 6) {
            gcpausemenu_80314AC8(0); /* as the pickup did: no pause menu until the HUD's upgrade is done */
        }
    } else if (received < sBkbtApHoneycombsGiven) {
        /* Fewer than given (a client's mistake, or a test): rebuild the counts from the received total,
         * quietly. The rebuild takes it as given (bkbt_apHoneycombsForHealth; the seed is committed here),
         * and an adjustment by 0 clamps the health to the lower total without a HUD. */
        func_8034789C();
        item_adjustByDiffWithoutHud(ITEM_13_EMPTY_HONEYCOMB, 0);
    }
}

/* Brings a held count the game saves and the player spends (the jiggies held, the Mumbo tokens held) in
 * line with the client's total of that item, given the number received the game has given so far, which
 * is saved beside the count (include/save.h). A total above it gives the difference in one step, with the
 * item's counter unless `quiet`. A total below it (a client's mistake, or a test) takes back quietly what
 * it can from what is held, never what was spent, and only that comes off `given`: the rest stays given,
 * so the total going back up gives nothing twice. The count is changed only through item_adjustByDiff,
 * which keeps the game's copies of it (ITEM_2B_UNKNOWN, ITEM_25_MUMBO_TOKEN_TOTAL) in step, and never
 * asked to go below 0, where ITEM_2B_UNKNOWN would go on without it. */
static void bkbt_apGiveHeld(enum item_e item, s32 received, s32 *given, bool quiet) {
    s32 held;
    s32 take;

    held = item_getCount(item);
    if (received > *given) {
        /* The counter counts up from what it shows, which is out of date when something spent from the
         * count or a lower total took some since the counter was last on the screen. */
        if (!quiet && !func_802FADD4(item)) {
            itemPrintValues[item] = held;
        }
        item_adjustByDiff(item, received - *given, quiet);
        *given = received;
    } else if (received < *given) {
        take = MIN(*given - received, held);
        if (take > 0) {
            item_adjustByDiff(item, -take, TRUE);
            *given -= take;
        }
    }
}

/* The jiggies (include/bkbt.h), on every mainLoop iteration while the chosen game is in play. */
static void bkbt_apJiggies(void) {
    s32 id;

    if (!bkbt_file_chosen) {
        return;
    }

    /* The locations: a copy of the game's own record of the jiggies picked up, which the jiggies, the
     * characters and switches that spawn them, the pause menu, the endings and the save keep reading. */
    for (id = 1; id < BKBT_AP_JIGGY_SLOTS; id++) {
        bkbt_ap_locations.jiggies[id] = jiggyscore_isCollected(id) != 0;
    }

    /* The items go into the game's held count, ITEM_26_JIGGY_TOTAL, the one the Lair's puzzles spend.
     * Only while Banjo is about, as for the honeycombs. At a puzzle, whose own counter shows the count in
     * the same place on the screen (ITEM_2B_UNKNOWN's), without the held-jiggy counter. */
    if (!bkbt_apItemsAllowed() || !func_8028F070() || player_isDead()) {
        return;
    }
    bkbt_apGiveHeld(ITEM_26_JIGGY_TOTAL, MIN(bkbt_ap_items.jiggies, BKBT_AP_JIGGY_MAX), &bkbt_jiggies_given,
                    func_802FADD4(ITEM_2B_UNKNOWN));
}

/* The Mumbo tokens (include/bkbt.h), on every mainLoop iteration while the chosen game is in play. */
static void bkbt_apTokens(void) {
    s32 id;

    if (!bkbt_file_chosen) {
        return;
    }

    /* The locations: a copy of the game's own record of the tokens picked up, which the tokens, the pause
     * menu and the save keep reading. */
    for (id = 1; id < BKBT_AP_TOKEN_SLOTS; id++) {
        bkbt_ap_locations.tokens[id] = mumboscore_get(id) != 0;
    }

    /* The items go into the game's held count, ITEM_1C_MUMBO_TOKEN, the one Mumbo charges, with its
     * counter (which is on the screen anyway near Mumbo, src/core2/ch/mumbo.c). Only while Banjo is
     * about, as for the jiggies. */
    if (!bkbt_apItemsAllowed() || !func_8028F070() || player_isDead()) {
        return;
    }
    bkbt_apGiveHeld(ITEM_1C_MUMBO_TOKEN, MIN(bkbt_ap_items.tokens, BKBT_AP_TOKEN_MAX), &bkbt_tokens_given, FALSE);
}

/* The note doors' thresholds (include/bkbt.h), on every mainLoop iteration while the chosen game is in
 * play: reached once the total of the worlds' best note counts, the total retail's doors compare with
 * their costs, reaches a door's cost. The best counts change as notes are picked up, not only when a
 * world is left (itemscore_noteScores_update, src/core2/code_BEF20.c), and the game saves them, so a
 * threshold comes back with the file. */
static void bkbt_apNoteDoors(void) {
    s32 total;
    s32 door;

    if (!bkbt_file_chosen) {
        return;
    }

    total = itemscore_noteScores_getTotal();
    for (door = 1; door < BKBT_AP_NOTE_DOOR_SLOTS; door++) {
        bkbt_ap_locations.note_doors[door] = total >= bkbt_note_door_costs[door - 1];
    }
}

/* Gruntilda beaten (include/bkbt.h), the Archipelago goal, on every mainLoop iteration while the chosen
 * game is in play: a copy of the game's own flag, which the final battle sets and the game saves, so it
 * comes back with the file too. */
static void bkbt_apGoal(void) {
    if (!bkbt_file_chosen) {
        return;
    }
    bkbt_ap_locations.gruntilda_beaten = fileProgressFlag_get(FILEPROG_FC_DEFEAT_GRUNTY) != 0;
}

/* The Jinjos (include/bkbt.h). The nine levels with Jinjos, enum level_e 1..0xA without the Lair. */
static bool bkbt_jinjoLevel(s32 level) {
    return level >= LEVEL_1_MUMBOS_MOUNTAIN && level <= LEVEL_A_MAD_MONSTER_MANSION &&
        level != LEVEL_6_LAIR;
}

bool bkbt_apJinjosActive(s32 level) {
    return bkbt_file_chosen && bkbt_jinjoLevel(level);
}

bool bkbt_apJinjoChecked(s32 level, s32 colour) {
    return bkbt_jinjoLevel(level) && colour >= 0 && colour < BKBT_AP_JINJO_COLOURS &&
        (bkbt_jinjos_checked[level - 1] & (1 << colour)) != 0;
}

void bkbt_apJinjoCheck(s32 level, s32 colour) {
    if (bkbt_jinjoLevel(level) && colour >= 0 && colour < BKBT_AP_JINJO_COLOURS) {
        bkbt_jinjos_checked[level - 1] |= 1 << colour;
    }
}

/* The Jinjos the client has sent for `level`, as a mask in the game's bit order. */
static s32 bkbt_apJinjosReceived(s32 level) {
    s32 mask = 0;
    s32 colour;

    for (colour = 0; colour < BKBT_AP_JINJO_COLOURS; colour++) {
        if (bkbt_ap_items.jinjos[BKBT_AP_JINJO_INDEX(level, colour)] != 0) {
            mask |= 1 << colour;
        }
    }
    return mask;
}

/* At a level entry (level_load, src/core2/code_9A740.c, right after it zeroes the level's mask; also on
 * the reload after a death): the mask becomes the Jinjos received, quietly, so that the heads do not
 * come up at every entry. bkbt_apItemsAllowed is not asked: level_load runs before the new map is set
 * and outside the normal game mode. While the seed is not committed the client is changing its items,
 * and the mask follows them at the next arrival (bkbt_apJinjos). */
void bkbt_apJinjosLevelEntry(s32 level) {
    if (!bkbt_apJinjosActive(level) || !bkbt_apSettingsCommitted()) {
        return;
    }
    item_adjustByDiff(ITEM_12_JINJOS, bkbt_apJinjosReceived(level) - item_getCount(ITEM_12_JINJOS), TRUE);
}

/* On every mainLoop iteration while the chosen game is in play: the locations, a copy of the checked
 * Jinjos (the game keeps no record of its own), and the level's mask, which follows the Jinjos received
 * while Banjo is in the level. A Jinjo that arrives shows the heads, as one picked up does in retail; one
 * taken back goes quietly. */
static void bkbt_apJinjos(void) {
    s32 level;
    s32 colour;
    s32 want;
    s32 have;

    if (!bkbt_file_chosen) {
        return;
    }

    for (level = 1; level <= BKBT_AP_JINJO_LEVELS; level++) {
        for (colour = 0; colour < BKBT_AP_JINJO_COLOURS; colour++) {
            bkbt_ap_locations.jinjos[BKBT_AP_JINJO_INDEX(level, colour)] = bkbt_apJinjoChecked(level, colour);
        }
    }

    level = level_get();
    if (!bkbt_jinjoLevel(level) || !bkbt_apItemsAllowed() || !func_8028F070() || player_isDead()) {
        return;
    }
    want = bkbt_apJinjosReceived(level);
    have = item_getCount(ITEM_12_JINJOS);
    if (want != have) {
        item_adjustByDiff(ITEM_12_JINJOS, want - have, (want & ~have) == 0);
    }
}

/* The level's Jinjo jiggy, on Banjo once the client has sent the level's five Jinjos. Called at the
 * end of the world update (func_80332E08, src/core2/code_ABC00.c), where retail spawns the witch
 * switches' jiggies: a jiggy queued there is not lost to a map change later in the same iteration, as one
 * queued from bkbt_frame, which runs before the world update, would be. jiggy_spawn marks the jiggy
 * spawned for the rest of the level visit (the mark is cleared at level entry), so it comes once a visit:
 * a jiggy left behind stays in its map, and comes again at the next entry or after a death. Collected,
 * it reads as spawned too. Only while Banjo stands, in control, with no text, transition, minigame or
 * parade going on, at his position and 50 up, where the fifth Jinjo would have spawned it. There it is
 * held, not thrown, and can be picked up at once (bkbt_apIsJinjoJiggy), so Banjo takes it where he is:
 * thrown, as the jiggy list throws it, it came down where he had stood, which at a level's entry is beside
 * the exit (the owner's hand test on ares v148, 2026-10-29). */
void bkbt_apJinjoJiggy(void) {
    s32 level;
    enum jiggy_e jiggy;
    f32 position[3];

    level = level_get();
    if (!bkbt_apJinjosActive(level) || !bkbt_apItemsAllowed() ||
        bkbt_apJinjosReceived(level) != BKBT_AP_JINJO_ALL) {
        return;
    }
    jiggy = 10 * level - 9; /* the level's Jinjo jiggy, as src/core2/ch/jinjo.c spawns it */
    if (jiggyscore_isSpawned(jiggy) ||
        !func_8028F070() || player_isDead() || !player_isStable() ||
        !gctransition_done() || gcdialog_hasCurrentTextId() ||
        volatileFlag_get(VOLATILE_FLAG_1) ||
        volatileFlag_get(VOLATILE_FLAG_2_FF_IN_MINIGAME) ||
        volatileFlag_get(VOLATILE_FLAG_1F_IN_CHARACTER_PARADE)) {
        return;
    }
    player_getPosition(position);
    position[1] += 50.0f;
    jiggy_spawn(jiggy, position);
}

/* The level's Jinjo jiggy while the Jinjos are Archipelago's (bkbt_apJinjosActive), when bkbt_apJinjoJiggy
 * is its only spawner: a pickup no longer fills the level's mask (src/core2/ch/jinjo.c). Asked by
 * func_803333DC (src/core2/code_ABC00.c) as a jiggy of the jiggy list spawns. */
bool bkbt_apIsJinjoJiggy(s32 jiggy) {
    s32 level;

    level = level_get();
    return bkbt_apJinjosActive(level) && jiggy == 10 * level - 9;
}

/* The notes (include/bkbt.h): every note picked up in the chosen game, one bit per note number, saved with
 * the game. Initialised data, a global for the save code and the map. */
u8 bkbt_notes_collected[BKBT_AP_NOTE_BYTES] = {0};

/* The maps with notes, by level and map id: the map (enum map_e), its level (enum level_e), the number n
 * of its first note in the level and its count of note sprites, which the game reads from the map's setup
 * in the order of their numbers. Written from the retail ROM's setups by tools/bk_notes.py of the BKBT
 * repository, which checks it against them (--hack): 100 notes a level, the huts' five of Mumbo's
 * Mountain and Bubblegloop Swamp after their placed 95. */
typedef struct {
    u8 map;
    u8 level;
    u8 first;
    u8 count;
} BkbtNoteMap;

static const BkbtNoteMap sBkbtNoteMaps[] = {
    /* BKBT notes table: begin (tools/bk_notes.py --hack reads and checks the rows between these two lines) */
    {0x02, 1, 0, 85}, {0x0C, 1, 85, 6}, {0x0E, 1, 91, 4}, {0x05, 2, 0, 8}, {0x06, 2, 8, 6}, {0x07, 2, 14, 82},
    {0x0A, 2, 96, 4}, {0x0B, 3, 0, 72}, {0x21, 3, 72, 6}, {0x22, 3, 78, 16}, {0x23, 3, 94, 6}, {0x0D, 4, 0, 83},
    {0x10, 4, 83, 6}, {0x11, 4, 89, 6}, {0x27, 5, 0, 82}, {0x48, 5, 82, 6}, {0x53, 5, 88, 12}, {0x12, 7, 0, 70},
    {0x13, 7, 70, 4}, {0x14, 7, 74, 7}, {0x15, 7, 81, 4}, {0x16, 7, 85, 8}, {0x1A, 7, 93, 7}, {0x40, 8, 0, 4},
    {0x43, 8, 4, 16}, {0x44, 8, 20, 16}, {0x45, 8, 36, 37}, {0x46, 8, 73, 16}, {0x4C, 8, 89, 4}, {0x5C, 8, 93, 4},
    {0x60, 8, 97, 3}, {0x31, 9, 0, 43}, {0x34, 9, 43, 16}, {0x35, 9, 59, 4}, {0x37, 9, 63, 8}, {0x38, 9, 71, 4},
    {0x39, 9, 75, 4}, {0x3B, 9, 79, 5}, {0x3C, 9, 84, 5}, {0x3D, 9, 89, 4}, {0x3F, 9, 93, 3}, {0x8B, 9, 96, 4},
    {0x1B, 10, 0, 47}, {0x1C, 10, 47, 10}, {0x1D, 10, 57, 4}, {0x24, 10, 61, 4}, {0x25, 10, 65, 7}, {0x26, 10, 72, 8},
    {0x29, 10, 80, 9}, {0x2D, 10, 89, 4}, {0x2F, 10, 93, 5}, {0x30, 10, 98, 2},
    /* BKBT notes table: end */
};
#define BKBT_NOTE_MAPS (s32)(sizeof(sBkbtNoteMaps) / sizeof(sBkbtNoteMaps[0]))
#define BKBT_NOTES_IN_A_MAP 100 /* room for the notes of one map: the most is 85, Mumbo's Mountain's 0x02 */

/* The notes of the map whose setup was read last (bkbt_notesSetupBegin), by number: their positions, the
 * same s16s the pickup sees (the setup's, include/prop.h), and the table's entry for the map, -1 when it has
 * no notes or its setup did not hold the table's count. sBkbtNotePos is set before it is read. */
static s32 sBkbtNoteMapIx = -1;
static s32 sBkbtNoteMap = -1;
static s32 sBkbtNotesRead = 0;
static s16 sBkbtNotePos[BKBT_NOTES_IN_A_MAP][3];

/* The chosen game's notesanity, latched when it is chosen (bkbt_apNotesLoaded): the client writes seed 0
 * before it changes anything, and the defaults say no notesanity. And the Notes received, taken only while
 * the settings are committed (the client writes the u16 a byte at a time), kept in between: a note door's
 * threshold, once published, cannot be taken back. */
static bool sBkbtNotesanity = FALSE;
static s32 sBkbtNotesReceived = 0;

/* Notes picked up that the chosen game had kept already. A note kept is hidden at the setup read and a hut
 * drops only the notes not kept, so this stays 0: for the tools, which read it as the proof that a note kept
 * is not there to pick up. A global, so that the map carries it. */
u32 bkbt_notes_picked_again = 0;

static bool bkbt_noteKept(s32 index) {
    return (bkbt_notes_collected[index >> 3] >> (index & 7)) & 1;
}

static void bkbt_noteKeep(s32 index) {
    bkbt_notes_collected[index >> 3] |= 1 << (index & 7);
}

/* The notes kept in `level`, one of the nine (bkbt_jinjoLevel). */
static s32 bkbt_notesInLevel(s32 level) {
    s32 n;
    s32 count = 0;

    for (n = 0; n < BKBT_AP_NOTES_PER_LEVEL; n++) {
        count += bkbt_noteKept(BKBT_AP_NOTE_INDEX(level, n));
    }
    return count;
}

/* While the chosen game is in play: not a demo, Bottles' Bonus, a picture or the parade, which all load
 * after clearScoreStates, nor the playback of recorded input. */
static bool bkbt_apNotesActive(void) {
    return bkbt_file_chosen && !func_802E4A08();
}

/* The levels whose first hut smashed drops five notes (src/MM/ch/hut.c, src/BGS/ch/mudhut.c). */
static bool bkbt_hutLevel(s32 level) {
    return level == LEVEL_1_MUMBOS_MOUNTAIN || level == LEVEL_4_BUBBLEGLOOP_SWAMP;
}

/* At the start of every setup read (cubeList_fromFile, src/core2/gccube.c), for the map just set. */
void bkbt_notesSetupBegin(void) {
    s32 i;

    sBkbtNoteMap = gsworld_getMap();
    sBkbtNoteMapIx = -1;
    sBkbtNotesRead = 0;
    for (i = 0; i < BKBT_NOTE_MAPS; i++) {
        if (sBkbtNoteMaps[i].map == sBkbtNoteMap) {
            sBkbtNoteMapIx = i;
            break;
        }
    }
}

/* Each note sprite the setup read meets (code7AF80_initCubeFromFile, src/core2/code_A5BC0.c), in its
 * order: the next number. TRUE for a note the chosen game has kept, which the caller hides. Only ever
 * hides: the game shows every prop of a setup it reads, and hides the notes itself in Furnace Fun. */
bool bkbt_notesSetupNote(s16 position[3]) {
    const BkbtNoteMap *entry;
    s32 i;

    i = sBkbtNotesRead++;
    if (sBkbtNoteMapIx < 0 || i >= sBkbtNoteMaps[sBkbtNoteMapIx].count) {
        return FALSE;
    }
    entry = &sBkbtNoteMaps[sBkbtNoteMapIx];
    sBkbtNotePos[i][0] = position[0];
    sBkbtNotePos[i][1] = position[1];
    sBkbtNotePos[i][2] = position[2];
    return bkbt_file_chosen && bkbt_noteKept(BKBT_AP_NOTE_INDEX(entry->level, entry->first + i));
}

/* At the end of the setup read: a map whose setup does not hold the table's count is not numbered. */
void bkbt_notesSetupEnd(void) {
    if (sBkbtNoteMapIx >= 0 && sBkbtNotesRead != sBkbtNoteMaps[sBkbtNoteMapIx].count) {
        sBkbtNoteMapIx = -1;
    }
}

/* A note kept: with notesanity the count does not rise and the level's best is set to its notes kept (the
 * pause menu's count); without, the game counts it as retail does, from where the level entry started it. */
static bool bkbt_apNoteCounted(s32 level) {
    if (sBkbtNotesanity) {
        sItemscoreNoteScores[level] = bkbt_notesInLevel(level);
        return FALSE;
    }
    return TRUE;
}

/* A placed note picked up (src/core2/ba/marker.c): found by its position among the notes of the map in
 * play, and kept. TRUE when the game is to count it as retail does: outside a chosen game, for a note the
 * table does not know, and for a note kept without notesanity. */
bool bkbt_apNotePicked(s16 position[3]) {
    const BkbtNoteMap *entry;
    s32 i;
    s32 index;

    if (!bkbt_apNotesActive() || sBkbtNoteMapIx < 0 || sBkbtNoteMap != gsworld_getMap()) {
        return TRUE;
    }
    entry = &sBkbtNoteMaps[sBkbtNoteMapIx];
    for (i = 0; i < entry->count; i++) {
        if (sBkbtNotePos[i][0] == position[0] && sBkbtNotePos[i][1] == position[1] &&
            sBkbtNotePos[i][2] == position[2]) {
            index = BKBT_AP_NOTE_INDEX(entry->level, entry->first + i);
            if (bkbt_noteKept(index)) {
                bkbt_notes_picked_again++; /* kept already, so it should have been hidden: nothing to count */
                return FALSE;
            }
            bkbt_noteKeep(index);
            return bkbt_apNoteCounted(entry->level);
        }
    }
    return TRUE;
}

/* A hut's note picked up (actor 0x51, src/core2/ba/marker.c): the five are alike, so it is the level's
 * first hut note not kept yet. TRUE as for bkbt_apNotePicked; FALSE when all five are kept already. */
bool bkbt_apHutNotePicked(void) {
    s32 level;
    s32 n;
    s32 index;

    level = level_get();
    if (!bkbt_apNotesActive() || !bkbt_hutLevel(level)) {
        return TRUE;
    }
    for (n = BKBT_AP_HUT_NOTE_FIRST; n < BKBT_AP_NOTES_PER_LEVEL; n++) {
        index = BKBT_AP_NOTE_INDEX(level, n);
        if (!bkbt_noteKept(index)) {
            bkbt_noteKeep(index);
            return bkbt_apNoteCounted(level);
        }
    }
    bkbt_notes_picked_again++; /* all five kept: the hut should not have dropped it */
    return FALSE;
}

/* How many of a hut's `count` notes drop (__bundle_spawnWithFirstActor, src/core2/code_41FB0.c): those of
 * the level not kept yet, while the chosen game is in play. */
s32 bkbt_apHutNotesLeft(s32 count) {
    s32 level;
    s32 n;
    s32 left = 0;

    level = level_get();
    if (!bkbt_file_chosen || !bkbt_hutLevel(level)) {
        return count;
    }
    for (n = BKBT_AP_HUT_NOTE_FIRST; n < BKBT_AP_NOTES_PER_LEVEL; n++) {
        left += !bkbt_noteKept(BKBT_AP_NOTE_INDEX(level, n));
    }
    return MIN(left, count);
}

/* At a level entry (level_load, src/core2/code_9A740.c, right after it zeroes the visit's count; also on the
 * reload after a death): the level's best becomes its notes kept, then the count goes there quietly (it
 * stays 0 with notesanity), in that order, so that the count's own code (item_adjustByDiff,
 * src/core2/code_BEF20.c, which runs without the HUD too) sees no new best: no text, no extra life. Only for
 * the nine levels: level_load runs for 0xC and 0xD too, past the end of sItemscoreNoteScores. */
void bkbt_apNotesLevelEntry(s32 level) {
    s32 kept;
    s32 want;

    if (!bkbt_file_chosen || !bkbt_jinjoLevel(level)) {
        return;
    }
    kept = bkbt_notesInLevel(level);
    sItemscoreNoteScores[level] = kept;
    want = sBkbtNotesanity ? 0 : kept;
    if (want != item_getCount(ITEM_C_NOTE)) {
        item_adjustByDiff(ITEM_C_NOTE, want - item_getCount(ITEM_C_NOTE), TRUE);
    }
}

/* After the chosen game is loaded (bkbt_gameFile_loadAP, src/core2/code_B5E00.c, which runs after the
 * game's own load of the record): notesanity is latched, and every level's best is its notes kept, the
 * game's saved bests notwithstanding. A mid-session reload may come while the client is changing its
 * settings; then the latch stays. */
void bkbt_apNotesLoaded(void) {
    s32 level;

    if (bkbt_apSettingsCommitted()) {
        sBkbtNotesanity = bkbt_ap_settings.notesanity != 0;
    }
    for (level = LEVEL_1_MUMBOS_MOUNTAIN; level <= LEVEL_A_MAD_MONSTER_MANSION; level++) {
        if (bkbt_jinjoLevel(level)) {
            sItemscoreNoteScores[level] = bkbt_notesInLevel(level);
        }
    }
}

/* The note total the game shows and the note doors' thresholds read (itemscore_noteScores_getTotal,
 * src/core2/code_BEF20.c): with notesanity in the chosen game the Notes received, otherwise its own sum of
 * the levels' bests, which with the notes kept is every note picked up. */
s32 bkbt_apNotesTotal(s32 total) {
    if (bkbt_file_chosen && sBkbtNotesanity) {
        return sBkbtNotesReceived;
    }
    return total;
}

bool bkbt_apNotesKept(void) {
    return bkbt_file_chosen;
}

/* On every mainLoop iteration: the Notes received while the settings are committed, and while the chosen
 * game is in play the locations, a copy of the notes kept. */
static void bkbt_apNotes(void) {
    s32 i;

    if (bkbt_apSettingsCommitted()) {
        sBkbtNotesReceived = MIN(bkbt_ap_items.notes, BKBT_AP_NOTE_MAX);
    }
    if (!bkbt_file_chosen) {
        return;
    }
    for (i = 0; i < BKBT_AP_NOTE_BYTES; i++) {
        bkbt_ap_locations.notes[i] = bkbt_notes_collected[i];
    }
}

/* The note doors received (include/bkbt.h), for the door actor (src/lair/lairspawnqueue.c), which asks
 * while Banjo is near it: the door opens on its item instead of on a note total. */
bool bkbt_apNoteDoorReceived(s32 door) {
    return bkbt_apItemsAllowed() &&
        door >= 1 && door < BKBT_AP_NOTE_DOOR_SLOTS &&
        bkbt_ap_items.note_doors[door] != 0;
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
    &bkbt_ap_items,
    &bkbt_ap_locations
};

/* mainLoop iterations on which bkbt_frame found the chord down, whether or not it went anywhere. For the
 * tools that test a ROM: a test that expects the chord to do nothing proves nothing unless the game is
 * shown to have seen the chord. Not initialised, like the two counters above: a tool reaches it through
 * the pointer below and checks that against this file's .bss (BKBT.md, "Exports for tools"). core1_main
 * clears it at every boot. */
static u32 sBkbtChordSeen;
u32 *bkbt_ptr_chord_seen = &sBkbtChordSeen;

/* Called before anything else in core1_init, so that the token is gone before the game runs. */
void bkbt_init(void) {
    volatile u32 *handoff = BKBT_HANDOFF;
    u32 token;
    u32 payload;
    u32 dest_map;
    u32 dest_exit;
    u32 *header;

    /* The block is above 4 MiB: without the Expansion Pak there is nothing there to read or to zero.
     * osMemSize is RDRAM 0x318, written by the IPL3 (lib/ultralib/src/os/parameters.s:13). */
    if (osMemSize < 0x800000) {
        return;
    }

    token = handoff[BKBT_HANDOFF_TOKEN];
    payload = handoff[BKBT_HANDOFF_PAYLOAD];
    dest_map = handoff[BKBT_HANDOFF_DEST_MAP];
    dest_exit = handoff[BKBT_HANDOFF_DEST_EXIT];
    handoff[BKBT_HANDOFF_TOKEN] = 0;
    handoff[BKBT_HANDOFF_PAYLOAD] = 0;
    handoff[BKBT_HANDOFF_DEST_MAP] = 0;
    handoff[BKBT_HANDOFF_DEST_EXIT] = 0;
    *BKBT_AP_ANCHOR = (u32) &bkbt_ap_anchor;

    if (token != (BKBT_TOKEN | (u32) bkbt_contract)) {
        return; /* standalone */
    }
    sBkbtLaunched = TRUE;
    sBkbtFlags = handoff[BKBT_HANDOFF_FLAGS];
    /* The hub door's destination is the only one this game knows (include/bkbt.h) */
    if (dest_map == (BKBT_DEST_TAG | MAP_69_GL_MM_LOBBY) && dest_exit == WARP_GL_MM_LOBBY_12_ENTRANCE) {
        sBkbtDoorArrival = TRUE;
        sBkbtDoorSession = TRUE;
    }

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
 * Good from bkbt_init on, which runs before anything else in the game. The save code no longer picks its
 * device by it (SRAM always since 2026-10-26, src/core1/eeprom.c); tools/bk_fork_info.py still wants it
 * in the map. */
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
void gameSelect_saveAndExit(void);      /* src/core2/ch/gameSelect.c */
void warp_smExitLair(s32 arg0, s32 arg1); /* src/core2/code_956B0.c */

static void bkbt_returnTo(u32 dest_map, u32 dest_exit) {
    volatile u32 *handoff = BKBT_HANDOFF;

    /* the destination, on every return (contract 3): Tooie reads it only with the magic the return
     * launcher writes */
    handoff[BKBT_HANDOFF_DEST_MAP] = dest_map != 0 ? (BKBT_DEST_TAG | dest_map) : 0;
    handoff[BKBT_HANDOFF_DEST_EXIT] = dest_map != 0 ? dest_exit : 0;
    *BKBT_AP_ANCHOR = 0;
    sBkbtReturnEntry();
}

static void bkbt_return(void) {
    bkbt_returnTo(0, 0);
}

/* The hub door (include/bkbt.h). The file select (src/core2/ch/gameSelect.c) asks bkbt_doorArrival whether to
 * choose the file itself and start it in the Lair, and says bkbt_doorArrived when it has. */
bool bkbt_doorArrival(void) {
    return sBkbtDoorArrival;
}

void bkbt_doorArrived(void) {
    sBkbtDoorArrival = FALSE;
}

/* warp_smExitLair (src/core2/code_956B0.c) asks this first: in a game the hub door started, the Lair's way out
 * to Spiral Mountain leads back to Tooie, from the next main-loop iteration on (bkbt_frame), and the warp
 * does nothing. A game started from the front end keeps its own Spiral Mountain. */
bool bkbt_doorOut(void) {
    if (!sBkbtDoorSession || sBkbtReturnEntry == NULL) {
        return FALSE;
    }
    sBkbtDoorLeaving = TRUE;
    return TRUE;
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
    bkbt_apHoneycombs();
    bkbt_apJiggies();
    bkbt_apTokens();
    bkbt_apNotes();     /* before the thresholds, which read the Notes received */
    bkbt_apNoteDoors();
    bkbt_apGoal();
    bkbt_apJinjos();

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

    /* The hub door out (bkbt_doorOut): the game saved as when it leaves a level for the file select
     * (gameSelect_saveAndExit saves in the Lair, level 6, and nowhere a save would be wrong), then back to
     * Tooie's Spiral Mountain. The save is synchronous (src/core1/eeprom.c). */
    if (sBkbtDoorLeaving) {
        sBkbtDoorLeaving = FALSE;
        gameSelect_saveAndExit();
        bkbt_returnTo(BKBT_DOOR_TOOIE_MAP, BKBT_DOOR_TOOIE_EXIT);
    }

    /* Tests: the Tooie hack leaves a number of iterations after which the game goes back by itself.
     * The count is stored before the call, which does not come back: the word has to read 0, stay, once
     * it has run out, whatever looks at it next. With BKBT_FLAG_DOOR_TEST it counts only in the Lair's
     * lobby during play, and then the game takes the lobby's way out, as if Banjo had walked out. */
    countdown = handoff[BKBT_HANDOFF_COUNTDOWN];
    if (countdown != 0 && !(sBkbtFlags & BKBT_FLAG_DOOR_TEST)) {
        countdown--;
        handoff[BKBT_HANDOFF_COUNTDOWN] = countdown;
        if (countdown == 0) {
            bkbt_return();
        }
    } else if (countdown != 0 && gsworld_getMap() == MAP_69_GL_MM_LOBBY && getGameMode() == GAME_MODE_3_NORMAL) {
        countdown--;
        handoff[BKBT_HANDOFF_COUNTDOWN] = countdown;
        if (countdown == 0) {
            warp_smExitLair(0, 0);
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
#if BKBT
    /* Through the hub door: straight into the file select, which chooses the file (bkbt_doorArrival) */
    setBootMap(bkbt_doorArrival() ? MAP_91_FILE_SELECT : getDefaultBootMap());
#else
    setBootMap(getDefaultBootMap());
#endif
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
