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
#ifdef __ANDROID__
#include "platform/shared/save_file.h"
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

#ifdef __ANDROID__
static SDL_AudioDeviceID sAndroidAudioDevice = 0;
static bool sAndroidSuspended = false;
static SDL_GameController *sAndroidController = NULL;
static char sAndroidStagePath[1024];
static bool sAndroidNativeUiCrop = false;
static bool sAndroidDirectBgr555 = false;
static Uint32 sAndroidColorLut[0x8000];
static bool sAndroidColorLutReady = false;
static int sAndroidOutputW = 0;
static int sAndroidOutputH = 0;
static SDL_Texture *sAndroidControlsTexture = NULL;
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
    // Android uses the full renderer output. This keeps drawing and normalized
    // finger coordinates in the same space and intentionally fills ultrawide.
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
#endif

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
#ifdef __ANDROID__
        // Do not swap duplicate frames while waiting for the next 60 Hz game
        // tick. Repeated swaps on a 90/120 Hz panel create uneven cadence.
        if (!frameDrawn) {
            SDL_Delay(1);
            continue;
        }
#endif
        SDL_RenderClear(sdlRenderer);
#ifdef __ANDROID__
        if (sAndroidNativeUiCrop) {
            SDL_Rect nativeUiRect = { 0, 0, 240, 160 };
            SDL_RenderCopy(sdlRenderer, sdlTexture, &nativeUiRect, NULL);
        } else {
            SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
        }

        AndroidDrawTouchControls(sdlRenderer);
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
#endif
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

static void AndroidClearInput(void)
{
    memset(sAndroidTouches, 0, sizeof(sAndroidTouches));
    sAndroidTouchKeys = 0;
    keys = 0;
    REG_KEYINPUT = KEYS_MASK;
}

static int AndroidControlBaseSize(void)
{
    int w = sAndroidOutputW;
    int h = sAndroidOutputH;

    if (w <= 0 || h <= 0)
        return 0;

    return (w < h) ? w : h;
}

static bool AndroidPointInCircle(float px, float py, float cx, float cy, float radius)
{
    const float dx = px - cx;
    const float dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

static bool AndroidPointInRect(float px, float py, float cx, float cy, float halfW, float halfH)
{
    return px >= (cx - halfW) && px <= (cx + halfW) && py >= (cy - halfH) && py <= (cy + halfH);
}

static u16 AndroidTouchMask(float x, float y)
{
    u16 mask = 0;
    const int outputW = sAndroidOutputW;
    const int outputH = sAndroidOutputH;
    const int base = AndroidControlBaseSize();
    float px;
    float py;

    if (base <= 0)
        return 0;

    px = x * outputW;
    py = y * outputH;

    {
        const float unit = base * 0.095f;
        const float cx = base * 0.22f;
        const float cy = outputH - base * 0.22f;
        const float dx = px - cx;
        const float dy = py - cy;
        const float reach = unit * 1.72f;
        const float dead = unit * 0.22f;

        // Slightly larger than the visible cross. Independent axes make
        // diagonals natural without requiring a separate diagonal button.
        if (fabsf(dx) <= reach && fabsf(dy) <= reach) {
            if (dx < -dead)
                mask |= DPAD_LEFT;
            if (dx > dead)
                mask |= DPAD_RIGHT;
            if (dy < -dead)
                mask |= DPAD_UP;
            if (dy > dead)
                mask |= DPAD_DOWN;
        }
    }

    {
        const float hitRadius = base * 0.090f;
        const float aX = outputW - base * 0.18f;
        const float aY = outputH - base * 0.28f;
        const float bX = outputW - base * 0.32f;
        const float bY = outputH - base * 0.16f;

        if (AndroidPointInCircle(px, py, aX, aY, hitRadius))
            mask |= A_BUTTON;
        if (AndroidPointInCircle(px, py, bX, bY, hitRadius))
            mask |= B_BUTTON;
    }

    {
        const float halfW = base * 0.145f;
        const float halfH = base * 0.055f;
        const float yPos = base * 0.085f;
        const float leftX = base * 0.19f;
        const float rightX = outputW - base * 0.19f;

        if (AndroidPointInRect(px, py, leftX, yPos, halfW, halfH))
            mask |= L_BUTTON;
        if (AndroidPointInRect(px, py, rightX, yPos, halfW, halfH))
            mask |= R_BUTTON;
    }

    {
        const float halfW = base * 0.085f;
        const float halfH = base * 0.045f;
        const float yPos = outputH - base * 0.070f;
        const float selectX = outputW * 0.5f - base * 0.10f;
        const float startX = outputW * 0.5f + base * 0.10f;

        if (AndroidPointInRect(px, py, selectX, yPos, halfW, halfH))
            mask |= SELECT_BUTTON;
        if (AndroidPointInRect(px, py, startX, yPos, halfW, halfH))
            mask |= START_BUTTON;
    }

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

static void AndroidSetControlColor(SDL_Renderer *renderer, bool pressed, bool border)
{
    if (border) {
        SDL_SetRenderDrawColor(renderer, 210, 214, 220, pressed ? 190 : 112);
    } else {
        SDL_SetRenderDrawColor(renderer, 24, 27, 31, pressed ? 150 : 82);
    }
}

static void AndroidFillCircle(SDL_Renderer *renderer, int cx, int cy, int radius)
{
    int y;

    for (y = -radius; y <= radius; y++) {
        const int span = (int)sqrtf((float)(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
}

static void AndroidFillCapsule(SDL_Renderer *renderer, int cx, int cy, int width, int height)
{
    const int radius = height / 2;
    SDL_Rect middle = { cx - width / 2 + radius, cy - radius, width - radius * 2, height };

    SDL_RenderFillRect(renderer, &middle);
    AndroidFillCircle(renderer, cx - width / 2 + radius, cy, radius);
    AndroidFillCircle(renderer, cx + width / 2 - radius, cy, radius);
}

static void AndroidDrawThickLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness)
{
    int ox;
    int oy;
    const int r = thickness / 2;

    for (oy = -r; oy <= r; oy++) {
        for (ox = -r; ox <= r; ox++) {
            SDL_RenderDrawLine(renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
        }
    }
}

static void AndroidDrawLetterA(SDL_Renderer *renderer, int cx, int cy, int size)
{
    const int h = size / 2;
    AndroidDrawThickLine(renderer, cx - h, cy + h, cx, cy - h, 2);
    AndroidDrawThickLine(renderer, cx, cy - h, cx + h, cy + h, 2);
    AndroidDrawThickLine(renderer, cx - h / 2, cy + h / 5, cx + h / 2, cy + h / 5, 2);
}

static void AndroidDrawLetterB(SDL_Renderer *renderer, int cx, int cy, int size)
{
    const int h = size / 2;
    AndroidDrawThickLine(renderer, cx - h / 2, cy - h, cx - h / 2, cy + h, 2);
    AndroidDrawThickLine(renderer, cx - h / 2, cy - h, cx + h / 3, cy - h, 2);
    AndroidDrawThickLine(renderer, cx - h / 2, cy, cx + h / 3, cy, 2);
    AndroidDrawThickLine(renderer, cx - h / 2, cy + h, cx + h / 3, cy + h, 2);
    AndroidDrawThickLine(renderer, cx + h / 3, cy - h, cx + h / 2, cy - h / 2, 2);
    AndroidDrawThickLine(renderer, cx + h / 2, cy - h / 2, cx + h / 3, cy, 2);
    AndroidDrawThickLine(renderer, cx + h / 3, cy, cx + h / 2, cy + h / 2, 2);
    AndroidDrawThickLine(renderer, cx + h / 2, cy + h / 2, cx + h / 3, cy + h, 2);
}

static void AndroidDrawLetterL(SDL_Renderer *renderer, int cx, int cy, int size)
{
    const int h = size / 2;
    AndroidDrawThickLine(renderer, cx - h / 3, cy - h, cx - h / 3, cy + h, 2);
    AndroidDrawThickLine(renderer, cx - h / 3, cy + h, cx + h / 2, cy + h, 2);
}

static void AndroidDrawLetterR(SDL_Renderer *renderer, int cx, int cy, int size)
{
    const int h = size / 2;
    AndroidDrawThickLine(renderer, cx - h / 2, cy - h, cx - h / 2, cy + h, 2);
    AndroidDrawThickLine(renderer, cx - h / 2, cy - h, cx + h / 3, cy - h, 2);
    AndroidDrawThickLine(renderer, cx + h / 3, cy - h, cx + h / 2, cy - h / 3, 2);
    AndroidDrawThickLine(renderer, cx + h / 2, cy - h / 3, cx - h / 2, cy, 2);
    AndroidDrawThickLine(renderer, cx, cy, cx + h / 2, cy + h, 2);
}

static void AndroidDrawArrow(SDL_Renderer *renderer, int cx, int cy, int size, u16 direction, bool pressed)
{
    const int half = size / 2;
    AndroidSetControlColor(renderer, pressed, true);

    if (direction == DPAD_LEFT) {
        AndroidDrawThickLine(renderer, cx + half / 2, cy - half, cx - half / 2, cy, 2);
        AndroidDrawThickLine(renderer, cx - half / 2, cy, cx + half / 2, cy + half, 2);
    } else if (direction == DPAD_RIGHT) {
        AndroidDrawThickLine(renderer, cx - half / 2, cy - half, cx + half / 2, cy, 2);
        AndroidDrawThickLine(renderer, cx + half / 2, cy, cx - half / 2, cy + half, 2);
    } else if (direction == DPAD_UP) {
        AndroidDrawThickLine(renderer, cx - half, cy + half / 2, cx, cy - half / 2, 2);
        AndroidDrawThickLine(renderer, cx, cy - half / 2, cx + half, cy + half / 2, 2);
    } else if (direction == DPAD_DOWN) {
        AndroidDrawThickLine(renderer, cx - half, cy - half / 2, cx, cy + half / 2, 2);
        AndroidDrawThickLine(renderer, cx, cy + half / 2, cx + half, cy - half / 2, 2);
    }
}

static void AndroidDrawDpad(SDL_Renderer *renderer, int base, int outputH)
{
    const int unit = (int)(base * 0.095f);
    const int cx = (int)(base * 0.22f);
    const int cy = outputH - (int)(base * 0.22f);
    const int border = 3;
    SDL_Rect vOuter = { cx - unit / 2, cy - (unit * 3) / 2, unit, unit * 3 };
    SDL_Rect hOuter = { cx - (unit * 3) / 2, cy - unit / 2, unit * 3, unit };
    SDL_Rect vInner = { vOuter.x + border, vOuter.y + border, vOuter.w - border * 2, vOuter.h - border * 2 };
    SDL_Rect hInner = { hOuter.x + border, hOuter.y + border, hOuter.w - border * 2, hOuter.h - border * 2 };

    AndroidSetControlColor(renderer, false, true);
    SDL_RenderFillRect(renderer, &vOuter);
    SDL_RenderFillRect(renderer, &hOuter);

    AndroidSetControlColor(renderer, false, false);
    SDL_RenderFillRect(renderer, &vInner);
    SDL_RenderFillRect(renderer, &hInner);

    if (sAndroidTouchKeys & DPAD_LEFT) {
        SDL_Rect r = { hInner.x, hInner.y, unit, hInner.h };
        AndroidSetControlColor(renderer, true, false);
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_RIGHT) {
        SDL_Rect r = { hInner.x + hInner.w - unit, hInner.y, unit, hInner.h };
        AndroidSetControlColor(renderer, true, false);
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_UP) {
        SDL_Rect r = { vInner.x, vInner.y, vInner.w, unit };
        AndroidSetControlColor(renderer, true, false);
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_DOWN) {
        SDL_Rect r = { vInner.x, vInner.y + vInner.h - unit, vInner.w, unit };
        AndroidSetControlColor(renderer, true, false);
        SDL_RenderFillRect(renderer, &r);
    }

    AndroidDrawArrow(renderer, cx - unit, cy, unit / 3, DPAD_LEFT, (sAndroidTouchKeys & DPAD_LEFT) != 0);
    AndroidDrawArrow(renderer, cx + unit, cy, unit / 3, DPAD_RIGHT, (sAndroidTouchKeys & DPAD_RIGHT) != 0);
    AndroidDrawArrow(renderer, cx, cy - unit, unit / 3, DPAD_UP, (sAndroidTouchKeys & DPAD_UP) != 0);
    AndroidDrawArrow(renderer, cx, cy + unit, unit / 3, DPAD_DOWN, (sAndroidTouchKeys & DPAD_DOWN) != 0);
}

static void AndroidDrawFaceButton(SDL_Renderer *renderer, int cx, int cy, int radius, bool pressed, char label)
{
    AndroidSetControlColor(renderer, pressed, true);
    AndroidFillCircle(renderer, cx, cy, radius);
    AndroidSetControlColor(renderer, pressed, false);
    AndroidFillCircle(renderer, cx, cy, radius - 3);

    SDL_SetRenderDrawColor(renderer, 225, 228, 232, pressed ? 240 : 170);
    if (label == 'A')
        AndroidDrawLetterA(renderer, cx, cy, radius / 2);
    else
        AndroidDrawLetterB(renderer, cx, cy, radius / 2);
}

static void AndroidDrawShoulderButton(SDL_Renderer *renderer, int cx, int cy, int width, int height, bool pressed, char label)
{
    AndroidSetControlColor(renderer, pressed, true);
    AndroidFillCapsule(renderer, cx, cy, width, height);
    AndroidSetControlColor(renderer, pressed, false);
    AndroidFillCapsule(renderer, cx, cy, width - 6, height - 6);

    SDL_SetRenderDrawColor(renderer, 225, 228, 232, pressed ? 240 : 160);
    if (label == 'L')
        AndroidDrawLetterL(renderer, cx, cy, height / 2);
    else
        AndroidDrawLetterR(renderer, cx, cy, height / 2);
}

static void AndroidDrawMiniButton(SDL_Renderer *renderer, int cx, int cy, int width, int height, bool pressed, bool isStart)
{
    AndroidSetControlColor(renderer, pressed, true);
    AndroidFillCapsule(renderer, cx, cy, width, height);
    AndroidSetControlColor(renderer, pressed, false);
    AndroidFillCapsule(renderer, cx, cy, width - 5, height - 5);

    SDL_SetRenderDrawColor(renderer, 225, 228, 232, pressed ? 230 : 145);
    if (isStart) {
        const int s = height / 4;
        AndroidDrawThickLine(renderer, cx - s / 2, cy - s, cx + s, cy, 2);
        AndroidDrawThickLine(renderer, cx + s, cy, cx - s / 2, cy + s, 2);
    } else {
        const int half = width / 6;
        AndroidDrawThickLine(renderer, cx - half, cy - 3, cx + half, cy - 3, 2);
        AndroidDrawThickLine(renderer, cx - half, cy + 4, cx + half, cy + 4, 2);
    }
}

static void AndroidDrawStaticControlsGeometry(SDL_Renderer *renderer, int outputW, int outputH)
{
    const int base = (outputW < outputH) ? outputW : outputH;
    const int radius = (int)(base * 0.065f);

    AndroidDrawDpad(renderer, base, outputH);
    AndroidDrawFaceButton(renderer, outputW - (int)(base * 0.18f), outputH - (int)(base * 0.28f), radius, false, 'A');
    AndroidDrawFaceButton(renderer, outputW - (int)(base * 0.32f), outputH - (int)(base * 0.16f), radius, false, 'B');

    AndroidDrawShoulderButton(renderer, (int)(base * 0.19f), (int)(base * 0.085f),
                              (int)(base * 0.23f), (int)(base * 0.060f), false, 'L');
    AndroidDrawShoulderButton(renderer, outputW - (int)(base * 0.19f), (int)(base * 0.085f),
                              (int)(base * 0.23f), (int)(base * 0.060f), false, 'R');

    AndroidDrawMiniButton(renderer, (int)(outputW * 0.5f - base * 0.10f), outputH - (int)(base * 0.070f),
                          (int)(base * 0.105f), (int)(base * 0.036f), false, false);
    AndroidDrawMiniButton(renderer, (int)(outputW * 0.5f + base * 0.10f), outputH - (int)(base * 0.070f),
                          (int)(base * 0.105f), (int)(base * 0.036f), false, true);
}

static void AndroidDestroyControlsTexture(void)
{
    if (sAndroidControlsTexture != NULL) {
        SDL_DestroyTexture(sAndroidControlsTexture);
        sAndroidControlsTexture = NULL;
    }

    sAndroidControlsTextureW = 0;
    sAndroidControlsTextureH = 0;
}

static bool AndroidEnsureControlsTexture(SDL_Renderer *renderer, int outputW, int outputH)
{
    SDL_Texture *previousTarget;
    u16 savedKeys;

    if (sAndroidControlsTexture != NULL
        && sAndroidControlsTextureW == outputW
        && sAndroidControlsTextureH == outputH)
        return true;

    AndroidDestroyControlsTexture();

    sAndroidControlsTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                SDL_TEXTUREACCESS_TARGET, outputW, outputH);
    if (sAndroidControlsTexture == NULL)
        return false;

    SDL_SetTextureBlendMode(sAndroidControlsTexture, SDL_BLENDMODE_BLEND);
    previousTarget = SDL_GetRenderTarget(renderer);

    if (SDL_SetRenderTarget(renderer, sAndroidControlsTexture) != 0) {
        AndroidDestroyControlsTexture();
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    savedKeys = sAndroidTouchKeys;
    sAndroidTouchKeys = 0;
    AndroidDrawStaticControlsGeometry(renderer, outputW, outputH);
    sAndroidTouchKeys = savedKeys;

    SDL_SetRenderTarget(renderer, previousTarget);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    sAndroidControlsTextureW = outputW;
    sAndroidControlsTextureH = outputH;
    return true;
}

static void AndroidDrawPressedHighlights(SDL_Renderer *renderer, int outputW, int outputH)
{
    const int base = (outputW < outputH) ? outputW : outputH;
    const int radius = (int)(base * 0.055f);
    const int unit = (int)(base * 0.095f);
    const int cx = (int)(base * 0.22f);
    const int cy = outputH - (int)(base * 0.22f);

    SDL_SetRenderDrawColor(renderer, 235, 238, 242, 42);

    if (sAndroidTouchKeys & A_BUTTON)
        AndroidFillCircle(renderer, outputW - (int)(base * 0.18f), outputH - (int)(base * 0.28f), radius);
    if (sAndroidTouchKeys & B_BUTTON)
        AndroidFillCircle(renderer, outputW - (int)(base * 0.32f), outputH - (int)(base * 0.16f), radius);

    if (sAndroidTouchKeys & DPAD_LEFT) {
        SDL_Rect r = { cx - (unit * 3) / 2, cy - unit / 2, unit, unit };
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_RIGHT) {
        SDL_Rect r = { cx + unit / 2, cy - unit / 2, unit, unit };
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_UP) {
        SDL_Rect r = { cx - unit / 2, cy - (unit * 3) / 2, unit, unit };
        SDL_RenderFillRect(renderer, &r);
    }
    if (sAndroidTouchKeys & DPAD_DOWN) {
        SDL_Rect r = { cx - unit / 2, cy + unit / 2, unit, unit };
        SDL_RenderFillRect(renderer, &r);
    }

    if (sAndroidTouchKeys & L_BUTTON)
        AndroidFillCapsule(renderer, (int)(base * 0.19f), (int)(base * 0.085f),
                           (int)(base * 0.20f), (int)(base * 0.044f));
    if (sAndroidTouchKeys & R_BUTTON)
        AndroidFillCapsule(renderer, outputW - (int)(base * 0.19f), (int)(base * 0.085f),
                           (int)(base * 0.20f), (int)(base * 0.044f));
    if (sAndroidTouchKeys & SELECT_BUTTON)
        AndroidFillCapsule(renderer, (int)(outputW * 0.5f - base * 0.10f), outputH - (int)(base * 0.070f),
                           (int)(base * 0.085f), (int)(base * 0.026f));
    if (sAndroidTouchKeys & START_BUTTON)
        AndroidFillCapsule(renderer, (int)(outputW * 0.5f + base * 0.10f), outputH - (int)(base * 0.070f),
                           (int)(base * 0.085f), (int)(base * 0.026f));
}

static void AndroidDrawTouchControls(SDL_Renderer *renderer)
{
    int outputW = 0;
    int outputH = 0;

    if (sAndroidController != NULL && SDL_GameControllerGetAttached(sAndroidController))
        return;

    if (SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0 || outputW <= 0 || outputH <= 0)
        return;

    sAndroidOutputW = outputW;
    sAndroidOutputH = outputH;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (AndroidEnsureControlsTexture(renderer, outputW, outputH))
        SDL_RenderCopy(renderer, sAndroidControlsTexture, NULL, NULL);
    else
        AndroidDrawStaticControlsGeometry(renderer, outputW, outputH);

    AndroidDrawPressedHighlights(renderer, outputW, outputH);

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
                    SDL_PauseAudioDevice(sAndroidAudioDevice, 0);
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
#ifdef __ANDROID__
                    // Android owns the native surface size. Never resize it back
                    // to an integer multiple of the GBA viewport; doing so leaves
                    // uncovered strips and desynchronizes touch coordinates.
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
