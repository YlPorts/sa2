#ifndef GUARD_SA2_OC_ABILITIES_H
#define GUARD_SA2_OC_ABILITIES_H

#include "game/shared/stage/player.h"

enum OcAbilityType {
    OC_ABILITY_NONE,
    OC_ABILITY_KATANA,
    OC_ABILITY_GUN,
    OC_ABILITY_PAN,
    OC_ABILITY_GLITCH,
    OC_ABILITY_DASH,
};

typedef struct {
    u8 kind;
    u8 age;
    u8 activeStart;
    u8 activeEnd;
    u8 duration;
    u8 cooldown;
    u8 projectiles;
    bool8 attacking;
    bool8 damaging;
    bool8 aerialPanUsed;
    u32 attacksStarted;
    u32 shotsFired;
    u32 hitsLanded;
} OcAbilityStatus;

bool32 OcAbilitiesOwnPlayer(Player *player);
void OcAbilitiesInit(Player *player);
void OcAbilitiesRelease(Player *player);
void OcAbilitiesCancel(Player *player);
/* TRUE means the ability advanced the player's physics this frame. */
bool32 OcAbilitiesUpdate(Player *player);
void OcAbilitiesApplyHitbox(Player *player);
void OcAbilitiesDraw(Player *player);

bool32 OcAbilityActive(Player *player);
u8 OcAbilityFramePhase(Player *player);
u8 OcAbilityKind(Player *player);
u16 OcAbilityAge(Player *player);
u16 OcAbilityCooldown(Player *player);
bool32 OcAbilityDodging(Player *player);
bool32 OcAbilityHitsTarget(Sprite *target, s32 x, s32 y, s16 hitbox);
void OcAbilityGetStatus(Player *player, OcAbilityStatus *status);

#endif
