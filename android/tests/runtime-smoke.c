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
#ifdef __ANDROID__
#include "platform/shared/android_controls.h"
extern void GetInput(void);

static void FastTouch(SDL_Window *window, int id)
{
    int width, height;
    SDL_FRect rect;
    SDL_GetRendererOutputSize(SDL_GetRenderer(window), &width, &height);
    AndroidControls_Describe(id, width, height, &rect);
    SDL_Event event = { 0 };
    event.type = SDL_FINGERDOWN;
    event.tfinger.touchId = 1;
    event.tfinger.fingerId = id + 1;
    event.tfinger.windowID = SDL_GetWindowID(window);
    event.tfinger.x = (rect.x + rect.w / 2) / width;
    event.tfinger.y = (rect.y + rect.h / 2) / height;
    if (id == 0) event.tfinger.x += rect.w / width * .3f;
    if (SDL_PushEvent(&event) != 1) abort();
    event.type = SDL_FINGERUP;
    if (SDL_PushEvent(&event) != 1) abort();
}
#endif

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
    const u16 masks[] = { A_BUTTON, START_BUTTON, DPAD_RIGHT, DPAD_LEFT, DPAD_UP };
    const SDL_Keycode codes[] = { SDLK_c, SDLK_RETURN, SDLK_RIGHT, SDLK_LEFT, SDLK_UP };
    if (!headless) {
        for (unsigned i = 0; i < ARRAY_COUNT(masks); i++) {
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
#ifdef __ANDROID__
    if (getenv("SA_TEST_FAST_TAP")) {
        EngineInit();
        GameInit();
        SDL_Window *window = SDL_GetWindowFromID(1);
        if (!window || headless) abort();
        /* Queue DOWN and UP before the real SDL VBlank wait. The renderer can
         * present/poll more than once before returning to the next logic tick.
         * Neither a render poll nor catch-up may consume the only tap. */
        const int ids[] = { 1, 2, 6, 0 };
        const u16 masks[] = { A_BUTTON, B_BUTTON, START_BUTTON, DPAD_RIGHT };
        for (unsigned repeat = 0; repeat < 8; repeat++) {
            for (unsigned i = 0; i < ARRAY_COUNT(ids); i++) {
                FastTouch(window, ids[i]);
                __real_VBlankIntrWait();
                GetInput();
                if (gPressedKeys != masks[i]) {
                    fprintf(stderr, "FAIL: real SDL fast tap %u/%u expected=%x pressed=%x input=%x\n", repeat, i, masks[i], gPressedKeys, gInput);
                    exit(2);
                }
                __real_VBlankIntrWait();
                GetInput();
                if (gInput || gReleasedKeys != masks[i]) abort();
            }
        }
        FastTouch(window, 0);
        FastTouch(window, 1);
        __real_VBlankIntrWait();
        GetInput();
        if (gPressedKeys != (DPAD_RIGHT | A_BUTTON)) abort();
        __real_VBlankIntrWait();
        GetInput();
        if (gInput || gReleasedKeys != (DPAD_RIGHT | A_BUTTON)) abort();
        fprintf(stderr, "PASS: real Android SDL frame loop retains 32 fast taps and simultaneous direction+jump, with clean release\n");
        exit(0);
    }
#endif
    if (getenv("SA_TEST_RECREATE")) {
        frames = 0;
        __real_AgbMain();
        if (frames < 10) abort();
        fprintf(stderr, "Passed: native SDL_QUIT returned through the real engine after %u frames\n", frames);
        return;
    }
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
    if (getenv("SA_TEST_RECREATE")) {
        if (++frames == 10) {
            SDL_Event event = { .type = SDL_QUIT };
            SDL_PushEvent(&event);
        }
        __real_VBlankIntrWait();
        return;
    }
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
            SetInput(cycle < 2 || (cycle >= 36 && cycle < 38) || (cycle >= 54 && cycle < 56) ? START_BUTTON
                : (cycle >= 4 && cycle < 6) || (cycle >= 8 && cycle < 10) ? DPAD_UP
                : (cycle >= 12 && cycle < 14) || (cycle >= 16 && cycle < 18) ? DPAD_LEFT
                : cycle >= 20 && cycle < 22 ? A_BUTTON : 0);
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
