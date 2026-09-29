#ifndef ANDROID_VIEWPORT_H
#define ANDROID_VIEWPORT_H
#include <SDL.h>
SDL_Rect AndroidViewport_Fit(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight);
SDL_Rect AndroidViewport_Game(int outputWidth, int outputHeight, int sourceWidth, int sourceHeight);
#endif
