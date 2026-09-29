#if defined(__ANDROID__) || defined(ANDROID_CONTROLS_TEST)
#include "global.h"
#include "platform/shared/android_controls.h"
#include <math.h>

typedef struct Control {
    float x, y, halfW, halfH, radius;
    u16 key;
    char label;
} Control;

static float Base(int width, int height) { return (float)(width < height ? width : height); }

static void Layout(int width, int height, Control controls[6])
{
    const float b = Base(width, height);
    controls[0] = (Control){ width - b * .19f, height - b * .295f, b * .082f, b * .082f, b * .082f, A_BUTTON, 'A' };
    controls[1] = (Control){ width - b * .355f, height - b * .17f, b * .073f, b * .073f, b * .073f, B_BUTTON, 'B' };
    controls[2] = (Control){ b * .17f, b * .082f, b * .105f, b * .034f, b * .034f, L_BUTTON, 'L' };
    controls[3] = (Control){ width - b * .17f, b * .082f, b * .105f, b * .034f, b * .034f, R_BUTTON, 'R' };
    controls[4] = (Control){ width * .5f - b * .10f, height - b * .07f, b * .065f, b * .026f, b * .026f, SELECT_BUTTON, '=' };
    controls[5] = (Control){ width * .5f + b * .10f, height - b * .07f, b * .065f, b * .026f, b * .026f, START_BUTTON, '>' };
}

uint16_t AndroidControls_TouchMask(float x, float y, int width, int height)
{
    if (width <= 0 || height <= 0) return 0;
    const float b = Base(width, height), unit = b * .105f;
    const float px = x * width, py = y * height;
    const float dx = px - b * .22f, dy = py - (height - b * .25f);
    u16 mask = 0;
    if (fabsf(dx) <= unit * 1.72f && fabsf(dy) <= unit * 1.72f) {
        const float dead = unit * .22f;
        if (dx < -dead) mask |= DPAD_LEFT;
        if (dx > dead) mask |= DPAD_RIGHT;
        if (dy < -dead) mask |= DPAD_UP;
        if (dy > dead) mask |= DPAD_DOWN;
    }
    Control controls[6];
    Layout(width, height, controls);
    for (int i = 0; i < 6; i++) {
        const Control *c = &controls[i];
        const float xDistance = px - c->x, yDistance = py - c->y;
        if (i < 2) {
            const float hitRadius = c->radius + b * .014f;
            if (xDistance * xDistance + yDistance * yDistance <= hitRadius * hitRadius) mask |= c->key;
        } else if (fabsf(xDistance) <= c->halfW + b * .015f && fabsf(yDistance) <= c->halfH + b * .012f) {
            mask |= c->key;
        }
    }
    // A visible action/menu target takes priority over the enlarged diagonal
    // area of the dpad on narrow surfaces. Separate fingers still combine in
    // the SDL touch-slot tracker.
    if (mask & (A_BUTTON | B_BUTTON | L_BUTTON | R_BUTTON | SELECT_BUTTON | START_BUTTON))
        mask &= ~(DPAD_LEFT | DPAD_RIGHT | DPAD_UP | DPAD_DOWN);
    return mask;
}

static float Clamp(float x, float low, float high) { return fminf(high, fmaxf(low, x)); }

// One signed-distance silhouette avoids the overlapping translucent rectangles
// and end circles that made the old capsules and cruceta look uneven.
static float RoundedBox(float x, float y, float halfW, float halfH, float radius)
{
    const float qx = fabsf(x) - halfW + radius, qy = fabsf(y) - halfH + radius;
    const float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
    return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - radius;
}

static void Blend(SDL_Surface *surface, int x, int y, int r, int g, int b, int alpha)
{
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * 4;
    const int previous = p[3] * (255 - alpha) / 255;
    const int combined = alpha + previous;
    if (combined == 0) return;
    p[0] = (r * alpha + p[0] * previous) / combined;
    p[1] = (g * alpha + p[1] * previous) / combined;
    p[2] = (b * alpha + p[2] * previous) / combined;
    p[3] = combined;
}

static void Shape(SDL_Surface *surface, Control c, int cross, int highlight)
{
    const int left = (int)fmaxf(0, floorf(c.x - c.halfW - 1));
    const int top = (int)fmaxf(0, floorf(c.y - c.halfH - 1));
    const int right = (int)fminf(surface->w - 1, ceilf(c.x + c.halfW + 1));
    const int bottom = (int)fminf(surface->h - 1, ceilf(c.y + c.halfH + 1));
    const float border = fmaxf(1.2f, Base(surface->w, surface->h) * .002f);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            const float dx = x + .5f - c.x, dy = y + .5f - c.y;
            float distance;
            if (cross) {
                distance = fminf(RoundedBox(dx, dy, c.halfW / 3, c.halfH, c.radius),
                                 RoundedBox(dx, dy, c.halfW, c.halfH / 3, c.radius));
            } else {
                distance = RoundedBox(dx, dy, c.halfW, c.halfH, c.radius);
            }
            const float coverage = Clamp(.5f - distance, 0, 1);
            if (coverage == 0) continue;
            if (highlight) {
                Blend(surface, x, y, 100, 192, 255, (int)(coverage * 105));
            } else {
                const float rim = Clamp(distance + border + .5f, 0, 1);
                const int r = 20 + (int)(rim * 194), g = 26 + (int)(rim * 197), b = 38 + (int)(rim * 194);
                Blend(surface, x, y, r, g, b, (int)(coverage * (88 + rim * 46)));
            }
        }
    }
}

typedef struct Stroke { float x1, y1, x2, y2; } Stroke;
static float SegmentDistance(float x, float y, Stroke s)
{
    const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
    const float t = Clamp(((x - s.x1) * dx + (y - s.y1) * dy) / (dx * dx + dy * dy), 0, 1);
    const float ox = x - s.x1 - t * dx, oy = y - s.y1 - t * dy;
    return sqrtf(ox * ox + oy * oy);
}

static void Glyph(SDL_Surface *surface, float cx, float cy, float size, char label)
{
    Stroke strokes[20];
    int count = 0;
#define LINE(a, b, c, d) strokes[count++] = (Stroke){ a, b, c, d }
    if (label == 'A') {
        LINE(-.34f, .5f, 0, -.5f); LINE(0, -.5f, .34f, .5f); LINE(-.22f, .15f, .22f, .15f);
    } else if (label == 'L') {
        LINE(-.24f, -.5f, -.24f, .5f); LINE(-.24f, .5f, .30f, .5f);
    } else if (label == 'B' || label == 'R') {
        LINE(-.28f, -.5f, -.28f, .5f);
        const int loops = label == 'B' ? 2 : 1;
        for (int i = 0; i < loops; i++) {
            const float top = -.5f + i * .5f;
            LINE(-.28f, top, .04f, top);
            LINE(.04f, top, .24f, top + .06f); LINE(.24f, top + .06f, .32f, top + .16f);
            LINE(.32f, top + .16f, .32f, top + .34f); LINE(.32f, top + .34f, .24f, top + .44f);
            LINE(.24f, top + .44f, .04f, top + .5f); LINE(.04f, top + .5f, -.28f, top + .5f);
        }
        if (label == 'R') { LINE(-.03f, 0, .34f, .5f); }
    } else if (label == '=') {
        LINE(-.4f, -.17f, .4f, -.17f); LINE(-.4f, .17f, .4f, .17f);
    } else if (label == '>') {
        LINE(-.2f, -.4f, .32f, 0); LINE(.32f, 0, -.2f, .4f); LINE(-.2f, .4f, -.2f, -.4f);
    } else if (label == 'l') {
        LINE(.22f, -.35f, -.22f, 0); LINE(-.22f, 0, .22f, .35f);
    } else if (label == 'r') {
        LINE(-.22f, -.35f, .22f, 0); LINE(.22f, 0, -.22f, .35f);
    } else if (label == 'u') {
        LINE(-.35f, .22f, 0, -.22f); LINE(0, -.22f, .35f, .22f);
    } else if (label == 'd') {
        LINE(-.35f, -.22f, 0, .22f); LINE(0, .22f, .35f, -.22f);
    }
#undef LINE
    const float strokeRadius = fmaxf(.9f, size * .047f);
    const int left = (int)fmaxf(0, floorf(cx - size * .6f - strokeRadius));
    const int right = (int)fminf(surface->w - 1, ceilf(cx + size * .6f + strokeRadius));
    const int top = (int)fmaxf(0, floorf(cy - size * .6f - strokeRadius));
    const int bottom = (int)fminf(surface->h - 1, ceilf(cy + size * .6f + strokeRadius));
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            float distance = size;
            for (int i = 0; i < count; i++) {
                distance = fminf(distance, size * SegmentDistance((x + .5f - cx) / size, (y + .5f - cy) / size, strokes[i]));
            }
            const int alpha = (int)(Clamp(strokeRadius + .5f - distance, 0, 1) * 216);
            if (alpha) Blend(surface, x, y, 239, 245, 255, alpha);
        }
    }
}

SDL_Surface *AndroidControls_CreateSurface(int width, int height, int highlight)
{
    if (width <= 0 || height <= 0) return NULL;
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == NULL) return NULL;
    const float b = Base(width, height), unit = b * .105f;
    const float cx = b * .22f, cy = height - b * .25f;
    Shape(surface, (Control){ cx, cy, unit * 1.5f, unit * 1.5f, unit * .14f, 0, 0 }, 1, highlight);
    if (!highlight) {
        Glyph(surface, cx - unit, cy, unit * .48f, 'l'); Glyph(surface, cx + unit, cy, unit * .48f, 'r');
        Glyph(surface, cx, cy - unit, unit * .48f, 'u'); Glyph(surface, cx, cy + unit, unit * .48f, 'd');
    }
    Control controls[6];
    Layout(width, height, controls);
    for (int i = 0; i < 6; i++) {
        Shape(surface, controls[i], 0, highlight);
        if (!highlight) Glyph(surface, controls[i].x, controls[i].y, controls[i].halfH * (i < 2 ? .78f : 1.02f), controls[i].label);
    }
    return surface;
}

static void CopyRegion(SDL_Renderer *renderer, SDL_Texture *texture, float left, float top, float width, float height)
{
    SDL_Rect region = { (int)floorf(left), (int)floorf(top), (int)ceilf(width), (int)ceilf(height) };
    SDL_RenderCopy(renderer, texture, &region, &region);
}

void AndroidControls_DrawHighlights(SDL_Renderer *renderer, SDL_Texture *texture, int width, int height, uint16_t keys)
{
    const float b = Base(width, height), unit = b * .105f;
    const float cx = b * .22f, cy = height - b * .25f;
    if (keys & DPAD_LEFT) CopyRegion(renderer, texture, cx - unit * 1.5f - 1, cy - unit * .5f - 1, unit + 2, unit + 2);
    if (keys & DPAD_RIGHT) CopyRegion(renderer, texture, cx + unit * .5f - 1, cy - unit * .5f - 1, unit + 2, unit + 2);
    if (keys & DPAD_UP) CopyRegion(renderer, texture, cx - unit * .5f - 1, cy - unit * 1.5f - 1, unit + 2, unit + 2);
    if (keys & DPAD_DOWN) CopyRegion(renderer, texture, cx - unit * .5f - 1, cy + unit * .5f - 1, unit + 2, unit + 2);
    Control controls[6];
    Layout(width, height, controls);
    for (int i = 0; i < 6; i++) {
        if (keys & controls[i].key) CopyRegion(renderer, texture, controls[i].x - controls[i].halfW - 1,
            controls[i].y - controls[i].halfH - 1, controls[i].halfW * 2 + 2, controls[i].halfH * 2 + 2);
    }
}
#endif
