#ifndef SA_ANDROID_PRESENTATION_H
#define SA_ANDROID_PRESENTATION_H
#include "global.h"
void Platform_SetGameplayViewport(bool8 enabled);
void Platform_SetPauseMenuVisible(bool8 visible);
s16 Platform_GetGameplayInsetY(void);
void AndroidPresentation_InsetHud(u16 firstOam);
#endif
