"""Configure and report a directory-based vcpkg archive cache for CI."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def cache_directory():
    directory = (Path(os.environ["RUNNER_TEMP"]) / "cuexis-vcpkg-archives").resolve()
    if any(char in str(directory) for char in ",;\r\n"):
        raise ValueError("Runner cache directory contains a binary-source separator")
    directory.mkdir(parents=True, exist_ok=True)
    return directory


def append_lines(variable, lines):
    with open(os.environ[variable], "a", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--report", action="store_true")
    parser.add_argument("--cache-hit", default="")
    arguments = parser.parse_args()
    directory = cache_directory()
    if arguments.report:
        archives = list(directory.rglob("*.zip"))
        print(json.dumps({"exact_key_hit": arguments.cache_hit,
                          "archives_after_restore": len(archives),
                          "archive_bytes_after_restore": sum(p.stat().st_size for p in archives)}))
        return
    if not arguments.compiler:
        parser.error("--compiler is required when preparing the cache")
    executable = shutil.which(arguments.compiler)
    if not executable:
        raise FileNotFoundError("Compiler not found: " + arguments.compiler)
    compiler = Path(executable).resolve()
    is_msvc = compiler.name.lower() in {"cl", "cl.exe"}
    command = [str(compiler)] + ([] if is_msvc else ["--version"])
    result = subprocess.run(command, capture_output=True, timeout=30)
    if result.returncode not in ({0, 2} if is_msvc else {0}):
        raise RuntimeError("Compiler version probe failed: " + str(result.returncode))
    digest = hashlib.sha256()
    with compiler.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    digest.update(result.stdout)
    digest.update(result.stderr)
    for variable in ["RUNNER_OS", "RUNNER_ARCH", "ImageVersion", "VCToolsVersion",
                     "WindowsSDKVersion", "VSCMD_ARG_TGT_ARCH"]:
        digest.update(variable.encode("ascii") + b"=" + os.environ.get(variable, "").encode("utf-8"))
    sources = "clear;files," + directory.as_posix() + ",readwrite"
    append_lines("GITHUB_ENV", ["VCPKG_DEFAULT_BINARY_CACHE=" + str(directory),
                                "VCPKG_BINARY_SOURCES=" + sources])
    append_lines("GITHUB_OUTPUT", ["path=" + directory.as_posix(),
                                   "compiler-key=" + digest.hexdigest()[:24]])
    print(json.dumps({"compiler": str(compiler), "compiler_returncode": result.returncode,
                      "compiler_key": digest.hexdigest()[:24], "cache_directory": str(directory),
                      "binary_sources": sources}))


if __name__ == "__main__":
    main()
