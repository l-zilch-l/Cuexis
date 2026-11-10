"""Rebuild and re-run the control matrix for the MinGW allocation-probe limit.

Every claim this archive makes about the limit is produced by this script from two source files and
the repository's own headers; no executable is archived, because each one is 20 MB of statically
linked Catch2 debug runtime. Run it with:

    python -B record_variants.py [--work <dir>] [--json <file>]

`--json` defaults to `variants.json` next to the script, which is the file the archive manifest binds.

The first four variants differ only in how the runtime and the operators are supplied:

  dynamic-runtime          tiny-repro.cpp, default flags: link libstdc++-6.dll, replace the operators
                           with the offset-payload scheme. This is the failing configuration.
  static-runtime           tiny-repro.cpp plus -static-libstdc++ -static-libgcc: the same replacement,
                           but every delete now reaches this module's operator delete.
  resource-only            tiny-repro.cpp plus CUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY=1: the header
                           keeps allocate/release and installs no operators, which is what the shared
                           and MinGW build rows do.
  passthrough-operators    control-passthrough-new.cpp: operators are replaced as well, but the
                           payload stays at the block start, so nothing is misaligned for the DLL.

The first three compile against `repro/pd_allocation_probe.as-run-2.hpp`, the header bytes the failing
rows actually built: this script materialises them from `git show 3fbc31f:tests/pd/pd_allocation_probe.hpp`
and refuses to continue unless their sha256 equals the hash that run 2's own manifest recorded for that
path. Compiling them against today's header would test a file the run never saw.

The fifth variant is exploratory and asserts nothing: `current-header-dynamic` compiles the same
reproduction against today's header with the dynamic runtime, and its exit code is recorded without a
prediction. It answers whether the header's own changes removed this crash; either answer leaves the
platform gate standing, because a full-operator census cannot be trusted in a topology whose C++ runtime
lives in another module.

The script exits non-zero if any variant stops matching its recorded expectation, so a stale archive
cannot silently keep asserting an old result.
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(r"D:\Cuexis-redesign")
HERE = Path(__file__).resolve().parent
CXX = Path(r"C:\msys64\ucrt64\bin\c++.exe")
CATCH = REPO / "out" / "build" / "mingw-headless-debug" / "vcpkg_installed" / "x64-mingw-static"
HEADER = REPO / "tests" / "pd" / "pd_allocation_probe.hpp"
AS_RUN_HEAD = "3fbc31f68430ce2d0ad576094ff617cc09772f10"
AS_RUN_PATH = "tests/pd/pd_allocation_probe.hpp"
AS_RUN_RUN = REPO / "out" / "rpa-acceptance" / "20261101-021741-4daf2bb2"
REPRO = HERE / "repro"
AS_RUN_COPY = REPRO / "pd_allocation_probe.as-run-2.hpp"
INTERMEDIATE_COPY = REPRO / "pd_allocation_probe.magic-check-intermediate.hpp"
SOURCES = HERE / "archive-src"

INCLUDES = [
    "-I" + str(REPO / "tests" / "pd"),
    "-I" + str(REPO / "engine" / "core" / "internal"),
    "-I" + str(CATCH / "include"),
]
LIBRARIES = [
    "-L" + str(CATCH / "debug" / "lib"),
    "-L" + str(CATCH / "debug" / "lib" / "manual-link"),
    "-lCatch2Maind",
    "-lCatch2d",
]
BASE = ["-std=c++20", "-O0", "-g"]

VARIANTS = [
    {
        "name": "dynamic-runtime",
        "source": "tiny-repro.cpp",
        "header": "as-run",
        "extra": [],
        "expect": "0xc0000374",
        "why": "offset payload deleted by libstdc++-6.dll's own operator delete",
    },
    {
        "name": "static-runtime",
        "source": "tiny-repro.cpp",
        "header": "as-run",
        "extra": ["-static-libstdc++", "-static-libgcc"],
        "expect": "0x00000000",
        "why": "every delete of a payload reaches the replacement in this module",
    },
    {
        "name": "resource-only",
        "source": "tiny-repro.cpp",
        "header": "as-run",
        "extra": ["-DCUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY=1"],
        "expect": "0x00000000",
        "why": "no operators installed, so no payload can be misaligned",
    },
    {
        "name": "passthrough-operators",
        "source": "control-passthrough-new.cpp",
        "header": "none",
        "extra": [],
        "expect": "0x00000000",
        "why": "operators replaced, payload at block start: isolates the header offset",
    },
    {
        "name": "current-header-dynamic",
        "source": "tiny-repro.cpp",
        "header": "current",
        "extra": [],
        "expect": None,
        "why": "exploratory: today's header under the same dynamic runtime; no prediction recorded",
    },
]

HEAP_CORRUPTION = 0xC0000374


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run(command, cwd):
    started = subprocess.run(command, cwd=str(cwd), capture_output=True)
    return started


def text(value):
    return value.decode("utf-8", "replace")


def blob_at(commit, path):
    result = subprocess.run(["git", "-C", str(REPO), "show", "{}:{}".format(commit, path)],
                            capture_output=True)
    if result.returncode != 0:
        raise SystemExit("git show {}:{} failed: {}".format(commit, path, text(result.stderr)))
    return result.stdout


def materialise_as_run():
    """Return the exact header bytes run 2 built, and refuse anything that does not reproduce them."""
    blob = blob_at(AS_RUN_HEAD, AS_RUN_PATH)
    manifest = json.loads((AS_RUN_RUN / "manifest.json").read_text(encoding="utf-8"))
    recorded = manifest["source"]["files"][AS_RUN_PATH]
    digest = hashlib.sha256(blob).hexdigest()
    if recorded != digest:
        raise SystemExit("as-run header does not reproduce run 2's recorded hash: {} != {}".format(
            digest, recorded))
    return blob, {
        "source": "git show {}:{}".format(AS_RUN_HEAD[:8], AS_RUN_PATH),
        "commit": AS_RUN_HEAD,
        "sha256": digest,
        "bytes": len(blob),
        "manifest_run": str(AS_RUN_RUN.relative_to(REPO)),
        "manifest_sha256": recorded,
    }


def intermediate_facts():
    """The unmeasured magic-check instrument, kept only so the report can name it by hash."""
    if not INTERMEDIATE_COPY.is_file():
        return None
    return {
        "path": str(INTERMEDIATE_COPY.relative_to(HERE)),
        "sha256": sha256_file(INTERMEDIATE_COPY),
        "bytes": INTERMEDIATE_COPY.stat().st_size,
        "note": ("intermediate instrument, never measured: the as-run header plus a block-magic test "
                 "that leaks the blocks it does not recognise instead of freeing them"),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=HERE / "variants")
    parser.add_argument("--json", type=Path, default=HERE / "variants.json")
    arguments = parser.parse_args()
    if not CXX.is_file():
        raise SystemExit("missing compiler: {}".format(CXX))
    if not (CATCH / "debug" / "lib" / "libCatch2d.a").is_file():
        raise SystemExit("missing Catch2 debug library under {}".format(CATCH))
    work = arguments.work.resolve()
    work.mkdir(parents=True, exist_ok=True)

    as_run_bytes, as_run_facts = materialise_as_run()
    REPRO.mkdir(parents=True, exist_ok=True)
    AS_RUN_COPY.write_bytes(as_run_bytes)
    # The failing rows compiled a header whose bytes were the ones committed at the run's head; run 3
    # measured a repair that was later committed, so only the head's blob reproduces them. The
    # reproduction stages those exact bytes under the name the sources include.
    as_run_include = work / "as-run-include"
    as_run_include.mkdir(parents=True, exist_ok=True)
    staged = as_run_include / "pd_allocation_probe.hpp"
    staged.write_bytes(as_run_bytes)
    headers = {
        "as_run": dict(as_run_facts, staged_to=str(staged), copy=str(AS_RUN_COPY.relative_to(HERE))),
        "current": {
            "path": str(HEADER.relative_to(REPO)),
            "sha256": sha256_file(HEADER),
            "bytes": HEADER.stat().st_size,
        },
        "intermediate": intermediate_facts(),
    }

    version = text(run([str(CXX), "--version"], work).stdout).splitlines()
    head = text(run(["git", "-C", str(REPO), "rev-parse", "HEAD"], work).stdout).strip()
    records = []
    failures = 0
    for variant in VARIANTS:
        source = SOURCES / variant["source"]
        executable = work / "{}-{}.exe".format(variant["name"], variant["source"].replace(".cpp", ""))
        includes = (["-I" + str(as_run_include)] + INCLUDES[1:]
                    if variant["header"] == "as-run" else list(INCLUDES))
        command = ([str(CXX)] + BASE + includes + [str(source)] + variant["extra"] +
                   ["-o", str(executable)] + LIBRARIES)
        built = run(command, work)
        record = {
            "name": variant["name"],
            "source": str(source.relative_to(HERE)),
            "source_sha256": sha256_file(source),
            "header": variant["header"],
            "command": command,
            "expected_exit": variant["expect"],
            "rationale": variant["why"],
            "compile_exit": built.returncode,
            "compile_stderr": text(built.stderr)[-4000:],
        }
        if built.returncode != 0:
            record["exit"] = None
            record["matches_expectation"] = False
            failures += 1
            records.append(record)
            print("{}: compile failed".format(variant["name"]))
            continue
        record["executable"] = str(executable.relative_to(HERE))
        record["executable_bytes"] = executable.stat().st_size
        record["executable_sha256"] = sha256_file(executable)
        ran = run([str(executable)], work)
        code = ran.returncode & 0xFFFFFFFF
        record["exit"] = "0x{:08x}".format(code)
        record["exit_signed"] = ran.returncode
        record["stdout"] = text(ran.stdout)[-4000:]
        record["stderr"] = text(ran.stderr)[-4000:]
        # A variant with no recorded expectation is exploratory: it is measured, not judged.
        record["matches_expectation"] = (
            True if variant["expect"] is None else record["exit"] == variant["expect"])
        if not record["matches_expectation"]:
            failures += 1
        records.append(record)
        print("{}: exit={} expected={}{}".format(
            variant["name"], record["exit"], variant["expect"] or "(exploratory)",
            "" if record["matches_expectation"] else "  MISMATCH"))

    payload = {
        "scope": (
            "MinGW allocation-probe limit: control matrix over the as-run header bytes, plus one "
            "exploratory variant against today's header"),
        "head": head,
        "compiler": str(CXX),
        "compiler_version": version,
        "headers": headers,
        "flags": {"base": BASE, "includes": INCLUDES, "libraries": LIBRARIES},
        "probe_header": str(HEADER.relative_to(REPO)),
        "probe_header_sha256": sha256_file(HEADER),
        "probe_header_as_run_sha256": headers["as_run"]["sha256"],
        "catch2_libraries": {
            "libCatch2d.a": sha256_file(CATCH / "debug" / "lib" / "libCatch2d.a"),
            "manual-link/libCatch2Maind.a":
                sha256_file(CATCH / "debug" / "lib" / "manual-link" / "libCatch2Maind.a"),
        },
        "heap_corruption_status": "0x{:08x}".format(HEAP_CORRUPTION),
        "variants": records,
        "mismatches": failures,
    }
    with open(arguments.json, "w", encoding="utf-8", newline="\r\n") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    print("wrote {} ({} mismatches)".format(arguments.json, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
