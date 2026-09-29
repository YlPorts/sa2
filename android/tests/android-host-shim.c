// Test-only bridge: run the Android-specific SDL backend on the host. SDL uses
// dummy video/audio drivers; JNI and physical Android drivers are not tested.
#define SDL_MAIN_HANDLED
#include <SDL.h>
int SDL_main(int argc, char **argv);
const char *SDL_AndroidGetInternalStoragePath(void) { return "."; }
int main(int argc, char **argv)
{
    SDL_SetMainReady();
    return SDL_main(argc, argv);
}
