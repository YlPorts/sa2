#ifndef GUARD_PLATFORM_SAVE_FILE_H
#define GUARD_PLATFORM_SAVE_FILE_H
#include <stddef.h>

/* Returns success only after a complete, durable write and atomic replacement. */
int Platform_WriteSaveAtomically(const char *path, const void *data, size_t size);

#endif
