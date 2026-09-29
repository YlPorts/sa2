#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "platform/shared/android_controls.h"

static u16 At(float x, float y, int width, int height)
{
    return AndroidControls_TouchMask(x / width, y / height, width, height);
}

int main(int argc, char **argv)
{
    const int sizes[][2] = { {1280, 720}, {2400, 1080}, {720, 1600}, {426, 240} };
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const int w = sizes[i][0], h = sizes[i][1];
        const float b = w < h ? w : h, unit = b * .105f;
        const float cx = b * .22f, cy = h - b * .25f;
        assert(At(cx, cy, w, h) == 0);
        assert(At(cx + unit, cy, w, h) == DPAD_RIGHT);
        assert(At(cx - unit, cy - unit, w, h) == (DPAD_LEFT | DPAD_UP));
        assert(At(cx + unit, cy + unit, w, h) == (DPAD_RIGHT | DPAD_DOWN));
        assert(At(w - b * .19f, h - b * .295f, w, h) == A_BUTTON);
        assert(At(w - b * .355f, h - b * .17f, w, h) == B_BUTTON);
        assert(At(b * .17f, b * .082f, w, h) == L_BUTTON);
        assert(At(w - b * .17f, b * .082f, w, h) == R_BUTTON);
        assert(At(w * .5f - b * .10f, h - b * .07f, w, h) == SELECT_BUTTON);
        assert(At(w * .5f + b * .10f, h - b * .07f, w, h) == START_BUTTON);
        assert(At(w * .5f, h * .5f, w, h) == 0);
        SDL_Surface *surface = AndroidControls_CreateSurface(w, h, 0);
        assert(surface != NULL);
        // The game area stays transparent, while visible control centers
        // coincide with the tested touch targets even at unusual aspect ratios.
        Uint8 *center = (Uint8 *)surface->pixels + (h / 2) * surface->pitch + (w / 2) * 4;
        assert(center[3] == 0);
        Uint8 *pad = (Uint8 *)surface->pixels + (int)cy * surface->pitch + (int)cx * 4;
        assert(pad[3] > 0 && pad[3] < 140);
        if (i == 0 && argc > 1) assert(SDL_SaveBMP(surface, argv[1]) == 0);
        SDL_FreeSurface(surface);
    }
    assert(AndroidControls_TouchMask(.5f, .5f, 0, 0) == 0);
    assert(AndroidControls_CreateSurface(0, 0, 0) == NULL);
    puts("Passed: controls, diagonals and touch regions at four display sizes");
    return 0;
}
