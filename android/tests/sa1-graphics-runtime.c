// Host-only SA1 graphics checks. Android APKs never link this harness.
// The selector sprites and level graphics are compiled from repository assets.
// Without SA_TEST_ROM, remaining ROM-only tables stay empty: these runs verify
// drawing and memory safety, not a complete playthrough with imported data.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "global.h"
#include "core.h"
#include "task.h"
#include "game/game.h"
#include "game/globals.h"
#include "game/sa1/save.h"
#include "game/sa1/ui/character_select.h"
#include "game/shared/stage/stage.h"
#include "game/shared/stage/player.h"
#include "platform/shared/rom_assets.h"
#include "platform/shared/video/gpsp_renderer.h"
#include <SDL.h>

extern uint16_t gameImage[];
extern void __real_VBlankIntrWait(void);
extern int __real_Sa1_LoadRomAssets(const char *, char *, size_t);
extern bool headless;
extern SDL_Texture *sdlTexture;
extern int __real_SDL_RenderCopy(SDL_Renderer *, SDL_Texture *, const SDL_Rect *, const SDL_Rect *);
static unsigned frames;
static unsigned gameCopies;

int __wrap_SDL_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *source, const SDL_Rect *destination)
{
    if (texture == sdlTexture) {
#ifdef __ANDROID__
        if (gGameStageTask == NULL) {
            assert(source && source->x == 0 && source->y == 0 && source->w == 240 && source->h == 160);
        } else {
            assert(source == NULL);
        }
        assert(destination && destination->w > 0 && destination->h > 0);
#endif
        gameCopies++;
    }
    return __real_SDL_RenderCopy(renderer, texture, source, destination);
}

int __wrap_Sa1_LoadRomAssets(const char *path, char *error, size_t errorSize)
{
    const char *rom = getenv("SA_TEST_ROM");
    return rom ? __real_Sa1_LoadRomAssets(rom, error, errorSize) : 1;
}

void __wrap_AgbMain(void)
{
    EngineInit();
    GameInit();
    TasksDestroyAll();
    gBackgroundsCopyQueueCursor = gBackgroundsCopyQueueIndex = 0;
    gVramGraphicsCopyCursor = gVramGraphicsCopyQueueIndex = 0;
    gGameMode = GAME_MODE_SINGLE_PLAYER;
    gSelectedCharacter = getenv("SA_TEST_CHARACTER") ? atoi(getenv("SA_TEST_CHARACTER")) : 0;
    if (getenv("SA_TEST_LEVEL")) {
        gCurrentLevel = atoi(getenv("SA_TEST_LEVEL"));
        assert(gCurrentLevel >= 0 && gCurrentLevel < NUM_LEVEL_IDS_SP);
        ApplyGameStageSettingsAndStart();
    } else {
        // Leave a previous screen in VRAM, as a real title-to-selector change
        // does. A skipped clear/map pass must not accidentally look correct.
        for (unsigned i = 0; i < sizeof(VRAM); i++) VRAM[i] = (i * 17 + 3) & 255;
        CreateCharacterSelectionScreen(gSelectedCharacter);
    }
    EngineMainLoop();
}

void __wrap_VBlankIntrWait(void)
{
    if (headless) gpsp_draw_frame(gameImage);
    __real_VBlankIntrWait();
    REG_KEYINPUT = KEYS_MASK;
    if (++frames == 240 && getenv("SA_TEST_CAPTURE")) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(gameImage, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            16, DISPLAY_WIDTH * 2, 0x001f, 0x03e0, 0x7c00, 0);
        SDL_SaveBMP(surface, getenv("SA_TEST_CAPTURE"));
        SDL_FreeSurface(surface);
    }
    if (frames >= (getenv("SA_TEST_FRAMES") ? atoi(getenv("SA_TEST_FRAMES")) : 300)) {
        if (!headless) assert(gameCopies > 0);
        fprintf(stderr, "Passed: SA1 %s, character=%d, %u frames, %dx%d\n",
            getenv("SA_TEST_LEVEL") ? "stage graphics" : "selector", gSelectedCharacter,
            frames, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        exit(0);
    }
}
