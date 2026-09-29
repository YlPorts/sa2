#ifndef SA_ANDROID_CONTROLS_H
#define SA_ANDROID_CONTROLS_H

#include <stdint.h>
#include <SDL.h>

// Drawing and touch testing share one layout in renderer pixels.
uint16_t AndroidControls_TouchMask(float x, float y, int width, int height);
SDL_Surface *AndroidControls_CreateSurface(int width, int height, int highlight);
void AndroidControls_DrawHighlights(SDL_Renderer *renderer, SDL_Texture *texture,
                                    int width, int height, uint16_t keys);
#endif
