// Linked only into the host sanitizer executable with --wrap. No test input or
// shortcuts are included in Android APKs. Exercise the real engine, level
// tasks, audio and software GBA renderer at the Android viewport size.
#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include "core.h"
#include "task.h"
#include "game/game.h"
#include "game/globals.h"
#include "game/sa2/save.h"
#include "game/shared/stage/stage.h"
#include "game/shared/stage/player.h"
#include "platform/shared/video/gpsp_renderer.h"
#include <SDL.h>

extern uint16_t gameImage[];
extern void __real_VBlankIntrWait(void);
static unsigned frames;

void __wrap_AgbMain(void)
{
    EngineInit();
    GameInit();
    TasksDestroyAll();
    // Credits' destructor creates the title task. Discard its pending graphics
    // exactly as a menu-to-stage transition does after destroying menu tasks.
    gBackgroundsCopyQueueCursor = gBackgroundsCopyQueueIndex = 0;
    gVramGraphicsCopyCursor = gVramGraphicsCopyQueueIndex = 0;
    NewSaveGame();
    gCurrentLevel = getenv("SA_TEST_LEVEL") ? atoi(getenv("SA_TEST_LEVEL")) : 0;
    gSelectedCharacter = 0;
    gGameMode = GAME_MODE_SINGLE_PLAYER;
    ApplyGameStageSettings();
    fprintf(stderr, "Starting level %d, viewport %dx%d\n", gCurrentLevel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    GameStageStart();
    EngineMainLoop();
}

void __wrap_VBlankIntrWait(void)
{
    gpsp_draw_frame(gameImage);
    __real_VBlankIntrWait();
    if (frames == 300 && getenv("SA_TEST_CAPTURE")) {
        SDL_Surface *capture = SDL_CreateRGBSurfaceFrom(gameImage, DISPLAY_WIDTH, DISPLAY_HEIGHT, 16,
            DISPLAY_WIDTH * 2, 0x001f, 0x03e0, 0x7c00, 0);
        SDL_SaveBMP(capture, getenv("SA_TEST_CAPTURE"));
        SDL_FreeSurface(capture);
    }
    // Advance through the entrance, then run and jump across the level.
    REG_KEYINPUT = KEYS_MASK ^ (frames > 200 ? DPAD_RIGHT | ((frames % 120 < 20) ? A_BUTTON : 0) : 0);
    if (++frames >= 1200) {
        fprintf(stderr, "Passed: 1200 level frames; x=%d y=%d tasks=%d\n", I(gPlayer.qWorldX), I(gPlayer.qWorldY), gNumTasks);
        exit(0);
    }
}
