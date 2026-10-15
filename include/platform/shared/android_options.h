#ifndef SA_ANDROID_OPTIONS_H
#define SA_ANDROID_OPTIONS_H
#include <stdbool.h>

enum {
    SA_OPT_SCREEN, SA_OPT_FILTER, SA_OPT_SIZE, SA_OPT_OPACITY, SA_OPT_HIT_AREA,
    SA_OPT_MASTER, SA_OPT_MUSIC, SA_OPT_EFFECTS, SA_OPT_TOUCH, SA_OPT_PRESENT,
    SA_OPT_DIAGNOSTICS, SA_OPT_LAYOUT,
    SA_OPT_COUNT = SA_OPT_LAYOUT + 7 * 3
};
#define SA_OPTIONS_EVENT 0x534132
typedef struct { int values[SA_OPT_COUNT]; bool open; } AndroidOptionsMessage;
extern int gAndroidOptions[SA_OPT_COUNT];
void AndroidOptions_Defaults(int values[SA_OPT_COUNT]);
void AndroidOptions_Apply(const int values[SA_OPT_COUNT]);
bool AndroidOptions_Load(const char *path);
int AndroidOptions_ChannelVolume(bool music);
#endif
