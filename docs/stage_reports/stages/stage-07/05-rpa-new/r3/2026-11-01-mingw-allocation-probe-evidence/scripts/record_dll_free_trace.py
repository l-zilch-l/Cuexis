"""Capture the release path that corrupts the heap, as debugger transcripts rather than a description.

The claim is narrow and was established by measurement, not assumed:

  `tests/pd/pd_allocation_probe.hpp` (the bytes run 2 compiled) returns `base + 16` from its
  `operator new`, and stores `{base, size}` in the sixteen bytes before that payload.

  `Catch::Detail::unique_ptr<T>::~unique_ptr()` compiles to `call *%rdx` with
  `%rdx = vptr[1]`, i.e. a virtual call to `T`'s *deleting* destructor. When `T` is a class the C++
  runtime defines and the runtime is a DLL (`libstdc++-6.dll`), that deleting destructor lives in the
  DLL and releases the block with the DLL's own `operator delete`, which is a plain `free(payload)`.
  The CRT then reads its block header from the middle of a live block, and the heap is corrupted.

Two consequences make the transcripts worth capturing, because both are falsifiable:

  * the corrupting call is a `free` of a pointer whose sixteen preceding bytes are the as-run
    header's own `base` (a self-referential `{base, size}` pair), so the pointer is provably one of
    our payloads and not a block start;
  * the call site is in this executable (`~unique_ptr + 39`, the `call *%rdx`) while the callee that
    frees is in the DLL, which is why a plain `bt` shows the exe frame as `free`'s caller: the DLL's
    deleting destructor and its `operator delete` are tail calls.

The script builds `archive-src/tiny-repro.cpp` twice from source - dynamic and static C++ runtime,
same compiler, same Catch2 static library, same as-run header bytes - and runs both under gdb with
one breakpoint on the CRT export `free` (`_free_base` is not a symbol in this CRT, and a breakpoint
set before the shared libraries load resolves only to a dead thunk inside the executable, so the
breakpoint is set after `main`).

  gdb/dll-free-of-offset-payload.txt   the dynamic build: `free` receives an offset payload.
  gdb/static-runtime-clean-free.txt    the static build: no such call, because the deleting
                                       destructor is linked into the executable and therefore
                                       reaches the replacement `operator delete`, which frees `base`.

Each transcript also records the loaded modules, so the presence of `libstdc++-6.dll` in one and its
absence in the other is visible rather than asserted, and a one-shot unconditional `free` hit, so a
zero count cannot be confused with a breakpoint that never resolved.

The dynamic transcript ends at the corruption itself: after the payload is freed the CRT detects the
damaged heap and raises a debug break, which a debugger sees as `SIGTRAP` plus
`Critical error detected c0000374` rather than as an exit status. The process's own exit status is
recorded separately from an unattended run of the same binary, because a debugger that stops on the
break reports no exit code.

Usage:
    python -B record_dll_free_trace.py

It exits non-zero when the dynamic build produces no hit, when the static build produces one, when
either build fails to reach `free` at all, or when a recorded expectation no longer reproduces, so a
stale transcript cannot keep asserting an old result.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(r"D:\Cuexis-redesign")
CXX = Path(r"C:\msys64\ucrt64\bin\c++.exe")
GDB = Path(r"C:\msys64\ucrt64\bin\gdb.exe")
CATCH = REPO / "out" / "build" / "mingw-headless-debug" / "vcpkg_installed" / "x64-mingw-static"
HERE = Path(__file__).resolve().parent
SOURCE = HERE / "archive-src" / "tiny-repro.cpp"
TRACE = HERE / "trace"
GDB_DIR = HERE / "gdb"
AS_RUN_HEAD = "3fbc31f68430ce2d0ad576094ff617cc09772f10"
AS_RUN_PATH = "tests/pd/pd_allocation_probe.hpp"
AS_RUN_RUN = REPO / "out" / "rpa-acceptance" / "20261101-021741-4daf2bb2"
AS_RUN_COPY = HERE / "repro" / "pd_allocation_probe.as-run-2.hpp"
COMMAND_FILE = GDB_DIR / "payload-free.cmd"

INCLUDES = ["-I" + str(TRACE / "as-run-include"), "-I" + str(CATCH / "include"),
            "-I" + str(REPO / "engine" / "core" / "internal")]
LIBRARIES = ["-L" + str(CATCH / "debug" / "lib"),
             "-L" + str(CATCH / "debug" / "lib" / "manual-link"),
             "-lCatch2Maind", "-lCatch2d"]
BASE = ["-std=c++20", "-O0", "-g"]

BUILDS = [
    {
        "name": "dynamic-runtime",
        "extra": [],
        "transcript": "dll-free-of-offset-payload.txt",
        "expect_hits": 1,
        "expect_libstdcxx": True,
        "why": "the deleting destructor of a DLL-defined class runs the DLL's own operator delete",
    },
    {
        "name": "static-runtime",
        "extra": ["-static-libstdc++", "-static-libgcc"],
        "transcript": "static-runtime-clean-free.txt",
        "expect_hits": 0,
        "expect_libstdcxx": False,
        "why": "the same destructor is linked into the executable and reaches our own delete",
    },
]

PREDICATE = "$rcx > 0x10000 && *(unsigned long long*)($rcx - 16) == $rcx - 16"

# `x/5i` around the return address shows `call *%rdx`, the virtual deleting-destructor dispatch whose
# callee is in the DLL; the return address itself is in this executable, which is the whole point.
COMMANDS = """\
set pagination off
set confirm off
set width 0
set print elements 200
set breakpoint pending on
break main
run
info sharedlibrary
delete breakpoints
break free
commands
silent
printf "FREE_BREAKPOINT_COVERAGE 1\\n"
delete 2
continue
end
break free if {predicate}
set $hits = 0
commands
silent
set $hits = $hits + 1
set $ret = *(void**)$rsp
printf "PAYLOAD_FREE %d ptr=%p wm16=0x%016llx wm8=0x%016llx probe_hit=%d ret=%p\\n", $hits, $rcx, *(unsigned long long*)($rcx - 16), *(unsigned long long*)($rcx - 8), $rcx == (unsigned long long)cux_probe_buffer, $ret
printf "CALL_SITE %s\\n", "return address belongs to:"
info symbol $ret
printf "CALL_SITE_CODE\\n"
x/5i ((char*)$ret) - 12
printf "BACKTRACE\\n"
bt 4
if $hits >= 2
  printf "KILLING_AFTER_SECOND_HIT\\n"
  kill
end
continue
end
continue
printf "PAYLOAD_FREE_HITS %d\\n", $hits
""".format(predicate=PREDICATE)


def sha256_file(path):
    with open(path, "rb") as stream:
        return hashlib.sha256(stream.read()).hexdigest()


def text(value):
    return value.decode("utf-8", "replace")


def blob_at(commit, path):
    result = subprocess.run(["git", "-C", str(REPO), "show", "{}:{}".format(commit, path)],
                            capture_output=True)
    if result.returncode != 0:
        raise SystemExit("git show {}:{} failed: {}".format(commit, path, text(result.stderr)))
    return result.stdout


def stage_as_run_header():
    blob = blob_at(AS_RUN_HEAD, AS_RUN_PATH)
    digest = hashlib.sha256(blob).hexdigest()
    recorded = json.loads((AS_RUN_RUN / "manifest.json").read_text(encoding="utf-8"))["source"]["files"][AS_RUN_PATH]
    if recorded != digest:
        raise SystemExit("as-run header does not reproduce run 2's recorded hash: {} != {}".format(digest, recorded))
    include = TRACE / "as-run-include"
    include.mkdir(parents=True, exist_ok=True)
    staged = include / "pd_allocation_probe.hpp"
    staged.write_bytes(blob)
    if sha256_file(AS_RUN_COPY) != digest:
        raise SystemExit("staged reproduction copy diverged from the as-run header")
    return {
        "source": "git show {}:{}".format(AS_RUN_HEAD[:8], AS_RUN_PATH),
        "commit": AS_RUN_HEAD,
        "staged_to": str(staged.relative_to(HERE)),
        "sha256": digest,
        "bytes": len(blob),
        "manifest_run": str(AS_RUN_RUN.relative_to(REPO)),
        "manifest_sha256": recorded,
        "repro_copy": str(AS_RUN_COPY.relative_to(HERE)),
        "repro_copy_sha256": sha256_file(AS_RUN_COPY),
    }


def compile_build(build):
    TRACE.mkdir(parents=True, exist_ok=True)
    binary = TRACE / (build["name"] + ".exe")
    command = ([str(CXX)] + BASE + INCLUDES + build["extra"]
               + ["-o", str(binary), str(SOURCE)] + LIBRARIES)
    started = subprocess.run(command, cwd=str(HERE), capture_output=True)
    if started.returncode != 0:
        raise SystemExit("{} failed to compile:\n{}".format(build["name"], text(started.stderr)[-4000:]))
    return {
        "command": [str(part) for part in command],
        "compile_exit": started.returncode,
        "compile_stderr": text(started.stderr)[-2000:],
        "binary": str(binary.relative_to(HERE)),
        "binary_bytes": binary.stat().st_size,
        "binary_sha256": sha256_file(binary),
    }


def run_unattended(binary):
    """Run the binary outside the debugger so the exit status is the process's own."""
    started = subprocess.run([str(binary)], cwd=str(HERE), capture_output=True, timeout=900)
    code = started.returncode & 0xFFFFFFFF
    return "0x{:08x}".format(code)


def trace_build(build, binary):
    command = [str(GDB), "-batch", "-x", str(COMMAND_FILE), str(binary)]
    started = subprocess.run(command, cwd=str(HERE), capture_output=True, timeout=3600)
    transcript = text(started.stdout) + text(started.stderr)
    target = GDB_DIR / build["transcript"]
    with open(target, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(transcript)
    return target, transcript, started.returncode


def parse(transcript):
    facts = {
        "payload_free_hits": len(re.findall(r"^PAYLOAD_FREE ", transcript, re.MULTILINE)),
        "free_breakpoint_coverage": 1 if "FREE_BREAKPOINT_COVERAGE 1" in transcript else 0,
        "libstdcxx_loaded": "libstdc++-6.dll" in transcript,
        "killed_after_second_hit": "KILLING_AFTER_SECOND_HIT" in transcript,
        "crash_in_transcript": "c0000374" in transcript,
        "payload_free_lines": re.findall(r"^PAYLOAD_FREE.*$", transcript, re.MULTILINE),
        "call_site_symbols": [],
        "call_site_code": [],
        "counted_hits": None,
    }
    blocks = re.split(r"^PAYLOAD_FREE \d+ .*$", transcript, flags=re.MULTILINE)[1:]
    for block in blocks:
        symbol = re.search(r"^CALL_SITE .*$\n(.*)$", block, re.MULTILINE)
        if symbol:
            facts["call_site_symbols"].append(symbol.group(1).strip())
        code = re.search(r"^CALL_SITE_CODE$\n(.*?)(?:^BACKTRACE$)", block, re.MULTILINE | re.DOTALL)
        if code:
            facts["call_site_code"].append([line.strip() for line in code.group(1).strip().splitlines()])
    counted = re.search(r"^PAYLOAD_FREE_HITS (\d+)$", transcript, re.MULTILINE)
    if counted:
        facts["counted_hits"] = int(counted.group(1))
    return facts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    if not CXX.is_file():
        raise SystemExit("missing compiler: {}".format(CXX))
    if not GDB.is_file():
        raise SystemExit("missing debugger: {}".format(GDB))
    if not SOURCE.is_file():
        raise SystemExit("missing reproduction source: {}".format(SOURCE))
    GDB_DIR.mkdir(parents=True, exist_ok=True)
    header = stage_as_run_header()
    COMMAND_FILE.write_text(COMMANDS, encoding="utf-8", newline="\n")
    gdb_version = subprocess.run([str(GDB), "--version"], capture_output=True, text=True).stdout.splitlines()[0]

    results = []
    failures = []
    for build in BUILDS:
        compiled = compile_build(build)
        binary = TRACE / (build["name"] + ".exe")
        exit_code = run_unattended(binary)
        target, transcript, gdb_exit = trace_build(build, binary)
        facts = parse(transcript)
        checks = {
            "payload_free_hits": facts["payload_free_hits"] == build["expect_hits"],
            "free_breakpoint_resolved": facts["free_breakpoint_coverage"] == 1,
            "libstdcxx_presence": facts["libstdcxx_loaded"] == build["expect_libstdcxx"],
            "transcript_self_consistent": facts["counted_hits"] in (None, facts["payload_free_hits"]),
        }
        if build["expect_hits"]:
            checks["call_site_named"] = bool(facts["call_site_symbols"]) and all(facts["call_site_symbols"])
            checks["virtual_dispatch_visible"] = any(
                "call" in " ".join(code) and "*" in " ".join(code) for code in facts["call_site_code"])
        record = {
            "build": build["name"],
            "why": build["why"],
            "extra_flags": build["extra"],
            "expected_payload_free_hits": build["expect_hits"],
            "expected_libstdcxx": build["expect_libstdcxx"],
            "transcript": str(target.relative_to(HERE)),
            "transcript_sha256": sha256_file(target),
            "transcript_bytes": target.stat().st_size,
            "gdb_exit": gdb_exit,
            "exit_code_unattended": exit_code,
        }
        record.update(compiled)
        record.update(facts)
        record["checks"] = checks
        record["matches_expectation"] = all(checks.values())
        if not record["matches_expectation"]:
            failures.append({"build": build["name"], "failed": sorted(k for k, v in checks.items() if not v)})
        results.append(record)
        print("{}: hits={} exit={} coverage={} libstdcxx={} ok={}".format(
            build["name"], facts["payload_free_hits"], exit_code, facts["free_breakpoint_coverage"],
            facts["libstdcxx_loaded"], record["matches_expectation"]))

    payload = {
        "gdb": gdb_version,
        "breakpoint": "free (the CRT export both modules reach); set after main, not before the DLLs load",
        "predicate": PREDICATE,
        "predicate_meaning": "the pointer being freed carries the as-run header's own {base, size} pair "
                             "in front of it, so it is a payload of the replacement and not a block start",
        "command_file": str(COMMAND_FILE.relative_to(HERE)),
        "command_file_sha256": sha256_file(COMMAND_FILE),
        "header": header,
        "source": str(SOURCE.relative_to(HERE)),
        "source_sha256": sha256_file(SOURCE),
        "runs": results,
        "mismatches": failures,
    }
    record_path = GDB_DIR / "trace-sessions.json"
    with open(record_path, "w", encoding="utf-8", newline="\r\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print("wrote {} and {} transcripts ({} mismatches)".format(record_path, len(results), len(failures)))
    for failure in failures:
        print("  MISMATCH {}: {}".format(failure["build"], ", ".join(failure["failed"])))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
