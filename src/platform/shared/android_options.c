#if defined(__ANDROID__) || defined(ANDROID_CONTROLS_TEST)
#include "platform/shared/android_options.h"
#include <stdio.h>
#include <string.h>

int gAndroidOptions[SA_OPT_COUNT] = {
    SA_SCREEN_FULL, 0, 85, 80, 120, 100, 100, 100, 0, 60, 0,
    -1, -1, 100, -1, -1, 100, -1, -1, 100, -1, -1, 100,
    -1, -1, 100, -1, -1, 100, -1, -1, 100
};

static int ClampInt(int n, int low, int high) { return n < low ? low : n > high ? high : n; }

void AndroidOptions_Defaults(int values[SA_OPT_COUNT])
{
    const int defaults[] = { SA_SCREEN_FULL, 0, 85, 80, 120, 100, 100, 100, 0, 60, 0 };
    memcpy(values, defaults, sizeof(defaults));
    for (int i = 0; i < 7; i++) {
        values[SA_OPT_LAYOUT + i * 3] = -1;
        values[SA_OPT_LAYOUT + i * 3 + 1] = -1;
        values[SA_OPT_LAYOUT + i * 3 + 2] = 100;
    }
}

void AndroidOptions_Apply(const int values[SA_OPT_COUNT])
{
    memcpy(gAndroidOptions, values, sizeof(gAndroidOptions));
    gAndroidOptions[SA_OPT_SCREEN] = ClampInt(values[SA_OPT_SCREEN], SA_SCREEN_FIT, SA_SCREEN_FULL);
    gAndroidOptions[SA_OPT_FILTER] = ClampInt(values[SA_OPT_FILTER], 0, 1);
    gAndroidOptions[SA_OPT_SIZE] = ClampInt(values[SA_OPT_SIZE], 55, 140);
    gAndroidOptions[SA_OPT_OPACITY] = ClampInt(values[SA_OPT_OPACITY], 10, 100);
    gAndroidOptions[SA_OPT_HIT_AREA] = ClampInt(values[SA_OPT_HIT_AREA], 100, 160);
    for (int i = SA_OPT_MASTER; i <= SA_OPT_EFFECTS; i++) gAndroidOptions[i] = ClampInt(values[i], 0, 100);
    gAndroidOptions[SA_OPT_TOUCH] = ClampInt(values[SA_OPT_TOUCH], 0, 2);
    gAndroidOptions[SA_OPT_PRESENT] = values[SA_OPT_PRESENT] == 30 ? 30 : 60;
    gAndroidOptions[SA_OPT_DIAGNOSTICS] = !!values[SA_OPT_DIAGNOSTICS];
    for (int i = 0; i < 7; i++) {
        int j = SA_OPT_LAYOUT + i * 3;
        gAndroidOptions[j] = ClampInt(values[j], -1, 1000);
        gAndroidOptions[j + 1] = ClampInt(values[j + 1], -1, 1000);
        gAndroidOptions[j + 2] = ClampInt(values[j + 2], 60, 160);
    }
}

bool AndroidOptions_Load(const char *path)
{
    int values[SA_OPT_COUNT], extra;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    for (int i = 0; i < SA_OPT_COUNT; i++) {
        if (fscanf(file, "%d", &values[i]) != 1) { fclose(file); return false; }
    }
    bool valid = fscanf(file, "%d", &extra) == EOF;
    fclose(file);
    if (valid) AndroidOptions_Apply(values);
    return valid;
}

int AndroidOptions_ChannelVolume(bool music)
{
    return gAndroidOptions[music ? SA_OPT_MUSIC : SA_OPT_EFFECTS];
}
#endif
