#ifdef __ANDROID__
#include "core.h"
#include "platform/shared/android_presentation.h"

/* Adjust only the HUD entries allocated by its own task. World sprites,
 * animation pivots, collisions and camera coordinates keep their native data. */
void AndroidPresentation_InsetHud(u16 firstOam)
{
    s16 inset = Platform_GetGameplayInsetY();
    for (u16 i = firstOam; inset > 0 && i < gOamFreeIndex; i++) {
        OamData *oam = &gOamMallocBuffer[i];
        if (oam->split.y < 48) oam->split.y += inset;
        else if (oam->split.y >= DISPLAY_HEIGHT - 32) oam->split.y -= inset;
    }
}
#endif
