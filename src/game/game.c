#include "global.h"
#include "core.h"
#include "flags.h"
#include "task.h"

#include "game/game.h"

#if (GAME == GAME_SA1)
#include "game/sa1/save.h"
#include "game/sa1/stage/tilemap_table.h"
#include "game/sa1/stage/ui_rendering.h"
#include "game/sa1/menus/title_screen.h"
#include "game/sa1/menus/options_screen.h"
#include "game/sa1/ui/character_select.h"
#include "game/sa1/menus/mp_mode_select_2.h"
#elif (GAME == GAME_SA2)
#include "game/sa2/save.h"
#include "game/sa2/stage/tilemap_table.h"
#include "game/sa2/title_screen.h"
#include "game/sa2/options_screen.h"
#include "game/sa2/collect_rings/singlepak_connection.h"
#endif

#include "game/shared/stage/entities_manager.h"

#include "game/decomp_credits.h"
#include "game/shared/stage/stage.h"
#include "game/shared/stage/player.h"
#include "game/shared/stage/camera.h"
#include "game/shared/stage/underwater_effects.h"
#include "game/shared/stage/dust_effect_braking.h"
#include "game/shared/stage/rings_scatter.h"

#if COLLECT_RINGS_ROM
#include "game/shared/collect_rings/results.h"
#endif

#include "game/shared/stage/water_effects.h"
#include "game/dummy_task.h"

#ifdef __ANDROID__
extern void Platform_SetStartupStage(const char *stage);
#define GAME_STARTUP_STAGE(name) Platform_SetStartupStage(name)
#else
#define GAME_STARTUP_STAGE(name) ((void)0)
#endif

#if TAS_TESTING
#include "data/sa2/recordings.h"
#endif

#if (GAME == GAME_SA1)
#include "data/sa1/sprite_tables.h"

void CreateEmptySaveGame(void);

#elif (GAME == GAME_SA2)
#include "data/sa2/sprite_tables.h"
#endif

#ifndef COLLECT_RINGS_ROM
void GameInit(void)
{
    u32 i;
    bool32 hasProfile = FALSE;

    GAME_STARTUP_STAGE("game_init_enter");

    // NOTE: cast because of const
    gTilemapsRef = (Tilemap **)gTilemaps;
    gRefSpriteTables = &gSpriteTables;

#if (GAME != GAME_SA1)
    gBgOffsetsPrimary = gBgOffsetsBuffer[0];
    gBgOffsetsSecondary = gBgOffsetsBuffer[1];
#endif

    gStageFlags = gPrevStageFlags = STAGE_FLAG__CLEAR;

#if (GAME == GAME_SA1)
    // "Cheat Code" Tails
    gTailsEnabled = FALSE;
#endif

    gRingsScatterTask = NULL;
    gDummyTask = NULL;
    gGameStageTask = NULL;
    gPlayer.spriteTask = NULL;
#if (GAME == GAME_SA1)
    // "Cheat Code" Tails
    gPartner.spriteTask = NULL;
#endif
    gCamera.movementTask = NULL;

    gDustEffectBrakingTask.t = NULL;
    gWater.t = NULL;

    SA2_LABEL(gUnknown_0300543C) = 0;
    gGameMode = GAME_MODE_SINGLE_PLAYER;
    gEntitiesManagerTask = NULL;
    gSmallAirBubbleCount = 0;
    gDemoPlayCounter = 0;
    gDestroySpotlights = FALSE;

    for (i = 0; i < 4; i++) {
        gMultiplayerPlayerTasks[i] = NULL;
    }

#if (GAME == GAME_SA1)
    gTask_03006240 = 0;
#endif

    for (i = 0; i < 4; i++) {
        gMultiplayerCharacters[i] = 0;
        gMultiplayerRanks[i] = 0;
        gMultiplayerMissingHeartbeats[i] = 0;
    }

#if (GAME == GAME_SA2)
    GAME_STARTUP_STAGE("before_save_init");
    SaveInit();
    GAME_STARTUP_STAGE("after_save_init");
#endif

#if (GAME == GAME_SA1)
    if (SaveGameExists() != TRUE) {
        CreateEmptySaveGame();
        hasProfile = TRUE;
    }
#elif (GAME == GAME_SA2)
    GAME_STARTUP_STAGE("before_save_exists");
    if (SaveGameExists()) {
        GAME_STARTUP_STAGE("before_load_save");
        LoadSaveGame();
        GAME_STARTUP_STAGE("after_load_save");
        hasProfile = TRUE;
    }
    GAME_STARTUP_STAGE("after_save_exists");
#endif

    // This flag is only set in EngineInit
    if (gFlags & FLAGS_200) {
        ShowSinglePakResults();
        return;
    }

#if TAS_TESTING
    gInputPlaybackData = gDemoRecordings[4];
    InputRecorderLoadTape();
    gInputRecorder.mode = RECORDER_PLAYBACK;
    CreateNewProfileScreen();

    // Use this snippet to skip to a specific level
    // zone 7 act 1 starts at frame 44928
    // gInputRecorder.playbackHead = 44928;
    // gCurrentLevel = LEVEL_INDEX(ZONE_7, ACT_1);
    // ApplyGameStageSettings();
    // GameStageStart();

#elif ENABLE_DECOMP_CREDITS
    // gCurrentLevel = LEVEL_INDEX(ZONE_4, ACT_1);
    // ApplyGameStageSettings();
    // GameStageStart();
    CreateDecompCreditsScreen(hasProfile);
#else
    if (gFlags & FLAGS_NO_FLASH_MEMORY) {
#if (GAME == GAME_SA1)
#if DEBUG
        CreateCharacterSelectionScreen(CHARACTER_TAILS);
#else
        CreateSegaLogo();
#endif
        for (i = 0; i < NUM_CHARACTERS; i++) {
            LOADED_SAVE->unlockedLevels[i] = 0xF;
        }
#else
        CreateTitleScreen();
        LoadCompletedSaveGame();
#endif
        return;
    }

#if (GAME == GAME_SA1)
    if (hasProfile) {
        CreateEditLanguageScreen(1);
        return;
    }
#elif (GAME == GAME_SA2)
    if (!hasProfile) {
        GAME_STARTUP_STAGE("before_new_profile");
        CreateNewProfileScreen();
        GAME_STARTUP_STAGE("after_new_profile");
        return;
    }
#endif

    // When a special button combination is pressed
    // skip the intro and go straight to the
    // title screen
    if (gFlags & FLAGS_SKIP_INTRO) {
#if (GAME == GAME_SA1)
#if DEBUG
        CreateCharacterSelectionScreen(CHARACTER_TAILS);
#else
        CreateTitleScreen(1);
#endif
#elif (GAME == GAME_SA2)
        CreateTitleScreenAndSkipIntro();
#endif
        gFlags &= ~FLAGS_SKIP_INTRO;
        return;
    }

#if (GAME == GAME_SA1)
#if DEBUG
    CreateCharacterSelectionScreen(CHARACTER_TAILS);
#else
    CreateSegaLogo();
#endif
#elif (GAME == GAME_SA2)
    GAME_STARTUP_STAGE("before_title_screen");
    CreateTitleScreen();
    GAME_STARTUP_STAGE("after_title_screen");
#endif
#endif
    GAME_STARTUP_STAGE("game_init_complete");
}
#else
void GameInit(void)
{
    u32 i;
#ifndef NON_MATCHING
    u8 *multiSioEnabled;
#endif
    gGameMode = 5;

    gBgOffsetsPrimary = gBgOffsetsBuffer[0];
    gBgOffsetsSecondary = gBgOffsetsBuffer[1];

    i = 0;
#ifndef NON_MATCHING
    multiSioEnabled = &gMultiSioEnabled;
#endif

    for (; i < 4; i++) {
        gMultiplayerCharacters[i] = 0;
        gMPRingCollectWins[i] = 0;
        gMultiplayerRanks[i] = i;
        gMultiplayerMissingHeartbeats[i] = 0;
    }
#ifndef NON_MATCHING
    *multiSioEnabled = TRUE;
#else
    gMultiSioEnabled = TRUE;
#endif
    // gMultiSioStatusFlags = 0;
    MultiSioInit(gMultiSioStatusFlags);
    MultiSioStart();
    CreateMultiplayerSinglePakResultsScreen(0);
}
#endif

#if (GAME == GAME_SA1)
void CreateEmptySaveGame(void)
{
    u32 i;

    DmaFill32(3, 0, LOADED_SAVE, sizeof(*LOADED_SAVE));
    LOADED_SAVE->unk4 = 0;
    LOADED_SAVE->unk420 = 50000;
    LOADED_SAVE->difficultyLevel = DIFFICULTY_NORMAL;

    for (i = 0; i < ARRAY_COUNT(LOADED_SAVE->unlockedLevels); i++) {
        LOADED_SAVE->unlockedLevels[i] = 0;
    }

    for (i = 0; i < 10; i++) {
        u32 charIndex;

        LOADED_SAVE->multiplayerScores[i].data.split.wins |= 0xFF;
        for (charIndex = 0; charIndex < (s32)ARRAY_COUNT(LOADED_SAVE->multiplayerScores[i].data.split.playerName); charIndex++) {
            LOADED_SAVE->multiplayerScores[i].data.split.playerName[charIndex] = ' ';
        }
    }

    DmaFill32(3, MAX_COURSE_TIME, &LOADED_SAVE->timeRecords, sizeof(LOADED_SAVE->timeRecords));

    LOADED_SAVE->uiLanguage = UILANG_DEFAULT;
    LOADED_SAVE->language = LANG_JAPANESE;
    LOADED_SAVE->timeLimitDisabled = 0;
    LOADED_SAVE->btnConfig = BTNCONFIG_NORMAL;
    LOADED_SAVE->score = 0;
}
#endif
