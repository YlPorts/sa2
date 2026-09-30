#include "global.h"

#if PORTABLE && (GAME == GAME_SA1)
#include "core.h"
#include "sprite.h"
#include "platform/shared/sa1_bg_sprites.h"

typedef struct {
    u8 *data;
    u32 width;
    u32 height;
    bool32 affine;
} BackgroundSpriteMap;

static BackgroundSpriteMap GetBackgroundSpriteMap(u32 layer)
{
    const u16 control = gBgCntRegs[layer];
    const u32 size = control >> 14;
    const u32 mode = gDispCnt & 7;
    BackgroundSpriteMap map;
    map.affine = (mode == DISPCNT_MODE_1 && layer == 2) || (mode == DISPCNT_MODE_2 && layer >= 2);
    map.data = BG_SCREEN_ADDR((control & BGCNT_SCREENBASE_MASK) >> 8);
    if (map.affine) {
        map.width = map.height = 16u << size;
    } else {
        // The portable software renderer and level-copy code use contiguous
        // rows for extended text maps, rather than GBA screen-block ordering.
        map.width = (size & 1) ? 64 : 32;
        map.height = (size & 2) ? 64 : 32;
    }
    return map;
}

static void WriteBackgroundSpriteTile(BackgroundSpriteMap map, s32 x, s32 y, u16 tile)
{
    if (x >= 0 && y >= 0 && (u32)x < map.width && (u32)y < map.height) {
        const u32 entrySize = map.affine ? 1 : 2;
        const u32 offset = (u32)(map.data - VRAM) + ((u32)y * map.width + (u32)x) * entrySize;
        // Affine maps may request more memory than a layer can address. Never
        // let a clipped or oversized frame write beyond the virtual VRAM.
        if (offset <= sizeof(VRAM) - entrySize) {
            if (map.affine) {
                VRAM[offset] = tile & 0xff;
            } else {
                memcpy(VRAM + offset, &tile, sizeof(tile));
            }
        }
    }
}

void Sa1_ClearBackgroundSpriteMaps(void)
{
    for (u32 layer = 0; layer < 4; layer++) {
        const u8 *rect = gBgSprites_Unknown2[layer];
        if (rect[0] != rect[2] || rect[1] != rect[3]) {
            BackgroundSpriteMap map = GetBackgroundSpriteMap(layer);
            const u32 right = rect[2] == 0xff ? map.width : MIN((u32)rect[2] + 1, map.width);
            const u32 bottom = MIN((u32)rect[3], map.height);
            for (u32 y = rect[1]; y < bottom; y++) {
                for (u32 x = rect[0]; x < right; x++) {
                    WriteBackgroundSpriteTile(map, x, y, gBgSprites_Unknown1[layer]);
                }
            }
        }
        memset(gBgSprites_Unknown2[layer], 0, sizeof(gBgSprites_Unknown2[layer]));
    }
}

void Sa1_DrawBackgroundSprites(void)
{
    for (u32 index = 0; index < gBgSpritesCount; index++) {
        const Sprite *sprite = gBgSprites[index];
        if (sprite == NULL || sprite->dimensions == NULL || sprite->dimensions == (void *)-1) continue;
        const SpriteOffset *dims = sprite->dimensions;
        const u32 layer = SPRITE_FLAG_GET(sprite, BG_ID);
        const u16 control = gBgCntRegs[layer];
        const BackgroundSpriteMap map = GetBackgroundSpriteMap(layer);
        const u32 tileSize = (control & BGCNT_256COLOR) ? TILE_SIZE_8BPP : TILE_SIZE_4BPP;
        const u8 *tiles = BG_CHAR_ADDR((control & BGCNT_CHARBASE(3)) >> 2);
        const uintptr_t destination = (uintptr_t)sprite->graphics.dest;
        if (destination < (uintptr_t)tiles || destination >= (uintptr_t)VRAM + sizeof(VRAM)) continue;
        const u32 tileBase = (destination - (uintptr_t)tiles) / tileSize;
        const OamDataShort *pieces = (const OamDataShort *)gRefSpriteTables->oamData[sprite->graphics.anim];
        const bool32 flipX = SPRITE_FLAG_GET(sprite, X_FLIP) ^ (dims->flip & 1);
        const bool32 flipY = SPRITE_FLAG_GET(sprite, Y_FLIP) ^ ((dims->flip >> 1) & 1);

        // DisplaySprite_BG applies the low four/eight position bits through
        // scroll registers. Only the remaining tile-aligned origin goes here.
        const s32 originX = (sprite->x - dims->offsetX) & ~15;
        const s32 originY = (sprite->y - dims->offsetY) & ~7;
        for (u32 part = 0; part < dims->numSubframes; part++) {
            const OamDataShort *piece = &pieces[dims->oamIndex + part];
            const u32 shape = (piece->shape << 2) | piece->size;
            if (shape >= 12) continue;
            const u32 width = gOamShapesSizes[shape][0] / 8;
            const u32 height = gOamShapesSizes[shape][1] / 8;
            s32 partX = piece->x;
            s32 partY = piece->y;
            if (flipX) partX = dims->width - width * 8 - partX;
            if (flipY) partY = dims->height - height * 8 - partY;
            const bool32 tileFlipX = flipX ^ !!(piece->matrixNum & ST_OAM_HFLIP);
            const bool32 tileFlipY = flipY ^ !!(piece->matrixNum & ST_OAM_VFLIP);
            const u16 attributes = (((piece->paletteNum + sprite->palId) & 15) << 12)
                | (tileFlipX ? TILE_MASK_X_FLIP : 0) | (tileFlipY ? TILE_MASK_Y_FLIP : 0);
            for (u32 y = 0; y < height; y++) {
                for (u32 x = 0; x < width; x++) {
                    const u32 sourceX = tileFlipX ? width - 1 - x : x;
                    const u32 sourceY = tileFlipY ? height - 1 - y : y;
                    const u16 tile = (tileBase + piece->tileNum + sourceY * width + sourceX) & TILE_MASK_INDEX;
                    WriteBackgroundSpriteTile(map, (originX + partX) / 8 + x, (originY + partY) / 8 + y,
                        map.affine ? tile : tile | attributes);
                }
            }
        }
    }
    gBgSpritesCount = 0;
}
#endif
