#include "global.h"

#if PORTABLE && (GAME == GAME_SA2)
#include <string.h>
#include "flags.h"
#include "sprite.h"
#include "task.h"
#include "data/sa2/oc_sprite_data.h"
#include "game/globals.h"
#include "game/sa2/oc_characters.h"
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
    u16 stateAge;
    u16 rollAngle;
    s8 lastState;
    s8 oc;
    u8 frame;
} OcPlayerRenderer;

static OcPlayerRenderer sOcPlayer;
static u32 sOcSpecialRunPhase;

static bool32 OcPlayerIsSelected(Player *player)
{
    return player == &gPlayer && player->playerID == PLAYER_1 && player->character == CHARACTER_SONIC
        && gSelectedOc >= 0 && gSelectedOc < OC_CHARACTER_COUNT && IS_SINGLE_PLAYER
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

static u8 OcPlayerFrame(Player *player, bool32 *rolling)
{
    u16 age = sOcPlayer.stateAge;
    u8 state = player->charState;
    *rolling = FALSE;

    if ((player->moveState & MOVESTATE_DEAD) || state == CHARSTATE_DEAD) {
        return 20;
    }
    switch (state) {
        case CHARSTATE_HIT_AIR:
        case CHARSTATE_HIT_STUNNED:
            return 18 + ((age / 8) & 1);
        case CHARSTATE_SPIN_DASH:
        case CHARSTATE_SPIN_ATTACK:
        case CHARSTATE_CURLED_IN_AIR:
        case CHARSTATE_IN_WHIRLWIND:
        case CHARSTATE_WINDUP_STICK_UPWARDS:
        case CHARSTATE_WINDUP_STICK_DOWNWARDS:
        case CHARSTATE_WINDUP_STICK_SINGLE_TURN_UP:
        case CHARSTATE_WINDUP_STICK_SINGLE_TURN_DOWN:
            *rolling = TRUE;
            return 30 + ((sOcPlayer.rollAngle >> 7) & 1);
        case CHARSTATE_JUMP_1:
        case CHARSTATE_JUMP_2:
        case CHARSTATE_GRINDING_SONIC_AMY_JUMP_OFF:
            if (age < 4 && player->variant == 0) {
                return 10;
            }
            if (player->variant == 1 && (player->moveState & MOVESTATE_SPIN_ATTACK)) {
                *rolling = TRUE;
                return 12;
            }
            return player->qSpeedAirY < -Q(1) ? 11 : 13;
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
            return ((age / 20) & 1) ? 28 : 27;
        case CHARSTATE_BOOSTLESS_ATTACK:
        case CHARSTATE_AIR_ATTACK:
        case CHARSTATE_BOOST_ATTACK:
        case CHARSTATE_SOME_ATTACK:
        case CHARSTATE_SOME_OTHER_ATTACK:
        case CHARSTATE_SONIC_FORWARD_THRUST:
        case CHARSTATE_TRICK_FORWARD:
        case CHARSTATE_TRICK_BACKWARD:
            return 29;
        case CHARSTATE_TRICK_DOWN:
            *rolling = TRUE;
            return 30 + ((sOcPlayer.rollAngle >> 7) & 1);
        case CHARSTATE_TRICK_UP:
            return 11;
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
            return 3 + ((sOcPlayer.runPhase >> 8) % 6);
        case CHARSTATE_FALLING_VULNERABLE_A:
        case CHARSTATE_FALLING_VULNERABLE_B:
        case CHARSTATE_SPRING_MUSIC_PLANT:
        case CHARSTATE_SPRING_B:
        case CHARSTATE_SPRING_C:
        case CHARSTATE_RAMP_AND_DASHRING:
        case CHARSTATE_NOTE_BLOCK:
        case CHARSTATE_FLUTE_EXHAUST:
        case CHARSTATE_LAUNCHER_IN_AIR:
            return player->qSpeedAirY < -Q(1) ? 11 : 13;
        default:
            if (player->moveState & MOVESTATE_IN_AIR) {
                return player->qSpeedAirY < -Q(1) ? 11 : 13;
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

static void OcPlayerLoadPalette(void)
{
    const u16 *palette = gOcPalettes[sOcPlayer.oc];
    /* Stage intro blends this same bank to black; let its controller own it
     * until it finishes, so skipping/loading retains the normal fade. */
    if (!(gStageFlags & STAGE_FLAG__100)) {
        memcpy(&gObjPalette[0], palette, 16 * sizeof(u16));
        gFlags |= FLAGS_UPDATE_SPRITE_PALETTES;
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
    u32 speed;
    bool32 rolling;
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
    speed = ABS(player->qSpeedGround);
    if (player->moveState & MOVESTATE_IN_AIR) {
        speed = ABS(player->qSpeedAirX);
    }
    sOcPlayer.runPhase += MAX(speed >> 5, 24) * elapsed;
    sOcPlayer.rollAngle += MAX(speed >> 3, 48) * elapsed;
    frame = OcPlayerFrame(player, &rolling);

    sprite = &sOcPlayer.sprite;
    sprite->oamFlags = body->s.oamFlags;
    sprite->frameFlags = body->s.frameFlags;
    sprite->x = body->s.x;
    sprite->y = body->s.y;
    /* Rolled poses pivot around their torso, rather than circling the feet
     * anchor as the affine angle changes. The jump tuck is centered higher. */
    sOcPlayer.dimensions.offsetY = rolling ? ((frame == 12) ? 29 : 32) : 48 - player->spriteOffsetY;
    if (frame != sOcPlayer.frame) {
        sOcPlayer.frame = frame;
        sprite->graphics.src = gOcFrameTiles[sOcPlayer.oc][frame];
        ADD_TO_GRAPHICS_QUEUE(&sprite->graphics);
    }
    OcPlayerLoadPalette();
    transform = body->transform;

    /* Atlas poses face right; native Sonic poses face left. Invert the native
     * horizontal transform exactly once, including on sloped/inverted ground. */
    if (sprite->frameFlags & SPRITE_FLAG_MASK_ROT_SCALE_ENABLE) {
        transform.qScaleX = -transform.qScaleX;
        sprite->x = transform.x;
        sprite->y = transform.y;
        sprite->frameFlags |= SPRITE_FLAG_MASK_ROT_SCALE_DOUBLE_SIZE;
        TransformSprite(sprite, &transform);
    } else if (rolling) {
        sprite->frameFlags &= ~(SPRITE_FLAG_MASK_X_FLIP | SPRITE_FLAG_MASK_Y_FLIP | SPRITE_FLAG_MASK_ROT_SCALE);
        sprite->frameFlags |= SPRITE_FLAG_MASK_ROT_SCALE_ENABLE | SPRITE_FLAG_MASK_ROT_SCALE_DOUBLE_SIZE | player->playerID;
        transform.x = body->s.x;
        transform.y = body->s.y;
        transform.rotation = sOcPlayer.rollAngle & 0x3FF;
        transform.qScaleX = (player->moveState & MOVESTATE_FACING_LEFT) ? -Q(1) : Q(1);
        transform.qScaleY = GRAVITY_IS_INVERTED ? -Q(1) : Q(1);
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
