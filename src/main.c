#include "core.h"
#include "game/game.h"

#ifdef __ANDROID__
extern void Platform_SetStartupStage(const char *stage);
#define STARTUP_STAGE(name) Platform_SetStartupStage(name)
#else
#define STARTUP_STAGE(name) ((void)0)
#endif

void AgbMain(void)
{
    STARTUP_STAGE("before_engine_init");
    EngineInit();
    STARTUP_STAGE("after_engine_init");

    STARTUP_STAGE("before_game_init");
    GameInit();
    STARTUP_STAGE("after_game_init");

    STARTUP_STAGE("before_engine_loop");
    EngineMainLoop();
    STARTUP_STAGE("after_engine_loop");
}
