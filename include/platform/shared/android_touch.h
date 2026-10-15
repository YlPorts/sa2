#ifndef SA_ANDROID_TOUCH_H
#define SA_ANDROID_TOUCH_H
#include <SDL.h>
#include <stdint.h>
void AndroidTouch_Reset(void);
/* phase: 0 up/cancel, 1 down, 2 motion. Motion cannot invent a contact. */
void AndroidTouch_Finger(SDL_TouchID device, SDL_FingerID finger, float x, float y,
                         int phase, int width, int height);
uint16_t AndroidTouch_Peek(void);
uint16_t AndroidTouch_Consume(void);
#endif
