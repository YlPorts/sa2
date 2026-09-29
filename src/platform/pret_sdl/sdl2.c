#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <xinput.h>
#endif

#ifdef __PSP__
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspgu.h>
#endif

#include <SDL.h>
#ifdef __ANDROID__
#include <SDL_system.h>
#endif

#include "global.h"
#include "core.h"
#include "lib/agb_flash/flash_internal.h"
#include "platform/shared/dma.h"
#include "platform/shared/input.h"
#include "platform/shared/video/gpsp_renderer.h"

#if ENABLE_AUDIO
#include "platform/shared/audio/cgb_audio.h"
#endif

ALIGNED(256) uint16_t gameImage[DISPLAY_WIDTH * DISPLAY_HEIGHT];

#if ENABLE_VRAM_VIEW
uint16_t vramBuffer[VRAM_VIEW_WIDTH * VRAM_VIEW_HEIGHT];
#endif

SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;
SDL_Texture *sdlTexture;
#if ENABLE_VRAM_VIEW
SDL_Window *vramWindow;
SDL_Renderer *vramRenderer;
SDL_Texture *vramTexture;
#endif
#define INITIAL_VIDEO_SCALE 1
unsigned int videoScale = INITIAL_VIDEO_SCALE;
unsigned int preFullscreenVideoScale = INITIAL_VIDEO_SCALE;

bool speedUp = false;
bool videoScaleChanged = false;
bool isRunning = true;
bool paused = false;
bool stepOneFrame = false;
bool headless = false;

#ifdef __ANDROID__
static SDL_AudioDeviceID sAndroidAudioDevice = 0;
static bool sAndroidSuspended = false;
static SDL_GameController *sAndroidController = NULL;
#endif

#ifdef __PSP__
static SDL_Joystick *joystick = NULL;
static SDL_Rect pspDestRect;
#endif

double lastGameTime = 0;
double curGameTime = 0;
double fixedTimestep = 1.0 / 60.0; // 16.666667ms
double timeScale = 1.0;
double accumulator = 0.0;

static FILE *sSaveFile = NULL;

#if (GAME == GAME_SA1)
#define SAVE_FILENAME "sa1.sav"
#else
#define SAVE_FILENAME "sa2.sav"
#endif

extern void AgbMain(void);
void DoSoftReset(void) {};

void ProcessSDLEvents(void);
void VDraw(SDL_Texture *texture);
void VramDraw(SDL_Texture *texture);
#ifdef __ANDROID__
static void AndroidDrawTouchControls(SDL_Renderer *renderer);
#endif

static void ReadSaveFile(char *path);
static void StoreSaveFile(void);
static void CloseSaveFile(void);

u16 Platform_GetKeyInput(void);

#ifdef _WIN32
void *Platform_malloc(size_t numBytes) { return HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, numBytes); }
void Platform_free(void *ptr) { HeapFree(GetProcessHeap(), 0, ptr); }
#endif

#ifdef __PSP__
PSP_MODULE_INFO("SonicAdvance2", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

unsigned int sce_newlib_stack_size = 512 * 1024;

extern bool isRunning;

int exitCallback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    isRunning = false;
    return 0;
}

int callbackThread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int setupPspCallbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}
#endif

int main(int argc, char **argv)
{
#ifdef __PSP__
    setupPspCallbacks();
#endif

    const char *headlessEnv = getenv("HEADLESS");

    if (headlessEnv && strcmp(headlessEnv, "true") == 0) {
        headless = true;
    }

    const char *parentEnv = getenv("SIO_PARENT");

    if (parentEnv && strcmp(parentEnv, "true") == 0) {
        SIO_MULTI_CNT->id = 0;
        SIO_MULTI_CNT->si = 1;
        SIO_MULTI_CNT->sd = 1;
        SIO_MULTI_CNT->enable = false;
    }

    // Open an output console on Windows
#if (defined _WIN32) && (DEBUG != 0)
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    freopen("CON", "w", stdout);
#endif

#ifndef __ANDROID__
    ReadSaveFile(SAVE_FILENAME);
#endif

    // Prevent the multiplayer screen from being drawn ( see core.c:EngineInit() )
    REG_RCNT = 0x8000;
    REG_KEYINPUT = 0x3FF;

    if (headless) {
#if ENABLE_AUDIO
        // Required or it makes an infinite loop
        cgb_audio_init(48000);
#endif
        AgbMain();
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __ANDROID__
    // Android's working directory is not a stable place for save data.
    // Keep each game's SRAM inside the application's private storage.
    {
        const char *internalStoragePath = SDL_AndroidGetInternalStoragePath();
        char savePath[1024];

        if (internalStoragePath != NULL
            && snprintf(savePath, sizeof(savePath), "%s/%s", internalStoragePath, SAVE_FILENAME) < (int)sizeof(savePath)) {
            ReadSaveFile(savePath);
            SDL_Log("SA Android save: %s", savePath);
        } else {
            ReadSaveFile(SAVE_FILENAME);
            SDL_Log("SA Android save fallback: %s", SAVE_FILENAME);
        }
    }
#endif

#ifdef __ANDROID__
    {
        int i;
        for (i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                sAndroidController = SDL_GameControllerOpen(i);
                if (sAndroidController != NULL)
                    break;
            }
        }
    }
#endif

#ifdef __PSP__
    if (SDL_NumJoysticks() > 0) {
        joystick = SDL_JoystickOpen(0);
    }
#endif

#ifdef TITLE_BAR
    const char *title = STR(TITLE_BAR);
#else
    const char *title = "SAT-R sa2";
#endif

#ifdef __PSP__
    sdlWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 480, 272, SDL_WINDOW_SHOWN);
#else
    sdlWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DISPLAY_WIDTH * videoScale,
                                 DISPLAY_HEIGHT * videoScale, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#endif
    if (sdlWindow == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

#if ENABLE_VRAM_VIEW
    int mainWindowX;
    int mainWindowWidth;
    SDL_GetWindowPosition(sdlWindow, &mainWindowX, NULL);
    SDL_GetWindowSize(sdlWindow, &mainWindowWidth, NULL);
    int vramWindowX = mainWindowX + mainWindowWidth;
    u16 vramWindowWidth = VRAM_VIEW_WIDTH;
    u16 vramWindowHeight = VRAM_VIEW_HEIGHT;
    vramWindow = SDL_CreateWindow("VRAM View", vramWindowX, SDL_WINDOWPOS_CENTERED, vramWindowWidth, vramWindowHeight,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (vramWindow == NULL) {
        fprintf(stderr, "VRAM Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#endif

#ifdef __PSP__
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (sdlRenderer == NULL)
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
    if (sdlRenderer == NULL)
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
#elif defined(__ANDROID__)
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (sdlRenderer == NULL)
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
    if (sdlRenderer == NULL)
        sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC);
#else
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_PRESENTVSYNC);
#endif
    if (sdlRenderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

#ifdef __ANDROID__
    {
        SDL_RendererInfo rendererInfo;
        int outputW = 0;
        int outputH = 0;

        SDL_GetRendererOutputSize(sdlRenderer, &outputW, &outputH);
        if (SDL_GetRendererInfo(sdlRenderer, &rendererInfo) == 0) {
            SDL_Log("SA Android renderer: %s flags=0x%x output=%dx%d",
                    rendererInfo.name ? rendererInfo.name : "unknown",
                    rendererInfo.flags, outputW, outputH);
        }
        SDL_Log("SA Android drivers: video=%s audio=%s joysticks=%d",
                SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown",
                SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "unknown",
                SDL_NumJoysticks());
    }
#endif

#if ENABLE_VRAM_VIEW
    vramRenderer = SDL_CreateRenderer(vramWindow, -1, SDL_RENDERER_PRESENTVSYNC);
    if (vramRenderer == NULL) {
        fprintf(stderr, "VRAM Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#endif

    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#ifdef __PSP__
    // SDL_RenderSetLogicalSize is broken on PSP, stretch to fill manually
    pspDestRect = (SDL_Rect) { 0, 0, GU_SCR_WIDTH, GU_SCR_HEIGHT };
#else
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif
#if ENABLE_VRAM_VIEW
    SDL_SetRenderDrawColor(vramRenderer, 0, 0, 0, 255);
    SDL_RenderClear(vramRenderer);
    SDL_RenderSetLogicalSize(vramRenderer, vramWindowWidth, vramWindowHeight);
#endif

    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (sdlTexture == NULL) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

#if ENABLE_VRAM_VIEW
    vramTexture = SDL_CreateTexture(vramRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, vramWindowWidth, vramWindowHeight);
    if (vramTexture == NULL) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#endif

#if ENABLE_AUDIO
    SDL_AudioSpec want;

    SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = (want.freq / 60);
    cgb_audio_init(want.freq);

#ifdef __ANDROID__
    sAndroidAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (sAndroidAudioDevice == 0) {
        SDL_Log("Failed to open Android audio: %s", SDL_GetError());
    } else {
        SDL_PauseAudioDevice(sAndroidAudioDevice, 0);
    }
#else
    if (SDL_OpenAudio(&want, 0) < 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    } else {
        if (want.format != AUDIO_S16) /* we let this one thing change. */
            SDL_Log("We didn't get S16 audio format.");
        SDL_PauseAudio(0);
    }
#endif
#endif

    VDraw(sdlTexture);
#if ENABLE_VRAM_VIEW
    VramDraw(vramTexture);
#endif
    AgbMain();

    return 0;
}

bool newFrameRequested = FALSE;

// called every gba frame. we process sdl events and render as many times
// as vsync needs, then return when a new game frame is needed.
void VBlankIntrWait(void)
{
#define HANDLE_VBLANK_INTRS()                                                                                                              \
    ({                                                                                                                                     \
        REG_DISPSTAT |= INTR_FLAG_VBLANK;                                                                                                  \
        RunDMAs(DMA_VBLANK);                                                                                                               \
        if (REG_DISPSTAT & DISPSTAT_VBLANK_INTR)                                                                                           \
            gIntrTable[INTR_INDEX_VBLANK]();                                                                                               \
        REG_DISPSTAT &= ~INTR_FLAG_VBLANK;                                                                                                 \
    })

    if (headless) {
        REG_VCOUNT = DISPLAY_HEIGHT + 1;
        HANDLE_VBLANK_INTRS();
        return;
    }

    bool frameAvailable = TRUE;
    bool frameDrawn = false;

    while (isRunning) {
#ifndef __PSP__
        ProcessSDLEvents();
#endif

#ifdef __ANDROID__
        if (sAndroidSuspended) {
            SDL_Delay(16);
            continue;
        }
#endif

        if (!paused || stepOneFrame) {
            double dt = fixedTimestep / timeScale; // TODO: Fix speedup

            // don't accumulate time if we already requested a new frame
            // this frame cycle (emulates threaded sdl behavior)
            if (!newFrameRequested) {
                double deltaTime = 0;

                curGameTime = SDL_GetPerformanceCounter();
                if (stepOneFrame) {
                    deltaTime = dt;
                } else {
                    deltaTime = (double)((curGameTime - lastGameTime) / (double)SDL_GetPerformanceFrequency());
                    if (deltaTime > (dt * 5))
                        deltaTime = dt * 5;
                }
                lastGameTime = curGameTime;

                accumulator += deltaTime;
            } else {
                newFrameRequested = FALSE;
            }

            while (accumulator >= dt) {
                REG_KEYINPUT = KEYS_MASK ^ Platform_GetKeyInput();
                if (frameAvailable) {
                    VDraw(sdlTexture);
                    frameAvailable = FALSE;
                    frameDrawn = true;

                    HANDLE_VBLANK_INTRS();

                    accumulator -= dt;
                } else {
                    newFrameRequested = TRUE;
                    return;
                }
            }

            if (paused && stepOneFrame) {
                stepOneFrame = false;
            }
        }

        // present
#ifdef __PSP__
        // manual blit since SDL_RenderSetLogicalSize doesn't work on psp
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &pspDestRect);
        SDL_RenderPresent(sdlRenderer);
#else
        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);

#ifdef __ANDROID__
        AndroidDrawTouchControls(sdlRenderer);
#endif

#if ENABLE_VRAM_VIEW
        VramDraw(vramTexture);
        SDL_RenderClear(vramRenderer);
        SDL_RenderCopy(vramRenderer, vramTexture, NULL, NULL);
#endif
        if (videoScaleChanged) {
            SDL_SetWindowSize(sdlWindow, DISPLAY_WIDTH * videoScale, DISPLAY_HEIGHT * videoScale);
            videoScaleChanged = false;
        }

        SDL_RenderPresent(sdlRenderer);
#if ENABLE_VRAM_VIEW
        SDL_RenderPresent(vramRenderer);
#endif
#endif
    }

    StoreSaveFile();
    CloseSaveFile();

#ifdef __ANDROID__
    if (sAndroidController != NULL) {
        SDL_GameControllerClose(sAndroidController);
        sAndroidController = NULL;
    }
    if (sAndroidAudioDevice != 0) {
        SDL_CloseAudioDevice(sAndroidAudioDevice);
        sAndroidAudioDevice = 0;
    }
#endif

    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
#ifdef __PSP__
    sceKernelExitGame();
#endif
    exit(0);
#undef HANDLE_VBLANK_INTRS
}

static void ReadSaveFile(char *path)
{
    // Check whether the saveFile exists, and create it if not
    sSaveFile = fopen(path, "r+b");
    if (sSaveFile == NULL) {
        sSaveFile = fopen(path, "w+b");
    }

    fseek(sSaveFile, 0, SEEK_END);
    int fileSize = ftell(sSaveFile);
    fseek(sSaveFile, 0, SEEK_SET);

    // Only read as many bytes as fit inside the buffer
    // or as many bytes as are in the file
    int bytesToRead = (fileSize < sizeof(FLASH_BASE)) ? fileSize : sizeof(FLASH_BASE);

    int bytesRead = fread(FLASH_BASE, 1, bytesToRead, sSaveFile);

    // Fill the buffer if the savefile was just created or smaller than the buffer itself
    for (int i = bytesRead; i < sizeof(FLASH_BASE); i++) {
        FLASH_BASE[i] = 0xFF;
    }
}

static void StoreSaveFile()
{
    if (sSaveFile != NULL) {
        fseek(sSaveFile, 0, SEEK_SET);
        fwrite(FLASH_BASE, 1, sizeof(FLASH_BASE), sSaveFile);
        fflush(sSaveFile);
    }
}

void Platform_StoreSaveFile(void) { StoreSaveFile(); }

static void CloseSaveFile()
{
    if (sSaveFile != NULL) {
        fclose(sSaveFile);
    }
}

static u16 keys;

#ifdef __ANDROID__
#define ANDROID_MAX_TOUCHES 10

typedef struct AndroidTouchSlot {
    SDL_FingerID id;
    u16 mask;
    bool active;
} AndroidTouchSlot;

static AndroidTouchSlot sAndroidTouches[ANDROID_MAX_TOUCHES];
static u16 sAndroidTouchKeys;

static u16 AndroidTouchMask(float x, float y)
{
    u16 mask = 0;

    // D-pad. The independent axes deliberately overlap to allow diagonals.
    if (x < 0.36f && y > 0.48f) {
        const float dx = x - 0.18f;
        const float dy = y - 0.72f;

        if (dx < -0.045f)
            mask |= DPAD_LEFT;
        if (dx > 0.045f)
            mask |= DPAD_RIGHT;
        if (dy < -0.045f)
            mask |= DPAD_UP;
        if (dy > 0.045f)
            mask |= DPAD_DOWN;
    }

    // Face buttons.
    if (x > 0.80f && y > 0.50f)
        mask |= A_BUTTON;
    if (x > 0.64f && x <= 0.82f && y > 0.64f)
        mask |= B_BUTTON;

    // Shoulder buttons.
    if (x < 0.22f && y < 0.20f)
        mask |= L_BUTTON;
    if (x > 0.78f && y < 0.20f)
        mask |= R_BUTTON;

    // Start / Select.
    if (x > 0.52f && x < 0.65f && y > 0.84f)
        mask |= START_BUTTON;
    if (x > 0.37f && x < 0.49f && y > 0.84f)
        mask |= SELECT_BUTTON;

    return mask;
}

static void AndroidRebuildTouchKeys(void)
{
    u16 mask = 0;
    int i;

    for (i = 0; i < ANDROID_MAX_TOUCHES; i++) {
        if (sAndroidTouches[i].active)
            mask |= sAndroidTouches[i].mask;
    }

    sAndroidTouchKeys = mask;
}

static void AndroidUpdateTouch(SDL_FingerID id, float x, float y, bool active)
{
    int i;
    int freeSlot = -1;

    for (i = 0; i < ANDROID_MAX_TOUCHES; i++) {
        if (sAndroidTouches[i].active && sAndroidTouches[i].id == id) {
            if (active) {
                sAndroidTouches[i].mask = AndroidTouchMask(x, y);
            } else {
                sAndroidTouches[i].active = false;
                sAndroidTouches[i].mask = 0;
            }
            AndroidRebuildTouchKeys();
            return;
        }

        if (!sAndroidTouches[i].active && freeSlot < 0)
            freeSlot = i;
    }

    if (active && freeSlot >= 0) {
        sAndroidTouches[freeSlot].id = id;
        sAndroidTouches[freeSlot].mask = AndroidTouchMask(x, y);
        sAndroidTouches[freeSlot].active = true;
    }

    AndroidRebuildTouchKeys();
}

static void AndroidDrawTouchRect(SDL_Renderer *renderer, float x, float y, float w, float h, bool pressed)
{
    SDL_Rect rect = {
        (int)(x * DISPLAY_WIDTH),
        (int)(y * DISPLAY_HEIGHT),
        (int)(w * DISPLAY_WIDTH),
        (int)(h * DISPLAY_HEIGHT),
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, pressed ? 100 : 48);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, pressed ? 210 : 120);
    SDL_RenderDrawRect(renderer, &rect);
}

static void AndroidDrawTouchControls(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    AndroidDrawTouchRect(renderer, 0.04f, 0.66f, 0.10f, 0.14f, (sAndroidTouchKeys & DPAD_LEFT) != 0);
    AndroidDrawTouchRect(renderer, 0.22f, 0.66f, 0.10f, 0.14f, (sAndroidTouchKeys & DPAD_RIGHT) != 0);
    AndroidDrawTouchRect(renderer, 0.13f, 0.53f, 0.10f, 0.14f, (sAndroidTouchKeys & DPAD_UP) != 0);
    AndroidDrawTouchRect(renderer, 0.13f, 0.79f, 0.10f, 0.14f, (sAndroidTouchKeys & DPAD_DOWN) != 0);

    AndroidDrawTouchRect(renderer, 0.82f, 0.56f, 0.12f, 0.16f, (sAndroidTouchKeys & A_BUTTON) != 0);
    AndroidDrawTouchRect(renderer, 0.67f, 0.70f, 0.12f, 0.16f, (sAndroidTouchKeys & B_BUTTON) != 0);

    AndroidDrawTouchRect(renderer, 0.03f, 0.05f, 0.17f, 0.09f, (sAndroidTouchKeys & L_BUTTON) != 0);
    AndroidDrawTouchRect(renderer, 0.80f, 0.05f, 0.17f, 0.09f, (sAndroidTouchKeys & R_BUTTON) != 0);

    AndroidDrawTouchRect(renderer, 0.39f, 0.87f, 0.10f, 0.07f, (sAndroidTouchKeys & SELECT_BUTTON) != 0);
    AndroidDrawTouchRect(renderer, 0.53f, 0.87f, 0.10f, 0.07f, (sAndroidTouchKeys & START_BUTTON) != 0);
}

static u16 AndroidPollControllerButtons(void)
{
    u16 result = 0;

    if (sAndroidController == NULL)
        return 0;

    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_A))
        result |= A_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_B)
        || SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_X))
        result |= B_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_START))
        result |= START_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_BACK))
        result |= SELECT_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
        result |= L_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        result |= R_BUTTON;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_DPAD_UP))
        result |= DPAD_UP;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        result |= DPAD_DOWN;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        result |= DPAD_LEFT;
    if (SDL_GameControllerGetButton(sAndroidController, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        result |= DPAD_RIGHT;

    return result;
}

static void AndroidOpenFirstController(void)
{
    int i;

    if (sAndroidController != NULL)
        return;

    for (i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            sAndroidController = SDL_GameControllerOpen(i);
            if (sAndroidController != NULL)
                return;
        }
    }
}
#endif

// Key mappings
#define KEY_A_BUTTON      SDLK_c
#define KEY_B_BUTTON      SDLK_x
#define KEY_START_BUTTON  SDLK_RETURN
#define KEY_SELECT_BUTTON SDLK_BACKSLASH
#define KEY_L_BUTTON      SDLK_s
#define KEY_R_BUTTON      SDLK_d
#define KEY_DPAD_UP       SDLK_UP
#define KEY_DPAD_DOWN     SDLK_DOWN
#define KEY_DPAD_LEFT     SDLK_LEFT
#define KEY_DPAD_RIGHT    SDLK_RIGHT

#define HANDLE_KEYUP(key)                                                                                                                  \
    case KEY_##key:                                                                                                                        \
        keys &= ~key;                                                                                                                      \
        break;

#define HANDLE_KEYDOWN(key)                                                                                                                \
    case KEY_##key:                                                                                                                        \
        keys |= key;                                                                                                                       \
        break;

#ifdef __PSP__
#define BTN_TRIANGLE 0
#define BTN_CIRCLE   1
#define BTN_CROSS    2
#define BTN_SQUARE   3
#define BTN_LTRIGGER 4
#define BTN_RTRIGGER 5
#define BTN_DOWN     6
#define BTN_LEFT     7
#define BTN_UP       8
#define BTN_RIGHT    9
#define BTN_SELECT   10
#define BTN_START    11

static u16 PollJoystickButtons(void)
{
    u16 newKeys = 0;
    if (joystick == NULL)
        return newKeys;

    SDL_JoystickUpdate();

    if (SDL_JoystickGetButton(joystick, BTN_CROSS))
        newKeys |= A_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_CIRCLE))
        newKeys |= B_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_SQUARE))
        newKeys |= B_BUTTON; // Square also B
    if (SDL_JoystickGetButton(joystick, BTN_START))
        newKeys |= START_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_SELECT))
        newKeys |= SELECT_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_LTRIGGER))
        newKeys |= L_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_RTRIGGER))
        newKeys |= R_BUTTON;
    if (SDL_JoystickGetButton(joystick, BTN_UP))
        newKeys |= DPAD_UP;
    if (SDL_JoystickGetButton(joystick, BTN_DOWN))
        newKeys |= DPAD_DOWN;
    if (SDL_JoystickGetButton(joystick, BTN_LEFT))
        newKeys |= DPAD_LEFT;
    if (SDL_JoystickGetButton(joystick, BTN_RIGHT))
        newKeys |= DPAD_RIGHT;

    return newKeys;
}

#endif

u32 fullScreenFlags = 0;
static SDL_DisplayMode sdlDispMode = { 0 };

void Platform_QueueAudio(const s16 *data, uint32_t bytesCount)
{
    if (headless) {
        return;
    }

#ifdef __ANDROID__
    if (sAndroidAudioDevice == 0 || sAndroidSuspended) {
        return;
    }

    if (SDL_GetQueuedAudioSize(sAndroidAudioDevice) > (bytesCount * 10)) {
        SDL_ClearQueuedAudio(sAndroidAudioDevice);
    }

    SDL_QueueAudio(sAndroidAudioDevice, data, bytesCount);
#else
    // Reset the audio buffer if we are 10 frames out of sync
    // If this happens it suggests there was some OS level lag
    // in playing audio. The queue length should remain stable at < 10 otherwise
    if (SDL_GetQueuedAudioSize(1) > (bytesCount * 10)) {
        SDL_ClearQueuedAudio(1);
    }

    SDL_QueueAudio(1, data, bytesCount);
#endif
}

void ProcessSDLEvents(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        SDL_Keycode keyCode = event.key.keysym.sym;
        Uint16 keyMod = event.key.keysym.mod;

        switch (event.type) {
            case SDL_QUIT:
                StoreSaveFile();
                isRunning = false;
                break;
#ifdef __ANDROID__
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                StoreSaveFile();
                sAndroidSuspended = true;
                if (sAndroidAudioDevice != 0) {
                    SDL_PauseAudioDevice(sAndroidAudioDevice, 1);
                    SDL_ClearQueuedAudio(sAndroidAudioDevice);
                }
                break;
            case SDL_APP_WILLENTERFOREGROUND:
                // Keep the game suspended until SDL confirms the activity is
                // fully foregrounded, but reset timing now to avoid a large dt.
                lastGameTime = SDL_GetPerformanceCounter();
                accumulator = 0.0;
                break;
            case SDL_APP_DIDENTERFOREGROUND:
                sAndroidSuspended = false;
                lastGameTime = SDL_GetPerformanceCounter();
                accumulator = 0.0;
                if (sAndroidAudioDevice != 0) {
                    SDL_PauseAudioDevice(sAndroidAudioDevice, 0);
                }
                break;
#endif
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    HANDLE_KEYUP(A_BUTTON)
                    HANDLE_KEYUP(B_BUTTON)
                    HANDLE_KEYUP(START_BUTTON)
                    HANDLE_KEYUP(SELECT_BUTTON)
                    HANDLE_KEYUP(L_BUTTON)
                    HANDLE_KEYUP(R_BUTTON)
                    HANDLE_KEYUP(DPAD_UP)
                    HANDLE_KEYUP(DPAD_DOWN)
                    HANDLE_KEYUP(DPAD_LEFT)
                    HANDLE_KEYUP(DPAD_RIGHT)
                    case SDLK_SPACE:
                        if (speedUp) {
                            speedUp = false;
                            timeScale = 1.0;
                            SDL_ClearQueuedAudio(1);
                            SDL_PauseAudio(0);
                        }
                        break;
                }
                break;
            case SDL_KEYDOWN:
                if (keyCode == SDLK_RETURN && (keyMod & KMOD_ALT)) {
                    fullScreenFlags ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
                    if (fullScreenFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_GetWindowDisplayMode(sdlWindow, &sdlDispMode);
                        preFullscreenVideoScale = videoScale;
                    } else {
                        SDL_SetWindowDisplayMode(sdlWindow, &sdlDispMode);
                        videoScale = preFullscreenVideoScale;
                    }
                    SDL_SetWindowFullscreen(sdlWindow, fullScreenFlags);

                    SDL_SetWindowSize(sdlWindow, DISPLAY_WIDTH * videoScale, DISPLAY_HEIGHT * videoScale);
                    videoScaleChanged = FALSE;
                } else
                    switch (event.key.keysym.sym) {
                        HANDLE_KEYDOWN(A_BUTTON)
                        HANDLE_KEYDOWN(B_BUTTON)
                        HANDLE_KEYDOWN(START_BUTTON)
                        HANDLE_KEYDOWN(SELECT_BUTTON)
                        HANDLE_KEYDOWN(L_BUTTON)
                        HANDLE_KEYDOWN(R_BUTTON)
                        HANDLE_KEYDOWN(DPAD_UP)
                        HANDLE_KEYDOWN(DPAD_DOWN)
                        HANDLE_KEYDOWN(DPAD_LEFT)
                        HANDLE_KEYDOWN(DPAD_RIGHT)
                        case SDLK_r:
                            if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) {
                                DoSoftReset();
                            }
                            break;
                        case SDLK_p:
                            if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) {
                                paused = !paused;
                            }
                            break;
                        case SDLK_SPACE:
                            if (!speedUp) {
                                speedUp = true;
                                timeScale = SPEEDUP_SCALE;
                                SDL_PauseAudio(1);
                            }
                            break;
                        case SDLK_F10:
                            paused = true;
                            stepOneFrame = true;
                            break;
                    }
                break;
#ifdef __ANDROID__
            case SDL_CONTROLLERDEVICEADDED:
                AndroidOpenFirstController();
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (sAndroidController != NULL
                    && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sAndroidController)) == event.cdevice.which) {
                    SDL_GameControllerClose(sAndroidController);
                    sAndroidController = NULL;
                    AndroidOpenFirstController();
                }
                break;
            case SDL_FINGERDOWN:
                AndroidUpdateTouch(event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, true);
                break;
            case SDL_FINGERMOTION:
                AndroidUpdateTouch(event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, true);
                break;
            case SDL_FINGERUP:
                AndroidUpdateTouch(event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, false);
                break;
#endif
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    unsigned int w = event.window.data1;
                    unsigned int h = event.window.data2;

                    videoScale = 0;
                    if (w / DISPLAY_WIDTH > videoScale)
                        videoScale = w / DISPLAY_WIDTH;
                    if (h / DISPLAY_HEIGHT > videoScale)
                        videoScale = h / DISPLAY_HEIGHT;
                    if (videoScale < 1)
                        videoScale = 1;

                    videoScaleChanged = true;
                }
                break;
        }
    }
}

u16 Platform_GetKeyInput(void)
{
#ifdef _WIN32
    SharedKeys gamepadKeys = GetXInputKeys();

    speedUp = (gamepadKeys & KEY_SPEEDUP) ? true : false;

    if (speedUp) {
        timeScale = SPEEDUP_SCALE;
        SDL_PauseAudio(1);
    } else {
        timeScale = 1.0f;
        SDL_PauseAudio(0);
    }

    return (gamepadKeys != 0) ? gamepadKeys : keys;
#endif

#ifdef __PSP__
    return keys | PollJoystickButtons();
#endif

#ifdef __ANDROID__
    return keys | sAndroidTouchKeys | AndroidPollControllerButtons();
#endif

    return keys;
}

#if ENABLE_VRAM_VIEW
void VramDraw(SDL_Texture *texture)
{
    memset(vramBuffer, 0, sizeof(vramBuffer));
    gpsp_draw_vram_view(vramBuffer);
    SDL_UpdateTexture(texture, NULL, vramBuffer, VRAM_VIEW_WIDTH * sizeof(Uint16));
}
#endif

void VDraw(SDL_Texture *texture)
{
    gpsp_draw_frame(gameImage);
    SDL_UpdateTexture(texture, NULL, gameImage, DISPLAY_WIDTH * sizeof(Uint16));
    REG_VCOUNT = DISPLAY_HEIGHT + 1; // prep for being in VBlank period
}
