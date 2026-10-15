#if defined(__ANDROID__) || defined(ANDROID_CONTROLS_TEST)
#include "global.h"
#include "platform/shared/android_touch.h"
#include "platform/shared/android_controls.h"
#include <string.h>
#include <stdbool.h>

typedef struct { SDL_TouchID device; SDL_FingerID finger; u16 mask; bool active; } Contact;
static Contact sContacts[10];
static u16 sPending;

void AndroidTouch_Reset(void) { memset(sContacts, 0, sizeof(sContacts)); sPending = 0; }

void AndroidTouch_Finger(SDL_TouchID device, SDL_FingerID finger, float x, float y,
                         int phase, int width, int height)
{
    int freeSlot = -1;
    u16 mask = phase ? AndroidControls_TouchMask(x, y, width, height) : 0;
    for (int i = 0; i < 10; i++) {
        Contact *c = &sContacts[i];
        if (c->active && c->device == device && c->finger == finger) {
            sPending |= mask & ~c->mask;
            c->mask = mask;
            c->active = phase != 0;
            return;
        }
        if (!c->active && freeSlot < 0) freeSlot = i;
    }
    if (phase == 1 && freeSlot >= 0) {
        sContacts[freeSlot] = (Contact){ device, finger, mask, true };
        sPending |= mask;
    }
}

uint16_t AndroidTouch_Peek(void)
{
    u16 mask = 0;
    for (int i = 0; i < 10; i++) if (sContacts[i].active) mask |= sContacts[i].mask;
    return mask;
}

uint16_t AndroidTouch_Consume(void)
{
    u16 mask = AndroidTouch_Peek() | sPending;
    sPending = 0;
    if ((mask & (DPAD_LEFT | DPAD_RIGHT)) == (DPAD_LEFT | DPAD_RIGHT)) mask &= ~(DPAD_LEFT | DPAD_RIGHT);
    if ((mask & (DPAD_UP | DPAD_DOWN)) == (DPAD_UP | DPAD_DOWN)) mask &= ~(DPAD_UP | DPAD_DOWN);
    return mask;
}
#endif
