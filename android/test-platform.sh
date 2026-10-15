#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/platform-check
read -r -a SDL_FLAGS <<< "$(sdl2-config --cflags)"
read -r -a SDL_LIBS <<< "$(sdl2-config --libs)"
gcc -g -O1 -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address,undefined \
    -DPORTABLE=1 -DPLATFORM_SDL=1 -DPLATFORM_GBA=0 -DCPU_ARCH_X86=1 -DGAME=GAME_SA2 \
    -DANDROID_CONTROLS_TEST=1 -DSDL_MAIN_HANDLED=1 -Iinclude \
    android/tests/controls-check.c src/platform/shared/android_controls.c \
    src/platform/shared/android_touch.c src/platform/shared/android_options.c \
    src/platform/shared/android_viewport.c "${SDL_FLAGS[@]}" "${SDL_LIBS[@]}" -lm -o build/platform-check/check
ASAN_OPTIONS=detect_leaks=0 build/platform-check/check build/platform-check/controls.bmp
for SIMD in 0 1; do
    gcc -O3 -DCPU_ARCH_X86="$SIMD" -DPLATFORM_GBA=0 -DPORTABLE=1 -DL_CpuFastSet=1 -Iinclude \
        libagbsyscall/libagbsyscall.c android/tests/cpu-fast-set-check.c -lm \
        -o "build/platform-check/cpu-fast-$SIMD"
    "build/platform-check/cpu-fast-$SIMD"
done
