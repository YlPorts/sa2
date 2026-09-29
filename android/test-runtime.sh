#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BITS="${SA_RUNTIME_BITS:-64}"
JOBS="${JOBS:-4}"
OBJ="build/runtime$BITS"
WORK="$ROOT/$OBJ"
mkdir -p "$WORK"
read -r -a SDL_FLAGS <<< "$(sdl2-config --cflags)"
read -r -a SDL_LIBS <<< "$(sdl2-config --libs)"
FLAGS=(-g -fno-omit-frame-pointer -fno-pie -no-pie)
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
# The harness lives outside Makefile's game source list, so force relinking.
rm -f "$WORK/sa2.runtime.elf"
make -j"$JOBS" PLATFORM=sdl CPU_ARCH=i386 GAME_NAME=sa2 OBJ_DIR="$OBJ" \
    ELF="$OBJ/sa2.runtime.elf" ROM="$OBJ/sa2.runtime" MAP="$OBJ/sa2.runtime.map" \
    CC1="gcc ${FLAGS[*]}" CXX="g++ ${FLAGS[*]}" CPP="gcc ${FLAGS[*]} -E" \
    AS="gcc ${FLAGS[*]} -c -x assembler" CC1FLAGS='-O1 -g -Wno-unused-value -x c -S' \
    LIBS="${SDL_LIBS[*]} -L$ROOT/libagbsyscall/$OBJ -lagbsyscall -lm $WORK/runtime-smoke.o -Wl,--wrap=AgbMain -Wl,--wrap=VBlankIntrWait"
gcc "${FLAGS[@]}" -O1 "${DEFINES[@]}" "${SDL_FLAGS[@]}" -DANDROID_CONTROLS_TEST=1 \
    src/platform/shared/android_controls.c android/tests/controls-check.c "${SDL_LIBS[@]}" -lm -o "$WORK/controls-check"
read -r -a RUNNER <<< "${SA_TEST_RUNNER:-}"
cd "$WORK"
# LeakSanitizer cannot inspect task namespaces in managed containers. Address
# checks remain enabled; the test process exits with the real game still alive.
export ASAN_OPTIONS=detect_leaks=0
"${RUNNER[@]}" ./controls-check "$WORK/controls.bmp"
for LEVEL in ${SA_TEST_LEVELS:-0 1}; do
    HEADLESS=true SA_TEST_LEVEL="$LEVEL" SA_TEST_CAPTURE="$WORK/level-$LEVEL.bmp" \
        "${RUNNER[@]}" ./sa2.runtime
done
