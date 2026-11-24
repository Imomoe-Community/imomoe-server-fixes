import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "build_paired.py"
SPEC = importlib.util.spec_from_file_location("serverfixes_build_paired", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

class PairedBuildSourceTests(unittest.TestCase):
    def test_generated_include_contains_checked_in_sdk_header(self):
        header = MODULE.GENERATED_INCLUDE / "s2sdk/s2sdk.hpp"
        self.assertTrue(header.is_file(), f"Missing checked-in S2SDK header: {header}")

    def test_source_matches_resume_checkout_used_by_sdk_builder(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sdk_work = root / "new-sdk-work"
            resumed = root / "pinned-source"
            resumed.mkdir()
            self.assertEqual(MODULE.choose_sdk_source(sdk_work, resumed), resumed.resolve())

    def test_new_clone_source_is_under_sdk_work_directory(self):
        sdk_work = Path("/tmp/sdk-run")
        self.assertEqual(MODULE.choose_sdk_source(sdk_work, None), sdk_work / "source")

    def test_windows_packaging_configures_the_supplied_sdk_without_funrounds(self):
        class ReachedConfigure(Exception):
            pass

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            source, build, game_bin = (root / name for name in ("source", "build", "game-bin"))
            for directory in (source, build, game_bin):
                directory.mkdir()
            (source / "plugify-plugin-s2sdk.pplugin.in").touch()
            (build / "CMakeCache.txt").write_text("CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026\n", encoding="utf-8")
            (game_bin / "tier0.dll").touch()

            def run(command):
                if command[0] == "cmake":
                    self.assertEqual(command[5:9], ["-G", "Visual Studio 18 2026", "-A", "x64"])
                    self.assertIn(f"-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR={source / 'external/sourcesdk/public'}", command)
                    self.assertIn(f"-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR={build}", command)
                    self.assertIn("-DSERVER_FIXES_CALLBACK_TESTS=ON", command)
                    raise ReachedConfigure
                self.assertEqual(command[1:3], ["-m", "unittest"])

            argv = [str(SCRIPT), "--sdk-source", str(source), "--sdk-build", str(build),
                    "--cs2-bin", str(game_bin)]
            with (patch.object(MODULE.sys, "argv", argv),
                  patch.object(MODULE.sys, "platform", "win32"),
                  patch.object(MODULE, "ROOT", root),
                  patch.object(MODULE, "LOG_DIR", None),
                  patch.object(MODULE, "run", run),
                  self.assertRaises(ReachedConfigure)):
                MODULE.main()

if __name__ == "__main__":
    unittest.main()
