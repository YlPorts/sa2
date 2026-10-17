#include <assert.h>
#include <stdio.h>
#include "global.h"
#include "platform/shared/android_controls.h"
#include "platform/shared/android_viewport.h"
#include "platform/shared/android_options.h"
#include "platform/shared/android_touch.h"
#include <unistd.h>

static u16 At(float x, float y, int width, int height)
{
    return AndroidControls_TouchMask(x / width, y / height, width, height);
}

int main(int argc, char **argv)
{
    const int sizes[][2] = { {1280, 720}, {1536, 709}, {2400, 1080}, {720, 1600}, {426, 240} };
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const int w = sizes[i][0], h = sizes[i][1];
        const float b = w < h ? w : h, unit = b * .105f * .85f;
        const float cx = b * .22f, cy = h - b * .25f;
        const int sources[][2] = { {426, 240}, {240, 160} };
        int initial[SA_OPT_COUNT]; AndroidOptions_Defaults(initial); initial[SA_OPT_SCREEN] = SA_SCREEN_FIT;
        AndroidOptions_Apply(initial);
        for (unsigned j = 0; j < 2; j++) {
            SDL_Rect fitted = AndroidViewport_Fit(w, h, sources[j][0], sources[j][1]);
            assert(fitted.w > 0 && fitted.h > 0);
            assert(fitted.x >= 0 && fitted.y >= 0 && fitted.x + fitted.w <= w && fitted.y + fitted.h <= h);
            assert(abs(fitted.w * sources[j][1] - fitted.h * sources[j][0]) < sources[j][0] + sources[j][1]);
            SDL_Rect game = AndroidViewport_Game(w, h, sources[j][0], sources[j][1]);
            assert(game.x == fitted.x && game.y == fitted.y && game.w == fitted.w && game.h == fitted.h);
            int options[SA_OPT_COUNT]; AndroidOptions_Defaults(options); options[SA_OPT_SCREEN] = 1;
            AndroidOptions_Apply(options);
            SDL_Rect integer = AndroidViewport_Game(w, h, sources[j][0], sources[j][1]);
            if (w >= sources[j][0] && h >= sources[j][1]) {
                assert(integer.w % sources[j][0] == 0 && integer.h % sources[j][1] == 0);
                assert(integer.w / sources[j][0] == integer.h / sources[j][1]);
            }
            AndroidOptions_Defaults(options); options[SA_OPT_SCREEN] = SA_SCREEN_FIT; AndroidOptions_Apply(options);
        }
        assert(At(cx, cy, w, h) == 0);
        assert(At(cx + unit, cy, w, h) == DPAD_RIGHT);
        assert(At(cx - unit, cy - unit, w, h) == (DPAD_LEFT | DPAD_UP));
        assert(At(cx + unit, cy + unit, w, h) == (DPAD_RIGHT | DPAD_DOWN));
        assert(At(w - b * .19f, h - b * .295f, w, h) == A_BUTTON);
        assert(At(w - b * .355f, h - b * .17f, w, h) == B_BUTTON);
        assert(At(b * .17f, b * .22f, w, h) == L_BUTTON);
        assert(At(w - b * .17f, b * .22f, w, h) == R_BUTTON);
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
    assert(AndroidViewport_Fit(0, 0, 426, 240).w == 0);
    assert(AndroidViewport_Game(0, 0, 426, 240).w == 0);
    // Galaxy A15 landscape: retain the 426x240 simulation and one scale on
    // both axes, fill its entire 2340x1080 panel, and protect the complete HUD.
    int full[SA_OPT_COUNT]; AndroidOptions_Defaults(full); AndroidOptions_Apply(full);
    SDL_Rect filled = AndroidViewport_Game(2340, 1080, 426, 240);
    assert(filled.x == 0 && filled.w == 2340 && filled.y < 0 && filled.y + filled.h >= 1080);
    assert(abs(filled.w * 240 - filled.h * 426) < 426);
    int inset = AndroidViewport_InsetY(2340, 1080, 426, 240);
    assert(inset == 22);
    assert(filled.y + (inset * filled.h / 240) >= 0);
    assert(filled.y + ((240 - inset) * filled.h / 240) <= 1080);
    SDL_Rect menu = AndroidViewport_Menu(2340, 1080, 240, 160);
    assert(menu.x >= 0 && menu.y >= 0 && menu.x + menu.w <= 2340 && menu.y + menu.h <= 1080);
    assert(AndroidViewport_InsetY(3840, 1080, 426, 240) == 0); // keep >=160 source rows
    assert(AndroidViewport_InsetY(1280, 960, 426, 240) == 0); // safe frame on narrow panels
    int options[SA_OPT_COUNT];
    AndroidOptions_Defaults(options);
    options[SA_OPT_LAYOUT + 3] = 700; options[SA_OPT_LAYOUT + 4] = 650;
    options[SA_OPT_LAYOUT + 5] = 130;
    AndroidOptions_Apply(options);
    SDL_FRect action;
    AndroidControls_Describe(1,1280,720,&action);
    assert(At(action.x+action.w/2,action.y+action.h/2,1280,720)==A_BUTTON);
    AndroidTouch_Reset();
    AndroidTouch_Finger(1,10,.7f,.65f,1,1280,720);
    AndroidTouch_Finger(1,10,.7f,.65f,0,1280,720);
    assert(AndroidTouch_Consume()==A_BUTTON); // fast down/up in one event poll
    assert(AndroidTouch_Consume()==0);
    AndroidTouch_Finger(1,10,.7f,.65f,2,1280,720);
    assert(AndroidTouch_Consume()==0); // stale motion after cancellation
    AndroidTouch_Finger(1,10,.7f,.65f,1,1280,720);
    AndroidTouch_Finger(2,10,(1280-720*.355f)/1280,(720-720*.17f)/720,1,1280,720);
    AndroidTouch_Finger(1,11,(720*.22f+720*.105f*.85f)/1280,.75f,1,1280,720);
    assert((AndroidTouch_Consume()&(A_BUTTON|B_BUTTON|DPAD_RIGHT))==(A_BUTTON|B_BUTTON|DPAD_RIGHT));
    AndroidTouch_Finger(1,10,.7f,.65f,0,1280,720);
    assert(AndroidTouch_Consume()==(B_BUTTON|DPAD_RIGHT)); // matching finger on a second device survives
    AndroidTouch_Reset(); assert(AndroidTouch_Consume()==0);
    options[SA_OPT_MASTER]=-500;options[SA_OPT_SIZE]=10000;options[SA_OPT_LAYOUT]=10000;
    AndroidOptions_Apply(options);assert(gAndroidOptions[SA_OPT_MASTER]==0&&gAndroidOptions[SA_OPT_SIZE]==140);
    assert(gAndroidOptions[SA_OPT_LAYOUT]==1000);
    char path[]="/tmp/sa2-options-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    FILE *file=fopen(path,"w");assert(file);fputs("1\n0\n",file);fclose(file);
    assert(!AndroidOptions_Load(path)&&gAndroidOptions[SA_OPT_SIZE]==140); // incomplete file preserves active state
    file=fopen(path,"w");assert(file);AndroidOptions_Defaults(options);options[SA_OPT_OPACITY]=37;
    for(int i=0;i<SA_OPT_COUNT;i++)fprintf(file,"%d\n",options[i]);fclose(file);
    assert(AndroidOptions_Load(path)&&gAndroidOptions[SA_OPT_OPACITY]==37);unlink(path);
    puts("PASS: five display sizes, square pixels, integer scaling, edited hit regions, three simultaneous actions, fast taps, device IDs, cancel and settings reload");
    return 0;
}
