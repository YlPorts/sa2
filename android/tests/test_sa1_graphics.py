import shutil
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFINES = ["-DPORTABLE=1", "-DPLATFORM_GBA=0", "-DPLATFORM_SDL=1", "-DGAME=GAME_SA1", "-I", str(ROOT / "include")]


class Sa1GraphicsTest(unittest.TestCase):
    def run_check(self, compiler, flags, runner):
        with tempfile.TemporaryDirectory() as work:
            executable = Path(work) / "graphics-check"
            subprocess.run([compiler, *flags, *DEFINES,
                str(ROOT / "src/platform/shared/sa1_bg_sprites.c"),
                str(ROOT / "android/tests/sa1-background-check.c"), "-o", str(executable)], check=True)
            # LeakSanitizer cannot inspect task namespaces in the managed host.
            # Address and undefined-behaviour checks remain enabled.
            environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
            subprocess.run([*runner, str(executable)], check=True, timeout=30, env=environment)

    def test_background_maps_and_ui_oam_under_sanitizers(self):
        self.run_check("cc", ["-O2", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-pie", "-no-pie"], [])

    @unittest.skipUnless(shutil.which("arm-linux-gnueabihf-gcc") and shutil.which("qemu-arm"), "ARM32 host-test tools unavailable")
    def test_optimized_arm32_background_maps_and_ui_oam(self):
        self.run_check("arm-linux-gnueabihf-gcc", ["-O3", "-mfpu=neon", "-mfloat-abi=hard"], ["qemu-arm", "-L", "/usr/arm-linux-gnueabihf"])


if __name__ == "__main__":
    unittest.main()
