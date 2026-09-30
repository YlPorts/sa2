#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "core.h"
#include "sprite.h"
#include "platform/shared/sa1_bg_sprites.h"
#include "platform/shared/sa1_ui_oam.h"

u8 VRAM[VRAM_SIZE];
u16 gBgCntRegs[4];
u16 gDispCnt;
u8 gBgSprites_Unknown2[4][4];
u8 gBgSprites_Unknown1[4];
Sprite *gBgSprites[16];
u8 gBgSpritesCount;
const u8 gOamShapesSizes[12][2] = {
    {8, 8}, {16, 16}, {32, 32}, {64, 64},
    {16, 8}, {32, 8}, {32, 16}, {64, 32},
    {8, 16}, {8, 32}, {16, 32}, {32, 64},
};
static const OamDataShort pieces[] ALIGNED(2) = {
    {.shape = 0, .size = 0},
    {.shape = 0, .size = 1, .tileNum = 10, .paletteNum = 4},
};
static const u16 *oamTables[] = {(const void *)pieces};
static const SpriteTables tables = {.oamData = oamTables};
const SpriteTables *gRefSpriteTables = &tables;

static u16 ReadTile(unsigned screen, unsigned offset)
{
    u16 tile;
    memcpy(&tile, BG_SCREEN_ADDR(screen) + offset * 2, 2);
    return tile;
}

int main(void)
{
    // A full clear must affect VRAM, consume its request, and preserve both
    // the character data and the neighbouring screen block.
    memset(VRAM, 0xa5, sizeof(VRAM));
    gBgCntRegs[1] = BGCNT_CHARBASE(1) | BGCNT_SCREENBASE(25);
    gBgSprites_Unknown1[1] = 5;
    gBgSprites_Unknown2[1][2] = 255;
    gBgSprites_Unknown2[1][3] = 32;
    Sa1_ClearBackgroundSpriteMaps();
    for (unsigned i = 0; i < 1024; i++) assert(ReadTile(25, i) == 5);
    assert(BG_CHAR_ADDR(1)[0] == 0xa5 && BG_SCREEN_ADDR(26)[0] == 0xa5);
    for (unsigned i = 0; i < 16; i++) assert(((u8 *)gBgSprites_Unknown2)[i] == 0);

    // The wide level maps live above physical GBA VRAM. Use their 64-column
    // portable stride and clear just the requested rows.
    memset(VRAM, 0xa5, sizeof(VRAM));
    gBgCntRegs[1] = BGCNT_TXT512x512 | BGCNT_SCREENBASE(52);
    gBgSprites_Unknown2[1][0] = 2;
    gBgSprites_Unknown2[1][1] = 1;
    gBgSprites_Unknown2[1][2] = 3;
    gBgSprites_Unknown2[1][3] = 3;
    Sa1_ClearBackgroundSpriteMaps();
    assert(ReadTile(52, 64 + 2) == 5 && ReadTile(52, 128 + 3) == 5);
    assert(ReadTile(52, 64 + 1) == 0xa5a5 && ReadTile(52, 192 + 2) == 0xa5a5);

    const SpriteOffset dimensions = {.oamIndex = 1, .numSubframes = 1, .width = 16, .height = 16};
    Sprite sprite = {0};
    sprite.dimensions = &dimensions;
    sprite.x = 16;
    sprite.y = 8;
    sprite.palId = 3;
    sprite.frameFlags = 1u << 15; // BG1
    sprite.graphics.dest = BG_CHAR_ADDR(1) + 64;
    gBgCntRegs[1] = BGCNT_CHARBASE(1) | BGCNT_SCREENBASE(25);
    gBgSprites[0] = &sprite;
    gBgSpritesCount = 1;
    Sa1_DrawBackgroundSprites();
    assert(ReadTile(25, 32 + 2) == (0x7000 | 12));
    assert(ReadTile(25, 32 + 3) == (0x7000 | 13));
    assert(ReadTile(25, 64 + 2) == (0x7000 | 14));
    assert(ReadTile(25, 64 + 3) == (0x7000 | 15));
    assert(gBgSpritesCount == 0);

    sprite.frameFlags |= SPRITE_FLAG_MASK_X_FLIP | SPRITE_FLAG_MASK_Y_FLIP;
    gBgSpritesCount = 1;
    Sa1_DrawBackgroundSprites();
    assert(ReadTile(25, 32 + 2) == (0x7c00 | 15));
    assert(ReadTile(25, 64 + 3) == (0x7c00 | 12));

    // Mode 1's BG2 is an affine byte map; BG3 remains a text map. An
    // oversized affine layout must not escape the virtual VRAM allocation.
    gDispCnt = DISPCNT_MODE_1;
    gBgCntRegs[2] = BGCNT_CHARBASE(1) | BGCNT_SCREENBASE(8) | BGCNT_256COLOR;
    sprite.frameFlags = 2u << 15;
    gBgSpritesCount = 1;
    Sa1_DrawBackgroundSprites();
    assert(BG_SCREEN_ADDR(8)[16 + 2] == 11);
    assert(BG_SCREEN_ADDR(8)[32 + 3] == 14);
    gBgCntRegs[2] = BGCNT_AFF1024x1024 | BGCNT_SCREENBASE(60) | BGCNT_256COLOR;
    gBgSprites_Unknown2[2][2] = 255;
    gBgSprites_Unknown2[2][3] = 128;
    Sa1_ClearBackgroundSpriteMaps();
    gBgSpritesCount = 1;
    Sa1_DrawBackgroundSprites();

    OamData ui = {0};
    ui.all.attr0 = 248 | (3 << 8) | (1 << 10) | (1 << 12) | (1 << 13) | (2 << 14);
    ui.all.attr1 = 480 | (17 << 9) | (2 << 14);
    ui.all.attr2 = 123 | (2 << 10) | (7 << 12);
    ui.all.affineParam = 0x1234;
    Sa1_ConvertUiOam(&ui);
    assert(ui.split.x == -32 && ui.split.y == -8);
    assert(ui.split.affineMode == 3 && ui.split.objMode == 1);
    assert(ui.split.mosaic == 1 && ui.split.bpp == 1 && ui.split.shape == 2);
    assert(ui.split.matrixNum == 17 && ui.split.size == 2);
    assert(ui.split.tileNum == 123 && ui.split.priority == 2 && ui.split.paletteNum == 7);
    assert(ui.all.affineParam == 0x1234);
    puts("Passed: SA1 background clears, text/affine maps, palette/flips, bounds and native UI OAM");
    return 0;
}
