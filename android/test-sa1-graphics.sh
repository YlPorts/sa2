#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
BITS="${SA_RUNTIME_BITS:-64}"
ANDROID_BACKEND="${SA_RUNTIME_ANDROID_BACKEND:-0}"
OBJ="build/sa1-graphics$BITS"
if [[ "$ANDROID_BACKEND" == 1 ]]; then OBJ="build/sa1-graphics-android$BITS"; fi
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
    mkdir -p "$WORK/config/SDL2"
    CONFIG="$(find /usr/include -path '*/SDL2/_real_SDL_config.h' -print -quit)"
    cp "$CONFIG" "$WORK/config/SDL2/"
    SDL_LIBS=(/usr/lib/i386-linux-gnu/libSDL2-2.0.so.0)
else
    echo "SA_RUNTIME_BITS must be 32 or 64" >&2
    exit 2
fi
DEFINES=(-DPORTABLE=1 -DPLATFORM_SDL=1 -DPLATFORM_GBA=0 -DCPU_ARCH_X86=1 -DGAME=GAME_SA1 -DSA1_RUNTIME_IMPORT=1 -Iinclude)
gcc "${FLAGS[@]}" -O1 "${DEFINES[@]}" "${SDL_FLAGS[@]}" -c android/tests/sa1-graphics-runtime.c -o "$WORK/harness.o"
HOST_SHIM=""
if [[ "$ANDROID_BACKEND" == 1 ]]; then
    gcc "${FLAGS[@]}" "${SDL_FLAGS[@]}" -c android/tests/android-host-shim.c -o "$WORK/android-host-shim.o"
    HOST_SHIM="$WORK/android-host-shim.o"
fi
rm -f "$WORK/sa1.graphics.elf"
make -j"${JOBS:-4}" PLATFORM=sdl CPU_ARCH=i386 GAME_NAME=sa1 OBJ_DIR="$OBJ" \
    ELF="$OBJ/sa1.graphics.elf" ROM="$OBJ/sa1.graphics" MAP="$OBJ/sa1.graphics.map" \
    SA1_IMPORT_CPPFLAGS=-DSA1_RUNTIME_IMPORT=1 \
    DATA_ASM_FILTER="python3 android/rom-data-asm.py --pointer-size $((BITS / 8))" \
    CC1="gcc ${FLAGS[*]}" CXX="g++ ${FLAGS[*]}" CPP="gcc ${FLAGS[*]} -E" \
    AS="gcc ${FLAGS[*]} -c -x assembler" CC1FLAGS='-O1 -g -Wno-unused-value -x c -S' \
    LIBS="${SDL_LIBS[*]} -L$ROOT/libagbsyscall/$OBJ -lagbsyscall -lm $WORK/harness.o $HOST_SHIM -Wl,--wrap=AgbMain -Wl,--wrap=VBlankIntrWait -Wl,--wrap=Sa1_LoadRomAssets -Wl,--wrap=SDL_RenderCopy"
cd "$WORK"
export ASAN_OPTIONS=detect_leaks=0
read -r -a RUNNER <<< "${SA_TEST_RUNNER:-}"
if [[ "$ANDROID_BACKEND" == 1 ]]; then
    export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy HEADLESS=false
else
    export HEADLESS=true
fi
for CHARACTER in ${SA_TEST_CHARACTERS:-0 1 2 3}; do
    SA_TEST_CHARACTER="$CHARACTER" SA_TEST_CAPTURE="$WORK/selector-$CHARACTER.bmp" \
        "${RUNNER[@]}" ./sa1.graphics "$WORK"
done
for LEVEL in ${SA_TEST_LEVELS:-0 1 13}; do
    SA_TEST_LEVEL="$LEVEL" SA_TEST_CAPTURE="$WORK/stage-$LEVEL.bmp" \
        "${RUNNER[@]}" ./sa1.graphics "$WORK"
done
