#if defined(__ANDROID__) || defined(ANDROID_CONTROLS_TEST)
#include "platform/shared/android_viewport.h"
#include <stdint.h>
#include "platform/shared/android_options.h"

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
    /* Cover wide phones with one uniform scale. The 426x240 simulation and
     * camera are untouched; only surplus vertical scenery is framed out.
     * Never frame out the original 160-pixel playable height. */
    if (gAndroidOptions[SA_OPT_SCREEN] == SA_SCREEN_FULL && outputHeight > 0 && sourceWidth > 0
        && (int64_t)outputWidth * sourceHeight > (int64_t)outputHeight * sourceWidth
        && (int64_t)outputWidth * 160 <= (int64_t)outputHeight * sourceWidth) {
        int height = (int)(((int64_t)outputWidth * sourceHeight + sourceWidth - 1) / sourceWidth);
        return (SDL_Rect){ 0, (outputHeight - height) / 2, outputWidth, height };
    }
    return AndroidViewport_Menu(outputWidth, outputHeight, sourceWidth, sourceHeight);
}

int AndroidViewport_InsetY(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight)
{
    SDL_Rect destination = AndroidViewport_Game(outputWidth, outputHeight, sourceWidth, sourceHeight);
    if (destination.y >= 0 || destination.h <= 0) return 0;
    int hidden = -destination.y;
    int lowerHidden = destination.y + destination.h - outputHeight;
    if (lowerHidden > hidden) hidden = lowerHidden;
    return (int)(((int64_t)hidden * sourceHeight + destination.h - 1) / destination.h);
}

SDL_Rect AndroidViewport_Menu(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight)
{
    if (outputWidth <= 0 || outputHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
        return (SDL_Rect){ 0, 0, 0, 0 };
    if (gAndroidOptions[SA_OPT_SCREEN] == SA_SCREEN_INTEGER) {
        int scaleX = outputWidth / sourceWidth, scaleY = outputHeight / sourceHeight;
        int scale = scaleX < scaleY ? scaleX : scaleY;
        if (scale >= 1) {
            int w = sourceWidth * scale, h = sourceHeight * scale;
            return (SDL_Rect){ (outputWidth - w) / 2, (outputHeight - h) / 2, w, h };
        }
    }
    // Preserve square pixels on both menu and gameplay viewports. Filling a
    // 19:9 panel with a 3:2 menu must never widen a character or crop its HUD.
    return AndroidViewport_Fit(outputWidth, outputHeight, sourceWidth, sourceHeight);
}
#endif
