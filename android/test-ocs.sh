#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
ARCH="${SA_OC_ARCH:-i386}"
if [[ "$ARCH" == arm ]]; then
    OBJ="${SA_OC_OBJ:-build/oc-runtime-arm}"
else
    OBJ="${SA_OC_OBJ:-build/oc-runtime64}"
fi
WORK="$ROOT/$OBJ"
CAPTURES="${SA_OC_CAPTURE_DIR:-$WORK/captures}"
mkdir -p "$WORK" "$CAPTURES"
read -r -a SDL_FLAGS <<< "$(sdl2-config --cflags)"
read -r -a SDL_LIBS <<< "$(sdl2-config --libs)"
RUNNER=()
if [[ "$ARCH" == arm ]]; then
    # Execute real ARM32/O3 code, rather than relying on x86 sanitizer results.
    # SDL must be an ARM Linux static build with its generated SDL_config.h.
    ARM_SDL="${SA_OC_ARM_SDL:-/tmp/sonic-sdl-arm}"
    test -f "$ARM_SDL/build/.libs/libSDL2.a"
    test -f "$ARM_SDL/include/SDL_config.h"
    mkdir -p "$WORK/sdl-include"
    cp /usr/include/SDL2/*.h "$WORK/sdl-include/"
    cp "$ARM_SDL/include/SDL_config.h" "$WORK/sdl-include/"
    SDL_FLAGS=("-I$WORK/sdl-include")
    SDL_LIBS=("$ARM_SDL/build/.libs/libSDL2.a" -lm -lpthread)
    FLAGS=(-g -mfpu=neon -mfloat-abi=hard "${SDL_FLAGS[@]}")
    CC=arm-linux-gnueabihf-gcc
    CXX=arm-linux-gnueabihf-g++
    OPT=-O3
    CPU_DEFINE=0
    RUNNER=(qemu-arm -cpu cortex-a15 -L /usr/arm-linux-gnueabihf)
else
    FLAGS=(-g -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address)
    CC=gcc
    CXX=g++
    OPT=-O1
    CPU_DEFINE=1
fi
DEFINES=(-DPORTABLE=1 -DPLATFORM_SDL=1 -DPLATFORM_GBA=0 "-DCPU_ARCH_X86=$CPU_DEFINE" -DGAME=GAME_SA2 -Iinclude)
"$CC" "${FLAGS[@]}" "$OPT" "${DEFINES[@]}" "${SDL_FLAGS[@]}" -c android/tests/oc-runtime-smoke.c -o "$WORK/oc-runtime-smoke.o"
# Unlink the previous executable as well: a debugger may still map its inode.
# Linking/copying a fresh candidate must never silently retain an older binary.
rm -f "$WORK/sa2.oc-runtime.elf" "$WORK/sa2.oc-runtime"
make -j"${JOBS:-4}" PLATFORM=sdl CPU_ARCH="$ARCH" GAME_NAME=sa2 OBJ_DIR="$OBJ" \
    ELF="$OBJ/sa2.oc-runtime.elf" ROM="$OBJ/sa2.oc-runtime" MAP="$OBJ/sa2.oc-runtime.map" \
    CC1="$CC ${FLAGS[*]}" CXX="$CXX ${FLAGS[*]}" CPP="$CC ${FLAGS[*]} -E" \
    AS="$CC ${FLAGS[*]} -c -x assembler" CC1FLAGS="$OPT -g -Wno-unused-value -x c -S" \
    LIBS="${SDL_LIBS[*]} -L$ROOT/libagbsyscall/$OBJ -lagbsyscall -lm $WORK/oc-runtime-smoke.o -Wl,--wrap=AgbMain -Wl,--wrap=VBlankIntrWait -Wl,--wrap=CreateCharacterSelectionScreen -Wl,--wrap=OcPlayerDraw -Wl,--wrap=OcSpecialPlayerDraw -Wl,--wrap=OcAbilitiesDraw -Wl,--wrap=OcAbilityHitsTarget -Wl,--wrap=TaskDestroy"
cd "$WORK"
export HEADLESS=true ASAN_OPTIONS=detect_leaks=0
export SA_OC_CAPTURE_DIR="$CAPTURES"
if [[ "${SA_OC_BUILD_ONLY:-0}" == 1 ]]; then exit 0; fi
if [[ -n "${SA_OC_ONLY_MODE:-}" ]]; then
    SA_OC_MODE="$SA_OC_ONLY_MODE" "${RUNNER[@]}" ./sa2.oc-runtime
    exit 0
fi
for ENTRY in ${SA_OC_TEST_ENTRIES:-0 1 2 3 4 5 6 7 8 9}; do
    SA_OC_MODE=select SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
done
SA_OC_MODE=locked SA_OC_ENTRY=4 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=cancel SA_OC_ENTRY=9 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=lifecycle "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=multiplayer "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=stage-lifecycle SA_OC_ENTRY=5 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=glitch-defense SA_OC_ENTRY=5 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=double-a SA_OC_ENTRY=6 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=native-amy SA_OC_ENTRY=4 "${RUNNER[@]}" ./sa2.oc-runtime
SA_OC_MODE=special SA_OC_ENTRY=0 "${RUNNER[@]}" ./sa2.oc-runtime
for ENTRY in ${SA_OC_STAGE_ENTRIES:-5 6 7 8 9}; do
    SA_OC_MODE=boot SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
    SA_OC_MODE=abilities SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
    SA_OC_MODE=air-abilities SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
    SA_OC_MODE=pause SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
    SA_OC_MODE=special SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
    SA_OC_MODE=victory SA_OC_ENTRY="$ENTRY" "${RUNNER[@]}" ./sa2.oc-runtime
done
printf 'OC integration checks passed. Framebuffer captures: %s\n' "$CAPTURES"
