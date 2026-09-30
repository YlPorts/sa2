#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"
TEST_WORK="$(mktemp -d)"
trap 'rm -rf "$TEST_WORK"' EXIT
DEFINES=(-DGAME=2 -DPLATFORM_GBA=0 -DPLATFORM_SDL=1 -DPLATFORM_WIN32=0 -DPORTABLE=1 -DCPU_ARCH_X86=1 -DNON_MATCHING=1 -Iinclude)
cc -std=gnu11 -ffunction-sections -fdata-sections "${DEFINES[@]}" -c src/game/sa2/oc_selection.c -o "$TEST_WORK/selection.o"
cc -std=gnu11 -Wl,--gc-sections "${DEFINES[@]}" android/tests/oc-storage-test.c "$TEST_WORK/selection.o" -o "$TEST_WORK/storage-test"
"$TEST_WORK/storage-test"
