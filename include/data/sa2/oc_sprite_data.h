#ifndef GUARD_DATA_SA2_OC_SPRITE_DATA_H
#define GUARD_DATA_SA2_OC_SPRITE_DATA_H

#include "global.h"

/* Character order: Elizabeth, Jude, Kiro, Yuliana, Kura. */
#define OC_CHARACTER_COUNT 5
#define OC_FRAME_COUNT 32
#define OC_SPECIAL_FRAME_COUNT 8
#define OC_ACTION_FRAME_COUNT 24
#define OC_FRAME_TILE_BYTES 2048
#define OC_FRAME_WIDTH 64
#define OC_FRAME_HEIGHT 64
#define OC_FRAME_PIVOT_X 32
#define OC_FRAME_PIVOT_Y 48

extern const u8 gOcFrameTiles[OC_CHARACTER_COUNT][OC_FRAME_COUNT][OC_FRAME_TILE_BYTES];
extern const u16 gOcPalettes[OC_CHARACTER_COUNT][16];
extern const u8 gOcSpecialFrameTiles[OC_CHARACTER_COUNT][OC_SPECIAL_FRAME_COUNT][OC_FRAME_TILE_BYTES];
extern const u8 gOcActionFrameTiles[OC_CHARACTER_COUNT][OC_ACTION_FRAME_COUNT][OC_FRAME_TILE_BYTES];

#endif
