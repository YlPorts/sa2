#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BITS="${SA_RUNTIME_BITS:-64}"
ANDROID_BACKEND="${SA_RUNTIME_ANDROID_BACKEND:-0}"
JOBS="${JOBS:-4}"
OBJ="build/runtime$BITS"
if [[ "$ANDROID_BACKEND" == 1 ]]; then OBJ="build/runtime-android$BITS"; fi
WORK="$ROOT/$OBJ"
mkdir -p "$WORK"
read -r -a SDL_FLAGS <<< "$(sdl2-config --cflags)"
read -r -a SDL_LIBS <<< "$(sdl2-config --libs)"
FLAGS=(-g -fno-omit-frame-pointer -fno-pie -no-pie)
if [[ "$ANDROID_BACKEND" == 1 ]]; then FLAGS+=(-D__ANDROID__=1); fi
if [[ "$BITS" == 64 ]]; then
    FLAGS+=(-fsanitize=address)
elif [[ "$BITS" == 32 ]]; then
    FLAGS+=(-m32 -msse2 -I"$WORK/config")
    # Debian's amd64 development package provides the architecture-neutral
    # generated config; copy only that SDL header, not amd64 libc headers.
    mkdir -p "$WORK/config/SDL2"
    CONFIG="$(find /usr/include -path '*/SDL2/_real_SDL_config.h' -print -quit)"
    cp "$CONFIG" "$WORK/config/SDL2/"
    SDL_LIBS=(/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0)
else
    echo "SA_RUNTIME_BITS must be 32 or 64" >&2
    exit 2
fi
DEFINES=(-DPORTABLE=1 -DPLATFORM_SDL=1 -DPLATFORM_GBA=0 -DCPU_ARCH_X86=1 -DGAME=GAME_SA2 -Iinclude)
gcc "${FLAGS[@]}" -O1 "${DEFINES[@]}" "${SDL_FLAGS[@]}" -c android/tests/runtime-smoke.c -o "$WORK/runtime-smoke.o"
HOST_SHIM=""
if [[ "$ANDROID_BACKEND" == 1 ]]; then
    gcc "${FLAGS[@]}" "${SDL_FLAGS[@]}" -c android/tests/android-host-shim.c -o "$WORK/android-host-shim.o"
    HOST_SHIM="$WORK/android-host-shim.o"
fi
# The harness lives outside Makefile's game source list, so force relinking.
rm -f "$WORK/sa2.runtime.elf"
make -j"$JOBS" PLATFORM=sdl CPU_ARCH=i386 GAME_NAME=sa2 OBJ_DIR="$OBJ" \
    ELF="$OBJ/sa2.runtime.elf" ROM="$OBJ/sa2.runtime" MAP="$OBJ/sa2.runtime.map" \
    CC1="gcc ${FLAGS[*]}" CXX="g++ ${FLAGS[*]}" CPP="gcc ${FLAGS[*]} -E" \
    AS="gcc ${FLAGS[*]} -c -x assembler" CC1FLAGS='-O1 -g -Wno-unused-value -x c -S' \
    LIBS="${SDL_LIBS[*]} -L$ROOT/libagbsyscall/$OBJ -lagbsyscall -lm $WORK/runtime-smoke.o $HOST_SHIM -Wl,--wrap=AgbMain -Wl,--wrap=VBlankIntrWait"
gcc "${FLAGS[@]}" -O1 "${DEFINES[@]}" "${SDL_FLAGS[@]}" -DANDROID_CONTROLS_TEST=1 -DSDL_MAIN_HANDLED=1 \
    src/platform/shared/android_controls.c src/platform/shared/android_viewport.c android/tests/controls-check.c "${SDL_LIBS[@]}" -lm -o "$WORK/controls-check"
read -r -a RUNNER <<< "${SA_TEST_RUNNER:-}"
cd "$WORK"
# LeakSanitizer cannot inspect task namespaces in managed containers. Address
# checks remain enabled; the test process exits with the real game still alive.
export ASAN_OPTIONS=detect_leaks=0
"${RUNNER[@]}" ./controls-check "$WORK/controls.bmp"
if [[ "$ANDROID_BACKEND" == 1 ]]; then
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SA_TEST_BOOT=1 \
        "${RUNNER[@]}" ./sa2.runtime "$WORK"
    exit
fi
HEADLESS=true SA_TEST_BOOT=1 "${RUNNER[@]}" ./sa2.runtime
for LEVEL in ${SA_TEST_LEVELS:-0 1}; do
    for CHARACTER in ${SA_TEST_CHARACTERS:-0 1 2 3 4}; do
        HEADLESS=true SA_TEST_LEVEL="$LEVEL" SA_TEST_CHARACTER="$CHARACTER" SA_TEST_CAPTURE="$WORK/level-$LEVEL-$CHARACTER.bmp" \
            "${RUNNER[@]}" ./sa2.runtime
    done
done
