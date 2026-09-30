/* Portable integration harness. All pictures come from the real engine's
 * software GBA framebuffer, and every menu choice uses normal key input. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL.h>
#include "global.h"
#include "core.h"
#include "task.h"
#include "flags.h"
#include "malloc_vram.h"
#include "game/game.h"
#include "game/globals.h"
#include "game/sa2/save.h"
#include "game/sa2/oc_characters.h"
#include "game/sa2/oc_player.h"
#include "game/sa2/oc_abilities.h"
#include "game/sa2/stage/enemies/buzzer.h"
#include "game/sa2/ui/character_select.h"
#include "game/sa2/special_stage/main.h"
#include "game/shared/stage/stage.h"
#include "game/shared/stage/player.h"
#include "game/shared/stage/water_effects.h"
#include "game/shared/stage/entities_manager.h"
#include "constants/sa2/char_states.h"
#include "constants/sa2/interactables.h"
#include "data/sa2/oc_sprite_data.h"
#include "platform/shared/video/gpsp_renderer.h"

extern uint16_t gameImage[];
extern bool headless;
extern void __real_VBlankIntrWait(void);
extern void __real_AgbMain(void);
extern void __real_CreateCharacterSelectionScreen(u8, bool8);
extern bool32 __real_OcPlayerDraw(Player *, PlayerSpriteInfo *);
extern bool32 __real_OcSpecialPlayerDraw(Sprite *, u16, u16);
extern bool32 __real_OcAbilityHitsTarget(Sprite *, s32, s32, s16);
extern void __real_OcAbilitiesDraw(Player *);
extern void __real_TaskDestroy(Task *);
extern void Player_InitHurt(Player *);

static unsigned frames, stageFrames, playFrames, menuFrames, movesLeft;
static unsigned drawCount, paletteChecks, rightDraws, leftDraws, slopeDraws;
static unsigned invulnerableFrames, invulnerableVisible, invulnerableHidden;
static unsigned specialDraws, specialLeft, specialRight, specialJump;
static unsigned finalPaletteChecks, countdownPaletteChecks, hiddenPaletteChecks;
static unsigned runMask, jumpMask, attackMask, projectileDraws, realEnemyHits;
static bool previousPaletteReady;
static const u8 *bodyTiles;
static Task *testEnemy;
static Sprite *testEnemySprite;
static MapEntity testEnemyMap;
static unsigned abilityTicks, abilityStarts, abilityActiveFrames, abilityWindupFrames, abilityRecoveryFrames;
static unsigned firstAbilityTick, secondAbilityTick, enemySpawnTick;
static bool enemySpawned, enemyKilled, abilityStarted;
static u16 initialStageHeap[ARRAY_COUNT(gVramHeapState)];
static unsigned initialStageTasks;
static unsigned stageCycles;
static bool dodgeChecked, recoveryDamageChecked;
static unsigned pausePhase, pausePurpose, pauseTicks, pausedStageTime, resumedAt;
static s32 pausedX, pausedY;
static bool introPauseAllowed;
static u16 pauseHeap[ARRAY_COUNT(gVramHeapState)];
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
    fprintf(stderr, "Selected OC=%d native=%d player=%d mode=%d pausePhase=%u purpose=%u flags=0x%x\n",
            gSelectedOc, gSelectedCharacter, gPlayer.character, gGameMode, pausePhase, pausePurpose, gFlags);
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

static u8 ExpectedBase(void) { return oc >= 0 ? OcBaseCharacter(oc) : entry < NUM_CHARACTERS ? entry : CHARACTER_SONIC; }

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

static bool victoryGoalSeen, victoryStarted, victorySettledCaptured;
static unsigned victoryStartFrame, victoryPhaseMask, victoryDrawChecks;
static unsigned victoryLastPhase = 0xff, victoryInitialPhaseMask;
static unsigned victoryStateMask;
static bool victoryBarLeft;
static bool victoryGoalFixture;

static void ApplyGoalFixture(void)
{
    EntitiesManager *manager = TASK_DATA(gEntitiesManagerTask);
    MapData *map = manager->interactables;
    s32 gateX = -1, leverX = -1, leverY = -1;
    for (u32 ry = 0; ry < map->v_regionCount; ry++) {
        for (u32 rx = 0; rx < map->h_regionCount; rx++) {
            u32 offset = map->offsets[ry * map->h_regionCount + rx];
            if (!offset) continue;
            MapEntity *entity = (MapEntity *)((u8 *)map->offsets + offset - 8);
            for (; (s8)entity->x != MAP_ENTITY_STATE_ARRAY_END; entity++) {
                if (entity->index == IA__TOGGLE__GOAL) gateX = TO_WORLD_POS(entity->x, rx);
                if (entity->index == IA__GOAL_LEVER) {
                    leverX = TO_WORLD_POS(entity->x, rx);
                    leverY = TO_WORLD_POS(entity->y, ry);
                }
            }
        }
    }
    Require(gateX > 0 && leverX > 0 && leverY > 0 && ABS(gateX - leverX) < 1024,
            "initial goal fixture comes from the real decompressed stage goal entities");
    Require(!(gPlayer.moveState & MOVESTATE_GOAL_REACHED) && !gStageGoalX,
            "initial fixture does not set a goal flag, goal transition or celebration state");
    gPlayer.qWorldX = Q(MIN(gateX, leverX) - 128);
    gPlayer.qWorldY = Q(leverY - gPlayer.spriteOffsetY);
    gPlayer.checkPointX = I(gPlayer.qWorldX);
    gPlayer.checkPointY = I(gPlayer.qWorldY);
    gCamera.x = I(gPlayer.qWorldX) - DISPLAY_CENTER_X;
    gCamera.y = I(gPlayer.qWorldY) - DISPLAY_CENTER_Y;
    victoryGoalFixture = true;
    fprintf(stderr, "Initial fixture before real goal: gateX=%d lever=%d/%d spawn=%d/%d; no goal/state override\n",
            gateX, leverX, leverY, I(gPlayer.qWorldX), I(gPlayer.qWorldY));
}

static bool VictoryState(int state)
{
    return state == CHARSTATE_ACT_CLEAR_A || state == CHARSTATE_ACT_CLEAR_B
        || state == CHARSTATE_ACT_CLEAR_C || state == CHARSTATE_ACT_CLEAR_TIME_ATTACK_OR_BOSS;
}

static int VictoryTilesPhase(void)
{
    if (!bodyTiles) return -1;
    for (unsigned phase = 0; phase < 4; phase++) {
        if (!memcmp(bodyTiles, gOcActionFrameTiles[oc][20 + phase], OC_FRAME_TILE_BYTES))
            return (int)phase;
    }
    return -1;
}

static u16 VictoryInput(void)
{
    if (VictoryState(gPlayer.charState)) return 0;
    /* The goal's original callback recognizes LEFT as the normal brake key. */
    if (gPlayer.moveState & MOVESTATE_GOAL_REACHED) return DPAD_LEFT;
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) return 0;
    /* The first bouncy bar gives almost no lift at its center. Approach its
     * outer portion with normal left/right input, then let the native launch
     * clear the adjacent wall; repeatedly holding RIGHT at the center stalls
     * original Sonic as well as the OCs. */
    if (I(gPlayer.qWorldX) >= 2280 && I(gPlayer.qWorldX) <= 2310 && I(gPlayer.qWorldY) > 850)
        victoryBarLeft = true;
    if (victoryBarLeft) {
        if (I(gPlayer.qWorldX) > 2255) return DPAD_LEFT;
        victoryBarLeft = false;
    }
    /* Walk/run the real course, with normal jump press/release edges. The
     * pulse also releases rotating handles without installing transitions. */
    return DPAD_RIGHT | (!victoryGoalFixture && stageFrames % 120 < 20 ? A_BUTTON : 0);
}

static void CheckVictory(void)
{
    int state = gPlayer.charState;
    unsigned age;
    int phase;

    Require(oc >= 0 && oc < OC_CHARACTER_COUNT, "victory uses a valid OC");
    Require(gCurrentLevel == 0, "victory traverses the real Leaf Forest Act 1");
    Require(gSelectedOc == oc && gSelectedCharacter == ExpectedBase()
                && gPlayer.character == ExpectedBase(),
            "victory retains intended OC identity and native physical base");
    Require(!(gPlayer.moveState & MOVESTATE_DEAD), "natural victory traversal keeps the player alive");
    if (!(gPlayer.moveState & MOVESTATE_IGNORE_INPUT)) playFrames++;
    if (state >= 0 && state < 128) statesSeen[state] = true;

    if (stageFrames % 300 == 0) {
        fprintf(stderr, "Victory route OC=%d frame=%u play=%u state=%d x=%d y=%d speed=%d goal=%d\n",
                oc, stageFrames, playFrames, state, I(gPlayer.qWorldX), I(gPlayer.qWorldY),
                gPlayer.qSpeedGround, !!(gPlayer.moveState & MOVESTATE_GOAL_REACHED));
    }

    if (gPlayer.moveState & MOVESTATE_GOAL_REACHED) {
        if (!victoryGoalSeen) Capture("goal", stageFrames);
        victoryGoalSeen = true;
        Require(gStageGoalX > DISPLAY_WIDTH && I(gPlayer.qWorldX) >= gStageGoalX, "real level goal toggle was reached by traversal");
    }
    SetInput(VictoryInput());

    if (!VictoryState(state)) {
        Require(!victoryStarted, "natural celebration stays in a continuous ACT_CLEAR state");
        return;
    }
    Require(victoryGoalSeen, "natural goal was observed before ACT_CLEAR");
    if (!victoryStarted) {
        victoryStarted = true;
        victoryStartFrame = stageFrames;
        Capture("act-clear", stageFrames);
    }
    age = stageFrames - victoryStartFrame;
    switch (state) {
        case CHARSTATE_ACT_CLEAR_A: victoryStateMask |= 1; break;
        case CHARSTATE_ACT_CLEAR_B: victoryStateMask |= 2; break;
        case CHARSTATE_ACT_CLEAR_C: victoryStateMask |= 4; break;
        default: victoryStateMask |= 8; break;
    }

    /* Tiles are read from the real OAM-selected OBJ VRAM allocation. The
     * native charState can advance before the next completed DMA frame, so
     * tolerate only the initial upload boundary, never a late hold mismatch. */
    phase = VictoryTilesPhase();
    if (age < 3 && phase < 0) return;
    Require(phase >= 0 && phase < 4, "natural ACT_CLEAR uploads an actual OC victory action frame");
    victoryDrawChecks++;
    if (!(victoryPhaseMask & (1u << phase))) Capture("victory-pose", 20 + phase);
    victoryPhaseMask |= 1u << phase;
    if (age < 100) victoryInitialPhaseMask |= 1u << phase;
    if (getenv("SA_OC_VICTORY_VIDEO")) Capture("victory-motion", age);

    if (age >= 100) {
        if (oc == OC_KIRO)
            Require(phase == 2 || phase == 3, "Kiro's light/first-puff poses occur only at celebration start");
        if (oc == OC_YULIANA)
            Require(phase == 3, "Yuliana settles into action23 beyond 100 celebration frames");
        if (age == 120 || age == 180 || age == 240) Capture("victory-hold", age);
        if (!victorySettledCaptured) {
            Capture("victory-settled", age);
            victorySettledCaptured = true;
        }
    }
    victoryLastPhase = (unsigned)phase;
    if (age < 260) return;

    Require(victoryPhaseMask == 0xf, "all four victory poses action20..23 reached actual OBJ VRAM");
    Require(victoryInitialPhaseMask == 0xf, "all four authored victory poses appear during the first 100 frames");
    Require(victoryDrawChecks > 200, "victory was observed through a continuous long real-engine celebration");
    Require(victoryStateMask & 1, "Act 1 reached its native ACT_CLEAR_A state");
    if (oc == OC_YULIANA) Require(victoryLastPhase == 3, "Yuliana remains settled at the end of the capture");
    Capture("victory-end", age);
    fprintf(stderr, "PASS: real-goal victory OC=%d initialFixture=%d goalX=%u poses=0x%x clearStates=0x%x checks=%u age=%u\n",
            oc, victoryGoalFixture, gStageGoalX, victoryPhaseMask, victoryStateMask, victoryDrawChecks, age);
    exit(0);
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
    if (OcAbilitiesOwnPlayer(player) && !OcAbilityActive(player))
        Require(!HITBOX_IS_ACTIVE(body->s.hitboxes[1]), "ordinary human running/jumping has no passive Sonic attack hitbox");
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
        bodyTiles = OBJ_VRAM0 + oam->split.tileNum * TILE_SIZE_4BPP;
        if (player->moveState & MOVESTATE_FACING_LEFT) leftDraws++;
        else rightDraws++;
        if (!(player->moveState & MOVESTATE_IN_AIR) && player->rotation != 0) slopeDraws++;
        if (player->timerInvulnerability) invulnerableVisible++;
        if (!(body->s.frameFlags & SPRITE_FLAG_MASK_18)) {
            Require(memcmp(gObjPalette, gOcPalettes[gSelectedOc], 16 * sizeof(u16)) == 0,
                    "OC RGB555 palette matches selected identity");
            paletteChecks++;
        }
    }
    return result;
}

void __wrap_OcAbilitiesDraw(Player *player)
{
    unsigned before = gOamFreeIndex;
    __real_OcAbilitiesDraw(player);
    for (unsigned i = before; i < gOamFreeIndex; i++) {
        const OamData *oam = &gOamMallocBuffer[i];
        const u8 *tile = OBJ_VRAM0 + oam->split.tileNum * TILE_SIZE_4BPP;
        unsigned pixels = 0;
        Require(oc == OC_YULIANA, "only Yuliana emits gun projectile objects");
        Require(oam->split.affineMode == ST_OAM_AFFINE_OFF && oam->split.objMode == ST_OAM_OBJ_NORMAL
                    && !oam->split.mosaic && oam->split.bpp == ST_OAM_4BPP
                    && oam->split.shape == ST_OAM_SQUARE && oam->split.size == ST_OAM_SIZE_0
                    && oam->split.matrixNum == 0 && oam->split.paletteNum == 0 && oam->split.priority == 2,
                "gun OAM has explicit valid geometry, flags, palette and priority");
        Require(oam->split.tileNum < OBJ_VRAM_TOTAL_SIZE / TILE_SIZE_4BPP, "gun OAM remains in allocated OBJ VRAM");
        for (unsigned b = 0; b < TILE_SIZE_4BPP; b++) pixels += tile[b] != 0;
        Require(pixels, "gun projectile has real visible tile pixels");
        projectileDraws++;
    }
}

bool32 __wrap_OcAbilityHitsTarget(Sprite *target, s32 x, s32 y, s16 hitbox)
{
    OcAbilityStatus status;
    OcAbilityGetStatus(&gPlayer, &status);
    bool32 hit = __real_OcAbilityHitsTarget(target, x, y, hitbox);
    if (hit) {
        Require(status.projectiles || (status.attacking && status.damaging),
                "real target damage comes from an active melee window or travelling projectile");
        if (target == testEnemySprite) {
            if (status.kind != OC_ABILITY_GUN)
                Require(!__real_OcAbilityHitsTarget(target, x + 1, y, hitbox),
                        "the same moving enemy/boss sprite cannot be damaged twice in one melee window");
            realEnemyHits++;
            fprintf(stderr, "Enemy hit through central collision: kind=%u age=%u damaging=%u projectiles=%u tick=%u\n",
                    status.kind, status.age, status.damaging, status.projectiles, abilityTicks);
        }
    }
    return hit;
}

void __wrap_TaskDestroy(Task *task)
{
    if (task == testEnemy) enemyKilled = true;
    __real_TaskDestroy(task);
}

/* Check the palette consumed by the renderer, one completed DMA frame later.
 * A check inside OcPlayerDraw cannot detect a later native animation overwrite. */
static void CheckFinalPalette(void)
{
    if (previousPaletteReady) {
        Require(memcmp((const void *)OBJ_PLTT, gOcPalettes[oc], 16 * sizeof(u16)) == 0,
                "final hardware OBJ palette preserves OC colors, including countdown and blink");
        finalPaletteChecks++;
        if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) countdownPaletteChecks++;
        if (gPlayer.timerInvulnerability && (gStageTime & 2)) hiddenPaletteChecks++;
    }
    previousPaletteReady = gPlayer.spriteInfoBody != NULL
        && !(gPlayer.spriteInfoBody->s.frameFlags & SPRITE_FLAG_MASK_18) && stageFrames > 200;
    if (stageFrames == 340 || stageFrames == 400 || stageFrames == 460) {
        Require(gPlayer.moveState & MOVESTATE_IGNORE_INPUT, "countdown capture is taken before playable movement");
        Require(previousPaletteReady, "countdown has released intro palette fade ownership");
        Capture("countdown", stageFrames);
    }
    if (!bodyTiles) return;
    /* This compares the tiles actually uploaded to OBJ VRAM. The animation
     * source alone would not prove that all twelve phases reached the screen. */
    for (unsigned phase = 0; phase < OC_ACTION_FRAME_COUNT; phase++) {
        if (memcmp(bodyTiles, gOcActionFrameTiles[oc][phase], OC_FRAME_TILE_BYTES)) continue;
        if (phase < 12 && !(gPlayer.moveState & MOVESTATE_IN_AIR)
            && ABS(gPlayer.qSpeedGround) > Q(1) && gPlayer.charState == CHARSTATE_WALK_A) {
            if (!(runMask & (1u << phase))) Capture("gait", phase);
            runMask |= 1u << phase;
        } else if (phase >= 12 && phase < 16 && (gPlayer.moveState & MOVESTATE_IN_AIR)) {
            jumpMask |= 1u << (phase - 12);
        } else if (phase >= 16 && phase < 20 && OcAbilityActive(&gPlayer)) {
            attackMask |= 1u << (phase - 16);
        }
        break;
    }
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
        gSelectedOc = (i & 1) ? (i / 2) % OC_CHARACTER_COUNT : -1;
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
        gSelectedCharacter = ExpectedBase();
        CreateSpecialStage(ExpectedBase(), 0);
    } else if (!strcmp(mode, "stage") || !strcmp(mode, "abilities") || !strcmp(mode, "stage-lifecycle")
               || !strcmp(mode, "glitch-defense") || !strcmp(mode, "pause") || !strcmp(mode, "air-abilities")
               || !strcmp(mode, "double-a") || !strcmp(mode, "victory") || !strcmp(mode, "native-amy") || !strcmp(mode, "route-native")) {
        if (!strcmp(mode, "stage-lifecycle")) {
            memcpy(initialStageHeap, gVramHeapState, sizeof(initialStageHeap));
            initialStageTasks = gNumTasks;
        }
        gCurrentLevel = getenv("SA_OC_LEVEL") ? atoi(getenv("SA_OC_LEVEL")) : 0;
        gSelectedOc = oc;
        gSelectedCharacter = ExpectedBase();
        ApplyGameStageSettings();
        GameStageStart();
        if (!strcmp(mode, "victory") && !getenv("SA_OC_VICTORY_TRAVERSE")) ApplyGoalFixture();
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
    Require(gSelectedOc == oc && gSelectedCharacter == ExpectedBase() && gPlayer.character == ExpectedBase(),
            "OC identity remains separate from native movement/save IDs");
    CheckFinalPalette();
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
    if (playFrames >= 30 && playFrames <= 600 && stageFrames % 2 == 0)
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
        Require(statesSeen[CHARSTATE_JUMP_1] || statesSeen[CHARSTATE_JUMP_2] || statesSeen[CHARSTATE_AMY_SA1_JUMP],
                "upright jump animation reached through input");
        Require(statesSeen[CHARSTATE_BRAKE], "skid animation reached through reverse input");
        Require(!(gPlayer.moveState & (MOVESTATE_SPIN_ATTACK | MOVESTATE_SPINDASH)),
                "human OC does not inherit a Sonic ball attack");
        Require(runMask == 0xfff, "all twelve distinct running phases reached actual OBJ VRAM");
        Require(jumpMask == 0xf, "all four upright jump phases reached actual OBJ VRAM");
        Require(countdownPaletteChecks > 100 && hiddenPaletteChecks > 20,
                "final hardware palette checked during countdown and invisible damage frames");
        Require(invulnerableFrames && invulnerableVisible && invulnerableHidden, "damage invulnerability renders and blinks");
        Capture("end", stageFrames);
        fprintf(stderr, "PASS: %s OC=%d 1200 playable frames, %u total stage frames; draw=%u palette=%u right=%u left=%u slope=%u invulnerable=%u visible=%u hidden=%u x=%d y=%d physicalPalette=%u countdown=%u blink=%u run=0x%x jump=0x%x\n",
                mode, oc, stageFrames, drawCount, paletteChecks, rightDraws, leftDraws, slopeDraws,
                invulnerableFrames, invulnerableVisible, invulnerableHidden, I(gPlayer.qWorldX), I(gPlayer.qWorldY),
                finalPaletteChecks, countdownPaletteChecks, hiddenPaletteChecks, runMask, jumpMask);
        exit(0);
    }
}

static void SpawnRealEnemy(void)
{
    s32 x = I(gPlayer.qWorldX) + (oc == OC_YULIANA ? 120 : 45);
    s32 y = I(gPlayer.qWorldY) - 8;
    memset(&testEnemyMap, 0, sizeof(testEnemyMap));
    testEnemyMap.x = ((x + 4) & 255) / 8;
    testEnemyMap.y = ((y + 4) & 255) / 8;
    testEnemyMap.d.sData[0] = -16;
    testEnemyMap.d.uData[2] = 32;
    CreateEntity_Buzzer(&testEnemyMap, (x + 4) / 256, (y + 4) / 256, 0);
    for (unsigned t = 0; t < MAX_TASK_NUM; t++) {
        if (gTasks[t].priority == 0x4030 && gTasks[t].data != NULL) {
            EnemyBase *enemy = TASK_DATA(&gTasks[t]);
            if (enemy->base.me == &testEnemyMap) {
                testEnemy = &gTasks[t];
                testEnemySprite = &enemy->s;
                break;
            }
        }
    }
    Require(testEnemy && testEnemySprite, "create a real Buzzer enemy task with actual enemy animation/collision");
    enemySpawned = true;
    enemySpawnTick = abilityTicks;
}

static void CheckAbilities(void)
{
    static const u8 kinds[] = { OC_ABILITY_GLITCH, OC_ABILITY_PAN, OC_ABILITY_KATANA, OC_ABILITY_GUN, OC_ABILITY_DASH };
    static const u8 start[] = { 3, 6, 5, 3, 2 };
    static const u8 end[] = { 8, 13, 11, 5, 8 };
    static const u8 duration[] = { 16, 24, 22, 14, 18 };
    static const u8 cooldown[] = { 50, 32, 30, 22, 36 };
    OcAbilityStatus status;
    Require(gSelectedOc == oc && gPlayer.character == ExpectedBase(), "ability mode uses intended OC physical base");
    CheckFinalPalette();
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) { SetInput(0); return; }
    abilityTicks++;
    OcAbilityGetStatus(&gPlayer, &status);
    Require(!(gPlayer.moveState & (MOVESTATE_SPIN_ATTACK | MOVESTATE_SPINDASH)), "OC abilities do not passively inherit ball damage");
    Require(status.projectiles <= 4, "gun projectile pool remains bounded");
    if (!enemySpawned) {
        if (abilityTicks >= 80 && !(gPlayer.moveState & MOVESTATE_IN_AIR) && ABS(gPlayer.qSpeedGround) < Q(0.125)) {
            SpawnRealEnemy();
            Capture("enemy-before", abilityTicks);
        }
        SetInput(0);
        return;
    }
    if (status.attacksStarted != abilityStarts) {
        Require(status.attacksStarted == abilityStarts + 1, "one B press starts exactly one ability");
        abilityStarts++;
        if (abilityStarts == 1) fprintf(stderr, "Ability start #%u tick=%u age=%u kind=%u enemyRect=%d,%d,%d,%d playerRect=%d,%d,%d,%d\n",
                abilityStarts, abilityTicks, status.age, status.kind,
                testEnemySprite->hitboxes[0].b.left, testEnemySprite->hitboxes[0].b.top,
                testEnemySprite->hitboxes[0].b.right, testEnemySprite->hitboxes[0].b.bottom,
                gPlayer.spriteInfoBody->s.hitboxes[0].b.left, gPlayer.spriteInfoBody->s.hitboxes[0].b.top,
                gPlayer.spriteInfoBody->s.hitboxes[0].b.right, gPlayer.spriteInfoBody->s.hitboxes[0].b.bottom);
        Require(status.kind == kinds[oc], "B selects the OC's real named ability");
        if (abilityStarts == 1) {
            firstAbilityTick = abilityTicks;
            abilityStarted = true;
        } else {
            Require(abilityStarts == 2 && abilityTicks - firstAbilityTick >= cooldown[oc],
                    "repeated B presses during attack/cooldown cannot restart ability early");
            secondAbilityTick = abilityTicks;
        }
    }
    if (abilityStarted) {
        unsigned elapsed = abilityTicks - (abilityStarts == 1 ? firstAbilityTick : secondAbilityTick);
        if (elapsed < duration[oc]) {
            bool expectedDamage = elapsed >= start[oc] && elapsed <= end[oc];
            Require(status.damaging == expectedDamage, "damage window matches actual elapsed engine frames");
            Require(OcAbilityDodging(&gPlayer) == (oc == OC_ELIZABETH && expectedDamage),
                    "glitch dodge exists only during its real active window");
            if (expectedDamage) abilityActiveFrames++;
            else if (elapsed < start[oc]) abilityWindupFrames++;
            else abilityRecoveryFrames++;
        }
        if (abilityStarts == 1 && elapsed < start[oc])
            Require(!enemyKilled && !realEnemyHits, "real enemy survives non-damaging windup");
        if (abilityTicks - firstAbilityTick < 70 && (abilityTicks - firstAbilityTick) % 2 == 0)
            Capture("ability", abilityTicks - firstAbilityTick);
        if (abilityStarts == 2 && abilityTicks - secondAbilityTick >= 100) {
            Require(realEnemyHits == 1 && enemyKilled && status.hitsLanded == 1,
                    "B ability kills a real enemy once through the existing central collision/task path");
            Require(abilityActiveFrames == 2 * (end[oc] - start[oc] + 1)
                        && abilityWindupFrames == 2 * start[oc]
                        && abilityRecoveryFrames == 2 * (duration[oc] - end[oc] - 1),
                    "both attacks complete their full windup, damage and recovery frames");
            Require(!status.attacking && !status.damaging && !status.cooldown && !status.projectiles,
                    "ability, cooldown and projectiles finish without stale state");
            Require(attackMask == 0xf, "all four ability phases reached real OBJ VRAM");
            if (oc == OC_YULIANA)
                Require(status.shotsFired == 2 && projectileDraws > 10, "gun fires two visible real travelling projectiles and expires them");
            else Require(!status.shotsFired && !projectileDraws, "melee ability does not spawn gun objects");
            Capture("ability-end", abilityTicks);
            fprintf(stderr, "PASS: abilities OC=%d kind=%u presses=2 windup=%u active=%u recovery=%u realEnemyHits=%u shots=%u projectileDraws=%u atlas=0x%x\n",
                    oc, status.kind, abilityWindupFrames, abilityActiveFrames, abilityRecoveryFrames, realEnemyHits,
                    status.shotsFired, projectileDraws, attackMask);
            exit(0);
        }
        unsigned sinceFirst = abilityTicks - firstAbilityTick;
        SetInput((sinceFirst == 2 || sinceFirst == end[oc] + 2 || sinceFirst == cooldown[oc] - 2
                    || (sinceFirst >= cooldown[oc] + 3 && sinceFirst < cooldown[oc] + 60)) ? B_BUTTON : 0);
    } else {
        Require(!enemyKilled && !realEnemyHits, "real enemy survives idle human OC without passive jump/roll attack");
        SetInput(abilityTicks - enemySpawnTick == 5 ? B_BUTTON : 0);
    }
}

static void CheckStageLifecycle(void)
{
    SetInput(stageFrames == 590 ? B_BUTTON : 0);
    if (stageFrames != 600) return;
    /* Recreate the whole real level, including the OC renderer and gun pool.
     * This also exercises teardown while a newly fired projectile is alive. */
    TasksDestroyAll();
    DiscardGraphics();
    Require(gNumTasks == initialStageTasks, "level teardown returns to the engine's original sentinel task count");
    Require(memcmp(initialStageHeap, gVramHeapState, sizeof(initialStageHeap)) == 0,
            "level teardown releases OC body/projectile and native stage VRAM allocations");
    OcAbilityStatus status;
    OcAbilityGetStatus(&gPlayer, &status);
    Require(!status.attacking && !status.projectiles && !status.attacksStarted,
            "player teardown clears the actual OC ability owner and projectile state");
    if (++stageCycles == OC_CHARACTER_COUNT) {
        fprintf(stderr, "PASS: %u real level create/600-frame/destroy cycles; VRAM heap identical and abilities cleared\n", stageCycles);
        exit(0);
    }
    oc = stageCycles;
    entry = OC_SELECT_FIRST + oc;
    gSelectedOc = oc;
    gSelectedCharacter = ExpectedBase();
    gCurrentLevel = 0;
    stageFrames = 0;
    SetInput(0);
    ApplyGameStageSettings();
    GameStageStart();
}

static void CheckGlitchDefense(void)
{
    OcAbilityStatus status;
    Require(oc == OC_ELIZABETH, "glitch defense fixture selects Elizabeth");
    CheckFinalPalette();
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) { SetInput(0); return; }
    abilityTicks++;
    OcAbilityGetStatus(&gPlayer, &status);
    SetInput(abilityTicks == 80 ? B_BUTTON : 0);
    if (status.attacking && status.age == 4) {
        u16 timer = gPlayer.timerInvulnerability;
        s32 state = gPlayer.charState;
        Require(!Coll_DamagePlayer(&gPlayer) && gPlayer.timerInvulnerability == timer && gPlayer.charState == state,
                "actual engine damage is rejected during the glitch dodge window");
        dodgeChecked = true;
    }
    if (status.attacking && status.age == 10) {
        /* A ring fixture lets the real damage/ring-loss/recoil path run instead
         * of killing the character before its post-dodge palette can be checked. */
        gRingCount = 10;
        Require(Coll_DamagePlayer(&gPlayer) && gPlayer.timerInvulnerability && !OcAbilityActive(&gPlayer),
                "actual engine damage resumes during recovery and cancels the glitch ability");
        recoveryDamageChecked = true;
    }
    if (abilityTicks == 160) {
        Require(dodgeChecked && recoveryDamageChecked && hiddenPaletteChecks > 10,
                "glitch defense tests active protection, recovery damage and final blink colors");
        fprintf(stderr, "PASS: glitch real damage rejected in active phase, accepted in recovery; physical blink palettes=%u\n", hiddenPaletteChecks);
        exit(0);
    }
}

static void CheckAirAbilities(void)
{
    static const u8 start[] = { 3, 6, 5, 3, 2 };
    static const u8 end[] = { 8, 13, 11, 5, 8 };
    static const u8 duration[] = { 16, 24, 22, 14, 18 };
    bool doubleA = !strcmp(mode, "double-a");
    OcAbilityStatus status;
    CheckFinalPalette();
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) { SetInput(0); return; }
    abilityTicks++;
    OcAbilityGetStatus(&gPlayer, &status);
    if (doubleA) Require(oc == OC_JUDE && gPlayer.character == CHARACTER_AMY, "second-A pan uses Jude's Amy physics");
    Require(!(gPlayer.moveState & (MOVESTATE_SPIN_ATTACK | MOVESTATE_SPINDASH)), "air ability preserves upright human collision mode");
    if (status.attacksStarted != abilityStarts) {
        Require(status.attacksStarted == abilityStarts + 1 && (gPlayer.moveState & MOVESTATE_IN_AIR),
                "real A jump followed by B/second A starts exactly one ability in the air");
        abilityStarts++;
        Require(abilityStarts <= (doubleA ? 2 : 1), "holding B or a third A cannot repeat the air ability");
        firstAbilityTick = abilityTicks;
        abilityStarted = true;
    }
    if (abilityStarted && abilityTicks - firstAbilityTick < duration[oc]) {
        unsigned elapsed = abilityTicks - firstAbilityTick;
        bool active = elapsed >= start[oc] && elapsed <= end[oc];
        Require(status.damaging == active, "airborne ability keeps its exact normal damage window");
        if (active) abilityActiveFrames++;
    }
    if (doubleA && (abilityTicks == 118 || abilityTicks == 208))
        Require((gPlayer.moveState & MOVESTATE_IN_AIR) && status.aerialPanUsed,
                "third-A fixture occurs in the same flight after the pan cooldown");
    if (abilityTicks > 30 && abilityTicks < 245 && abilityTicks % 2 == 0) Capture("air", abilityTicks);
    if (abilityTicks == 260) {
        Require(abilityStarts == (doubleA ? 2 : 1) && !(gPlayer.moveState & MOVESTATE_IN_AIR)
                    && !status.attacking && !status.projectiles && !status.cooldown,
                "air ability falls and lands through native physics with no stale attack or projectile");
        Require(abilityActiveFrames == (doubleA ? 2 : 1) * (end[oc] - start[oc] + 1) && attackMask == 0xf,
                "complete air damage window and all four real attack poses were rendered");
        if (doubleA) Require(!status.aerialPanUsed, "landing resets second-A pan for the next real jump");
        if (oc == OC_YULIANA) Require(status.shotsFired == 1 && projectileDraws > 20, "air gun fires and expires a real travelling projectile");
        fprintf(stderr, "PASS: %s OC=%d real jump/air ability/landing; attacks=%u active=%u atlas=0x%x shots=%u\n",
                mode, oc, abilityStarts, abilityActiveFrames, attackMask, status.shotsFired);
        exit(0);
    }
    u16 input = 0;
    if (abilityTicks == 80 || (doubleA && abilityTicks == 170)) input = A_BUTTON;
    if (abilityTicks == 84 || (doubleA && abilityTicks == 174)) {
        Require((gPlayer.moveState & MOVESTATE_IN_AIR) && gPlayer.qSpeedAirY < 0,
                "air ability key is pressed during a real ascending jump");
        input = doubleA ? A_BUTTON : B_BUTTON;
    }
    if (doubleA && (abilityTicks == 118 || abilityTicks == 208)) input = A_BUTTON;
    SetInput(input);
}

static void CheckNativeRoute(void)
{
    Require(gSelectedOc == -1 && gPlayer.character == CHARACTER_SONIC, "native route comparison uses unmodified Sonic");
    if (!(gPlayer.moveState & MOVESTATE_IGNORE_INPUT)) playFrames++;
    if (stageFrames % 300 == 0)
        fprintf(stderr, "Native route frame=%u state=%d x=%d y=%d move=0x%x offsets=%d/%d air=%d/%d\n",
                stageFrames, gPlayer.charState, I(gPlayer.qWorldX), I(gPlayer.qWorldY), gPlayer.moveState,
                gPlayer.spriteOffsetX, gPlayer.spriteOffsetY, gPlayer.qSpeedAirX, gPlayer.qSpeedAirY);
    SetInput(VictoryInput());
    if (VictoryState(gPlayer.charState)) {
        fprintf(stderr, "PASS: identical natural route reaches native Sonic goal x=%d after %u stage frames\n", gStageGoalX, stageFrames);
        exit(0);
    }
}

static void CheckNativeAmy(void)
{
    OcAbilityStatus status;
    Require(oc == -1 && gSelectedOc == -1 && gSelectedCharacter == CHARACTER_AMY && gPlayer.character == CHARACTER_AMY,
            "original Amy remains the real native Amy identity and physical character");
    OcAbilityGetStatus(&gPlayer, &status);
    Require(!OcAbilitiesOwnPlayer(&gPlayer) && !status.attacksStarted && !drawCount,
            "original Amy uses her native sprites and moves without OC renderer/ability overrides");
    if (gPlayer.moveState & MOVESTATE_IGNORE_INPUT) { SetInput(0); return; }
    playFrames++;
    if (gPlayer.charState >= 0 && gPlayer.charState < 128) statesSeen[gPlayer.charState] = true;
    SetInput(playFrames == 80 || playFrames == 84 ? A_BUTTON : playFrames == 160 ? B_BUTTON : 0);
    if (playFrames == 260) {
        Require(statesSeen[CHARSTATE_JUMP_1] || statesSeen[CHARSTATE_JUMP_2] || statesSeen[CHARSTATE_AMY_SA1_JUMP],
                "original Amy still jumps through normal A input");
        Require(statesSeen[CHARSTATE_BOOSTLESS_ATTACK] || statesSeen[CHARSTATE_AIR_ATTACK]
                    || statesSeen[CHARSTATE_AMY_HAMMER_ATTACK] || statesSeen[CHARSTATE_AMY_MID_AIR_HAMMER_SWIRL],
                "original Amy still executes a native hammer action through normal input");
        Capture("native-end", playFrames);
        fprintf(stderr, "PASS: original Amy normal jump/second-A/native hammer, no OC hooks\n");
        exit(0);
    }
}

static void RequestPause(unsigned purpose)
{
    pausePurpose = purpose;
    pausePhase = 1;
    pauseTicks = 0;
    memcpy(pauseHeap, gVramHeapState, sizeof(pauseHeap));
    SetInput(START_BUTTON);
}

static void CheckPauseFlow(void)
{
    if (!(gPlayer.moveState & MOVESTATE_IGNORE_INPUT)) playFrames++;
    Require(gSelectedOc == oc && gSelectedCharacter == ExpectedBase() && gPlayer.character == ExpectedBase(),
            "pause/restart retains OC identity and safe original physics ID");
    /* Water's HBlank path legitimately tints OBJ colors, so the generic
     * untinted-palette assertion belongs to the dry portions of this test. */
    if (!gWater.isActive) CheckFinalPalette();
    if (!pausePhase) {
        if (stageFrames == 60) {
            introPauseAllowed = !(gStageFlags & STAGE_FLAG__DISABLE_PAUSE_MENU);
            RequestPause(0);
        } else SetInput(0);
        return;
    }
    if (pausePhase == 1) {
        SetInput(0);
        if (++pauseTicks < 8) return;
        if (!pausePurpose && !introPauseAllowed) {
            Require(!(gFlags & FLAGS_PAUSE_GAME), "START during disabled intro cannot create a partial pause menu");
            pausePhase = 3;
            resumedAt = stageFrames;
            return;
        }
        Require(gFlags & FLAGS_PAUSE_GAME, "normal START input opens the real pause task");
        pausedStageTime = gStageTime;
        pausedX = gPlayer.qWorldX;
        pausedY = gPlayer.qWorldY;
        pauseTicks = 0;
        pausePhase = 2;
    }
    if (pausePhase == 2) {
        Require((gFlags & FLAGS_PAUSE_GAME) && gStageTime == pausedStageTime
                    && gPlayer.qWorldX == pausedX && gPlayer.qWorldY == pausedY,
                "real pause freezes game time and native player position");
        pauseTicks++;
        if (pauseTicks == 25) Capture("pause", pausePurpose);
        if (pausePurpose == 2 && pauseTicks == 25) {
            Require(gWater.isActive && gWater.t, "water pause uses a live stage water task");
            WaterData *water = TASK_DATA(gWater.t);
            Require(memcmp(&water->pal[15][9], &((const u16 *)OBJ_PLTT)[249], 6 * sizeof(u16)) == 0,
                    "pause option colors update the real underwater and physical OBJ palette together");
        }
        if (pausePurpose >= 3) {
            SetInput(pauseTicks == 10 ? DPAD_DOWN : pauseTicks == 30 ? A_BUTTON : 0);
            if (pauseTicks == 31) pausePhase = 4;
        } else {
            SetInput(pauseTicks == 10 ? DPAD_DOWN : pauseTicks == 20 ? DPAD_UP : pauseTicks == 40 ? START_BUTTON : 0);
            if (pauseTicks == 40) { pausePhase = 3; resumedAt = stageFrames; }
        }
        return;
    }
    if (pausePhase == 3) {
        SetInput(0);
        if (stageFrames - resumedAt < 8) return;
        Require(!(gFlags & FLAGS_PAUSE_GAME), "continue/release START closes the real pause task");
        if (pausePurpose <= 1)
            Require(memcmp(pauseHeap, gVramHeapState, sizeof(pauseHeap)) == 0,
                    "continue returns exactly the pause menu's VRAM allocation");
        if (!pausePurpose) {
            pausePhase = 9;
        } else if (pausePurpose == 1) {
            /* A location fixture enters Leaf Forest's actual water stretch.
             * The stage water task itself computes isActive on the next frame;
             * no water pointer or activity flag is fabricated by the test. */
            gPlayer.qWorldX = Q(7000);
            gPlayer.qWorldY = Q(2108);
            gPlayer.qSpeedGround = gPlayer.qSpeedAirX = gPlayer.qSpeedAirY = 0;
            gCamera.x = 7000 - DISPLAY_CENTER_X;
            gCamera.y = 2108 - DISPLAY_CENTER_Y;
            pausePhase = 6;
            pauseTicks = 0;
            previousPaletteReady = false;
        } else if (pausePurpose == 2) {
            Require(gWater.isActive && gWater.t, "water activity was produced by the real Leaf Forest task");
            TasksDestroyAll();
            DiscardGraphics();
            Require(!gWater.t && !gWater.isActive, "destroying active water clears task and activity before the next frame");
            gCurrentLevel = 1;
            stageFrames = playFrames = 0;
            previousPaletteReady = false;
            bodyTiles = NULL;
            ApplyGameStageSettings();
            GameStageStart();
            Require(!gWater.t && !gWater.isActive, "Leaf Forest Act2 starts dry after leaving active Act1 water");
            pausePhase = 7;
        }
        return;
    }
    if (pausePhase == 6) {
        SetInput(0);
        if (++pauseTicks >= 8) {
            Require(gWater.t && gWater.isActive, "real stage water callback activates at its configured world location");
            RequestPause(2);
        }
    } else if (pausePhase == 7) {
        SetInput(0);
        if (playFrames >= 80) RequestPause(3);
    } else if (pausePhase == 5) {
        /* A real title/menu selection just restarted the level. */
        stageFrames = playFrames = 0;
        previousPaletteReady = false;
        bodyTiles = NULL;
        pausePhase = 8;
        SetInput(0);
    } else if (pausePhase == 8) {
        SetInput(0);
        if (playFrames >= 50) RequestPause(4);
    } else if (pausePhase == 9) {
        SetInput(0);
        if (playFrames >= 80) RequestPause(1);
    }
}

static bool CheckPauseQuit(void)
{
    if (pausePhase != 4) return false;
    Require(!(gFlags & FLAGS_PAUSE_GAME) && !gWater.t && !gWater.isActive,
            "QUIT destroys the stage and water without stale pause or water state");
    Capture("quit", pausePurpose);
    if (pausePurpose == 4) {
        fprintf(stderr, "PASS: pause OC=%d intro START, continue, cursor options, live water pause, water->dry Act2, quit/title/menu/restart/quit\n", oc);
        exit(0);
    }
    Require(pausePurpose == 3, "dry-stage QUIT returns to the title for real restart flow");
    pausePhase = 5;
    menuCreated = confirmed = false;
    stageWasStarted = false;
    previousPaletteReady = false;
    bodyTiles = NULL;
    stageFrames = playFrames = 0;
    return true;
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
    } else if (!strcmp(mode, "stage") || !strcmp(mode, "boot") || !strcmp(mode, "abilities")
               || !strcmp(mode, "stage-lifecycle") || !strcmp(mode, "glitch-defense") || !strcmp(mode, "pause") || !strcmp(mode, "air-abilities")
               || !strcmp(mode, "double-a") || !strcmp(mode, "victory") || !strcmp(mode, "native-amy") || !strcmp(mode, "route-native")) {
        if (!strcmp(mode, "pause") && !gGameStageTask) CheckPauseQuit();
        if (gGameStageTask != NULL) {
            if (!stageWasStarted) {
                Require(gSelectedOc == oc && gSelectedCharacter == ExpectedBase(), "boot selects intended OC through menu");
                stageWasStarted = true;
                fprintf(stderr, "Boot reached real level after %u frames; OC=%d\n", frames, oc);
            }
            stageFrames++;
            if (!strcmp(mode, "victory")) CheckVictory();
            else if (!strcmp(mode, "abilities")) CheckAbilities();
            else if (!strcmp(mode, "stage-lifecycle")) CheckStageLifecycle();
            else if (!strcmp(mode, "glitch-defense")) CheckGlitchDefense();
            else if (!strcmp(mode, "air-abilities") || !strcmp(mode, "double-a")) CheckAirAbilities();
            else if (!strcmp(mode, "native-amy")) CheckNativeAmy();
            else if (!strcmp(mode, "route-native")) CheckNativeRoute();
            else if (!strcmp(mode, "pause")) CheckPauseFlow();
            else CheckGameplay();
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
            Require(gSelectedCharacter == (entry < NUM_CHARACTERS ? entry : OcBaseCharacter(oc)), "confirmed native character ID");
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
