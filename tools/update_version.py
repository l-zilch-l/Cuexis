"""Validate and update the Cuexis date-based build identity."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CMAKE_VERSION_FILE = ROOT / "cmake" / "CuexisVersion.cmake"
VCPKG_MANIFEST = ROOT / "vcpkg.json"
VERSION_RE = re.compile(
    r"^(?P<year>[0-9]{2})[.](?P<month>[0-9]{2})[.](?P<day>[0-9]{2})-"
    r"(?P<build>[1-9][0-9]*)$"
)
MAX_BUILD = 2_147_483_647


@dataclass(frozen=True)
class Version:
    year: int
    month: int
    day: int
    build: int

    @property
    def canonical(self) -> str:
        return f"{self.year:02d}.{self.month:02d}.{self.day:02d}-{self.build}"


def parse_version(value: str) -> Version:
    match = VERSION_RE.fullmatch(value)
    if match is None:
        raise ValueError("version must use yy.mm.dd-build, for example 26.08.01-1")

    version = Version(**{name: int(component) for name, component in match.groupdict().items()})
    if version.build > MAX_BUILD:
        raise ValueError(f"build must not exceed {MAX_BUILD}")
    try:
        date(2000 + version.year, version.month, version.day)
    except ValueError as error:
        raise ValueError(f"invalid calendar date in version: {error}") from error
    return version


def read_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8").replace("\r\n", "\n")


def cmake_component(text: str, name: str) -> int:
    pattern = re.compile(rf"^set[(]{re.escape(name)} ([0-9]+)[)]$", re.MULTILINE)
    matches = pattern.findall(text)
    if len(matches) != 1:
        raise ValueError(f"expected exactly one {name} assignment in {CMAKE_VERSION_FILE}")
    return int(matches[0])


def version_from_cmake(text: str) -> Version:
    if "CUEXIS_VERSION_HOUR" in text:
        raise ValueError("legacy CUEXIS_VERSION_HOUR assignment is not allowed")
    return parse_version(
        f"{cmake_component(text, 'CUEXIS_VERSION_YEAR'):02d}."
        f"{cmake_component(text, 'CUEXIS_VERSION_MONTH'):02d}."
        f"{cmake_component(text, 'CUEXIS_VERSION_DAY'):02d}-"
        f"{cmake_component(text, 'CUEXIS_VERSION_BUILD')}"
    )


def manifest_object(text: str) -> dict[str, Any]:
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError(f"{VCPKG_MANIFEST} root must be a JSON object")
    return value


def version_from_manifest(text: str) -> Version:
    value = manifest_object(text).get("version-string")
    if not isinstance(value, str):
        raise ValueError(f"{VCPKG_MANIFEST} must contain a string version-string")
    return parse_version(value)


def replace_cmake_component(text: str, name: str, value: int) -> str:
    pattern = re.compile(rf"^set[(]{re.escape(name)} [0-9]+[)]$", re.MULTILINE)
    updated, count = pattern.subn(f"set({name} {value})", text)
    if count != 1:
        raise ValueError(f"expected exactly one {name} assignment in {CMAKE_VERSION_FILE}")
    return updated


def render_cmake(text: str, version: Version) -> str:
    updates = {
        "CUEXIS_VERSION_YEAR": version.year,
        "CUEXIS_VERSION_MONTH": version.month,
        "CUEXIS_VERSION_DAY": version.day,
        "CUEXIS_VERSION_BUILD": version.build,
    }
    for name, value in updates.items():
        text = replace_cmake_component(text, name, value)
    if version_from_cmake(text) != version:
        raise ValueError("rendered CMake version does not match the requested version")
    return text


def render_manifest(text: str, version: Version) -> str:
    value = manifest_object(text)
    value["version-string"] = version.canonical
    rendered = json.dumps(value, ensure_ascii=True, indent=2) + "\n"
    if version_from_manifest(rendered) != version:
        raise ValueError("rendered vcpkg version does not match the requested version")
    return rendered


def encoded_with_original_newlines(path: Path, text: str) -> bytes:
    original = path.read_bytes()
    newline = "\r\n" if b"\r\n" in original else "\n"
    return text.replace("\n", newline).encode("utf-8")


def write_transaction(updates: dict[Path, str]) -> None:
    originals = {path: path.read_bytes() for path in updates}
    temporary: dict[Path, Path] = {}
    try:
        for path, text in updates.items():
            descriptor, name = tempfile.mkstemp(
                prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
            )
            temporary[path] = Path(name)
            with os.fdopen(descriptor, "wb") as output:
                output.write(encoded_with_original_newlines(path, text))
        for path, temporary_path in temporary.items():
            os.replace(temporary_path, path)
    except Exception:
        for path, content in originals.items():
            path.write_bytes(content)
        raise
    finally:
        for temporary_path in temporary.values():
            temporary_path.unlink(missing_ok=True)


def check_current() -> Version:
    cmake_version = version_from_cmake(read_text(CMAKE_VERSION_FILE))
    manifest_version = version_from_manifest(read_text(VCPKG_MANIFEST))
    if cmake_version != manifest_version:
        raise ValueError(
            f"version mismatch: CMake is {cmake_version.canonical}, "
            f"vcpkg is {manifest_version.canonical}"
        )
    return cmake_version


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Update CuexisVersion.cmake and vcpkg.json to one yy.mm.dd-build version."
    )
    parser.add_argument("version", nargs="?", help="canonical version, for example 26.08.01-1")
    parser.add_argument(
        "--check", action="store_true", help="validate the current files without writing"
    )
    arguments = parser.parse_args()

    if arguments.check:
        if arguments.version is not None:
            parser.error("--check does not accept a version")
        current = check_current()
        print(f"Cuexis version is consistent: {current.canonical}")
        return 0
    if arguments.version is None:
        parser.error("provide a version or use --check")

    requested = parse_version(arguments.version)
    cmake_text = render_cmake(read_text(CMAKE_VERSION_FILE), requested)
    manifest_text = render_manifest(read_text(VCPKG_MANIFEST), requested)
    write_transaction({CMAKE_VERSION_FILE: cmake_text, VCPKG_MANIFEST: manifest_text})
    current = check_current()
    print(f"Updated Cuexis version to {current.canonical}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        print(f"Version update failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
