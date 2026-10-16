#include <assert.h>
#include <stdbool.h>
#include <math.h>
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
#if PORTABLE && (GAME == GAME_SA2)
#include "game/sa2/oc_characters.h"
#endif
#ifdef __ANDROID__
#include "platform/shared/save_file.h"
#include "platform/shared/android_controls.h"
#include "platform/shared/android_viewport.h"
#include "platform/shared/android_options.h"
#include "platform/shared/android_touch.h"
#ifdef SA1_RUNTIME_IMPORT
#include "platform/shared/rom_assets.h"
#endif
#endif

#if ENABLE_AUDIO
#include "platform/shared/audio/cgb_audio.h"
#endif

ALIGNED(256) uint16_t gameImage[DISPLAY_WIDTH * DISPLAY_HEIGHT];

#ifdef __ANDROID__
static Uint32 sAndroidFrameRGBA[DISPLAY_WIDTH * DISPLAY_HEIGHT];
#endif

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
bool newFrameRequested = FALSE;

#ifdef __ANDROID__
static SDL_AudioDeviceID sAndroidAudioDevice = 0;
static bool sAndroidSuspended = false;
static bool sAndroidOptionsOpen = false;
static unsigned sAndroidPresentationFrame;
static Uint32 sAndroidFpsStart;
static unsigned sAndroidFpsFrames, sAndroidFps;
static SDL_GameController *sAndroidController = NULL;
static char sAndroidStagePath[1024];
static bool sAndroidNativeUiCrop = false;
static bool sAndroidDirectBgr555 = false;
static Uint32 sAndroidColorLut[0x8000];
static bool sAndroidColorLutReady = false;
static int sAndroidOutputW = 0;
static int sAndroidOutputH = 0;
static SDL_Texture *sAndroidControlsTexture = NULL;
static SDL_Texture *sAndroidControlsHighlightTexture = NULL;
static int sAndroidControlsTextureW = 0;
static int sAndroidControlsTextureH = 0;

void Platform_SetStartupStage(const char *stage);

static void AndroidSetStartupStage(const char *stage)
{
    FILE *stageFile;

    if (sAndroidStagePath[0] == '\0')
        return;

    stageFile = fopen(sAndroidStagePath, "wb");
    if (stageFile == NULL)
        return;

    fwrite(stage, 1, strlen(stage), stageFile);
    fflush(stageFile);
    fclose(stageFile);
}

static void AndroidSetStartupError(const char *where)
{
    char message[768];
    const char *error = SDL_GetError();

    if (error == NULL || error[0] == '\0')
        error = "unknown SDL error";

    snprintf(message, sizeof(message), "%s: %s", where, error);
    AndroidSetStartupStage(message);
}

void Platform_SetStartupStage(const char *stage)
{
    AndroidSetStartupStage(stage);
    SDL_Log("SA Android stage: %s", stage);
    if (strcmp(stage, "create_game_stage_complete") == 0) SDL_Log("SA Android gameplay ready");
}

void Platform_SetNativeUiCrop(bool8 enabled)
{
    sAndroidNativeUiCrop = enabled != FALSE;
}

static void AndroidInitColorLut(void)
{
    Uint32 i;

    if (sAndroidColorLutReady)
        return;

    for (i = 0; i < ARRAY_COUNT(sAndroidColorLut); i++) {
        const Uint8 r5 = i & 0x1F;
        const Uint8 g5 = (i >> 5) & 0x1F;
        const Uint8 b5 = (i >> 10) & 0x1F;
        const Uint8 r8 = (r5 << 3) | (r5 >> 2);
        const Uint8 g8 = (g5 << 3) | (g5 >> 2);
        const Uint8 b8 = (b5 << 3) | (b5 >> 2);

        sAndroidColorLut[i] = ((Uint32)0xFF << 24) | ((Uint32)b8 << 16)
                            | ((Uint32)g8 << 8) | (Uint32)r8;
    }

    sAndroidColorLutReady = true;
}
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
#ifdef __ANDROID__
static char sAndroidSavePath[1024];
static u8 sAndroidSavedFlash[sizeof(FLASH_BASE)];
static bool sAndroidSavedFlashValid = false;
#endif

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
static void AndroidClearInput(void);
static void AndroidDestroyControlsTexture(void);
static void AndroidDrawDiagnostics(SDL_Renderer *renderer);
#endif

static void ReadSaveFile(char *path);
static void StoreSaveFile(void);
static void CloseSaveFile(void);
static void ShutdownSdl(void);

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
#ifdef __ANDROID__
    // SDL can call main again after Android recreates the activity. Never kill
    // the Java process on SDL_QUIT; return through AgbMain and release SDL.
    isRunning = true;
    newFrameRequested = FALSE;
    paused = false;
    stepOneFrame = false;
    speedUp = false;
    timeScale = 1.0;
    memset(REG_BASE, 0, IO_SIZE);
    memset(DMAList, 0, sizeof(DMAList));
    sAndroidSuspended = false;
    sAndroidOptionsOpen = false;
    sAndroidNativeUiCrop = false;
    AndroidClearInput();
    accumulator = 0.0;
    lastGameTime = SDL_GetPerformanceCounter();
#endif
#ifdef SA1_RUNTIME_IMPORT
    char romPath[1024];
    char importError[192];
    int pathSize = argc > 1 && argv[1] != NULL
        ? snprintf(romPath, sizeof(romPath), "%s/sa1-assets.gba", argv[1]) : -1;
    if (pathSize < 0 || pathSize >= (int)sizeof(romPath)
        || !Sa1_LoadRomAssets(romPath, importError, sizeof(importError))) {
        SDL_Log("SA1 import failed: %s", pathSize < 0 || pathSize >= (int)sizeof(romPath)
            ? "Missing internal storage path" : importError);
        return 1;
    }
#endif
#ifdef __ANDROID__
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        snprintf(sAndroidStagePath, sizeof(sAndroidStagePath), "%s/sa_startup_stage.txt", argv[1]);
        AndroidSetStartupStage("native_enter");
    }
#endif

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
#if (GAME == GAME_SA2)
    OcSelectionSetStoragePath("sa2-selected-oc.bin");
#endif
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

#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
#ifdef __ANDROID__
        AndroidSetStartupError("sdl_init_failed");
#endif
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    AndroidSetStartupStage("sdl_init_ok");
#endif

#ifdef __ANDROID__
    // Android's working directory is not a stable place for save data.
    // Keep each game's SRAM inside the application's private storage.
    {
        const char *internalStoragePath = SDL_AndroidGetInternalStoragePath();
        char savePath[1024];

        if (internalStoragePath != NULL
            && snprintf(savePath, sizeof(savePath), "%s/%s", internalStoragePath, SAVE_FILENAME) < (int)sizeof(savePath)) {
            char optionsPath[1024];
            if (snprintf(optionsPath, sizeof(optionsPath), "%s/sa-android-options.cfg", internalStoragePath) < (int)sizeof(optionsPath))
                AndroidOptions_Load(optionsPath);
            SDL_Log("SA Android options loaded screen=%d opacity=%d", gAndroidOptions[SA_OPT_SCREEN], gAndroidOptions[SA_OPT_OPACITY]);
            ReadSaveFile(savePath);
            SDL_Log("SA Android save: %s", savePath);
#if (GAME == GAME_SA2)
            if (snprintf(savePath, sizeof(savePath), "%s/sa2-selected-oc.bin", internalStoragePath) < (int)sizeof(savePath)) {
                OcSelectionSetStoragePath(savePath);
            }
#endif
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
#ifdef __ANDROID__
        AndroidSetStartupError("window_failed");
#endif
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    AndroidSetStartupStage("window_ok");
    SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_ShowCursor(SDL_DISABLE);
#endif

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
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
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
#ifdef __ANDROID__
        AndroidSetStartupError("renderer_failed");
#endif
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    AndroidSetStartupStage("renderer_ok");
#endif

#ifdef __ANDROID__
    {
        SDL_RendererInfo rendererInfo;
        int outputW = 0;
        int outputH = 0;

        SDL_GetRendererOutputSize(sdlRenderer, &outputW, &outputH);
        sAndroidOutputW = outputW;
        sAndroidOutputH = outputH;
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
#elif defined(__ANDROID__)
    // Controls and normalized finger coordinates use the full output. Fit the
    // game separately so a portrait surface during launch never stretches it.
    SDL_RenderSetViewport(sdlRenderer, NULL);
#else
    SDL_RenderSetLogicalSize(sdlRenderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif
#if ENABLE_VRAM_VIEW
    SDL_SetRenderDrawColor(vramRenderer, 0, 0, 0, 255);
    SDL_RenderClear(vramRenderer);
    SDL_RenderSetLogicalSize(vramRenderer, vramWindowWidth, vramWindowHeight);
#endif

#ifdef __ANDROID__
    // GBA colors are xBBBBBGGGGGRRRRR, exactly SDL's BGR555 layout.
    // Avoid a full 32-bit color conversion every frame when GLES accepts it.
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (sdlTexture != NULL) {
        sAndroidDirectBgr555 = true;
    } else {
        SDL_ClearError();
        sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        sAndroidDirectBgr555 = false;
    }
#else
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif
    if (sdlTexture == NULL) {
#ifdef __ANDROID__
        AndroidSetStartupError("texture_failed");
#endif
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    SDL_SetTextureBlendMode(sdlTexture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(sdlTexture, gAndroidOptions[SA_OPT_FILTER] ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    AndroidSetStartupStage(sAndroidDirectBgr555 ? "texture_bgr555_ok" : "texture_rgba_fallback");
#endif

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
    /* Device callback buffers are independent of the game's 800-sample tick.
     * A power-of-two request is compatible with AAudio/OpenSL backends. */
    want.samples = 1024;
    cgb_audio_init(want.freq);

#ifdef __ANDROID__
    sAndroidAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (sAndroidAudioDevice == 0) {
        // Some Android drivers reject AAudio's requested stream attributes.
        // Keep the same format/rate and try SDL's other real Android backends.
        const char *fallbacks[] = { "openslES", "android" };
        for (unsigned i = 0; i < ARRAY_COUNT(fallbacks) && !sAndroidAudioDevice; i++) {
            SDL_Log("SA Android audio fallback from %s: %s", SDL_GetCurrentAudioDriver(), SDL_GetError());
            SDL_AudioQuit();
            if (SDL_AudioInit(fallbacks[i]) == 0)
                sAndroidAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        }
    }
    if (sAndroidAudioDevice == 0) {
        SDL_Log("Failed to open Android audio: %s", SDL_GetError());
    } else {
        SDL_Log("SA Android audio ready driver=%s", SDL_GetCurrentAudioDriver());
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

#ifdef __ANDROID__
    AndroidSetStartupStage("before_first_frame");
#endif
    VDraw(sdlTexture);
#ifdef __ANDROID__
    AndroidSetStartupStage("first_frame_ok");
#endif
#if ENABLE_VRAM_VIEW
    VramDraw(vramTexture);
#endif
#ifdef __ANDROID__
    AndroidSetStartupStage("game_enter");
#endif
    AgbMain();
#ifdef __ANDROID__
    AndroidSetStartupStage("game_returned");
    ShutdownSdl();
#endif

    return 0;
}

#ifdef __ANDROID__
bool8 Platform_IsRunning(void) { return isRunning ? TRUE : FALSE; }
#endif

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
        if (sAndroidSuspended || sAndroidOptionsOpen) {
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
#ifdef __ANDROID__
        // Do not swap duplicate frames while waiting for the next 60 Hz game
        // tick. Repeated swaps on a 90/120 Hz panel create uneven cadence.
        if (!frameDrawn) {
            SDL_Delay(1);
            continue;
        }
#endif
#ifdef __ANDROID__
        if (gAndroidOptions[SA_OPT_PRESENT] == 30 && (++sAndroidPresentationFrame & 1)) {
            frameDrawn = false;
            continue;
        }
#endif
        SDL_RenderClear(sdlRenderer);
#ifdef __ANDROID__
        int outputW = 0, outputH = 0;
        SDL_GetRendererOutputSize(sdlRenderer, &outputW, &outputH);
        if (sAndroidNativeUiCrop) {
            SDL_Rect nativeUiRect = { 0, 0, 240, 160 };
            SDL_Rect destination = AndroidViewport_Game(outputW, outputH, 240, 160);
            SDL_RenderCopy(sdlRenderer, sdlTexture, &nativeUiRect, &destination);
        } else {
            SDL_Rect destination = AndroidViewport_Game(outputW, outputH, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &destination);
        }

        AndroidDrawTouchControls(sdlRenderer);
        if (gAndroidOptions[SA_OPT_DIAGNOSTICS]) AndroidDrawDiagnostics(sdlRenderer);
#else
        SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
#endif

#if ENABLE_VRAM_VIEW
        VramDraw(vramTexture);
        SDL_RenderClear(vramRenderer);
        SDL_RenderCopy(vramRenderer, vramTexture, NULL, NULL);
#endif
#ifndef __ANDROID__
        if (videoScaleChanged) {
            SDL_SetWindowSize(sdlWindow, DISPLAY_WIDTH * videoScale, DISPLAY_HEIGHT * videoScale);
            videoScaleChanged = false;
        }
#endif

        SDL_RenderPresent(sdlRenderer);
#ifdef __ANDROID__
        frameDrawn = false;
        Uint32 now = SDL_GetTicks();
        sAndroidFpsFrames++;
        if (now - sAndroidFpsStart >= 1000) {
            sAndroidFps = sAndroidFpsFrames * 1000 / (now - sAndroidFpsStart);
            sAndroidFpsStart = now;
            sAndroidFpsFrames = 0;
        }
#endif
#if ENABLE_VRAM_VIEW
        SDL_RenderPresent(vramRenderer);
#endif
#endif
    }

#ifdef __ANDROID__
    // EngineMainLoop observes Platform_IsRunning and unwinds to SDL_main.
    return;
#else
    ShutdownSdl();
#ifdef __PSP__
    sceKernelExitGame();
#endif
    exit(0);
#endif
#undef HANDLE_VBLANK_INTRS
}

static void ShutdownSdl(void)
{
#ifdef __ANDROID__
    for (int i = 0; i < DMA_COUNT; i++) DmaStop(i);
#endif
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
    AndroidDestroyControlsTexture();
#endif

    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    sdlTexture = NULL;
    sdlRenderer = NULL;
    sdlWindow = NULL;
}

static void ReadSaveFile(char *path)
{
#ifdef __ANDROID__
    snprintf(sAndroidSavePath, sizeof(sAndroidSavePath), "%s", path);
#endif
    // Check whether the saveFile exists, and create it if not
    sSaveFile = fopen(path, "r+b");
    if (sSaveFile == NULL) {
        sSaveFile = fopen(path, "w+b");
    }

    if (sSaveFile == NULL) {
        memset(FLASH_BASE, 0xFF, sizeof(FLASH_BASE));
#ifdef __ANDROID__
        AndroidSetStartupStage("save_open_failed_using_blank_save");
#endif
        return;
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
#ifdef __ANDROID__
    memcpy(sAndroidSavedFlash, FLASH_BASE, sizeof(FLASH_BASE));
    sAndroidSavedFlashValid = true;
#endif
}

static void StoreSaveFile()
{
#ifdef __ANDROID__
    if (sAndroidSavePath[0] != '\0'
        && (!sAndroidSavedFlashValid || memcmp(sAndroidSavedFlash, FLASH_BASE, sizeof(FLASH_BASE)) != 0)) {
        if (Platform_WriteSaveAtomically(sAndroidSavePath, FLASH_BASE, sizeof(FLASH_BASE))) {
            memcpy(sAndroidSavedFlash, FLASH_BASE, sizeof(FLASH_BASE));
            sAndroidSavedFlashValid = true;
        } else {
            SDL_Log("Unable to persist save: %s", sAndroidSavePath);
        }
    }
#else
    if (sSaveFile != NULL) {
        fseek(sSaveFile, 0, SEEK_SET);
        fwrite(FLASH_BASE, 1, sizeof(FLASH_BASE), sSaveFile);
        fflush(sSaveFile);
    }
#endif
}

void Platform_StoreSaveFile(void) { StoreSaveFile(); }

static void CloseSaveFile()
{
    if (sSaveFile != NULL) {
        fclose(sSaveFile);
        sSaveFile = NULL;
    }
}

static u16 keys;

#ifdef __ANDROID__
static void AndroidClearInput(void)
{
    AndroidTouch_Reset();
    keys = 0;
    REG_KEYINPUT = KEYS_MASK;
}

static void AndroidDestroyControlsTexture(void)
{
    SDL_DestroyTexture(sAndroidControlsTexture);
    SDL_DestroyTexture(sAndroidControlsHighlightTexture);
    sAndroidControlsTexture = NULL;
    sAndroidControlsHighlightTexture = NULL;
    sAndroidControlsTextureW = 0;
    sAndroidControlsTextureH = 0;
}

static bool AndroidEnsureControlsTexture(SDL_Renderer *renderer, int outputW, int outputH)
{
    if (sAndroidControlsTexture != NULL && sAndroidControlsHighlightTexture != NULL
        && sAndroidControlsTextureW == outputW && sAndroidControlsTextureH == outputH)
        return true;
    AndroidDestroyControlsTexture();
    SDL_Surface *surface = AndroidControls_CreateSurface(outputW, outputH, 0);
    if (surface != NULL) {
        sAndroidControlsTexture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    surface = AndroidControls_CreateSurface(outputW, outputH, 1);
    if (surface != NULL) {
        sAndroidControlsHighlightTexture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    if (sAndroidControlsTexture == NULL || sAndroidControlsHighlightTexture == NULL) {
        AndroidDestroyControlsTexture();
        return false;
    }
    SDL_SetTextureBlendMode(sAndroidControlsTexture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(sAndroidControlsHighlightTexture, SDL_BLENDMODE_BLEND);
    sAndroidControlsTextureW = outputW;
    sAndroidControlsTextureH = outputH;
    return true;
}

static void AndroidDrawTouchControls(SDL_Renderer *renderer)
{
    int outputW = 0;
    int outputH = 0;

    if (gAndroidOptions[SA_OPT_TOUCH] == 2 || (gAndroidOptions[SA_OPT_TOUCH] == 0
        && sAndroidController != NULL && SDL_GameControllerGetAttached(sAndroidController))) return;

    if (SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0 || outputW <= 0 || outputH <= 0)
        return;

    sAndroidOutputW = outputW;
    sAndroidOutputH = outputH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (AndroidEnsureControlsTexture(renderer, outputW, outputH)) {
        Uint8 opacity = gAndroidOptions[SA_OPT_OPACITY] * 255 / 100;
        SDL_SetTextureAlphaMod(sAndroidControlsTexture, opacity);
        SDL_SetTextureAlphaMod(sAndroidControlsHighlightTexture, opacity);
        SDL_RenderCopy(renderer, sAndroidControlsTexture, NULL, NULL);
        AndroidControls_DrawHighlights(renderer, sAndroidControlsHighlightTexture, outputW, outputH, AndroidTouch_Peek());
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

static void AndroidDrawDiagnostics(SDL_Renderer *renderer)
{
    /* Small bitmap counter: measured SDL presentations per elapsed second. */
    static const u16 digits[10] = { 31599, 11415, 29671, 29647, 23497, 31183, 31215, 29257, 31727, 31695 };
    const u16 glyphs[6] = { 31140, 31524, 31183, digits[(sAndroidFps / 100) % 10],
                           digits[(sAndroidFps / 10) % 10], digits[sAndroidFps % 10] };
    int unit = MAX(1, sAndroidOutputH / 240);
    int x = sAndroidOutputW / 2 - 13 * unit, y = 3 * unit;
    SDL_Rect panel = { x - 2 * unit, y - unit, 28 * unit, 7 * unit };
    SDL_SetRenderDrawColor(renderer, 0, 12, 32, 220);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 224, 64, 255);
    for (int c = 0; c < 6; c++) for (int row = 0; row < 5; row++) for (int col = 0; col < 3; col++) {
        if (glyphs[c] & (1 << (14 - row * 3 - col))) {
            SDL_Rect pixel = { x + (c * 4 + col) * unit, y + row * unit, unit, unit };
            SDL_RenderFillRect(renderer, &pixel);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
    if (sAndroidAudioDevice == 0 || sAndroidSuspended || sAndroidOptionsOpen) {
        return;
    }

    if (SDL_GetQueuedAudioSize(sAndroidAudioDevice) > (bytesCount * 10)) {
        SDL_ClearQueuedAudio(sAndroidAudioDevice);
    }

    /* Fixed-size chunks avoid allocating every frame and preserve timing at
     * zero volume. Music/effect gain is applied per source in the mixer. */
    s16 mixed[2048];
    uint32_t samples = bytesCount / sizeof(s16);
    while (samples) {
        uint32_t count = samples < ARRAY_COUNT(mixed) ? samples : ARRAY_COUNT(mixed);
        for (uint32_t i = 0; i < count; i++) mixed[i] = (s32)data[i] * gAndroidOptions[SA_OPT_MASTER] / 100;
        SDL_QueueAudio(sAndroidAudioDevice, mixed, count * sizeof(s16));
        samples -= count;
        data += count;
    }
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
            case SDL_USEREVENT:
                if (event.user.code == SA_OPTIONS_EVENT && event.user.data1) {
                    AndroidOptionsMessage *message = event.user.data1;
                    AndroidOptions_Apply(message->values);
                    sAndroidOptionsOpen = message->open;
                    SDL_Log("SA Android options applied open=%d screen=%d opacity=%d", sAndroidOptionsOpen,
                            gAndroidOptions[SA_OPT_SCREEN], gAndroidOptions[SA_OPT_OPACITY]);
                    SDL_free(message);
                    AndroidClearInput();
                    AndroidDestroyControlsTexture();
                    SDL_SetTextureScaleMode(sdlTexture, gAndroidOptions[SA_OPT_FILTER] ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
                    lastGameTime = SDL_GetPerformanceCounter();
                    accumulator = 0.0;
                    newFrameRequested = FALSE;
                    if (sAndroidAudioDevice) {
                        SDL_ClearQueuedAudio(sAndroidAudioDevice);
                        SDL_PauseAudioDevice(sAndroidAudioDevice, sAndroidOptionsOpen || sAndroidSuspended);
                    }
                }
                break;
            case SDL_APP_WILLENTERBACKGROUND:
            case SDL_APP_DIDENTERBACKGROUND:
                StoreSaveFile();
                AndroidClearInput();
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
                AndroidClearInput();
                sAndroidSuspended = false;
                newFrameRequested = FALSE;
                lastGameTime = SDL_GetPerformanceCounter();
                accumulator = 0.0;
                if (sAndroidAudioDevice != 0) {
                    SDL_PauseAudioDevice(sAndroidAudioDevice, sAndroidOptionsOpen);
                }
                break;
            case SDL_RENDER_TARGETS_RESET:
            case SDL_RENDER_DEVICE_RESET:
                AndroidDestroyControlsTexture();
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
                if (!sAndroidOptionsOpen) AndroidTouch_Finger(event.tfinger.touchId, event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, 1, sAndroidOutputW, sAndroidOutputH);
                break;
            case SDL_FINGERMOTION:
                if (!sAndroidOptionsOpen) AndroidTouch_Finger(event.tfinger.touchId, event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, 2, sAndroidOutputW, sAndroidOutputH);
                break;
            case SDL_FINGERUP:
                AndroidTouch_Finger(event.tfinger.touchId, event.tfinger.fingerId, event.tfinger.x, event.tfinger.y, 0, sAndroidOutputW, sAndroidOutputH);
                break;
#endif
            case SDL_WINDOWEVENT:
#ifdef __ANDROID__
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) AndroidClearInput();
#endif
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
#ifdef __ANDROID__
                    // Android owns the native surface size. Never resize it back
                    // to an integer multiple of the GBA viewport; doing so leaves
                    // uncovered strips and desynchronizes touch coordinates.
                    AndroidClearInput();
                    sAndroidOutputW = event.window.data1;
                    sAndroidOutputH = event.window.data2;
                    AndroidDestroyControlsTexture();
                    videoScaleChanged = false;
#else
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
#endif
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
    return keys | AndroidTouch_Consume() | AndroidPollControllerButtons();
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

#ifdef __ANDROID__
    if (sAndroidDirectBgr555) {
        SDL_UpdateTexture(texture, NULL, gameImage, DISPLAY_WIDTH * sizeof(Uint16));
    } else {
        size_t i;
        AndroidInitColorLut();

        for (i = 0; i < ARRAY_COUNT(gameImage); i++) {
            sAndroidFrameRGBA[i] = sAndroidColorLut[gameImage[i] & 0x7FFF];
        }

        SDL_UpdateTexture(texture, NULL, sAndroidFrameRGBA, DISPLAY_WIDTH * sizeof(Uint32));
    }
#else
    SDL_UpdateTexture(texture, NULL, gameImage, DISPLAY_WIDTH * sizeof(Uint16));
#endif

    REG_VCOUNT = DISPLAY_HEIGHT + 1; // prep for being in VBlank period
}
