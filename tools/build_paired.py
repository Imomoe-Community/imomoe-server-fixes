#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Build the pinned paired S2SDK, test ServerFixes, and produce one platform ZIP."""
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
    parser.add_argument("--funrounds-source", type=Path, required=True,
                        help="FunRounds checkout containing the pinned scripts/build-sdk.py and runtime/s2sdk.json")
    parser.add_argument("--generator", type=Path, help="plugify-gen v1.2.15 executable; defaults to .work/tools")
    parser.add_argument("--work-name", help="new unique name under this repo's .work/; defaults to the current timestamp")
    parser.add_argument("--resume-source", type=Path, help="reuse a clean pinned SDK source checkout for the selected host")
    parser.add_argument("--cs2-bin", type=Path, help="CS2 game/bin/win64 or game/bin/linuxsteamrt64 for callback tests; defaults to CS2_GAME_BIN")
    args = parser.parse_args()
    main_source = args.funrounds_source.resolve()
    if not (main_source / "runtime/s2sdk.json").is_file():
        parser.error(f"Not a FunRounds checkout: {main_source}")
    generator = args.generator.resolve() if args.generator else None
    if generator is None:
        candidates = [ROOT / ".work/tools/plugify-gen-v1.2.15" / ("plugify-gen.exe" if sys.platform == "win32" else "plugify-gen"),
                      main_source / ".work/tools/plugify-gen-v1.2.15" / ("plugify-gen.exe" if sys.platform == "win32" else "plugify-gen")]
        generator = next((path for path in candidates if path.is_file()), None)
        if generator is None:
            parser.error("plugify-gen 1.2.15 is not installed in .work/tools; download it from the project build setup or pass --generator PATH")
    if not generator.is_file():
        parser.error(f"Missing plugify-gen: {generator}")
    work_name = args.work_name or datetime.now().strftime("%Y%m%d-%H%M%S")
    if not work_name or Path(work_name).name != work_name or work_name in {".", ".."}:
        parser.error("--work-name must be one directory name, not a path")
    work = ROOT / ".work" / f"serverfixes-{work_name}"
    if work.exists():
        parser.error(f"Work directory already exists; choose another --work-name: {work}")
    sdk_work = main_source / ".work" / f"serverfixes-sdk-{work_name}"
    if sdk_work.exists():
        parser.error(f"SDK work directory already exists; choose another --work-name: {sdk_work}")
    sdk = load_sdk(main_source)
    target = sdk.host_name()
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
    sdk_args = [sys.executable, main_source / "scripts/build-sdk.py", "--generator", generator,
                "--work-dir", sdk_work]
    if target == "linux-x64":
        sdk_args += ["--container-image", sdk.STEAMRT_BASE_IMAGE]
    if args.resume_source:
        sdk_args += ["--resume-source", args.resume_source.resolve()]
    run([sys.executable, "-m", "unittest", "discover", "-s", ROOT / "tests", "-p", "test_*.py"])
    run([sys.executable, "-m", "unittest", "discover", "-s", ROOT / "tools/tests", "-p", "test_*.py"])
    run(sdk_args)
    source = choose_sdk_source(sdk_work, args.resume_source)
    build = sdk_work / "build"
    output = sdk_work / "output"
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
    sdk_runtime_container = str(source_sdk / "lib/linuxsteamrt64").replace(str(main_source), "/workspace")
    production = work / "production"
    if target == "windows-x64":
        gen = sdk.windows_cmake_generator()
        cmake_gen = ["-G", gen, "-A", "x64"]
        build_args = ["--config", "Release", "--parallel", "1", "--", "/nr:false"]
        container = None
    else:
        cmake_gen = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=/usr/bin/gcc-14",
                     "-DCMAKE_CXX_COMPILER=/usr/bin/g++-14"]
        build_args = ["--config", "Release", "--parallel", str(min(4, os.cpu_count() or 1))]
        sdk.prepare_steamrt_build_image(sdk.STEAMRT_BASE_IMAGE)
        container = sdk.STEAMRT_BUILD_IMAGE

    def cmake(command: list[str | Path]) -> None:
        if container is None:
            run(command)
        else:
            run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                 "--env", "HOME=/tmp", "--volume", f"{ROOT}:/serverfixes",
                 "--volume", f"{main_source}:/workspace", "--volume", f"{cs2_bin}:/cs2-bin:ro",
                 "--workdir", "/serverfixes", container, *[str(x) for x in command]])

    # Container paths are remapped explicitly after composing host paths.
    def cmake_args(root: Path, binary: Path, extra: list[str]) -> list[str | Path]:
        values: list[str | Path] = ["cmake", "-S", root, "-B", binary, *cmake_gen, *extra]
        if container:
            values = [str(x).replace(str(ROOT), "/serverfixes").replace(str(main_source), "/workspace") for x in values]
        return values
    def build_target(binary: Path, targets: list[str]) -> None:
        values: list[str | Path] = ["cmake", "--build", binary, *(["--target", *targets] if targets else []), *build_args]
        if container:
            values = [str(x).replace(str(ROOT), "/serverfixes").replace(str(main_source), "/workspace") for x in values]
        if container:
            run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                 "--env", "HOME=/tmp", "--env", f"LD_LIBRARY_PATH=/cs2-bin:{sdk_runtime_container}",
                 "--volume", f"{ROOT}:/serverfixes", "--volume", f"{main_source}:/workspace",
                 "--volume", f"{cs2_bin}:/cs2-bin:ro", "--workdir", "/serverfixes", container, *values])
        else:
            run(values)
    def test(binary: Path) -> None:
        values = ["ctest", "--test-dir", binary, "--build-config", "Release", "--output-on-failure"]
        if container:
            values = [str(x).replace(str(ROOT), "/serverfixes").replace(str(main_source), "/workspace") for x in values]
            run(["docker", "run", "--rm", "--network", "none", "--user", f"{os.getuid()}:{os.getgid()}",
                 "--env", "HOME=/tmp", "--env", f"LD_LIBRARY_PATH=/cs2-bin:{sdk_runtime_container}",
                 "--volume", f"{ROOT}:/serverfixes", "--volume", f"{main_source}:/workspace",
                 "--volume", f"{cs2_bin}:/cs2-bin:ro", "--workdir", "/serverfixes", container, *values])
        else:
            old_path = os.environ.get("PATH", "")
            os.environ["PATH"] = str(cs2_bin) + os.pathsep + old_path
            try:
                run(values)
            finally:
                os.environ["PATH"] = old_path
    cmake(cmake_args(ROOT, policy, [*config, "-DSERVER_FIXES_POLICY_TESTS_ONLY=ON", "-DSERVER_FIXES_CALLBACK_TESTS=ON"]))
    build_target(policy, [])
    test(policy)
    cmake(cmake_args(ROOT, production, [*config]))
    build_target(production, ["imomoe-server-fixes"])
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
