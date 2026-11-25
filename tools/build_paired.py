#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Build and test ServerFixes against a paired SDK and produce one platform ZIP."""
from __future__ import annotations
import argparse
import importlib.util
import os
from datetime import datetime
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
GENERATED_INCLUDE = ROOT / "generated-s2sdk/external/plugify/include"
LOG_DIR: Path | None = None
_COMMAND_INDEX = 0

def run(args: list[str | Path], *, cwd: Path | None = None) -> None:
    global _COMMAND_INDEX
    command = [str(x) for x in args]
    print("+ " + subprocess.list2cmdline(command), flush=True)
    _COMMAND_INDEX += 1
    log = LOG_DIR / f"{_COMMAND_INDEX:02d}.log" if LOG_DIR else None
    with (log.open("x", encoding="utf-8") if log else open(os.devnull, "w", encoding="utf-8")) as output:
        process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                   encoding="utf-8", errors="replace")
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="", flush=True)
            output.write(line)
            output.flush()
        status = process.wait()
    if status:
        raise subprocess.CalledProcessError(status, command)

def choose_sdk_source(sdk_work: Path, resume_source: Path | None) -> Path:
    """Pair CMake inputs with the source checkout the SDK builder actually used."""
    return resume_source.resolve() if resume_source else sdk_work / "source"

def load_sdk(main_source: Path):
    path = main_source / "scripts/build-sdk.py"
    spec = importlib.util.spec_from_file_location("paired_sdk_builder", path)
    if spec is None or spec.loader is None:
        raise FileNotFoundError(f"FunRounds SDK builder not found: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    provider = parser.add_mutually_exclusive_group(required=True)
    provider.add_argument("--sdk-source", type=Path, help="official S2SDK source checkout")
    provider.add_argument("--funrounds-source", type=Path,
                          help="use a FunRounds checkout to build its pinned paired SDK")
    parser.add_argument("--sdk-build", type=Path, help="configured build of --sdk-source")
    parser.add_argument("--steamrt-image", help="Steam Runtime GCC 14 image used for --sdk-build on Linux")
    parser.add_argument("--generator", type=Path,
                        help="plugify-gen v1.2.15 for --funrounds-source; otherwise search IMOMOE_WORK_ROOT/tools, "
                             "then ServerFixes and FunRounds .work/tools")
    parser.add_argument("--work-name", help="unique session name in IMOMOE_WORK_ROOT/sessions or checkout-local "
                                            ".work; defaults to the current timestamp")
    parser.add_argument("--resume-source", type=Path, help="reuse a clean pinned SDK source checkout for the selected host")
    parser.add_argument("--cs2-bin", type=Path, help="CS2 game/bin/win64 or game/bin/linuxsteamrt64 for callback tests; defaults to CS2_GAME_BIN")
    args = parser.parse_args()
    main_source = args.funrounds_source.resolve() if args.funrounds_source else None
    work_name = args.work_name or datetime.now().strftime("%Y%m%d-%H%M%S")
    if not work_name or Path(work_name).name != work_name or work_name in {".", ".."}:
        parser.error("--work-name must be one directory name, not a path")
    raw_work_root = os.environ.get("IMOMOE_WORK_ROOT", "").strip()
    work_root = Path(raw_work_root).expanduser().resolve() if raw_work_root else None
    sessions = work_root / "sessions" if work_root else None
    work = (sessions if sessions else ROOT / ".work") / f"serverfixes-{work_name}"
    if work.exists():
        parser.error(f"Work directory already exists; choose another --work-name: {work}")
    sdk_args = None
    sdk = None
    if main_source:
        if args.sdk_build or args.steamrt_image:
            parser.error("--sdk-build and --steamrt-image are for --sdk-source")
        if not (main_source / "runtime/s2sdk.json").is_file():
            parser.error(f"Not a FunRounds checkout: {main_source}")
        executable = "plugify-gen.exe" if sys.platform == "win32" else "plugify-gen"
        tool_dirs = [ROOT / ".work/tools", main_source / ".work/tools"]
        if work_root:
            tool_dirs.insert(0, work_root / "tools")
        generator = args.generator.resolve() if args.generator else next(
            (path for directory in tool_dirs
             if (path := directory / "plugify-gen-v1.2.15" / executable).is_file()), None)
        if generator is None or not generator.is_file():
            parser.error("Install plugify-gen 1.2.15 in plugify-gen-v1.2.15 under IMOMOE_WORK_ROOT/tools "
                         "or a checkout's .work/tools, or pass --generator PATH")
        sdk_work = (sessions if sessions else main_source / ".work") / f"serverfixes-sdk-{work_name}"
        if sdk_work.exists():
            parser.error(f"SDK work directory already exists; choose another --work-name: {sdk_work}")
        sdk = load_sdk(main_source)
        target = sdk.host_name()
        sdk_args = [sys.executable, main_source / "scripts/build-sdk.py", "--generator", generator,
                    "--work-dir", sdk_work]
        if target == "linux-x64":
            sdk_args += ["--container-image", sdk.STEAMRT_BASE_IMAGE]
        if args.resume_source:
            sdk_args += ["--resume-source", args.resume_source.resolve()]
        source = choose_sdk_source(sdk_work, args.resume_source)
        build = sdk_work / "build"
    else:
        if args.generator or args.resume_source:
            parser.error("--generator and --resume-source are for --funrounds-source")
        if not args.sdk_build:
            parser.error("--sdk-source requires --sdk-build")
        source, build = args.sdk_source.resolve(), args.sdk_build.resolve()
        if not (source / "plugify-plugin-s2sdk.pplugin.in").is_file() or not (build / "CMakeCache.txt").is_file():
            parser.error("Pass the S2SDK checkout and its configured build directory")
        if sys.platform not in ("win32", "linux"):
            parser.error("Only Windows and Linux builds are supported")
        target = "windows-x64" if sys.platform == "win32" else "linux-x64"
        if target == "windows-x64":
            cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
            generator = next((line.split("=", 1)[1] for line in cache.splitlines()
                              if line.startswith("CMAKE_GENERATOR:INTERNAL=")), "")
            if not generator.startswith("Visual Studio "):
                parser.error("Configure the Windows S2SDK build with a Visual Studio generator and -A x64")
        elif not args.steamrt_image:
            parser.error("Linux builds with --sdk-source require --steamrt-image")
    cs2_bin = args.cs2_bin or (Path(os.environ["CS2_GAME_BIN"]) if os.environ.get("CS2_GAME_BIN") else None)
    if cs2_bin is None or not cs2_bin.is_dir():
        parser.error("Callback tests need the CS2 runtime library directory; pass --cs2-bin or set CS2_GAME_BIN")
    cs2_bin = cs2_bin.resolve()
    runtime_file = "tier0.dll" if target == "windows-x64" else "libtier0.so"
    if not (cs2_bin / runtime_file).is_file():
        parser.error(f"The selected game runtime directory does not contain {runtime_file}: {cs2_bin}")
    work.mkdir(parents=True)
    global LOG_DIR
    LOG_DIR = work / "logs"
    LOG_DIR.mkdir()
    run([sys.executable, "-m", "unittest", "discover", "-s", ROOT / "tests", "-p", "test_*.py"])
    run([sys.executable, "-m", "unittest", "discover", "-s", ROOT / "tools/tests", "-p", "test_*.py"])
    if sdk_args:
        run(sdk_args)
    plugify = source / "external/plugify/include"
    source_sdk = source / "external/sourcesdk"
    config = [
        f"-DSERVER_FIXES_PLUGIFY_INCLUDE_DIR={plugify}",
        f"-DSERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR={GENERATED_INCLUDE}",
        f"-DSERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR={source_sdk / 'public'}",
        f"-DSERVER_FIXES_SOURCE_SDK_BUILD_DIR={build}",
        f"-DSERVER_FIXES_SOURCE_SDK_LIBRARY_DIR={source_sdk / ('lib/win64' if target == 'windows-x64' else 'lib/linuxsteamrt64')}",
        f"-DSERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR={build / 'external/sourcesdk/protos'}",
    ]
    policy = work / "policy"
    production = work / "production"
    if target == "windows-x64":
        cmake_gen = ["-G", sdk.windows_cmake_generator() if sdk else generator, "-A", "x64"]
        build_args = ["--config", "Release", "--parallel", "1", "--", "/nr:false"]
        container = None
    else:
        cmake_gen = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=/usr/bin/gcc-14",
                     "-DCMAKE_CXX_COMPILER=/usr/bin/g++-14"]
        build_args = ["--config", "Release", "--parallel", str(min(4, os.cpu_count() or 1))]
        if sdk:
            sdk.prepare_steamrt_build_image(sdk.STEAMRT_BASE_IMAGE)
            container = sdk.STEAMRT_BUILD_IMAGE
        else:
            container = args.steamrt_image

    if container:
        mounts = [(ROOT, "/serverfixes" if main_source else str(ROOT), ""),
                  (cs2_bin, "/cs2-bin", ":ro")]
        if main_source:
            mounts.append((main_source, "/workspace", ""))
        else:
            mounts += [(path, str(path), ":ro") for path in (source, build)]
        if sessions:
            # Keep external SDK paths identical to those in the SDK builder's CMake cache.
            mounts.append((work, str(work), ""))
            if main_source:
                mounts.append((sdk_work, str(sdk_work), ""))
                if args.resume_source and not source.is_relative_to(main_source):
                    mounts.append((source, str(source), ":ro"))
        volume_args = [arg for host, guest, mode in mounts for arg in ("--volume", f"{host}:{guest}{mode}")]
        path_mappings = sorted(mounts, key=lambda mount: len(mount[0].parts), reverse=True)

    def in_container(item: str | Path) -> str:
        value = str(item)
        flag, sep, raw_path = value.partition("=") if value.startswith("-D") else ("", "", value)
        path = Path(raw_path)
        for host, guest, _ in path_mappings:
            if path.is_relative_to(host):
                if guest == str(host):
                    return value
                if path == host:
                    return flag + sep + guest
                return f"{flag}{sep}{guest}/{path.relative_to(host).as_posix()}"
        return value

    def cmake(command: list[str | Path], *, test_runtime: bool = False) -> None:
        if container:
            runtime = in_container(source_sdk / "lib/linuxsteamrt64")
            run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                 "--env", "HOME=/tmp", "--env", f"LD_LIBRARY_PATH=/cs2-bin:{runtime}",
                 *volume_args, "--workdir", in_container(ROOT), container, *map(in_container, command)])
        elif test_runtime:
            old_path = os.environ.get("PATH", "")
            os.environ["PATH"] = str(cs2_bin) + os.pathsep + old_path
            try:
                run(command)
            finally:
                os.environ["PATH"] = old_path
        else:
            run(command)

    cmake(["cmake", "-S", ROOT, "-B", policy, *cmake_gen, *config,
           "-DSERVER_FIXES_POLICY_TESTS_ONLY=ON", "-DSERVER_FIXES_CALLBACK_TESTS=ON"])
    cmake(["cmake", "--build", policy, *build_args])
    cmake(["ctest", "--test-dir", policy, "--build-config", "Release", "--output-on-failure"], test_runtime=True)
    cmake(["cmake", "-S", ROOT, "-B", production, *cmake_gen, *config])
    cmake(["cmake", "--build", production, "--target", "imomoe-server-fixes", *build_args])
    binary = production / "bin" / ("imomoe-server-fixes.dll" if target == "windows-x64" else "libimomoe-server-fixes.so")
    if not binary.is_file():
        raise FileNotFoundError(f"Expected production plugin missing: {binary}")
    package = work / "package"
    plugin = package / "game/csgo/addons/plugify/envs/imomoe_server_fixes"
    plugin_bin = plugin / "bin"
    plugin_bin.mkdir(parents=True)
    shutil.copy2(binary, plugin_bin / binary.name)
    shutil.copy2(ROOT / "imomoe_server_fixes.pplugin", plugin / "imomoe_server_fixes.pplugin")
    shutil.copy2(ROOT / "LICENSE", plugin / "LICENSE")
    third_party = plugin / "THIRD_PARTY"
    third_party.mkdir()
    license_names = ("LICENSE", "LICENSE.txt", "LICENSE.md", "COPYING", "COPYING.txt")
    license_files = [path for path in source.rglob("*") if path.is_file() and path.name in license_names
                     and ".git" not in path.parts and "build" not in path.parts]
    if not license_files:
        raise FileNotFoundError(f"Pinned SDK source tree contains no third-party license files: {source}")
    for path in license_files:
        destination = third_party / path.relative_to(source)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
    (plugin / "gamedata").mkdir()
    shutil.copy2(ROOT / "gamedata/imomoe_server_fixes.jsonc", plugin / "gamedata/imomoe_server_fixes.jsonc")
    (package / "game/csgo/cfg").mkdir(parents=True)
    shutil.copy2(ROOT / "configs/imomoe_server_fixes.cfg", package / "game/csgo/cfg/imomoe_server_fixes.cfg")
    archive = ROOT / "dist" / f"imomoe-server-fixes-{target}-{work_name}.zip"
    archive.parent.mkdir(exist_ok=True)
    if archive.exists():
        raise FileExistsError(archive)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(package).as_posix())
    required = {f"game/csgo/addons/plugify/envs/imomoe_server_fixes/bin/{binary.name}",
                "game/csgo/addons/plugify/envs/imomoe_server_fixes/imomoe_server_fixes.pplugin",
                "game/csgo/addons/plugify/envs/imomoe_server_fixes/gamedata/imomoe_server_fixes.jsonc",
                "game/csgo/cfg/imomoe_server_fixes.cfg",
                "game/csgo/addons/plugify/envs/imomoe_server_fixes/LICENSE"}
    with zipfile.ZipFile(archive) as zf:
        missing = required - set(zf.namelist())
        if missing:
            raise RuntimeError(f"Package is missing required files: {sorted(missing)}")
    if not any(name.startswith("game/csgo/addons/plugify/envs/imomoe_server_fixes/THIRD_PARTY/")
               for name in zipfile.ZipFile(archive).namelist()):
        raise RuntimeError("Package is missing paired SDK third-party license files")
    print(f"Verified {target} package: {archive}")

if __name__ == "__main__":
    main()
