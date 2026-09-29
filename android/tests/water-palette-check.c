#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "task.h"
#include "game/globals.h"
#include "game/shared/stage/player.h"
#include "game/shared/stage/water_effects.h"
#include "data/sa2/sprite_data.h"
#include "game/sa2/stage/boost_effect.h"

// Actual engine inputs, deliberately two-byte aligned but not word aligned.
// Assembly prevents the host compiler from silently over-aligning the arrays.
__asm__(".pushsection .data.palette_alignment_test,\"aw\"\n"
        ".balign 4\n.short 0\n.globl gSpritePalettes\ngSpritePalettes:\n"
        ".set test_palette_color, 0\n.rept 256\n"
        ".short (test_palette_color * 17 + 3) & 0x7fff\n"
        ".set test_palette_color, test_palette_color + 1\n.endr\n"
        ".balign 4\n.short 0\n.globl gBgPalette\ngBgPalette:\n.space 512\n.popsection\n");

Player gPlayer;
u8 gGameMode;
u8 gMultiplayerConnections;
s8 gMultiplayerCharacters[4];
const AnimId sCharacterPalettesBoostEffect[NUM_CHARACTERS] = { 0 };
static const ACmd paletteCommand = { .pal = { .palId = 0 } };
static const ACmd *paletteCommands[] = { &paletteCommand };
const ACmd **const gAnimations[NUM_SPRITE_ANIMATIONS] = {
    [0 ... NUM_SPRITE_ANIMATIONS - 1] = paletteCommands,
};

int main(void)
{
    _Alignas(8) unsigned char storage[sizeof(WaterData) + 8];
    Task task = { 0 };
    assert(((uintptr_t)gSpritePalettes & 3) == 2);
    assert(((uintptr_t)gBgPalette & 3) == 2);
    for (unsigned i = 0; i < 256; ++i) gBgPalette[i] = (i * 29 + 7) & 0x7fff;
    for (unsigned offset = 0; offset <= 2; offset += 2) {
        WaterData *water = (WaterData *)(storage + offset);
        task.data = water;
        gWater.t = &task;
        gWater.blendColors = 0x42104210;
        for (unsigned mode = 0; mode < 2; ++mode) {
            gGameMode = mode ? GAME_MODE_MULTI_PLAYER : GAME_MODE_SINGLE_PLAYER;
            gMultiplayerConnections = 0x0f;
            for (unsigned character = 0; character < NUM_CHARACTERS; ++character) {
                memset(storage, 0xa5, sizeof(storage));
                gPlayer.character = character;
                for (unsigned i = 0; i < 4; ++i) gMultiplayerCharacters[i] = (character + i) % NUM_CHARACTERS;
                InitWaterPalettes();
                for (unsigned palette = 0; palette < (mode ? 4 : 2); ++palette)
                    assert(memcmp(water->pal[palette], gSpritePalettes[0], 32) == 0);
                if (!mode) {
                    for (unsigned palette = 2; palette < 4; ++palette)
                        for (unsigned color = 0; color < 16; ++color) assert(water->pal[palette][color] == 0xa5a5);
                }
                assert(memcmp(water->pal[4], gSpritePalettes[4], 12 * 32) == 0);
                for (unsigned i = 0; i < 256; ++i) {
                    unsigned color = gBgPalette[i];
                    unsigned expected = ((color & 0x7bde) + (((color & 0x739c) + (0x4210 & 0x739c)) >> 1)) >> 1;
                    assert(water->pal[16 + i / 16][i % 16] == expected);
                }
                for (unsigned i = 0; i < offset; ++i) assert(storage[i] == 0xa5);
                for (unsigned i = offset + sizeof(WaterData); i < sizeof(storage); ++i) assert(storage[i] == 0xa5);
            }
        }
    }
    puts("Passed: actual InitWaterPalettes with halfword-only aligned source, background and task data; five characters, single/multiplayer, palette bounds and colors");
    return 0;
}
