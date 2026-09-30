#include "global.h"

#if PORTABLE && (GAME == GAME_SA2)
#include <string.h>
#include "sprite.h"
#include "lib/m4a/m4a.h"
#include "game/globals.h"
#include "game/sa2/oc_characters.h"
#include "game/sa2/oc_abilities.h"
#include "data/sa2/oc_sprite_data.h"
#include "game/sa2/stage/player_controls.h"
#include "game/shared/stage/camera.h"
#include "game/shared/stage/terrain_collision.h"
#include "constants/sa2/char_states.h"
#include "constants/sa2/songs.h"

#define OC_PROJECTILE_COUNT 4
#define OC_MELEE_TARGET_COUNT 8

typedef struct {
    s32 qX, qPreviousX, qY;
    s16 qSpeedX;
    u8 life;
} OcProjectile;

typedef struct {
    Sprite *sprite;
} OcMeleeTarget;

typedef struct {
    Player *owner;
    GraphicsData projectileGraphics;
    u8 projectileTiles[TILE_SIZE_4BPP] ALIGNED(4);
    OcProjectile projectiles[OC_PROJECTILE_COUNT];
    OcMeleeTarget targets[OC_MELEE_TARGET_COUNT];
    OcAbilityStatus status;
    s8 oc;
    bool8 facingLeft;
    u8 targetCount;
} OcAbilities;

static OcAbilities sAbilities;

/* Streak mask; each nonzero pixel is recolored with the OC's brightest
 * neutral color, keeping the existing palette bank's fade/water treatment. */
static const u8 ALIGNED(4) sBulletTiles[TILE_SIZE_4BPP] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0x20, 0x22, 0x22, 0x02,
    0x20, 0x22, 0x22, 0x02, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

extern void Player_HandlePhysics(Player *player);
extern void Player_Jumping(Player *player);
extern void Player_Idle(Player *player);
extern void Player_TouchGround(Player *player);
extern void Player_Rolling(Player *player);
extern void Player_Spindash(Player *player);

bool32 OcAbilitiesOwnPlayer(Player *player)
{
    return player == &gPlayer && player->playerID == PLAYER_1 && player->spriteInfoBody != NULL
        && OcIdentityIsActive() && player->character == OcBaseCharacter(gSelectedOc);
}

void OcAbilitiesRelease(Player *player)
{
    if (sAbilities.owner == player) {
        if (sAbilities.projectileGraphics.dest != NULL) {
            VramFree(sAbilities.projectileGraphics.dest);
        }
        memset(&sAbilities, 0, sizeof(sAbilities));
    }
}

void OcAbilitiesInit(Player *player)
{
    if (sAbilities.owner != NULL) {
        OcAbilitiesRelease(sAbilities.owner);
    }
    if (OcAbilitiesOwnPlayer(player)) {
        sAbilities.owner = player;
        sAbilities.oc = gSelectedOc;
        if (gSelectedOc == OC_YULIANA) {
            void *tiles = VramMalloc(1);
            if (tiles != ewram_end) {
                u8 color = 1, index;
                s32 best = -1;
                for (index = 1; index < PALETTE_LEN_4BPP; index++) {
                    u16 rgb = gOcPalettes[gSelectedOc][index];
                    s32 red = rgb & 31, green = (rgb >> 5) & 31, blue = (rgb >> 10) & 31;
                    s32 score = red + green + blue - (MAX(red, MAX(green, blue)) - MIN(red, MIN(green, blue)));
                    if (score > best) {
                        best = score;
                        color = index;
                    }
                }
                for (index = 0; index < sizeof(sBulletTiles); index++) {
                    sAbilities.projectileTiles[index] = (sBulletTiles[index] & 15 ? color : 0)
                        | (sBulletTiles[index] & 0xF0 ? color << 4 : 0);
                }
                sAbilities.projectileGraphics.src = sAbilities.projectileTiles;
                sAbilities.projectileGraphics.dest = tiles;
                sAbilities.projectileGraphics.size = sizeof(sBulletTiles);
                ADD_TO_GRAPHICS_QUEUE(&sAbilities.projectileGraphics);
            }
        }
    }
}

bool32 OcAbilityActive(Player *player)
{
    return OcAbilitiesOwnPlayer(player) && sAbilities.owner == player && sAbilities.status.attacking;
}

u8 OcAbilityKind(Player *player) { return OcAbilityActive(player) ? sAbilities.status.kind : OC_ABILITY_NONE; }
u16 OcAbilityAge(Player *player) { return OcAbilityActive(player) ? sAbilities.status.age : 0; }
u16 OcAbilityCooldown(Player *player)
{
    return sAbilities.owner == player && OcAbilitiesOwnPlayer(player) ? sAbilities.status.cooldown : 0;
}

u8 OcAbilityFramePhase(Player *player)
{
    OcAbilityStatus *status = &sAbilities.status;
    if (!OcAbilityActive(player) || status->age < status->activeStart) {
        return 0;
    }
    if (status->age > status->activeEnd) {
        return 3;
    }
    return status->age <= (status->activeStart + status->activeEnd) / 2 ? 1 : 2;
}

bool32 OcAbilityDodging(Player *player)
{
    return OcAbilityActive(player) && sAbilities.status.kind == OC_ABILITY_GLITCH && sAbilities.status.damaging;
}

void OcAbilityGetStatus(Player *player, OcAbilityStatus *status)
{
    memset(status, 0, sizeof(*status));
    if (sAbilities.owner == player && OcAbilitiesOwnPlayer(player)) {
        *status = sAbilities.status;
    }
}

void OcAbilitiesCancel(Player *player)
{
    if (sAbilities.owner == player) {
        sAbilities.status.attacking = FALSE;
        sAbilities.status.damaging = FALSE;
        sAbilities.status.age = 0;
        sAbilities.targetCount = 0;
    }
}

static void OcSuppressSpin(Player *player)
{
    player->moveState &= ~(MOVESTATE_SPIN_ATTACK | MOVESTATE_SPINDASH | MOVESTATE_FLIP_WITH_MOVE_DIR);
    /* Compact jump/pipe bounds belong to terrain traversal, independently
     * of the OC's upright art and its strictly timed weapon hitbox. */
    /* Interactables can request a roll directly, bypassing normal input. */
    if (!(player->moveState & (MOVESTATE_IN_SCRIPTED | MOVESTATE_IA_OVERRIDE))
        && (player->callback == Player_Rolling || player->callback == Player_Spindash)) {
        if (player->moveState & MOVESTATE_IN_AIR) {
            player->callback = Player_Jumping;
            player->charState = CHARSTATE_FALLING_VULNERABLE_A;
        } else {
            PLAYERFN_CHANGE_SHIFT_OFFSETS(player, 6, 14);
            player->callback = Player_Idle;
            player->charState = player->qSpeedGround ? CHARSTATE_WALK_A : CHARSTATE_IDLE;
        }
    }
}

static void OcBeginAbility(Player *player)
{
    OcAbilityStatus *status = &sAbilities.status;
    u16 sound;
    status->age = 0;
    status->attacking = TRUE;
    status->damaging = FALSE;
    status->attacksStarted++;
    sAbilities.facingLeft = (player->moveState & MOVESTATE_FACING_LEFT) != 0;
    sAbilities.targetCount = 0;
    switch (gSelectedOc) {
        case OC_KIRO:
            status->kind = OC_ABILITY_KATANA;
            status->activeStart = 5;
            status->activeEnd = 11;
            status->duration = 22;
            status->cooldown = 30;
            sound = SE_AMY_HAMMER_SWIRL;
            break;
        case OC_YULIANA:
            status->kind = OC_ABILITY_GUN;
            status->activeStart = 3;
            status->activeEnd = 5;
            status->duration = 14;
            status->cooldown = 22;
            sound = 0; /* The gun report belongs to the actual shot frame. */
            break;
        case OC_JUDE:
            status->kind = OC_ABILITY_PAN;
            status->activeStart = 6;
            status->activeEnd = 13;
            status->duration = 24;
            status->cooldown = 32;
            sound = (player->moveState & MOVESTATE_IN_AIR) ? SE_AMY_SUPER_HAMMER_ATTACK : SE_AMY_GROUND_HAMMER;
            break;
        case OC_ELIZABETH:
            status->kind = OC_ABILITY_GLITCH;
            status->activeStart = 3;
            status->activeEnd = 8;
            status->duration = 16;
            status->cooldown = 50;
            sound = SE_SONIC_INSTA_SHIELD;
            break;
        default:
            status->kind = OC_ABILITY_DASH;
            status->activeStart = 2;
            status->activeEnd = 8;
            status->duration = 18;
            status->cooldown = 36;
            sound = SE_DASH_RING;
            break;
    }
    OcSuppressSpin(player);
    player->charState = CHARSTATE_BOOSTLESS_ATTACK;
    player->prevCharState = CHARSTATE_INVALID;
    player->moveState &= ~MOVESTATE_20;
    if (sound != 0) {
        m4aSongNumStart(sound);
    }
}

static void OcFireProjectile(Player *player)
{
    u8 index;
    if (sAbilities.projectileGraphics.dest == NULL) {
        return;
    }
    for (index = 0; index < OC_PROJECTILE_COUNT; index++) {
        OcProjectile *shot = &sAbilities.projectiles[index];
        if (shot->life == 0) {
            shot->qSpeedX = sAbilities.facingLeft ? -Q(8) : Q(8);
            shot->qX = player->qWorldX + (sAbilities.facingLeft ? -Q(17) : Q(17));
            shot->qPreviousX = shot->qX;
            shot->qY = player->qWorldY + (GRAVITY_IS_INVERTED ? Q(6) : -Q(6));
            shot->life = 40;
            sAbilities.status.shotsFired++;
            sAbilities.status.projectiles++;
            m4aSongNumStart(SE_PROJECTILE_IMPACT);
            return;
        }
    }
}

static void OcUpdateProjectiles(Player *player)
{
    u8 index;
    sAbilities.status.projectiles = 0;
    for (index = 0; index < OC_PROJECTILE_COUNT; index++) {
        OcProjectile *shot = &sAbilities.projectiles[index];
        if (shot->life != 0) {
            s8 direction = shot->qSpeedX < 0 ? -8 : 8;
            shot->qPreviousX = shot->qX;
            shot->qX += shot->qSpeedX;
            shot->life--;
            /* Stop at stage walls as well as at enemies; no shots through walls. */
            if (SA2_LABEL(sub_801E4E4)(I(shot->qX), I(shot->qY), player->layer, direction, NULL, SA2_LABEL(sub_801ED24)) <= 0) {
                shot->life = 0;
            }
            if (shot->life != 0) {
                sAbilities.status.projectiles++;
            }
        }
    }
}

bool32 OcAbilitiesUpdate(Player *player)
{
    OcAbilityStatus *status = &sAbilities.status;
    bool32 blocked;
    bool32 aerialPan = FALSE;
    if (!OcAbilitiesOwnPlayer(player)) {
        return FALSE;
    }
    if (sAbilities.owner != player || sAbilities.oc != gSelectedOc) {
        OcAbilitiesInit(player);
    }
    if (!(player->moveState & MOVESTATE_IN_AIR)) {
        status->aerialPanUsed = FALSE;
    }
    OcUpdateProjectiles(player);
    if (status->cooldown != 0) {
        status->cooldown--;
    }
    blocked = (player->moveState & (MOVESTATE_DEAD | MOVESTATE_IGNORE_INPUT | MOVESTATE_IA_OVERRIDE | MOVESTATE_IN_SCRIPTED
                                   | MOVESTATE_GOAL_REACHED | MOVESTATE_100000)) != 0
        || player->charState == CHARSTATE_HIT_AIR || player->charState == CHARSTATE_HIT_STUNNED
        || player->charState == CHARSTATE_GRINDING || player->charState == CHARSTATE_HANGING
        || player->charState == CHARSTATE_GRABBING_HANDLE_A || player->charState == CHARSTATE_GRABBING_HANDLE_B
        || (player->charState >= CHARSTATE_WINDUP_STICK_UPWARDS && player->charState <= CHARSTATE_WINDUP_STICK_SINGLE_TURN_DOWN)
        || (player->charState >= CHARSTATE_WALLRUN_INIT && player->charState <= CHARSTATE_WALLRUN_ON_WALL)
        || player->charState == CHARSTATE_LAUNCHER_IN_CART || player->charState == CHARSTATE_POLE;
    if (blocked) {
        OcAbilitiesCancel(player);
        if (player->moveState & (MOVESTATE_DEAD | MOVESTATE_IN_SCRIPTED | MOVESTATE_IA_OVERRIDE | MOVESTATE_GOAL_REACHED)) {
            memset(sAbilities.projectiles, 0, sizeof(sAbilities.projectiles));
            status->projectiles = 0;
        }
        return FALSE;
    }
    OcSuppressSpin(player);
    /* The age exposed to collision and rendering is the age processed now,
     * including both endpoints of the active window. */
    if (status->attacking && ++status->age >= status->duration) {
        OcAbilitiesCancel(player);
        if (player->moveState & MOVESTATE_IN_AIR) {
            player->callback = Player_Jumping;
            player->charState = CHARSTATE_FALLING_VULNERABLE_A;
        } else {
            player->callback = Player_Idle;
            player->charState = player->qSpeedGround ? CHARSTATE_WALK_A : CHARSTATE_IDLE;
        }
    }
    if (gSelectedOc == OC_JUDE && (player->moveState & MOVESTATE_IN_AIR) && !status->aerialPanUsed
        && (player->frameInput & gPlayerControls.jump)) {
        /* Amy's native second-A gesture preserves velocity and gravity. It
         * gains a timed pan hitbox, rather than a new jump or heart effect.
         * An attempt during cooldown is still this flight's single attempt. */
        status->aerialPanUsed = TRUE;
        aerialPan = TRUE;
    }
    if (!status->attacking && status->cooldown == 0 && ((player->frameInput & gPlayerControls.attack) || aerialPan)) {
        OcBeginAbility(player);
    }
    if (!status->attacking) {
        return FALSE;
    }
    status->damaging = status->age >= status->activeStart && status->age <= status->activeEnd;
    if (status->kind == OC_ABILITY_GUN && status->age == status->activeStart) {
        OcFireProjectile(player);
    }
    player->moveState &= ~MOVESTATE_FACING_LEFT;
    if (sAbilities.facingLeft) {
        player->moveState |= MOVESTATE_FACING_LEFT;
    }
    if (status->kind == OC_ABILITY_DASH && status->damaging) {
        player->qSpeedGround = sAbilities.facingLeft ? -Q(7) : Q(7);
        player->qSpeedAirX = player->qSpeedGround;
    } else if (!(player->moveState & MOVESTATE_IN_AIR)) {
        if (status->kind == OC_ABILITY_PAN) {
            /* Amy's grounded attack slows by the native 0.375px/frame. */
            s32 speed = player->qSpeedGround;
            player->qSpeedGround = speed > 0 ? MAX(0, speed - Q(0.375)) : MIN(0, speed + Q(0.375));
        } else {
            player->qSpeedGround = (player->qSpeedGround * 7) / 8;
        }
    }
    if (player->moveState & MOVESTATE_IN_AIR) {
        Player_HandlePhysicsWithAirInput(player);
    } else {
        Player_HandlePhysics(player);
    }
    if (!(player->moveState & MOVESTATE_IN_AIR)) {
        bool32 touchGround = player->spriteOffsetY == 9 || player->callback == Player_TouchGround
            || (player->moveState & MOVESTATE_100);
        status->aerialPanUsed = FALSE;
        /* A platform can set TouchGround while this attack still owns the
         * frame. Restore its standing bounds here instead of deferring the
         * callback until recovery ends; keep the contacted feet fixed. */
        if (player->spriteOffsetY == 9) {
            PLAYERFN_CHANGE_SHIFT_OFFSETS(player, 6, 14);
        }
        if (touchGround) {
            /* Retain TouchGround's native cleanup without integrating a
             * second movement frame while the weapon is still recovering. */
            Player_TransitionCancelFlyingAndBoost(player);
            player->callback = Player_Idle;
        }
    }
    player->charState = CHARSTATE_BOOSTLESS_ATTACK;
    return TRUE;
}

void OcAbilitiesApplyHitbox(Player *player)
{
    Sprite *body;
    Rect8 rectangle;
    if (!OcAbilitiesOwnPlayer(player)) {
        return;
    }
    if (!(player->moveState & MOVESTATE_IN_AIR)) {
        sAbilities.status.aerialPanUsed = FALSE;
    }
    OcSuppressSpin(player);
    body = &player->spriteInfoBody->s;
    body->hitboxes[1].index = HITBOX_STATE_INACTIVE;
    if (!OcAbilityActive(player) || !sAbilities.status.damaging || sAbilities.status.kind == OC_ABILITY_GUN) {
        return;
    }
    if (sAbilities.status.kind == OC_ABILITY_GLITCH) {
        rectangle = (Rect8) { -18, -18, 18, 12 };
    } else if (sAbilities.status.kind == OC_ABILITY_KATANA) {
        rectangle = (Rect8) { 5, -18, 30, 8 };
    } else if (sAbilities.status.kind == OC_ABILITY_PAN) {
        rectangle = (Rect8) { 3, -22, 24, 9 };
    } else {
        rectangle = (Rect8) { 0, -13, 20, 12 };
    }
    if (sAbilities.facingLeft) {
        s8 left = rectangle.left;
        rectangle.left = -rectangle.right;
        rectangle.right = -left;
    }
    if (GRAVITY_IS_INVERTED) {
        s8 top = rectangle.top;
        rectangle.top = -rectangle.bottom;
        rectangle.bottom = -top;
    }
    body->hitboxes[1].index = 1;
    body->hitboxes[1].b = rectangle;
}

bool32 OcAbilityHitsTarget(Sprite *target, s32 x, s32 y, s16 hitbox)
{
    Player *player = sAbilities.owner;
    u8 index;
    if (player == NULL || !OcAbilitiesOwnPlayer(player)
        || (player->moveState & (MOVESTATE_DEAD | MOVESTATE_IN_SCRIPTED | MOVESTATE_IA_OVERRIDE | MOVESTATE_GOAL_REACHED))
        || !HITBOX_IS_ACTIVE(target->hitboxes[hitbox])) {
        return FALSE;
    }
    for (index = 0; index < OC_PROJECTILE_COUNT; index++) {
        OcProjectile *shot = &sAbilities.projectiles[index];
        if (shot->life != 0) {
            s32 left = MIN(I(shot->qPreviousX), I(shot->qX)) - 3;
            s32 right = MAX(I(shot->qPreviousX), I(shot->qX)) + 3;
            s32 yShot = I(shot->qY);
            Rect8 swept = { 0, -2, right - left, 2 };
            if (HB_COLLISION(x, y, target->hitboxes[hitbox].b, left, yShot, swept)) {
                shot->life = 0;
                sAbilities.status.projectiles--;
                sAbilities.status.hitsLanded++;
                return TRUE;
            }
        }
    }
    if (OcAbilityActive(player) && sAbilities.status.damaging && sAbilities.status.kind != OC_ABILITY_GUN
        && HITBOX_IS_ACTIVE(player->spriteInfoBody->s.hitboxes[1])
        && HB_COLLISION(x, y, target->hitboxes[hitbox].b, I(player->qWorldX), I(player->qWorldY),
                        player->spriteInfoBody->s.hitboxes[1].b)) {
        for (index = 0; index < sAbilities.targetCount; index++) {
            if (sAbilities.targets[index].sprite == target) {
                return FALSE;
            }
        }
        if (sAbilities.targetCount >= OC_MELEE_TARGET_COUNT) {
            return FALSE;
        }
        sAbilities.targets[sAbilities.targetCount++].sprite = target;
        sAbilities.status.hitsLanded++;
        return TRUE;
    }
    return FALSE;
}

void OcAbilitiesDraw(Player *player)
{
    u8 index;
    if (!OcAbilitiesOwnPlayer(player) || sAbilities.owner != player || sAbilities.projectileGraphics.dest == NULL) {
        return;
    }
    for (index = 0; index < OC_PROJECTILE_COUNT; index++) {
        OcProjectile *shot = &sAbilities.projectiles[index];
        if (shot->life != 0) {
            s32 x = I(shot->qX) - gCamera.x - 4;
            s32 y = I(shot->qY) - gCamera.y - 4;
            OamData *oam;
            if (x < -8 || x > DISPLAY_WIDTH || y < -8 || y > DISPLAY_HEIGHT) {
                continue;
            }
            oam = OamMalloc(15);
            if (oam == iwram_end) {
                return;
            }
#if EXTENDED_OAM
            oam->split.x = x;
            oam->split.y = y;
            oam->split.affineMode = ST_OAM_AFFINE_OFF;
            oam->split.objMode = ST_OAM_OBJ_NORMAL;
            oam->split.mosaic = 0;
            oam->split.bpp = ST_OAM_4BPP;
            oam->split.shape = ST_OAM_SQUARE;
            oam->split.size = ST_OAM_SIZE_0;
            oam->split.matrixNum = 0;
            oam->split.priority = 2;
            oam->split.paletteNum = 0;
            oam->split.tileNum = GET_TILE_NUM(sAbilities.projectileGraphics.dest);
#else
            oam->all.attr0 = y & 0xFF;
            oam->all.attr1 = x & 0x1FF;
            oam->all.attr2 = GET_TILE_NUM(sAbilities.projectileGraphics.dest) | 0x800;
#endif
        }
    }
}
#endif
