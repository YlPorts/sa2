#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OBJ="${SA_OC_OBJ:-build/oc-runtime64}"
WORK="$ROOT/$OBJ"
CAPTURES="${SA_OC_CAPTURE_DIR:-$WORK/captures}"
mkdir -p "$WORK" "$CAPTURES"
read -r -a SDL_FLAGS <<< "$(sdl2-config --cflags)"
read -r -a SDL_LIBS <<< "$(sdl2-config --libs)"
FLAGS=(-g -fno-omit-frame-pointer -fno-pie -no-pie -fsanitize=address)
DEFINES=(-DPORTABLE=1 -DPLATFORM_SDL=1 -DPLATFORM_GBA=0 -DCPU_ARCH_X86=1 -DGAME=GAME_SA2 -Iinclude)
gcc "${FLAGS[@]}" -O1 "${DEFINES[@]}" "${SDL_FLAGS[@]}" -c android/tests/oc-runtime-smoke.c -o "$WORK/oc-runtime-smoke.o"
rm -f "$WORK/sa2.oc-runtime.elf"
make -j"${JOBS:-4}" PLATFORM=sdl CPU_ARCH=i386 GAME_NAME=sa2 OBJ_DIR="$OBJ" \
    ELF="$OBJ/sa2.oc-runtime.elf" ROM="$OBJ/sa2.oc-runtime" MAP="$OBJ/sa2.oc-runtime.map" \
    CC1="gcc ${FLAGS[*]}" CXX="g++ ${FLAGS[*]}" CPP="gcc ${FLAGS[*]} -E" \
    AS="gcc ${FLAGS[*]} -c -x assembler" CC1FLAGS='-O1 -g -Wno-unused-value -x c -S' \
    LIBS="${SDL_LIBS[*]} -L$ROOT/libagbsyscall/$OBJ -lagbsyscall -lm $WORK/oc-runtime-smoke.o -Wl,--wrap=AgbMain -Wl,--wrap=VBlankIntrWait -Wl,--wrap=CreateCharacterSelectionScreen -Wl,--wrap=OcPlayerDraw -Wl,--wrap=OcSpecialPlayerDraw"
cd "$WORK"
export HEADLESS=true ASAN_OPTIONS=detect_leaks=0
export SA_OC_CAPTURE_DIR="$CAPTURES"
for ENTRY in ${SA_OC_TEST_ENTRIES:-0 1 2 3 4 5 6 7 8}; do
    SA_OC_MODE=select SA_OC_ENTRY="$ENTRY" ./sa2.oc-runtime
done
SA_OC_MODE=locked SA_OC_ENTRY=4 ./sa2.oc-runtime
SA_OC_MODE=cancel SA_OC_ENTRY=8 ./sa2.oc-runtime
SA_OC_MODE=lifecycle ./sa2.oc-runtime
SA_OC_MODE=multiplayer ./sa2.oc-runtime
SA_OC_MODE=special SA_OC_ENTRY=0 ./sa2.oc-runtime
for ENTRY in ${SA_OC_STAGE_ENTRIES:-5 6 7 8}; do
    SA_OC_MODE=boot SA_OC_ENTRY="$ENTRY" ./sa2.oc-runtime
    SA_OC_MODE=special SA_OC_ENTRY="$ENTRY" ./sa2.oc-runtime
done
printf 'OC integration checks passed. Framebuffer captures: %s\n' "$CAPTURES"
