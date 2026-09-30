#include "global.h"

#if PORTABLE && (GAME == GAME_SA2)
#include <string.h>
#include "flags.h"
#include "sprite.h"
#include "task.h"
#include "data/sa2/oc_sprite_data.h"
#include "game/globals.h"
#include "game/sa2/oc_characters.h"
#include "game/sa2/oc_abilities.h"
#include "game/sa2/oc_player.h"
#include "game/shared/stage/water_effects.h"
#include "constants/sa2/animations.h"
#include "constants/sa2/char_states.h"

/* One atlas frame occupies one 64x64 4bpp object. No native animation table is
 * extended: its script still sets the exact collision boxes and transitions. */
typedef struct {
    Sprite sprite;
    SpriteOffset dimensions;
    Player *owner;
    u32 lastStageTime;
    u32 runPhase;
    s32 lastWorldX;
    s32 lastWorldY;
    u32 lastMoveState;
    u32 victoryAge;
    u16 stateAge;
    s8 lastState;
    s8 oc;
    u8 frame;
    bool8 victoryActive;
} OcPlayerRenderer;

static OcPlayerRenderer sOcPlayer;
static u32 sOcSpecialRunPhase;

/* Extended art keeps the original atlas stable. The action atlas contains
 * twelve gait phases, four upright jumps, four ability phases and four cheers. */
#define OC_ACTION_FRAME(index) (OC_FRAME_COUNT + (index))
#define OC_RUN_PHASE_COUNT 12
#define OC_RUN_PHASE_DISTANCE Q(8)
#define OC_REVISED_RUN_FIRST (OC_FRAME_COUNT + OC_ACTION_FRAME_COUNT)
#define OC_REVISED_ATTACK_FIRST (OC_REVISED_RUN_FIRST + OC_REVISED_RUN_FRAME_COUNT)

/* These revisions are isolated from Elizabeth and the legacy jump/victory
 * atlases. Kura shares only the revised ability atlas, keeping her run intact. */
static bool32 OcPlayerUsesRevisedRun(void)
{
    return sOcPlayer.oc == OC_JUDE || sOcPlayer.oc == OC_KIRO || sOcPlayer.oc == OC_YULIANA;
}

static u8 OcPlayerRevisedIndex(void)
{
    return sOcPlayer.oc == OC_JUDE ? 0 : sOcPlayer.oc == OC_KIRO ? 1 : sOcPlayer.oc == OC_YULIANA ? 2 : 3;
}

static bool32 OcPlayerIsVictoryState(u8 state)
{
    return state == CHARSTATE_ACT_CLEAR_A || state == CHARSTATE_ACT_CLEAR_B || state == CHARSTATE_ACT_CLEAR_C
        || state == CHARSTATE_ACT_CLEAR_TIME_ATTACK_OR_BOSS;
}

static u8 OcPlayerVictoryFrame(void)
{
    u32 age = sOcPlayer.victoryAge;
    u8 phase;
    switch (sOcPlayer.oc) {
        case OC_KIRO:
            /* Light once, take the first puff, then keep the smoke loop. */
            phase = age < 20 ? 0 : age < 40 ? 1 : 2 + (((age - 40) / 28) & 1);
            break;
        case OC_ELIZABETH:
            phase = (age / 18) % 4;
            break;
        case OC_YULIANA:
            /* Hold the gesture, then lower the hand and settle. */
            phase = MIN(age / 20, 3);
            break;
        default:
            phase = (age / 20) % 4;
            break;
    }
    return OC_ACTION_FRAME(20 + phase);
}

static u8 OcPlayerJumpFrame(Player *player)
{
    if (sOcPlayer.stateAge < 3 && (player->charState == CHARSTATE_JUMP_1 || player->charState == CHARSTATE_JUMP_2
                                 || player->charState == CHARSTATE_AMY_SA1_JUMP)) {
        return OC_ACTION_FRAME(12);
    }
    if (player->qSpeedAirY < -Q(1)) {
        return OC_ACTION_FRAME(13);
    }
    return OC_ACTION_FRAME(player->qSpeedAirY <= Q(1) ? 14 : 15);
}

static u8 OcPlayerRunFrame(Player *player)
{
    if (ABS(player->qSpeedGround) < Q(0.125)) {
        return 0;
    }
    if (OcPlayerUsesRevisedRun()) {
        return OC_REVISED_RUN_FIRST + ((sOcPlayer.runPhase >> 8) % OC_REVISED_RUN_FRAME_COUNT);
    }
    return OC_ACTION_FRAME((sOcPlayer.runPhase >> 8) % OC_RUN_PHASE_COUNT);
}

static bool32 OcPlayerIsSelected(Player *player)
{
    return player == &gPlayer && player->playerID == PLAYER_1 && gSelectedOc >= 0 && gSelectedOc < OC_CHARACTER_COUNT
        && player->character == OcBaseCharacter(gSelectedOc) && IS_SINGLE_PLAYER
        && !(gStageFlags & STAGE_FLAG__DEMO_RUNNING);
}

void OcPlayerRelease(Player *player)
{
    if (sOcPlayer.owner == player) {
        if (sOcPlayer.sprite.graphics.dest != NULL) {
            VramFree(sOcPlayer.sprite.graphics.dest);
        }
        memset(&sOcPlayer, 0, sizeof(sOcPlayer));
    }
}

void OcPlayerInit(Player *player)
{
    if (sOcPlayer.owner != NULL) {
        OcPlayerRelease(sOcPlayer.owner);
    }
    if (OcPlayerIsSelected(player)) {
        void *tiles = VramMalloc(64);
        /* The engine returns ewram_end, rather than NULL, on exhaustion. */
        if (tiles == ewram_end) {
            return;
        }
        sOcPlayer.owner = player;
        sOcPlayer.oc = gSelectedOc;
        sOcPlayer.frame = 0xFF;
        sOcPlayer.lastState = CHARSTATE_INVALID;
        sOcPlayer.lastStageTime = gStageTime;
        sOcPlayer.lastWorldX = player->qWorldX;
        sOcPlayer.lastWorldY = player->qWorldY;
        sOcPlayer.lastMoveState = player->moveState;
        sOcPlayer.sprite.graphics.dest = tiles;
        sOcPlayer.sprite.graphics.size = 64 * TILE_SIZE_4BPP;
        sOcPlayer.sprite.palId = 0;
        sOcPlayer.dimensions.numSubframes = 1;
        sOcPlayer.dimensions.width = 64;
        sOcPlayer.dimensions.height = 64;
        sOcPlayer.dimensions.offsetX = 32;
        sOcPlayer.dimensions.offsetY = 34;
        sOcPlayer.sprite.dimensions = &sOcPlayer.dimensions;
    }
}

static u8 OcPlayerFrame(Player *player)
{
    u16 age = sOcPlayer.stateAge;
    u8 state = player->charState;

    if ((player->moveState & MOVESTATE_DEAD) || state == CHARSTATE_DEAD) {
        return 20;
    }
    if (state == CHARSTATE_HIT_AIR || state == CHARSTATE_HIT_STUNNED) {
        return 18 + ((age / 8) & 1);
    }
    if (OcAbilityActive(player)) {
        if (OcPlayerUsesRevisedRun() || sOcPlayer.oc == OC_KURA) {
            return OC_REVISED_ATTACK_FIRST + MIN(OcAbilityVisualPhase(player), OC_REVISED_ATTACK_FRAME_COUNT - 1);
        }
        return OC_ACTION_FRAME(16 + MIN(OcAbilityFramePhase(player), 3));
    }
    switch (state) {
        case CHARSTATE_SPIN_DASH:
            return 9;
        case CHARSTATE_SPIN_ATTACK:
            return (player->moveState & MOVESTATE_IN_AIR) ? OcPlayerJumpFrame(player) : OcPlayerRunFrame(player);
        case CHARSTATE_CURLED_IN_AIR:
            return OcPlayerJumpFrame(player);
        case CHARSTATE_IN_WHIRLWIND:
        case CHARSTATE_WINDUP_STICK_UPWARDS:
        case CHARSTATE_WINDUP_STICK_DOWNWARDS:
        case CHARSTATE_WINDUP_STICK_SINGLE_TURN_UP:
        case CHARSTATE_WINDUP_STICK_SINGLE_TURN_DOWN:
            return 23;
        case CHARSTATE_JUMP_1:
        case CHARSTATE_JUMP_2:
        case CHARSTATE_AMY_SA1_JUMP:
        case CHARSTATE_GRINDING_SONIC_AMY_JUMP_OFF:
            return OcPlayerJumpFrame(player);
        case CHARSTATE_HIT_GROUND:
            return 14;
        case CHARSTATE_BRAKE:
        case CHARSTATE_BRAKE_GOAL:
        case CHARSTATE_GOAL_BRAKE_A:
        case CHARSTATE_GOAL_BRAKE_B:
        case CHARSTATE_GOAL_BRAKE_C:
            return 15 + ((age / 4) % 3);
        case CHARSTATE_TURN_SLOW:
        case CHARSTATE_TURN_AFTER_BRAKE:
            return 17;
        case CHARSTATE_CROUCH:
            return 9;
        case CHARSTATE_TAUNT:
            return ((age / 14) & 1) ? 28 : 0;
        case CHARSTATE_HANGING:
        case CHARSTATE_GRABBING_HANDLE_A:
        case CHARSTATE_GRABBING_HANDLE_B:
        case CHARSTATE_TURNAROUND_BAR:
        case CHARSTATE_POLE:
            return 23;
        case CHARSTATE_GRINDING:
        case CHARSTATE_ICE_SLIDE:
        case CHARSTATE_LAUNCHER_IN_CART:
            return 25;
        case CHARSTATE_ACT_CLEAR_A:
        case CHARSTATE_ACT_CLEAR_B:
        case CHARSTATE_ACT_CLEAR_C:
        case CHARSTATE_ACT_CLEAR_TIME_ATTACK_OR_BOSS:
            return OcPlayerVictoryFrame();
        case CHARSTATE_BOOSTLESS_ATTACK:
        case CHARSTATE_AIR_ATTACK:
        case CHARSTATE_BOOST_ATTACK:
        case CHARSTATE_SOME_ATTACK:
        case CHARSTATE_SOME_OTHER_ATTACK:
        case CHARSTATE_SONIC_FORWARD_THRUST:
        case CHARSTATE_TRICK_FORWARD:
        case CHARSTATE_TRICK_BACKWARD:
        case CHARSTATE_AMY_HAMMER_ATTACK:
        case CHARSTATE_AMY_SA1_HAMMER_ATTACK:
        case CHARSTATE_AMY_MID_AIR_HAMMER_SWIRL:
            /* Scripted/native attack states can outlive a blocked ability.
             * Show a weapon only while its timed custom ability is active. */
            if (OcPlayerUsesRevisedRun() || sOcPlayer.oc == OC_KURA) {
                return (player->moveState & MOVESTATE_IN_AIR) ? OcPlayerJumpFrame(player) : OcPlayerRunFrame(player);
            }
            return OC_ACTION_FRAME(16 + ((age / 4) % 4));
        case CHARSTATE_TRICK_DOWN:
        case CHARSTATE_TRICK_UP:
            return OcPlayerJumpFrame(player);
        case CHARSTATE_WALK_A:
        case CHARSTATE_WALLRUN_INIT:
        case CHARSTATE_WALLRUN_TO_WALL:
        case CHARSTATE_WALLRUN_ON_WALL:
        case CHARSTATE_IN_CORKSCREW:
        case CHARSTATE_IN_CORKSCREW_3D_RUNNING_DOWN:
        case CHARSTATE_IN_CORKSCREW_3D_RUNNING_UP:
        case CHARSTATE_BOUNCE:
            if (player->moveState & MOVESTATE_20) {
                return 24;
            }
            return OcPlayerRunFrame(player);
        case CHARSTATE_FALLING_VULNERABLE_A:
        case CHARSTATE_FALLING_VULNERABLE_B:
        case CHARSTATE_SPRING_MUSIC_PLANT:
        case CHARSTATE_SPRING_B:
        case CHARSTATE_SPRING_C:
        case CHARSTATE_RAMP_AND_DASHRING:
        case CHARSTATE_NOTE_BLOCK:
        case CHARSTATE_FLUTE_EXHAUST:
        case CHARSTATE_LAUNCHER_IN_AIR:
            return OcPlayerJumpFrame(player);
        default:
            if (player->moveState & MOVESTATE_IN_AIR) {
                return OcPlayerJumpFrame(player);
            }
            if (player->moveState & MOVESTATE_20) {
                return 24;
            }
            if (age % 180 >= 174) {
                return 1;
            }
            return (age % 48 >= 24) ? 2 : 0;
    }
}

void OcPlayerPreparePalette(Player *player, PlayerSpriteInfo *body)
{
    const u16 *palette;
    const Sprite *nativeBody = &body->s;
    if (!OcPlayerIsSelected(player)) {
        return;
    }
    palette = gOcPalettes[gSelectedOc];
    /* Only the intro fade owns this bank. STAGE_FLAG__100 also covers the
     * countdown, after the fade has ended and the native Sonic script can
     * write its own incompatible colors here. The intro clears MASK_18 when
     * it releases palette ownership; restore the OC from that point onward. */
    if (!(nativeBody->frameFlags & SPRITE_FLAG_MASK_18)) {
        memcpy(&gObjPalette[0], palette, 16 * sizeof(u16));
        gFlags |= FLAGS_UPDATE_SPRITE_PALETTES;
    } else {
        /* The lower half of a water split must share the same intro fade. */
        palette = &gObjPalette[0];
    }
    if (gWater.t != NULL && gWater.isActive) {
        WaterData *data = TASK_DATA(gWater.t);
        u32 mask = gWater.blendColors;
        u32 maskA = 0x7BDE7BDE;
        u32 maskB = 0x739C739C;
        u8 index;
        /* Preserve the engine's packed RGB555 water tint and HBlank split.
         * memcpy also handles halfword-aligned palettes on ARM safely. */
        for (index = 0; index < 16; index += 2) {
            u32 colors;
            memcpy(&colors, &palette[index], sizeof(colors));
            colors = ((colors & maskA) + (((colors & maskB) + (maskB & mask)) >> 1)) >> 1;
            memcpy(&data->pal[0][index], &colors, sizeof(colors));
        }
    }
}

/* DisplaySprite reads its OAM shape from the native animation table. This
 * single-frame atlas uses the same allocator/order/matrices without pretending
 * that its tiles are an existing Sonic animation. */
static void OcPlayerDisplaySprite(Sprite *sprite)
{
    const SpriteOffset *dimensions = sprite->dimensions;
    s32 x = sprite->x;
    s32 y = sprite->y;
    s32 extent = 64;
    OamData *oam;
    bool32 affine = sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE_ENABLE;

    if (sprite->frameFlags & SPRITE_FLAG_GLOBAL_OFFSET) {
        x -= gSpriteOffset.x;
        y -= gSpriteOffset.y;
    }
    if (affine) {
        if (sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE_DOUBLE_SIZE) {
            x -= 32;
            y -= 32;
            extent = 128;
        }
    } else {
        x -= (sprite->frameFlags & SPRITE_FLAG_MASK_X_FLIP) ? 64 - dimensions->offsetX : dimensions->offsetX;
        y -= (sprite->frameFlags & SPRITE_FLAG_MASK_Y_FLIP) ? 64 - dimensions->offsetY : dimensions->offsetY;
    }
    if (x + extent < 0 || x > DISPLAY_WIDTH || y + extent < 0 || y > DISPLAY_HEIGHT) {
        return;
    }
    oam = OamMalloc(GET_SPRITE_OAM_ORDER(sprite));
    if (oam == iwram_end) {
        return;
    }
    sprite->oamBaseIndex = gOamFreeIndex - 1;
    sprite->numSubFrames = 1;
#if !EXTENDED_OAM
    oam->all.attr0 = y & 0xFF;
    oam->all.attr1 = (x & 0x1FF) | 0xC000;
    oam->all.attr2 = GET_TILE_NUM(sprite->graphics.dest) | (sprite->palId << 12)
        | ((sprite->frameFlags & SPRITE_FLAG_MASK_PRIORITY) >> 2);
    oam->all.attr0 |= (sprite->frameFlags & SPRITE_FLAG_MASK_OBJ_MODE) * 8;
    if (affine) {
        oam->all.attr0 |= 0x100;
        oam->all.attr1 |= (sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE) << 9;
        if (extent == 128) {
            oam->all.attr0 |= 0x200;
        }
    } else {
        if (sprite->frameFlags & SPRITE_FLAG_MASK_X_FLIP) {
            oam->all.attr1 |= 0x1000;
        }
        if (sprite->frameFlags & SPRITE_FLAG_MASK_Y_FLIP) {
            oam->all.attr1 |= 0x2000;
        }
    }
    if ((gMosaicReg >> 8) && (sprite->frameFlags & SPRITE_FLAG_MASK_MOSAIC)) {
        oam->all.attr0 |= 0x1000;
    }
#else
    /* Leave the fractional allocator link intact. */
    oam->split.x = x;
    oam->split.y = y;
    oam->split.affineMode = affine ? ((extent == 128) ? ST_OAM_AFFINE_DOUBLE : ST_OAM_AFFINE_NORMAL) : ST_OAM_AFFINE_OFF;
    oam->split.objMode = (sprite->frameFlags & SPRITE_FLAG_MASK_OBJ_MODE) >> SPRITE_FLAG_SHIFT_OBJ_MODE;
    oam->split.mosaic = (gMosaicReg >> 8) && (sprite->frameFlags & SPRITE_FLAG_MASK_MOSAIC);
    oam->split.bpp = ST_OAM_4BPP;
    oam->split.shape = ST_OAM_SQUARE;
    oam->split.size = ST_OAM_SIZE_3;
    oam->split.matrixNum = affine ? (sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE)
        : ((sprite->frameFlags & SPRITE_FLAG_MASK_X_FLIP) ? ST_OAM_HFLIP : 0)
            | ((sprite->frameFlags & SPRITE_FLAG_MASK_Y_FLIP) ? ST_OAM_VFLIP : 0);
    oam->split.priority = (sprite->frameFlags & SPRITE_FLAG_MASK_PRIORITY) >> SPRITE_FLAG_SHIFT_PRIORITY;
    oam->split.paletteNum = sprite->palId;
    oam->split.tileNum = GET_TILE_NUM(sprite->graphics.dest);
#endif
}

bool32 OcPlayerDraw(Player *player, PlayerSpriteInfo *body)
{
    Sprite *sprite;
    SpriteTransform transform;
    u32 elapsed;
    u32 distanceX;
    u32 distanceY;
    u32 distance;
    u8 frame;

    if (!OcPlayerIsSelected(player)) {
        return FALSE;
    }
    if (body->s.dimensions == (void *)-1) {
        return TRUE;
    }
    if (sOcPlayer.owner != player || sOcPlayer.oc != gSelectedOc) {
        OcPlayerInit(player);
    }
    if (sOcPlayer.owner != player) {
        return FALSE;
    }
    elapsed = gStageTime - sOcPlayer.lastStageTime;
    sOcPlayer.lastStageTime = gStageTime;
    /* Avoid a frame-time discontinuity on checkpoint or stage restarts. */
    if (elapsed > 8) {
        elapsed = 1;
    }
    if (player->charState != sOcPlayer.lastState) {
        sOcPlayer.lastState = player->charState;
        sOcPlayer.stateAge = 0;
    } else {
        sOcPlayer.stateAge += elapsed;
    }
    /* Native clear A/B/C transitions must not restart the OC's celebration. */
    if (OcPlayerIsVictoryState(player->charState)) {
        if (sOcPlayer.victoryActive) {
            sOcPlayer.victoryAge += elapsed;
        } else {
            sOcPlayer.victoryAge = 0;
            sOcPlayer.victoryActive = TRUE;
        }
    } else {
        sOcPlayer.victoryAge = 0;
        sOcPlayer.victoryActive = FALSE;
    }
    distanceX = ABS(player->qWorldX - sOcPlayer.lastWorldX);
    distanceY = ABS(player->qWorldY - sOcPlayer.lastWorldY);
    sOcPlayer.lastWorldX = player->qWorldX;
    sOcPlayer.lastWorldY = player->qWorldY;
    /* Advance a gait from actual travel, so a blocked character and a pause
     * cannot run in place. max + 3/8 min approximates distance on slopes
     * without making diagonal ground animation run sqrt(2) times too fast. */
    distance = MAX(distanceX, distanceY) + ((MIN(distanceX, distanceY) * 3) >> 3);
    if (elapsed && !(player->moveState & MOVESTATE_IN_AIR) && !(sOcPlayer.lastMoveState & MOVESTATE_IN_AIR)
        && distance < Q(64) && ABS(player->qSpeedGround) >= Q(0.125) && !OcAbilityActive(player)
        && !(player->moveState & (MOVESTATE_DEAD | MOVESTATE_IGNORE_INPUT))) {
        bool32 revised = OcPlayerUsesRevisedRun();
        /* Eight poses still cover the same 96px stride as the legacy twelve.
         * At high velocity hold each revised pose for at least two ticks,
         * rather than flickering through 45 poses per second. */
        u32 stride = revised ? Q(12) : OC_RUN_PHASE_DISTANCE;
        u32 limit = revised ? Q(0.5) : Q(0.75);
        u32 count = revised ? OC_REVISED_RUN_FRAME_COUNT : OC_RUN_PHASE_COUNT;
        u32 advance = MIN((distance << 8) / stride, limit * elapsed);
        sOcPlayer.runPhase = (sOcPlayer.runPhase + advance) % (count * Q(1));
    }
    sOcPlayer.lastMoveState = player->moveState;
    frame = OcPlayerFrame(player);

    sprite = &sOcPlayer.sprite;
    sprite->oamFlags = body->s.oamFlags;
    sprite->frameFlags = body->s.frameFlags;
    sprite->x = body->s.x;
    sprite->y = body->s.y;
    /* All human poses use the same feet anchor, including upright jumps and
     * attacks. The native body still owns collision and sloped-ground motion. */
    sOcPlayer.dimensions.offsetY = OC_FRAME_PIVOT_Y - player->spriteOffsetY;
    if (frame != sOcPlayer.frame) {
        sOcPlayer.frame = frame;
        if (frame < OC_FRAME_COUNT) {
            sprite->graphics.src = gOcFrameTiles[sOcPlayer.oc][frame];
        } else if (frame < OC_REVISED_RUN_FIRST) {
            sprite->graphics.src = gOcActionFrameTiles[sOcPlayer.oc][frame - OC_FRAME_COUNT];
        } else if (frame < OC_REVISED_ATTACK_FIRST) {
            sprite->graphics.src = gOcRevisedRunTiles[OcPlayerRevisedIndex()][frame - OC_REVISED_RUN_FIRST];
        } else {
            sprite->graphics.src = gOcRevisedAttackTiles[OcPlayerRevisedIndex()][frame - OC_REVISED_ATTACK_FIRST];
        }
        ADD_TO_GRAPHICS_QUEUE(&sprite->graphics);
    }
    OcPlayerPreparePalette(player, body);
    transform = body->transform;

    /* Atlas poses face right; native Sonic poses face left. Invert the native
     * horizontal transform exactly once, including on sloped/inverted ground. */
    if (sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE_ENABLE) {
        transform.qScaleX = -transform.qScaleX;
        sprite->x = transform.x;
        sprite->y = transform.y;
        sprite->frameFlags |= SPRITE_FLAG_MASK_ROT_SCALE_DOUBLE_SIZE;
        TransformSprite(sprite, &transform);
    } else {
        sprite->frameFlags ^= SPRITE_FLAG_MASK_X_FLIP;
    }
    OcPlayerDisplaySprite(sprite);
    return TRUE;
}

void OcSpecialPlayerInit(void) { sOcSpecialRunPhase = 0; }

void OcSpecialPlayerTick(s32 speed)
{
    /* The special stage uses its own velocity units and four rear run poses. */
    if (speed != 0) {
        sOcSpecialRunPhase += MAX(ABS(speed) >> 7, 24);
    }
}

bool32 OcSpecialPlayerDraw(Sprite *nativeBody, u16 state, u16 input)
{
    Sprite sprite;
    SpriteOffset dimensions;
    u8 frame;

    if (gSelectedOc < 0 || gSelectedOc >= OC_CHARACTER_COUNT) {
        return FALSE;
    }
    if (nativeBody->dimensions == (void *)-1) {
        return TRUE;
    }
    if (state == 6 || state == 10 || state == 14) {
        frame = 7;
    } else if (state == 4 || state == 5 || state == 7 || state == 9 || state == 12 || state == 15) {
        frame = 6;
    } else if (input & DPAD_LEFT) {
        frame = 4;
    } else if (input & DPAD_RIGHT) {
        frame = 5;
    } else if (state == 0 || state == 13 || state == 16) {
        frame = 0;
    } else {
        frame = (sOcSpecialRunPhase >> 8) & 3;
    }
    sprite = *nativeBody;
    memset(&dimensions, 0, sizeof(dimensions));
    dimensions.numSubframes = 1;
    dimensions.width = 64;
    dimensions.height = 64;
    dimensions.offsetX = 32;
    dimensions.offsetY = 48;
    sprite.dimensions = &dimensions;
    sprite.frameFlags &= ~(SPRITE_FLAG_MASK_X_FLIP | SPRITE_FLAG_MASK_Y_FLIP);
    sprite.graphics.src = gOcSpecialFrameTiles[gSelectedOc][frame];
    sprite.graphics.size = 64 * TILE_SIZE_4BPP;
    /* The native script still updates before this draw, so copy even if the
     * rear pose did not change: it can have queued new body tiles this frame. */
    /* PORTABLE copies GraphicsData into its persistent queue buffer here. */
    ADD_TO_GRAPHICS_QUEUE(&sprite.graphics);
    memcpy(&gObjPalette[0], gOcPalettes[gSelectedOc], 16 * sizeof(u16));
    gFlags |= FLAGS_UPDATE_SPRITE_PALETTES;
    OcPlayerDisplaySprite(&sprite);
    return TRUE;
}
#endif
