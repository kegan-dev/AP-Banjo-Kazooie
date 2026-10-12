#include <ultra64.h>
#include <PRinternal/macros.h>
#include "core1/core1.h"
#include "functions.h"
#include "variables.h"
#include "core1/joy.h"

#include "version.h"
#include "checksums.h"
#if BKBT
#include "bkbt.h"
#endif

s32 D_80275D30 = VER_SELECT(0xC3A68832, 0xED7BCDB7, 0, 0); // CCW_DATA_CRC2
s32 D_80275D34 = VER_SELECT(0xDDC3A724, 0xF82DC7AC, 0, 0); // FIGHT_DATA_CRC2

/* Number of held frames for certain L+R button combos */
#define NUM_L_R_COMBOS (5)
static s32 sHeldFramesCombos[MAXCONTROLLERS][NUM_L_R_COMBOS];

static u8 sOsContBitPattern;

static s32 sHeldFrames[MAXCONTROLLERS][JOY_BUTTON_COUNT];
static s32 sHeldFramesPrimary[JOY_BUTTON_COUNT];

static struct CachedInputs sCachedInputs[MAXCONTROLLERS];

static OSMesg sSiEventQueueBuf;
static OSMesg sSiReplyQueueBuf;
static OSContPad sInputs[MAXCONTROLLERS];
static OSContPad sInputsPrimary;
static OSMesgQueue sSiEventQueue;
static OSMesgQueue sSiReplyQueue;

/* Timer for consecutive unchanged input, in seconds */
static f32 sInputUnchangedTimer[MAXCONTROLLERS];

static OSContStatus sOsContStatus;
static u8 pad_D_80281320[0x8];
static volatile s32 sJoyBusy;
static OSThread sJoyThread;
STACK(sJoyThreadStack, 0x200);
static f32 sSecsPostTransition;

static OSMesgQueue D_802816E8;
static OSMesg D_80281700[5];
static s32 D_80275D38 = 0;

#if BKBT
/* Pad 0 as the controller last answered: what the chord that returns to Tooie is read from
 * (include/bkbt.h, BkbtPad; bkbt_frame in code_0.c). joy_readData below is the only writer.
 *
 * The game's own copies cannot serve. In the modes that play recorded input demo_readInput stores the
 * demo's input into sInputs[0] (joy_update, below), and it stays there until the next read of the pad
 * comes back. An iteration that starts before that copies the demo's word into sInputsPrimary. A chord
 * held all along then read as not down for one iteration, which armed it, and the game went back to
 * Tooie by itself: seen 2026-10-12 on mupen64plus with a controller plugged in, 75 frames into the first
 * attract demo.
 *
 * Uninitialised on purpose: core1_main clears core1's bss at every boot, so samples is 0 until the pad
 * has been read, which is how bkbt_frame knows that button means nothing yet.
 *
 * volatile because two threads use it, and the order of the accesses is the whole protection. The pad
 * thread (priority 40, include/core1/joy.h:5) can interrupt the main thread (20, include/core1/main.h:8)
 * anywhere outside joy_update, which raises the main thread above it for its own copy; the main thread
 * never interrupts the pad thread. So bkbt_padButton looks at samples first and at button second: when
 * samples was not 0, button has been stored at least once, and a read that comes in between only makes
 * it newer. The other order could pair the cleared button with the count of the first read, a chord
 * that is down seen as released. */
static volatile BkbtPad sBkbtPad;

/* For the tools that test a ROM (BKBT.md, "Exports for tools"): the static above has no symbol, so the
 * map cannot say where it is. This pointer can be found in the map and read from RAM. Initialised data,
 * never bss, and below the static it points to. Without a controller the game never writes the record
 * (joy_readData), so what a tool stores there stays. */
volatile BkbtPad *bkbt_ptr_pad = &sBkbtPad;
#endif

/**
 * Clamps and normalises joystick input axis
 * 
 * `min` is effectively the deadzone
 */
f32 controller_clampAndNormaliseJoyAxis(s32 input, s32 min, s32 max)
{
    f32 unit = 1 / 80.f;

#if ANTI_TAMPER
    if (gChecksumsCore2.text_checksum2 != D_803727F4 || gChecksumsCore2.data_checksum2 != D_80276574)
        // Reduce stick range by half
        unit = 1 / 160.f;
#endif

    if (input > 0)
    {
        input = input > max ? max : input < min ? min : input;
        input = ((input - min) * 80) / (max - min);
    }
    else if (input < 0)
    {
        input = input < -max ? -max : input > -min ? -min : input;
        input = ((input + min) * 80) / (max - min);
    }

    return unit *= input;
}

void controller_copyFaceButtons(s32 controllerIdx, s32 dst[6])
{
    dst[FACE_BUTTON(BUTTON_A)]       = sHeldFrames[controllerIdx][JOY_BUTTON_A];
    dst[FACE_BUTTON(BUTTON_B)]       = sHeldFrames[controllerIdx][JOY_BUTTON_B];
    dst[FACE_BUTTON(BUTTON_C_LEFT)]  = sHeldFrames[controllerIdx][JOY_BUTTON_C_LEFT];
    dst[FACE_BUTTON(BUTTON_C_DOWN)]  = sHeldFrames[controllerIdx][JOY_BUTTON_C_DOWN];
    dst[FACE_BUTTON(BUTTON_C_UP)]    = sHeldFrames[controllerIdx][JOY_BUTTON_C_UP];
    dst[FACE_BUTTON(BUTTON_C_RIGHT)] = sHeldFrames[controllerIdx][JOY_BUTTON_C_RIGHT];
}

void controller_copyFaceButtonsPrimary(s32 controllerIdx, s32 dst[6])
{
    dst[FACE_BUTTON(BUTTON_A)]       = sHeldFramesPrimary[JOY_BUTTON_A];
    dst[FACE_BUTTON(BUTTON_B)]       = sHeldFramesPrimary[JOY_BUTTON_B];
    dst[FACE_BUTTON(BUTTON_C_LEFT)]  = sHeldFramesPrimary[JOY_BUTTON_C_LEFT];
    dst[FACE_BUTTON(BUTTON_C_DOWN)]  = sHeldFramesPrimary[JOY_BUTTON_C_DOWN];
    dst[FACE_BUTTON(BUTTON_C_UP)]    = sHeldFramesPrimary[JOY_BUTTON_C_UP];
    dst[FACE_BUTTON(BUTTON_C_RIGHT)] = sHeldFramesPrimary[JOY_BUTTON_C_RIGHT];
}

/**
 * Returns the number of frames a certain combo has been held for
 */
s32 controller_getHeldFramesForCombo(s32 controllerIdx, s32 comboIdx)
{
    return sHeldFramesCombos[controllerIdx][comboIdx];
}

s32 controller_copySideButtons(s32 controllerIdx, s32 dst[3])
{
    dst[SIDE_BUTTON(BUTTON_Z)] = sHeldFrames[controllerIdx][JOY_BUTTON_Z];
    dst[SIDE_BUTTON(BUTTON_L)] = sHeldFrames[controllerIdx][JOY_BUTTON_L];
    dst[SIDE_BUTTON(BUTTON_R)] = sHeldFrames[controllerIdx][JOY_BUTTON_R];
}

s32 controller_copySideButtonsPrimary(s32 controllerIdx, s32 dst[3])
{
    dst[SIDE_BUTTON(BUTTON_Z)] = sHeldFramesPrimary[JOY_BUTTON_Z];
    dst[SIDE_BUTTON(BUTTON_L)] = sHeldFramesPrimary[JOY_BUTTON_L];
    dst[SIDE_BUTTON(BUTTON_R)] = sHeldFramesPrimary[JOY_BUTTON_R];
}

/**
 * Returns number of seconds of consecutive unchanged input
 */
f32 controller_getInputUnchangedTimer(s32 controllerIdx)
{
    return sInputUnchangedTimer[controllerIdx];
}

s32 controller_getStartButton(s32 controllerIdx)
{
    return sHeldFrames[controllerIdx][JOY_BUTTON_START];
}

s32 controller_getStartButtonSafe(s32 controllerIdx)
{
    if (globalTimer_getTime() < 2)
        return 0;

    return sHeldFrames[controllerIdx][JOY_BUTTON_START];
}

void controller_copyDpadButtons(s32 controllerIdx, s32 dst[4])
{
    dst[DPAD_BUTTON(BUTTON_D_UP)]    = sHeldFrames[controllerIdx][JOY_BUTTON_D_UP];
    dst[DPAD_BUTTON(BUTTON_D_DOWN)]  = sHeldFrames[controllerIdx][JOY_BUTTON_D_DOWN];
    dst[DPAD_BUTTON(BUTTON_D_LEFT)]  = sHeldFrames[controllerIdx][JOY_BUTTON_D_LEFT];
    dst[DPAD_BUTTON(BUTTON_D_RIGHT)] = sHeldFrames[controllerIdx][JOY_BUTTON_D_RIGHT];
}

void controller_copyJoystick(s32 controllerIdx, f32 dst[2])
{
    if (func_802E4A08())
    {
        dst[0] = sCachedInputs[controllerIdx].currJoystick.x;
        dst[1] = sCachedInputs[controllerIdx].currJoystick.y;
    }
    else
    {
        dst[0] = controller_clampAndNormaliseJoyAxis(sInputs[controllerIdx].stick_x, 7, 59);
        dst[1] = controller_clampAndNormaliseJoyAxis(sInputs[controllerIdx].stick_y, 7, 61);
    }
}


#define INCREMENT_OR_CLEAR_IF(dst, test) dst = (test) ? ((dst) + 1) : (0)

void joy_update(void)
{
    s32 j;
    s32 i;

    /*demo*/
    s32  curr_demo_lag;
    u16  maybe_start_btn;
    bool demo_is_over;

    /*btn tmps*/
    u32 prev_buttons;
    u32 curr_buttons;

    if (func_8023E000() == 3)
        func_802E4384();

    osSetThreadPri(0, 0x29);

    sInputsPrimary.stick_x = sInputs[0].stick_x;
    sInputsPrimary.stick_y = sInputs[0].stick_y;
    sInputsPrimary.button  = sInputs[0].button;

    if (getGameMode() == GAME_MODE_6_FILE_PLAYBACK
        || getGameMode() == GAME_MODE_7_ATTRACT_DEMO
        || getGameMode() == GAME_MODE_8_BOTTLES_BONUS
        || getGameMode() == GAME_MODE_A_SNS_PICTURE
        || getGameMode() == GAME_MODE_9_BANJO_AND_KAZOOIE
    )
    {
        maybe_start_btn = START_BUTTON;

        if (gctransition_done())
            sSecsPostTransition += time_getDelta();

        if (sSecsPostTransition < 1.0 || getGameMode() == GAME_MODE_9_BANJO_AND_KAZOOIE)
            maybe_start_btn = 0;

        demo_is_over = demo_readInput(&sInputs, &curr_demo_lag) == 0;

        if ((sInputsPrimary.button & maybe_start_btn) || demo_is_over)
        {
            if (sInputsPrimary.button & maybe_start_btn)
                // Finished demo via start button
                volatileFlag_set(VOLATILE_FLAG_64_DEMO_FINISHED_VIA_START_BTN, TRUE);
            else
                // Finished demo via demo done (no more inputs left in demo)
                volatileFlag_set(VOLATILE_FLAG_63_DEMO_FINISHED_VIA_DEMO_DONE, TRUE);
        }

        time_setDeltaReal_frames(curr_demo_lag);
    }

    curr_demo_lag = time_getDeltaReal_frames();
    randf();

    for (i = 0; i < MAXCONTROLLERS; i++)
    {
        if ((sInputs[i].button & L_TRIG) && (sInputs[i].button & R_TRIG))
        {
            // Handle L+R+_ button combos
            INCREMENT_OR_CLEAR_IF(sHeldFramesCombos[i][0], sInputs[i].button     & D_CBUTTONS);
            INCREMENT_OR_CLEAR_IF(sHeldFramesCombos[i][1], sInputs[i].button     & Z_TRIG);
            INCREMENT_OR_CLEAR_IF(sHeldFramesCombos[i][2], sInputs[i].button     & A_BUTTON);
            INCREMENT_OR_CLEAR_IF(sHeldFramesCombos[i][3], sInputs[i].button     & B_BUTTON);
            INCREMENT_OR_CLEAR_IF(sHeldFramesCombos[i][4], sInputsPrimary.button & B_BUTTON);

            // Clear all other inputs when L+R are pressed

            for (j = 0; j < JOY_BUTTON_COUNT; j++)
                sHeldFrames[i][j] = 0;

            for (j = 0; j < JOY_BUTTON_COUNT && i == 0; j++)
                sHeldFramesPrimary[j] = 0;

            sCachedInputs[i].currButtons = 0;
            sCachedInputs[i].prevButtons = 0;
            sCachedInputs[i].newlyPressedButtons = 0;
            sCachedInputs[i].newlyReleasedButtons = 0;
            sCachedInputs[i].currJoystick.x = 0.0f;
            sCachedInputs[i].currJoystick.y = 0.0f;
            sCachedInputs[i].prevJoystick.x = 0.0f;
            sCachedInputs[i].prevJoystick.y = 0.0f;

            continue;
        }

        for (j = 0; j < NUM_L_R_COMBOS; j++)
            sHeldFramesCombos[i][j] = 0;

        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_A],       sInputs[i].button & A_BUTTON);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_B],       sInputs[i].button & B_BUTTON);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_C_LEFT],  sInputs[i].button & L_CBUTTONS);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_C_DOWN],  sInputs[i].button & D_CBUTTONS);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_C_UP],    sInputs[i].button & U_CBUTTONS);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_C_RIGHT], sInputs[i].button & R_CBUTTONS);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_Z],       sInputs[i].button & Z_TRIG);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_L],       sInputs[i].button & L_TRIG);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_R],       sInputs[i].button & R_TRIG);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_D_UP],    sInputs[i].button & U_JPAD);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_D_DOWN],  sInputs[i].button & D_JPAD);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_D_LEFT],  sInputs[i].button & L_JPAD);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_D_RIGHT], sInputs[i].button & R_JPAD);
        INCREMENT_OR_CLEAR_IF(sHeldFrames[i][JOY_BUTTON_START],   sInputs[i].button & START_BUTTON);

        if (i == 0)
        {
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_A],       sInputsPrimary.button & A_BUTTON);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_B],       sInputsPrimary.button & B_BUTTON);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_C_LEFT],  sInputsPrimary.button & L_CBUTTONS);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_C_DOWN],  sInputsPrimary.button & D_CBUTTONS);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_C_UP],    sInputsPrimary.button & U_CBUTTONS);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_C_RIGHT], sInputsPrimary.button & R_CBUTTONS);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_Z],       sInputsPrimary.button & Z_TRIG);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_L],       sInputsPrimary.button & L_TRIG);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_R],       sInputsPrimary.button & R_TRIG);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_D_UP],    sInputsPrimary.button & U_JPAD);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_D_DOWN],  sInputsPrimary.button & D_JPAD);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_D_LEFT],  sInputsPrimary.button & L_JPAD);
            INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_D_RIGHT], sInputsPrimary.button & R_JPAD);
            // INCREMENT_OR_CLEAR_IF(sHeldFramesPrimary[JOY_BUTTON_START],   sInputsPrimary.button & START_BUTTON);

            // TODO resolve fake
            sHeldFramesPrimary[JOY_BUTTON_START] = sInputsPrimary.button & START_BUTTON ? sHeldFramesPrimary[JOY_BUTTON_START] + 1LL/*fake*/ : 0;
        }

        prev_buttons = ((u16)sCachedInputs[i].currButtons);
        curr_buttons = (u16)sInputs[i].button;

        sCachedInputs[i].currButtons          =  curr_buttons;
        sCachedInputs[i].prevButtons          =  prev_buttons;
        sCachedInputs[i].newlyPressedButtons  = ~prev_buttons &  curr_buttons;
        sCachedInputs[i].newlyReleasedButtons =  prev_buttons & ~curr_buttons;
        sCachedInputs[i].prevJoystick.x       = sCachedInputs[i].currJoystick.x;
        sCachedInputs[i].prevJoystick.y       = sCachedInputs[i].currJoystick.y;
        sCachedInputs[i].currJoystick.x       = controller_clampAndNormaliseJoyAxis(sInputs[i].stick_x, 7, 59);
        sCachedInputs[i].currJoystick.y       = controller_clampAndNormaliseJoyAxis(sInputs[i].stick_y, 7, 61);

        if (sCachedInputs[i].newlyPressedButtons
            || sCachedInputs[i].prevJoystick.x != sCachedInputs[i].currJoystick.x
            || sCachedInputs[i].prevJoystick.y != sCachedInputs[i].currJoystick.y
        )
        {
            sInputUnchangedTimer[i] = 0.0f;
        }
        else
        {
            sInputUnchangedTimer[i] += time_getDelta();
        }
    }

    osSetThreadPri(0, 0x14);
}

void joy_readData()
{
    controller_func_8024F35C(0);

#if BKBT
    if (!sOsContStatus.errno)
    {
        osContGetReadData(sInputs);

        /* Pad 0 answered this read: record it (sBkbtPad, above). osContGetReadData stores a pad's
         * buttons only when that pad's errno comes out 0 and leaves them alone otherwise
         * (lib/ultralib/src/io/contreaddata.c:40-48), and what it leaves alone can be a demo's input.
         * button before samples, like the reader expects them. Nothing in between can let the main
         * thread run, so sInputs[0] is still what the call above stored. */
        if (!sInputs[0].errno)
        {
            sBkbtPad.button = sInputs[0].button;
            sBkbtPad.samples++;
        }
    }
#else
    if (!sOsContStatus.errno)
        osContGetReadData(sInputs);
#endif
}

#if BKBT
/**
 * Pad 0's buttons as the controller last answered, whatever a demo has put into the game's own pad state.
 * Returns FALSE, and leaves `button` alone, while the pad has not been read since boot.
 */
bool bkbt_padButton(u16 *button)
{
    /* samples first, button second: see sBkbtPad */
    if (sBkbtPad.samples == 0)
        return FALSE;

    *button = sBkbtPad.button;
    return TRUE;
}
#endif

void joy_main(void *null)
{
    while (1)
    {
        osRecvMesg(&sSiEventQueue, 0, 1);

        if (sJoyBusy == TRUE)
            joy_readData();
        else
            osSendMesg(&sSiReplyQueue, 0, 0);
    }
}

void joy_thread_init(void)
{
    osCreateMesgQueue(&sSiEventQueue, &sSiEventQueueBuf, 1);
    osCreateMesgQueue(&sSiReplyQueue, &sSiReplyQueueBuf, 1);
    osCreateThread(&sJoyThread, CONTROLLER_THREAD_ID, joy_main, NULL, STACK_START(sJoyThreadStack), CONTROLLER_THREAD_PRI);
    osSetEventMesg(OS_EVENT_SI, &sSiEventQueue, &sSiEventQueueBuf);
    osContInit(&sSiEventQueue, &sOsContBitPattern, &sOsContStatus);
    osContSetCh(1);
    joy_reset();
    thread5_enableControllerTimer();
    osStartThread(&sJoyThread);
}

bool joy_contHasErr(void)
{
    return BOOL(sOsContStatus.errno);
}

void joy_spawnNoControllerOverlay(void)
{
    if (joy_contHasErr())
        chOverlayNoController_spawn(0, 0);
}

void joy_noControllerOverlay_8024F180(void)
{
    if (joy_contHasErr())
        chOverlayNoController_func_802DD040(0, 0);
}

void joy_getStartReadData(void)
{
    if (sJoyBusy == 0)
    {
        controller_func_8024F35C(1);
        osContStartReadData(&sSiEventQueue);
    }
}

void joy_waitForEventAndUpdate(void)
{
    osRecvMesg(&sSiEventQueue, NULL, 1);
    joy_update();
}

/**
 * Clear data for all controllers
 */
void joy_reset(void)
{
    s32 i, j;

    for(i = 0; i < MAXCONTROLLERS; i++)
    {
        sCachedInputs[i].currButtons = 0;
        sCachedInputs[i].prevButtons = 0;
        sCachedInputs[i].newlyPressedButtons = 0;
        sCachedInputs[i].newlyReleasedButtons = 0;
        sCachedInputs[i].currJoystick.x = 0.0f;
        sCachedInputs[i].currJoystick.y = 0.0f;
        sCachedInputs[i].prevJoystick.x = 0.0f;
        sCachedInputs[i].prevJoystick.y = 0.0f;

        for(j = 0; j < NUM_L_R_COMBOS; j++)
            sHeldFramesCombos[i][j] = 0;

        for(j = 0; j < JOY_BUTTON_COUNT; j++)
            sHeldFrames[i][j] = 0;

        sInputUnchangedTimer[i] = 0.0f;
    }
}

void joy_copyInputs(s32 controllerIdx, struct CachedInputs *dst)
{
    bk_memcpy(dst, &sCachedInputs[controllerIdx], sizeof(struct CachedInputs));
}

void joy_setHeldFramesForZ(s32 controllerIdx, s32 heldFrames)
{
    sHeldFrames[controllerIdx][JOY_BUTTON_Z] = heldFrames;
}

OSMesgQueue *si_getReplyQueue(void)
{
    return &sSiReplyQueue;
}

OSMesgQueue *si_getEventQueue(void)
{
    return &sSiEventQueue;
}

void controller_func_8024F35C(s32 arg0)
{
    if (!arg0)
        func_8024F4AC();
    else
        func_8024F450();

    if (arg0 || D_802816E8.validCount == 1)
        sJoyBusy = arg0; 
}

bool joy_getBusy(void)
{
    return sJoyBusy;
}

int joy_8024F3C4(int controllerIdx)
{
    return sInputs[controllerIdx].button
         + sInputs[controllerIdx].stick_x
         + sInputs[controllerIdx].stick_y;
}

OSContPad *joy_getInputsPrimary(void)
{
    return &sInputsPrimary;
}

/* initilizes D_802816E8 message queue */
void func_8024F400(void)
{
    D_80275D38 = TRUE;
    osCreateMesgQueue(&D_802816E8, D_80281700, ARRLEN(D_80281700));
    osSendMesg(&D_802816E8, 0, OS_MESG_NOBLOCK);
}

void func_8024F450(void)
{
    if (!D_80275D38)
        func_8024F400();

    osRecvMesg(&D_802816E8, NULL, OS_MESG_BLOCK);
    osSetEventMesg(OS_EVENT_SI, &sSiEventQueue, &sSiEventQueueBuf);
}

void func_8024F4AC(void)
{
    osSendMesg(&D_802816E8, NULL, OS_MESG_NOBLOCK);
}
