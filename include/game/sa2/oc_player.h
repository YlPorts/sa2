#ifndef GUARD_SA2_OC_PLAYER_H
#define GUARD_SA2_OC_PLAYER_H

#include "game/shared/stage/player.h"

/* The original player animation continues to own movement and hitboxes. */
void OcPlayerInit(Player *player);
void OcPlayerRelease(Player *player);
bool32 OcPlayerDraw(Player *player, PlayerSpriteInfo *body);

/* Special-stage native player storage reserves the atlas VRAM up front. */
void OcSpecialPlayerInit(void);
void OcSpecialPlayerTick(s32 speed);
bool32 OcSpecialPlayerDraw(Sprite *nativeBody, u16 state, u16 input);

#endif
