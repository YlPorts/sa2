#if defined(__ANDROID__) || defined(ANDROID_CONTROLS_TEST)
#include "platform/shared/android_viewport.h"
#include <stdint.h>

SDL_Rect AndroidViewport_Fit(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight)
{
    SDL_Rect result = { 0, 0, 0, 0 };
    if (outputWidth <= 0 || outputHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0) return result;
    result.w = outputWidth;
    result.h = (int)((int64_t)outputWidth * sourceHeight / sourceWidth);
    if (result.h > outputHeight) {
        result.h = outputHeight;
        result.w = (int)((int64_t)outputHeight * sourceWidth / sourceHeight);
    }
    if (result.w == 0) result.w = 1;
    if (result.h == 0) result.h = 1;
    result.x = (outputWidth - result.w) / 2;
    result.y = (outputHeight - result.h) / 2;
    return result;
}

SDL_Rect AndroidViewport_Game(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight)
{
    if (outputWidth <= 0 || outputHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
        return (SDL_Rect){ 0, 0, 0, 0 };
    // Fill the landscape screen as in earlier builds. Fit only a temporary
    // portrait surface while Android applies the activity's fixed orientation.
    if (outputWidth >= outputHeight) return (SDL_Rect){ 0, 0, outputWidth, outputHeight };
    return AndroidViewport_Fit(outputWidth, outputHeight, sourceWidth, sourceHeight);
}
#endif
