// Linked only into the host sanitizer executable with --wrap. No test input or
// shortcuts are included in Android APKs. Exercise the real engine, level
// tasks, audio and software GBA renderer at the Android viewport size.
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
extern void __real_AgbMain(void);
static unsigned frames;
static unsigned stageFrames;
static unsigned targetFrames;
extern bool headless;

static void SetInput(u16 pressed)
{
    static u16 previous;
    const u16 masks[] = { A_BUTTON, START_BUTTON, DPAD_RIGHT };
    const SDL_Keycode codes[] = { SDLK_c, SDLK_RETURN, SDLK_RIGHT };
    if (!headless) {
        for (unsigned i = 0; i < 3; i++) {
            if ((previous ^ pressed) & masks[i]) {
                SDL_Event event = { 0 };
                event.type = pressed & masks[i] ? SDL_KEYDOWN : SDL_KEYUP;
                event.key.keysym.sym = codes[i];
                SDL_PushEvent(&event);
            }
        }
    }
    previous = pressed;
    REG_KEYINPUT = KEYS_MASK ^ pressed;
}

void __wrap_AgbMain(void)
{
    targetFrames = getenv("SA_TEST_FRAMES") ? atoi(getenv("SA_TEST_FRAMES")) : 1200;
    if (getenv("SA_TEST_BOOT")) {
        __real_AgbMain();
        return;
    }
    EngineInit();
    GameInit();
    TasksDestroyAll();
    // Credits' destructor creates the title task. Discard its pending graphics
    // exactly as a menu-to-stage transition does after destroying menu tasks.
    gBackgroundsCopyQueueCursor = gBackgroundsCopyQueueIndex = 0;
    gVramGraphicsCopyCursor = gVramGraphicsCopyQueueIndex = 0;
    NewSaveGame();
    gCurrentLevel = getenv("SA_TEST_LEVEL") ? atoi(getenv("SA_TEST_LEVEL")) : 0;
    gSelectedCharacter = getenv("SA_TEST_CHARACTER") ? atoi(getenv("SA_TEST_CHARACTER")) : 0;
    gGameMode = GAME_MODE_SINGLE_PLAYER;
    ApplyGameStageSettings();
    fprintf(stderr, "Starting level %d, viewport %dx%d\n", gCurrentLevel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    GameStageStart();
    EngineMainLoop();
}

void __wrap_VBlankIntrWait(void)
{
    if (headless) gpsp_draw_frame(gameImage);
    __real_VBlankIntrWait();
    if (frames == 300 && getenv("SA_TEST_CAPTURE")) {
        SDL_Surface *capture = SDL_CreateRGBSurfaceFrom(gameImage, DISPLAY_WIDTH, DISPLAY_HEIGHT, 16,
            DISPLAY_WIDTH * 2, 0x001f, 0x03e0, 0x7c00, 0);
        SDL_SaveBMP(capture, getenv("SA_TEST_CAPTURE"));
        SDL_FreeSurface(capture);
    }
    if (getenv("SA_TEST_BOOT")) {
        if (gGameStageTask) {
            if (++stageFrames >= targetFrames) {
                fprintf(stderr, "Passed: full boot, menus and %u stage frames; level=%d x=%d y=%d\n", targetFrames, gCurrentLevel, I(gPlayer.qWorldX), I(gPlayer.qWorldY));
                exit(0);
            }
            SetInput(stageFrames > 200 ? DPAD_RIGHT | ((stageFrames % 120 < 20) ? A_BUTTON : 0) : 0);
        } else {
            const unsigned cycle = frames % 90;
            SetInput(cycle < 4 ? A_BUTTON : cycle >= 45 && cycle < 49 ? START_BUTTON : 0);
        }
        if (frames % 180 == 0) {
            fprintf(stderr, "Boot frame=%u stage=%u tasks=%d mode=%d x=%d\n", frames, stageFrames, gNumTasks, gGameMode, I(gPlayer.qWorldX));
        }
        if (++frames >= 7200) {
            fprintf(stderr, "Failed: boot did not reach the target stage frames\n");
            exit(2);
        }
        return;
    }
    // Advance through the entrance, then run and jump across the level.
    SetInput(frames > 200 ? DPAD_RIGHT | ((frames % 120 < 20) ? A_BUTTON : 0) : 0);
    if (++frames >= targetFrames) {
        fprintf(stderr, "Passed: %u level frames; x=%d y=%d tasks=%d\n", targetFrames, I(gPlayer.qWorldX), I(gPlayer.qWorldY), gNumTasks);
        exit(0);
    }
}
