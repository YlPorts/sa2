#if defined(__ANDROID__) && !defined(SA_ANDROID_HOST_TEST)
#include <jni.h>
#include <SDL.h>
#include "platform/shared/android_options.h"

/* UI threads enqueue a copy; all renderer/audio/input state changes remain on
 * the game thread. No JNI reference or Java array outlives this call. */
JNIEXPORT void JNICALL Java_org_libsdl_app_SA2Activity_nativeOptions(
    JNIEnv *env, jclass cls, jintArray values, jboolean open)
{
    (void)cls;
    if (!values || (*env)->GetArrayLength(env, values) != SA_OPT_COUNT) return;
    AndroidOptionsMessage *message = SDL_malloc(sizeof(*message));
    if (!message) return;
    (*env)->GetIntArrayRegion(env, values, 0, SA_OPT_COUNT, message->values);
    if ((*env)->ExceptionCheck(env)) { SDL_free(message); return; }
    message->open = open;
    SDL_Event event = {0};
    event.type = SDL_USEREVENT;
    event.user.code = SA_OPTIONS_EVENT;
    event.user.data1 = message;
    if (SDL_PushEvent(&event) <= 0) SDL_free(message);
}
#endif
