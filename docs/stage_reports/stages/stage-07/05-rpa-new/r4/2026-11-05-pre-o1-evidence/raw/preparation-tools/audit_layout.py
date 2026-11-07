"""Compile sizeof-only probes; no gameplay, device access, or formal sampling."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
VCVARS = Path("D:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    destination = args.output.resolve()
    destination.mkdir(parents=True, exist_ok=False)
    source = Path(__file__).with_name("element_sizes.cpp")
    definitions = (
        "CUEXIS_AUDIO_STATIC_DEFINE", "CUEXIS_CONTENT_STATIC_DEFINE", "CUEXIS_CORE_STATIC_DEFINE",
        "CUEXIS_PLAYBACK_STATIC_DEFINE", "CUEXIS_JUDGEMENT_STATIC_DEFINE",
        "CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE=1", "CUEXIS_EXPERIMENTAL_BUILD=1",
        "NOMINMAX", "WIN32_LEAN_AND_MEAN", "NDEBUG",
    )
    includes = [build / "generated", ROOT / "app/execution_host", ROOT / "engine/core/internal",
                ROOT / "engine/judgement/src", ROOT / "engine/judgement/internal",
                build / "vcpkg_installed/x64-windows/include", *sorted(ROOT.glob("engine/*/include"))]
    executable = destination / "element-sizes.exe"
    command = ["cl.exe", "/nologo", "/TP", "/std:c++20", "/EHsc", "/MD", "/O2", "/utf-8",
               *["/D" + value for value in definitions],
               *["/I" + str(path) for path in includes],
               "/Fo" + str(destination / "element-sizes.obj"), "/Fe" + str(executable), str(source)]
    batch = destination / "compile.cmd"
    batch.write_text('@echo off\ncall "' + str(VCVARS) +
                     '" >nul\nif errorlevel 1 exit /b %errorlevel%\n' +
                     subprocess.list2cmdline(command) + "\nexit /b %errorlevel%\n", encoding="utf-8")
    with (destination / "compile.log").open("wb") as log:
        subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=destination,
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    output = subprocess.check_output([str(executable)], cwd=destination)
    (destination / "raw.json").write_bytes(output)
    result = json.loads(output)
    files = [source, Path(__file__), ROOT / "vcpkg.json", ROOT / "vcpkg-configuration.json",
             ROOT / "CMakePresets.json", ROOT / "cmake/CuexisVersion.cmake",
             ROOT / "app/execution_host/frame_delivery.cpp",
             ROOT / "app/execution_host/audio_worker.cpp",
             ROOT / "app/player/src/execution_host_close.cpp",
             ROOT / "app/player/src/player_execution.cpp"]
    # Hash the complete project header inputs conservatively, including private type definitions.
    for tree in (ROOT / "engine", ROOT / "app/execution_host"):
        files.extend(path for path in tree.rglob("*")
                     if path.is_file() and path.suffix in (".hpp", ".h", ".inl"))
    result.update(schema="rpa.r4.element-sizes.v1", config="Release", sizeof_only=True,
                  command=command, files={str(path.relative_to(ROOT)):
                  hashlib.sha256(path.read_bytes()).hexdigest() for path in files})
    (destination / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8",
                                           newline="\n")
    print(json.dumps(result["sizes"], indent=2))


if __name__ == "__main__":
    main()
