// Test-only bridge: run the Android-specific SDL backend on the host. SDL uses
// dummy video/audio drivers; JNI and physical Android drivers are not tested.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
int SDL_main(int argc, char **argv);
const char *SDL_AndroidGetInternalStoragePath(void) { return "."; }
int main(int argc, char **argv)
{
    SDL_SetMainReady();
    if (getenv("SA_TEST_RECREATE")) {
        for (int i = 0; i < 2; i++) {
            if (SDL_main(argc, argv) != 0) return 1;
        }
        fprintf(stderr, "Passed: Android backend initialized, quit, cleaned up and restarted twice in the same process\n");
        return 0;
    }
    return SDL_main(argc, argv);
}
