import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

SCRIPT = Path(__file__).resolve().parents[1] / "build_paired.py"
SPEC = importlib.util.spec_from_file_location("serverfixes_build_paired", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ReachedConfigure(Exception):
    pass


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
            with (patch.dict(os.environ, clear=True),
                  patch.object(MODULE.sys, "argv", argv),
                  patch.object(MODULE.sys, "platform", "win32"),
                  patch.object(MODULE, "ROOT", root),
                  patch.object(MODULE, "LOG_DIR", None),
                  patch.object(MODULE, "run", run),
                  self.assertRaises(ReachedConfigure)):
                MODULE.main()

    def test_paired_session_paths_and_generator_precedence(self):
        cases = (
            ("unset", ("repo", "main", "shared"), "repo"),
            ("blank", ("main", "shared"), "main"),
            ("shared", ("repo", "main", "shared"), "shared"),
            ("shared", ("repo", "main"), "repo"),
            ("shared", ("main",), "main"),
            ("explicit", ("shared", "explicit"), "explicit"),
        )
        for layout, available, selected in cases:
            with self.subTest(layout=layout, available=available), tempfile.TemporaryDirectory() as temp:
                root = Path(temp).resolve()
                repo, main, shared, game_bin = (root / name for name in ("repo", "funrounds", "shared", "game-bin"))
                (main / "runtime").mkdir(parents=True)
                (main / "runtime/s2sdk.json").touch()
                game_bin.mkdir()
                (game_bin / "tier0.dll").touch()
                suffix = "plugify-gen-v1.2.15/plugify-gen.exe"
                generators = {"repo": repo / ".work/tools" / suffix, "main": main / ".work/tools" / suffix,
                              "shared": shared / "tools" / suffix, "explicit": root / "custom.exe"}
                for key in available:
                    generators[key].parent.mkdir(parents=True, exist_ok=True)
                    generators[key].touch()
                env = {}
                work, sdk_work = repo / ".work", main / ".work"
                if layout == "blank":
                    env["IMOMOE_WORK_ROOT"] = " \t "
                elif layout != "unset":
                    env["IMOMOE_WORK_ROOT"] = f"  {shared / '..' / shared.name}  "
                    work = sdk_work = shared / "sessions"
                work /= "serverfixes-test"
                sdk_work /= "serverfixes-sdk-test"

                argv = [str(SCRIPT), "--funrounds-source", str(main), "--cs2-bin", str(game_bin),
                        "--work-name", "test"]
                if layout == "explicit":
                    argv += ["--generator", str(generators["explicit"])]
                sdk = Mock()
                sdk.host_name.return_value = "windows-x64"
                sdk.windows_cmake_generator.return_value = "Visual Studio 18 2026"
                calls = []

                def run(command):
                    calls.append(command)
                    if command[0] == "cmake":
                        raise ReachedConfigure

                with (patch.dict(os.environ, env, clear=True),
                      patch.object(MODULE.sys, "argv", argv),
                      patch.object(MODULE.sys, "platform", "win32"),
                      patch.object(MODULE, "ROOT", repo),
                      patch.object(MODULE, "LOG_DIR", None),
                      patch.object(MODULE, "load_sdk", return_value=sdk),
                      patch.object(MODULE, "run", run),
                      self.assertRaises(ReachedConfigure)):
                    MODULE.main()

                self.assertTrue((work / "logs").is_dir())
                self.assertEqual(calls[-2], [MODULE.sys.executable, main / "scripts/build-sdk.py",
                                             "--generator", generators[selected], "--work-dir", sdk_work])
                self.assertEqual(calls[-1][3:5], ["-B", work / "policy"])
                self.assertIn(f"-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR={sdk_work / 'build'}", calls[-1])
                if layout in ("shared", "explicit"):
                    self.assertFalse((repo / ".work/serverfixes-test").exists())
                    self.assertFalse((main / ".work/serverfixes-sdk-test").exists())

    def test_linux_container_maps_shared_work_and_resumed_sdk_inputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            repo, main, game_bin = (root / name for name in ("repo", "funrounds", "game-bin"))
            shared = repo / "shared"
            work, sdk_work, source = (shared / "sessions" / name for name in
                                      ("serverfixes-test", "serverfixes-sdk-test", "resumed-source"))
            source.mkdir(parents=True)
            (main / "runtime").mkdir(parents=True)
            (main / "runtime/s2sdk.json").touch()
            game_bin.mkdir()
            (game_bin / "libtier0.so").touch()
            generator = shared / "tools/plugify-gen-v1.2.15/plugify-gen"
            generator.parent.mkdir(parents=True)
            generator.touch()
            argv = [str(SCRIPT), "--funrounds-source", str(main), "--cs2-bin", str(game_bin),
                    "--work-name", "test", "--resume-source", str(source)]
            sdk = Mock(STEAMRT_BASE_IMAGE="steamrt-base", STEAMRT_BUILD_IMAGE="steamrt-build")
            sdk.host_name.return_value = "linux-x64"
            calls = []
            generated_include = "generated-s2sdk/external/plugify/include"

            def run(command):
                calls.append(command)
                if command[0] == "docker":
                    raise ReachedConfigure

            with (patch.dict(os.environ, IMOMOE_WORK_ROOT=str(shared)),
                  patch.object(MODULE.sys, "argv", argv),
                  patch.object(MODULE.sys, "platform", "linux"),
                  patch.object(os, "getuid", return_value=1000, create=True),
                  patch.object(os, "getgid", return_value=1000, create=True),
                  patch.object(MODULE, "ROOT", repo),
                  patch.object(MODULE, "GENERATED_INCLUDE", repo / generated_include),
                  patch.object(MODULE, "LOG_DIR", None),
                  patch.object(MODULE, "load_sdk", return_value=sdk),
                  patch.object(MODULE, "run", run),
                  self.assertRaises(ReachedConfigure)):
                MODULE.main()

            self.assertEqual(calls[-2], [MODULE.sys.executable, main / "scripts/build-sdk.py",
                                         "--generator", generator, "--work-dir", sdk_work,
                                         "--container-image", "steamrt-base", "--resume-source", source])
            docker = calls[-1]
            volumes = [docker[index + 1] for index, item in enumerate(docker) if item == "--volume"]
            self.assertIn(f"{work}:{work}", volumes)
            self.assertIn(f"{sdk_work}:{sdk_work}", volumes)
            self.assertIn(f"{source}:{source}:ro", volumes)
            self.assertIn(f"-DSERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR=/serverfixes/{generated_include}", docker)
            self.assertIn(f"-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR={source / 'external/sourcesdk/public'}", docker)
            self.assertIn(f"-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR={sdk_work / 'build'}", docker)
            cmake = docker.index("cmake")
            self.assertEqual(docker[cmake + 1:cmake + 5], ["-S", "/serverfixes", "-B", str(work / "policy")])


if __name__ == "__main__":
    unittest.main()
