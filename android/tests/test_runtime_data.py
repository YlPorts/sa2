import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("rom_data", ROOT / "android/rom-data-asm.py")
rom_data = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rom_data)
verify_spec = importlib.util.spec_from_file_location("verify_native", ROOT / "android/verify-native.py")
verify_native = importlib.util.module_from_spec(verify_spec)
verify_spec.loader.exec_module(verify_native)


class RuntimeDataTests(unittest.TestCase):
    def test_water_palettes_accept_halfword_aligned_assets_and_task_data(self):
        with tempfile.TemporaryDirectory() as work:
            executable = str(Path(work) / "water-palette-check")
            subprocess.run([
                "cc", "-O3", "-fsanitize=undefined", "-fno-sanitize-recover=all",
                "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                "-DPORTABLE=1", "-DPLATFORM_SDL=1", "-DPLATFORM_GBA=0", "-DGAME=GAME_SA2",
                "-I", str(ROOT / "include"),
                str(ROOT / "src/game/shared/stage/water_effects.c"),
                str(ROOT / "android/tests/water-palette-check.c"), "-o", executable,
            ], check=True)
            subprocess.run([executable], check=True, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})

    def test_android_crash_report_decodes_the_crashing_thread_and_rejects_truncated_input(self):
        with tempfile.TemporaryDirectory() as work:
            subprocess.run([
                "javac", "-d", work,
                str(ROOT / "android/template/app/src/main/java/org/libsdl/app/NativeCrashReport.java"),
                str(ROOT / "android/tests/NativeCrashReportTest.java"),
            ], check=True)
            subprocess.run(["java", "-cp", work, "org.libsdl.app.NativeCrashReportTest"], check=True)

    def test_importer_preserves_existing_data_on_invalid_or_interrupted_import(self):
        with tempfile.TemporaryDirectory() as work:
            subprocess.run([
                "javac", "-d", work,
                str(ROOT / "android/template/app/src/main/java/org/libsdl/app/Sa1RomImporter.java"),
                str(ROOT / "android/tests/Sa1RomImporterTest.java"),
            ], check=True)
            subprocess.run(["java", "-cp", work, "org.libsdl.app.Sa1RomImporterTest", work], check=True)

    def test_runtime_data_matches_source_ranges_and_is_writable(self):
        # Link the actual generated asset registry to the actual native loader,
        # then import synthetic bytes. This catches relocation/alignment errors
        # as well as accidental writes into linker-protected RELRO sections.
        source = '''.text
.global test_unrelated
test_unrelated:
    ret
.macro mSectionRodata
    .section .data.rel.ro,"aw",%progbits
.endm
mSectionRodata
.global first_asset
first_asset:
    .incbin "baserom_sa1.gba", 0x487134, 12
.global second_asset
second_asset:
    .incbin "baserom_sa1.gba", 0x6ACB34, 0x29C0
.section .note.GNU-stack,"",%progbits
'''
        harness = r'''
#include "platform/shared/rom_assets.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern unsigned char first_asset[], second_asset[];
int main(int argc, char **argv) {
    char error[200];
    FILE *file = fopen(argv[1], "wb");
    assert(file);
    for (unsigned i = 0; i < 8 * 1024 * 1024; ++i) fputc((i * 17 + 3) & 255, file);
    fclose(file);
    assert(Sa1_LoadRomAssets(argv[1], error, sizeof(error)));
    for (unsigned i = 0; i < 12; ++i) assert(first_asset[i] == (((0x487134 + i) * 17 + 3) & 255));
    for (unsigned i = 0; i < 0x29C0; ++i) assert(second_asset[i] == (((0x6ACB34 + i) * 17 + 3) & 255));
    file = fopen(argv[1], "wb"); fputc(0, file); fclose(file);
    assert(!Sa1_LoadRomAssets(argv[1], error, sizeof(error)));
    remove(argv[1]);
    assert(!Sa1_LoadRomAssets(argv[1], error, sizeof(error)));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            (directory / "assets.s").write_text(rom_data.transform(source, 8))
            (directory / "test.c").write_text(harness)
            subprocess.run([
                "cc", "-DSA1_RUNTIME_IMPORT=1", "-I", str(ROOT / "include"),
                str(ROOT / "src/platform/shared/rom_assets.c"), str(directory / "test.c"),
                str(directory / "assets.s"), "-o", str(directory / "test"),
            ], check=True)
            self.assertEqual(verify_native.check_sa1_assets(directory / "test")['asset_ranges'], 2)
            subprocess.run([str(directory / "test"), str(directory / "synthetic.gba")], check=True)

    def test_linked_read_only_assets_are_rejected_before_packaging(self):
        # Reproduce the bug that a section's SHF_WRITE flag alone cannot catch:
        # GNU_RELRO still makes it read-only after the dynamic linker runs.
        source = '''.section .data.rel.ro,"aw",%progbits
bad_asset:
    .space 12, 0
.section sa1_rom_assets,"aw",%progbits
    .balign 8
    .quad bad_asset
    .long 0x487134
    .long 12
.section .note.GNU-stack,"",%progbits
'''
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            (directory / "assets.s").write_text(source)
            subprocess.run(['cc', '-shared', str(directory / 'assets.s'), '-o', str(directory / 'test.so')], check=True)
            with self.assertRaisesRegex(ValueError, 'RELRO'):
                verify_native.check_sa1_assets(directory / 'test.so')

    def test_legacy_header_without_a_section_is_writable(self):
        source = '_0800032C:\n.incbin "baserom_sa1.gba", 0x32C, 0xC0\n'
        source += '.section .note.GNU-stack,"",%progbits\n'
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            (directory / 'header.s').write_text(rom_data.transform(source, 8))
            subprocess.run(['cc', '-shared', str(directory / 'header.s'), '-o', str(directory / 'test.so')], check=True)
            self.assertEqual(verify_native.check_sa1_assets(directory / 'test.so')['asset_bytes'], 0xC0)

    def test_unsupported_and_out_of_range_directives_are_rejected(self):
        for directive in (
            '.incbin "baserom_sa1.gba", 0x7FFFFF, 2',
            '.incbin "baserom_sa1.gba", 0, 0',
            '.incbin "baserom_sa1.gba", 0, unknown_size',
        ):
            with self.assertRaises(ValueError):
                rom_data.transform(directive, 4)
        self.assertEqual(rom_data.transform('.space 10, 0\n', 4), '.space 10, 0\n')

    def test_failed_save_cannot_clobber_previous_complete_file(self):
        harness = r'''
#include "platform/shared/save_file.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
int main(int argc, char **argv) {
    unsigned char data[131072], restored[131072];
    char temp[1024];
    memset(data, 0x19, sizeof(data));
    assert(Platform_WriteSaveAtomically(argv[1], data, sizeof(data)));
    snprintf(temp, sizeof(temp), "%s.tmp", argv[1]);
    assert(mkdir(temp, 0700) == 0);
    memset(data, 0x52, sizeof(data));
    assert(!Platform_WriteSaveAtomically(argv[1], data, sizeof(data)));
    FILE *file = fopen(argv[1], "rb"); assert(file);
    assert(fread(restored, 1, sizeof(restored), file) == sizeof(restored));
    assert(fgetc(file) == EOF); fclose(file);
    for (unsigned i = 0; i < sizeof(restored); ++i) assert(restored[i] == 0x19);
    remove(temp);
    assert(Platform_WriteSaveAtomically(argv[1], data, sizeof(data)));
    file = fopen(argv[1], "rb"); assert(file);
    assert(fread(restored, 1, sizeof(restored), file) == sizeof(restored));
    fclose(file); assert(memcmp(restored, data, sizeof(data)) == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as work:
            directory = Path(work)
            (directory / "test.c").write_text(harness)
            subprocess.run([
                "cc", "-DSAVE_FILE_TEST=1", "-I", str(ROOT / "include"),
                str(ROOT / "src/platform/shared/save_file.c"), str(directory / "test.c"),
                "-o", str(directory / "test"),
            ], check=True)
            subprocess.run([str(directory / "test"), str(directory / "sa.sav")], check=True)


if __name__ == "__main__":
    unittest.main()
