/* Host-only integration harness. All pictures come from the real engine's
 * software GBA framebuffer, and every menu choice uses normal key input. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL.h>
#include "global.h"
#include "core.h"
#include "task.h"
#include "malloc_vram.h"
#include "game/game.h"
#include "game/globals.h"
#include "game/sa2/save.h"
#include "game/sa2/oc_characters.h"
#include "game/sa2/oc_player.h"
#include "game/sa2/ui/character_select.h"
#include "game/sa2/special_stage/main.h"
#include "game/shared/stage/stage.h"
#include "game/shared/stage/player.h"
#include "constants/sa2/char_states.h"
#include "data/sa2/oc_sprite_data.h"
#include "platform/shared/video/gpsp_renderer.h"

extern uint16_t gameImage[];
extern bool headless;
extern void __real_VBlankIntrWait(void);
extern void __real_AgbMain(void);
extern void __real_CreateCharacterSelectionScreen(u8, bool8);
extern bool32 __real_OcPlayerDraw(Player *, PlayerSpriteInfo *);
extern bool32 __real_OcSpecialPlayerDraw(Sprite *, u16, u16);
extern void Player_InitHurt(Player *);

static unsigned frames, stageFrames, playFrames, menuFrames, movesLeft;
static unsigned drawCount, paletteChecks, rightDraws, leftDraws, slopeDraws;
static unsigned invulnerableFrames, invulnerableVisible, invulnerableHidden;
static unsigned specialDraws, specialLeft, specialRight, specialJump;
static bool stageWasStarted, menuCreated, confirmed, cancelled;
static bool statesSeen[128];
static bool stateCaptured[128];
static int previousState = -1;
static unsigned stateStreak;
static int entry, oc;
static const char *mode, *captureDir;
static char selectionFile[1024];

static void Fail(const char *message)
{
    fprintf(stderr, "FAIL: %s; mode=%s entry=%d oc=%d frame=%u stage=%u state=%d x=%d y=%d\n",
            message, mode, entry, oc, frames, stageFrames, gPlayer.charState,
            I(gPlayer.qWorldX), I(gPlayer.qWorldY));
    fprintf(stderr, "Observed native states:");
    for (unsigned i = 0; i < ARRAY_COUNT(statesSeen); i++)
        if (statesSeen[i]) fprintf(stderr, " %u", i);
    fprintf(stderr, "\n");
    exit(2);
}

static void Require(bool condition, const char *message)
{
    if (!condition) Fail(message);
}

static void SetInput(u16 pressed) { REG_KEYINPUT = KEYS_MASK ^ pressed; }

static s32 OamExpectedX(s32 x)
{
#if EXTENDED_OAM
    return x;
#else
    return x & 511;
#endif
}

static s32 OamExpectedY(s32 y)
{
#if EXTENDED_OAM
    return y;
#else
    return y & 255;
#endif
}

static void Capture(const char *label, unsigned number)
{
    char path[1024];
    SDL_Surface *capture;
    if (!captureDir) return;
    snprintf(path, sizeof(path), "%s/%s-entry%d-oc%d-%s-%04u.bmp", captureDir, mode, entry, oc, label, number);
    capture = SDL_CreateRGBSurfaceFrom(gameImage, DISPLAY_WIDTH, DISPLAY_HEIGHT, 16,
                                      DISPLAY_WIDTH * 2, 0x001f, 0x03e0, 0x7c00, 0);
    Require(capture != NULL, "create framebuffer surface");
    Require(SDL_SaveBMP(capture, path) == 0, "save framebuffer capture");
    SDL_FreeSurface(capture);
}

static void CheckOcMenuGraphics(void)
{
    unsigned preview = 0, font = 0, circles = 0;
    if (entry < OC_SELECT_FIRST) return;
    for (unsigned i = 0; i < gOamFreeIndex; i++) {
        const OamData *oam = &gOamMallocBuffer[i];
        if (oam->split.paletteNum == 15 && oam->split.size == ST_OAM_SIZE_3) {
            const u8 *tiles = OBJ_VRAM0 + oam->split.tileNum * TILE_SIZE_4BPP;
            unsigned nonzero = 0;
            Require(oam->split.tileNum + 64 <= OBJ_VRAM_TOTAL_SIZE / TILE_SIZE_4BPP,
                    "selector OC preview tiles stay inside VRAM");
            for (unsigned b = 0; b < OC_FRAME_TILE_BYTES; b++) nonzero += tiles[b] != 0;
            Require(nonzero > 50, "selector OC preview has visible pixels");
            preview++;
        }
        if (oam->split.paletteNum == 14) {
            const u8 *tiles = OBJ_VRAM0 + oam->split.tileNum * TILE_SIZE_4BPP;
            unsigned nonzero = 0;
            unsigned bytes = oam->split.size == ST_OAM_SIZE_3 ? 2048
                : oam->split.size == ST_OAM_SIZE_2 ? 512 : TILE_SIZE_4BPP;
            for (unsigned b = 0; b < bytes; b++) nonzero += tiles[b] != 0;
            Require(nonzero, "selector circle/name glyph has visible pixels");
            if (oam->split.size == ST_OAM_SIZE_0) font++;
            else circles++;
        }
    }
    Require(preview == 1, "OC selector submits exactly one visible custom preview OAM");
    Require(font >= strlen(OcCharacterName(oc)), "OC selector submits visible name glyphs");
    Require(circles, "OC selector submits visible color circles");
    Require(memcmp(&gObjPalette[15 * 16], gOcPalettes[oc], 16 * sizeof(u16)) == 0,
            "selector preview palette retains OC identity");
}

static void DiscardGraphics(void)
{
    gBackgroundsCopyQueueCursor = gBackgroundsCopyQueueIndex = 0;
    gVramGraphicsCopyCursor = gVramGraphicsCopyQueueIndex = 0;
}

static void CheckConfirmedStorage(void)
{
    FILE *file;
    u8 record[5];
    if (!selectionFile[0]) return;
    file = fopen(selectionFile, "rb");
    Require(file != NULL, "real menu confirmation creates OC selection sidecar");
    Require(fread(record, 1, sizeof(record), file) == sizeof(record) && fgetc(file) == EOF,
            "menu persistence record has exact length");
    fclose(file);
    Require(memcmp(record, "OCS1", 4) == 0 && record[4] == (u8)oc,
            "real menu saves expected OC identity");
    OcSetSelection(-1);
    OcSelectionSetStoragePath(selectionFile);
    Require(gSelectedOc == oc, "saved menu choice survives reloading selection storage");
}

void __wrap_CreateCharacterSelectionScreen(u8 initialSelection, bool8 allUnlocked)
{
    int initial = gSelectedOc >= 0 ? OC_SELECT_FIRST + gSelectedOc : initialSelection;
    __real_CreateCharacterSelectionScreen(initialSelection, allUnlocked);
    menuFrames = 0;
    movesLeft = (entry - initial + NUM_CHARACTERS + OC_CHARACTER_COUNT) % (NUM_CHARACTERS + OC_CHARACTER_COUNT);
    menuCreated = true;
}

bool32 __wrap_OcPlayerDraw(Player *player, PlayerSpriteInfo *body)
{
    unsigned before = gOamFreeIndex;
    bool32 result = __real_OcPlayerDraw(player, body);
    if (result && gOamFreeIndex != before) {
        OamData *oam = &gOamMallocBuffer[gOamFreeIndex - 1];
        Require(oam->split.paletteNum == 0, "OC player uses its own palette bank");
        Require(oam->split.bpp == ST_OAM_4BPP && oam->split.shape == ST_OAM_SQUARE
                    && oam->split.size == ST_OAM_SIZE_3, "OC OAM is a valid 64x64 4bpp object");
        Require(oam->split.tileNum + 64 <= OBJ_VRAM_TOTAL_SIZE / TILE_SIZE_4BPP, "OC tiles remain inside OBJ VRAM");
        if (oam->split.affineMode == ST_OAM_AFFINE_OFF) {
            Require(((oam->split.matrixNum & ST_OAM_HFLIP) != 0)
                        == ((player->moveState & MOVESTATE_FACING_LEFT) != 0),
                    "right-facing atlas turns toward actual movement direction");
            Require(oam->split.x == OamExpectedX(body->s.x - OC_FRAME_PIVOT_X),
                    "OC remains centered on native player collision position");
        }
        drawCount++;
        if (player->moveState & MOVESTATE_FACING_LEFT) leftDraws++;
        else rightDraws++;
        if (!(player->moveState & MOVESTATE_IN_AIR) && player->rotation != 0) slopeDraws++;
        if (player->timerInvulnerability) invulnerableVisible++;
        if (!(gStageFlags & STAGE_FLAG__100)) {
            Require(memcmp(gObjPalette, gOcPalettes[gSelectedOc], 16 * sizeof(u16)) == 0,
                    "OC RGB555 palette matches selected identity");
            paletteChecks++;
        }
    }
    return result;
}

bool32 __wrap_OcSpecialPlayerDraw(Sprite *body, u16 state, u16 input)
{
    unsigned before = gOamFreeIndex;
    bool32 result = __real_OcSpecialPlayerDraw(body, state, input);
    if (result && before != gOamFreeIndex) {
        OamData *oam = &gOamMallocBuffer[gOamFreeIndex - 1];
        Require(oam->split.paletteNum == 0 && oam->split.bpp == ST_OAM_4BPP
                    && oam->split.shape == ST_OAM_SQUARE && oam->split.size == ST_OAM_SIZE_3,
                "special stage OC OAM is valid 64x64 4bpp palette zero");
        Require(oam->split.tileNum + 64 <= OBJ_VRAM_TOTAL_SIZE / TILE_SIZE_4BPP,
                "special stage OC atlas remains inside OBJ VRAM");
        Require(memcmp(gObjPalette, gOcPalettes[gSelectedOc], 16 * sizeof(u16)) == 0,
                "special stage preserves OC palette");
        Require(oam->split.x == OamExpectedX(body->x - 32) && oam->split.y == OamExpectedY(body->y - 48),
                "special stage OC feet retain native position");
        specialDraws++;
        if (input & DPAD_LEFT) specialLeft++;
        if (input & DPAD_RIGHT) specialRight++;
        if (state == 4 || state == 5 || state == 7 || state == 9 || state == 12 || state == 15)
            specialJump++;
    }
    return result;
}

static void CheckMenuLifecycle(void)
{
    u16 originalHeap[ARRAY_COUNT(gVramHeapState)];
    unsigned i;
    int originalTasks = gNumTasks;
    memcpy(originalHeap, gVramHeapState, sizeof(originalHeap));
    for (i = 0; i < 24; i++) {
        gSelectedOc = -1;
        CreateOcCharacterSelectionScreen(i % NUM_CHARACTERS, FALSE);
        Require(gNumTasks == originalTasks + 1, "exactly one OC menu task created");
        /* Destroy only the newly created selector, preserving unrelated tasks. */
        unsigned t;
        for (t = 0; t < MAX_TASK_NUM; t++) {
            if (gTasks[t].priority == 0x4100 && gTasks[t].data != NULL) {
                TaskDestroy(&gTasks[t]);
                break;
            }
        }
        DiscardGraphics();
        Require(memcmp(originalHeap, gVramHeapState, sizeof(originalHeap)) == 0,
                "menu destruction returns every VRAM allocation");
        Require(gNumTasks == originalTasks, "menu destruction returns task count");
    }
    fprintf(stderr, "PASS: 24 selector create/destroy cycles, identical VRAM heap and task count\n");
}

void __wrap_AgbMain(void)
{
    mode = getenv("SA_OC_MODE") ? getenv("SA_OC_MODE") : "stage";
    entry = getenv("SA_OC_ENTRY") ? atoi(getenv("SA_OC_ENTRY")) : OC_SELECT_FIRST;
    oc = entry >= OC_SELECT_FIRST ? entry - OC_SELECT_FIRST : -1;
    captureDir = getenv("SA_OC_CAPTURE_DIR");
    Require(entry >= 0 && entry < NUM_CHARACTERS + OC_CHARACTER_COUNT, "valid test selector entry");
    if (!strcmp(mode, "boot")) {
        __real_AgbMain();
        return;
    }
    EngineInit();
    GameInit();
    TasksDestroyAll();
    DiscardGraphics();
    NewSaveGame();
    gSelectedCharacter = CHARACTER_SONIC;
    gSelectedOc = -1;
    gGameMode = GAME_MODE_SINGLE_PLAYER;
    LOADED_SAVE->unlockedCharacters = (1 << NUM_CHARACTERS) - 1;
    if (!strcmp(mode, "lifecycle")) {
        CheckMenuLifecycle();
        exit(0);
    }
    if (!strcmp(mode, "multiplayer")) {
        gSelectedOc = OC_KIRO;
        gGameMode = GAME_MODE_MULTI_PLAYER;
        CreateCharacterSelectionScreen(OC_SELECT_FIRST + OC_KIRO, TRUE);
        Require(gSelectedOc == -1, "multiplayer discards OC selection");
        TasksDestroyAll();
        DiscardGraphics();
        fprintf(stderr, "PASS: multiplayer resets OC and accepts only native selector IDs\n");
        exit(0);
    }
    if (!strcmp(mode, "special")) {
        gSelectedOc = oc;
        CreateSpecialStage(CHARACTER_SONIC, 0);
    } else if (!strcmp(mode, "stage")) {
        gCurrentLevel = getenv("SA_OC_LEVEL") ? atoi(getenv("SA_OC_LEVEL")) : 0;
        gSelectedOc = oc;
        ApplyGameStageSettings();
        GameStageStart();
        stageWasStarted = true;
    } else {
        if (captureDir) {
            snprintf(selectionFile, sizeof(selectionFile), "%s/%s-entry%d-choice.ocs", captureDir, mode, entry);
            remove(selectionFile);
            OcSelectionSetStoragePath(selectionFile);
        }
        if (!strcmp(mode, "locked"))
            LOADED_SAVE->unlockedCharacters = (1 << NUM_CHARACTERS) - 1 - CHARACTER_BIT(CHARACTER_AMY);
        if (!strcmp(mode, "cancel"))
            gSelectedOc = OC_KIRO;
        __wrap_CreateCharacterSelectionScreen(CHARACTER_SONIC, FALSE);
    }
    EngineMainLoop();
}

static void DriveMenu(void)
{
    unsigned cycle;
    menuFrames++;
    if (menuFrames < 45) { SetInput(0); return; }
    cycle = (menuFrames - 45) % 18;
    if (movesLeft) {
        if (cycle == 0) {
            SetInput(DPAD_RIGHT);
            movesLeft--;
        } else SetInput(0);
        return;
    }
    if (cycle != 16) { SetInput(0); return; }
    if (strcmp(mode, "cancel")) CheckOcMenuGraphics();
    Capture("selector", menuFrames);
    if (!strcmp(mode, "cancel")) {
        if (!cancelled) { SetInput(B_BUTTON); cancelled = true; }
    } else if (!confirmed) { SetInput(A_BUTTON); confirmed = true; }
}

static u16 GameplayInput(void)
{
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) return 0;
    if (playFrames < 100) return DPAD_RIGHT;
    if (playFrames < 175) return DPAD_LEFT;
    if (playFrames < 280) return DPAD_RIGHT;
    if (playFrames < 360)
        return (gPlayer.moveState & MOVESTATE_IN_AIR) ? DPAD_RIGHT : DPAD_DOWN;
    if (playFrames < 465) return DPAD_RIGHT | ((playFrames % 60 < 12) ? A_BUTTON : 0);
    if (playFrames < 545) return DPAD_LEFT;
    return DPAD_RIGHT | ((playFrames % 120 < 16) ? A_BUTTON : 0);
}

static void CheckGameplay(void)
{
    int state = gPlayer.charState;
    if (!(gPlayer.moveState & MOVESTATE_IGNORE_INPUT)) playFrames++;
    Require(gSelectedOc == oc && gSelectedCharacter == CHARACTER_SONIC && gPlayer.character == CHARACTER_SONIC,
            "OC identity remains separate from native movement/save IDs");
    if (state >= 0 && state < 128) statesSeen[state] = true;
    if (stageFrames % 100 == 0)
        fprintf(stderr, "Stage=%u OC=%d state=%d x=%d y=%d speed=%d rotation=%u move=0x%x\n",
                stageFrames, oc, state, I(gPlayer.qWorldX), I(gPlayer.qWorldY),
                gPlayer.qSpeedGround, gPlayer.rotation, gPlayer.moveState);
    stateStreak = previousState == state ? stateStreak + 1 : 0;
    previousState = state;
    if (gPlayer.timerInvulnerability) {
        invulnerableFrames++;
        if (gStageTime & 2) invulnerableHidden++;
    }
    if (stageFrames > 210 && stateStreak >= 2 && state >= 0 && state < 128 && !stateCaptured[state]) {
        Capture("state", (unsigned)state);
        stateCaptured[state] = true;
    }
    if (playFrames >= 30 && playFrames <= 600 && stageFrames % 12 == 0)
        Capture("motion", stageFrames);
    /* Use the game's actual damage transition to check recoil and its blink
     * controller, rather than inventing a pose or changing elapsed time. */
    if (stageFrames == 905) Player_InitHurt(&gPlayer);
    SetInput(GameplayInput());
    if (playFrames == 1200) {
        Require(drawCount > 300 && paletteChecks > 300, "OC rendered throughout the stage");
        Require(rightDraws && leftDraws, "both facing directions rendered");
        Require(slopeDraws, "native sloped ground renders OC body");
        Require(statesSeen[CHARSTATE_WALK_A], "normal movement animation reached");
        Require(statesSeen[CHARSTATE_JUMP_1] || statesSeen[CHARSTATE_JUMP_2], "jump animation reached through input");
        Require(statesSeen[CHARSTATE_BRAKE], "skid animation reached through reverse input");
        Require(statesSeen[CHARSTATE_SPIN_ATTACK], "roll animation reached through down input");
        Require(invulnerableFrames && invulnerableVisible && invulnerableHidden, "damage invulnerability renders and blinks");
        Capture("end", stageFrames);
        fprintf(stderr, "PASS: %s OC=%d 1200 playable frames, %u total stage frames; draw=%u palette=%u right=%u left=%u slope=%u invulnerable=%u visible=%u hidden=%u x=%d y=%d\n",
                mode, oc, stageFrames, drawCount, paletteChecks, rightDraws, leftDraws, slopeDraws,
                invulnerableFrames, invulnerableVisible, invulnerableHidden, I(gPlayer.qWorldX), I(gPlayer.qWorldY));
        exit(0);
    }
}

void __wrap_VBlankIntrWait(void)
{
    if (headless) gpsp_draw_frame(gameImage);
    __real_VBlankIntrWait();
    frames++;
    if (!strcmp(mode, "special")) {
        unsigned cycle = frames % 150;
        SetInput(DPAD_UP | (cycle < 50 ? DPAD_LEFT : cycle < 100 ? DPAD_RIGHT : 0)
                    | (frames % 100 < 12 ? A_BUTTON : 0));
        if (frames >= 320 && frames <= 920 && frames % 20 == 0) Capture("motion", frames);
        if (frames == 1200) {
            if (oc >= 0)
                Require(specialDraws > 300 && specialLeft && specialRight && specialJump,
                        "special stage rear atlas runs, steers and jumps");
            else Require(gSelectedOc == -1, "native special stage retains native identity");
            Capture("end", frames);
            fprintf(stderr, "PASS: special OC=%d 1200 frames; draw=%u left=%u right=%u jump=%u\n",
                    oc, specialDraws, specialLeft, specialRight, specialJump);
            exit(0);
        }
    } else if (!strcmp(mode, "stage") || !strcmp(mode, "boot")) {
        if (gGameStageTask != NULL) {
            if (!stageWasStarted) {
                Require(gSelectedOc == oc && gSelectedCharacter == CHARACTER_SONIC, "boot selects intended OC through menu");
                stageWasStarted = true;
                fprintf(stderr, "Boot reached real level after %u frames; OC=%d\n", frames, oc);
            }
            stageFrames++;
            CheckGameplay();
        } else if (menuCreated && confirmed) {
            /* A previously used host save can route through course selection
             * after the character selector; advance it using the normal key. */
            SetInput(frames % 60 < 3 ? A_BUTTON : 0);
        } else if (menuCreated) DriveMenu();
        else {
            unsigned cycle = frames % 90;
            SetInput(cycle < 4 ? A_BUTTON : cycle >= 45 && cycle < 49 ? START_BUTTON : 0);
        }
    } else {
        if (confirmed && strcmp(mode, "locked")) {
            Require(gSelectedCharacter == (entry < NUM_CHARACTERS ? entry : CHARACTER_SONIC), "confirmed native character ID");
            Require(gSelectedOc == oc, "confirmed OC identity");
            CheckConfirmedStorage();
            fprintf(stderr, "PASS: selector entry=%d -> native=%d OC=%d\n", entry, gSelectedCharacter, gSelectedOc);
            exit(0);
        }
        if (!strcmp(mode, "locked") && confirmed && menuFrames > 190) {
            Require(gSelectedOc == -1 && gSelectedCharacter == CHARACTER_SONIC && !gGameStageTask,
                    "locked Amy cannot be confirmed");
            fprintf(stderr, "PASS: Amy remains locked after confirm input\n");
            exit(0);
        }
        if (cancelled && menuFrames > 115) {
            Require(gSelectedOc == OC_KIRO && gSelectedCharacter == CHARACTER_SONIC && !gGameStageTask,
                    "cancel restores previous OC identity");
            fprintf(stderr, "PASS: selector cancellation preserves previous Kiro selection\n");
            exit(0);
        }
        DriveMenu();
    }
    if (frames >= 7200) Fail("test timeout before requested flow completed");
}
