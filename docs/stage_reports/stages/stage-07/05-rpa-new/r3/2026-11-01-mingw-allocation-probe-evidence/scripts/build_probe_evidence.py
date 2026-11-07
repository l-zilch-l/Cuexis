"""Assemble the MinGW allocation-probe evidence archive from the staged diagnosis and the run-2 logs.

Usage:
    python -B build_probe_evidence.py [--run <run-2 directory>]

The archive has six groups that answer different questions:

  run2/       the second acceptance-matrix run, content unchanged: its per-row phase logs, the CTest
              failure lists and the run manifest. This is the run that exposed the defect, and its own
              manifest binds every row's command, exit code and source state.
  repro/      the three-line reproduction, its pass-through control, the framework-free isolation
              program, the record of the five compiled variants, and the probe header exactly as the
              failing rows compiled it.
  gdb/        the two captured debugger sessions (dynamic build: the DLL's own free receives an
              offset payload; static build: no such call), the predicated breakpoint driver, and the
              record that binds them.
  isolation/  four framework-free runs: two link modes times two release patterns, which separate
              "the payload is offset" from "the C++ runtime lives in a DLL".
  post-fix/   fresh-process runs of every suite the defect broke, with their Catch2 summary lines.
  scripts/    the programs that rebuild and re-run every record above, and the source table that
              binds this diagnosis to the two matrix runs it compares.

Every file is either copied (text normalised to CRLF, source and archived hashes both recorded) or
generated; nothing is summarised. Executables and objects are excluded with their hashes and a
reason, because each one is 20-86 MB of statically linked Catch2 runtime and every one of them can be
rebuilt from `scripts/record_variants.py` or `scripts/record_isolation_matrix.py`.

The build refuses to run unless every record reports zero mismatches and holds the expected number of
rows, so an archive cannot be assembled around a stale or failed reproduction. It also re-derives the
relation between the two matrix runs and the committing worktree, so a source claim that the record
contradicts stops the build instead of being published.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

WORKTREE = r"D:\Cuexis-redesign"
STAGING = os.path.join(WORKTREE, "out", "verification-20261031", "evidence-staging", "r3-mingw-probe")
DEFAULT_RUN = os.path.join(WORKTREE, "out", "rpa-acceptance", "20261101-021741-4daf2bb2")
ARCHIVE_NAME = "2026-11-01-mingw-allocation-probe-evidence"

TEXT_SUFFIXES = (".cpp", ".hpp", ".py", ".json", ".md", ".txt", ".log", ".cmd", ".ps1", ".bak")

RUN_EXCLUDED = [
    (r"LastTest\.log$",
     "CTest scratch copy of the ctest output; the archived -ctest.log holds the same text and is the "
     "copy bound by the run manifest"),
    (r"-CMakeCache\.txt$",
     "configure cache; the run manifest records its sha256 per row as cache_sha256"),
]

EXCLUDED_REASON = [
    (r"\.exe$", "statically linked Catch2 debug executable (0.4-86 MB); rebuilt by "
                "scripts/record_variants.py or scripts/record_isolation_matrix.py"),
    (r"\.obj$", "intermediate object file; rebuilt by the recorded compile command"),
    (r"\.pyc$", "bytecode cache of a recorder; the recorder source is archived"),
    (r"^gdb-", "debugger command file from the first diagnosis; its session output was never captured, "
               "so the archive carries the re-captured predicated sessions in gdb/ instead"),
    (r"^probe_catch\.cpp$|^probe_dll\.cpp$|^probe\.cpp$|^probe-(compile|link)",
     "ad-hoc probe translation unit and the compile/link lines recovered for the first diagnosis; "
     "superseded by repro/ and by the command arrays recorded in gdb/trace-sessions.json and "
     "isolation/isolation-matrix.json"),
    (r"^tiny\.cpp$|^tiny[-.]", "ad-hoc reproduction binary from the first diagnosis; superseded by "
                               "repro/tiny-repro.cpp"),
    (r"^plain\.exe$|^freehook|^guard\.exe$|^sha-noreplace|^probe-plain\.exe$|^probe-replace\.exe$",
     "ad-hoc control binary from the first diagnosis"),
    (r"^variant_link\.ps1$|^build_probe\.ps1$",
     "PowerShell driver from the first diagnosis: it recovered one build line, rewrote it for a "
     "single substitution and rebuilt an ad-hoc probe; superseded by the recorders in scripts/, "
     "which store their full command arrays"),
    (r"^hash_core_tests\.cpp\.nodep\.obj$", "object from an unrelated dependency check"),
    (r"pd_allocation_probe\.hpp$",
     "staging copy of the as-run probe header; archived as repro/pd_allocation_probe.as-run-2.hpp "
     "and bound by sha256 in gdb/trace-sessions.json and isolation/isolation-matrix.json"),
    (r"^pd_allocation_probe\.magic-check-intermediate\.hpp$",
     "intermediate header the diagnosis passed through (it leaks instead of freeing and was never "
     "measured); superseded by the committed header"),
    (r"^2026-11-01-r3-run2-and-mingw-allocation-probe-limit\.md$",
     "the report itself; published under docs/stage_reports/stages/stage-07/05-rpa-new/r3/ and "
     "linking to this archive, which does not duplicate it"),
]


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def sha256_file(path):
    with open(path, "rb") as stream:
        return sha256_bytes(stream.read())


def crlf(value):
    return value.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")


def disk_form(disk_bytes, blob_bytes):
    """Classify on-disk bytes against a commit blob: identical, CRLF-expanded, or different."""
    if disk_bytes == blob_bytes:
        return "blob"
    if disk_bytes == crlf(blob_bytes):
        return "blob-crlf"
    return "differ"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", default=DEFAULT_RUN)
    arguments = parser.parse_args()
    run = os.path.abspath(arguments.run)
    final_archive = os.path.join(STAGING, ARCHIVE_NAME)
    # Everything is written into a partial directory and renamed at the very end, so a build that
    # stops at a gate leaves no half-archive behind under the published name.
    archive = final_archive + ".partial"
    if os.path.isdir(final_archive):
        raise SystemExit("archive already exists: " + final_archive)
    if os.path.isdir(archive):
        raise SystemExit("partial archive already exists: " + archive)

    def load(*parts):
        path = os.path.join(STAGING, *parts)
        if not os.path.isfile(path):
            raise SystemExit("missing {}: run the recorder first".format(path))
        with open(path, encoding="utf-8") as stream:
            value = json.load(stream)
        if value.get("mismatches"):
            raise SystemExit("{} reports {} mismatches".format(path, value["mismatches"]))
        return value

    variants = load("variants.json")
    trace = load("gdb", "trace-sessions.json")
    isolation = load("isolation", "isolation-matrix.json")
    post_fix = load("post-fix", "summary.json")
    source_table = load("source-table.json")

    # Gates. Counting predictions rather than rows keeps the variant gate from being satisfied by
    # rows that assert nothing: four variants carry a prediction, the fifth is exploratory.
    predicted = [record for record in variants.get("variants", [])
                 if record.get("expected_exit") is not None]
    if len(variants.get("variants", [])) != 5 or len(predicted) != 4:
        raise SystemExit("variants.json does not hold five variants with four predictions")
    if len(trace.get("runs", [])) != 2 or not all(r["matches_expectation"] for r in trace["runs"]):
        raise SystemExit("trace record is incomplete or contradicts its prediction")
    if len(isolation.get("runs", [])) != 4 or not all(
            r["matches_expectation"] for r in isolation["runs"]):
        raise SystemExit("isolation matrix is incomplete or contradicts its prediction")
    if len({(r["build"], r["mode"]) for r in isolation["runs"]}) != 4:
        raise SystemExit("isolation matrix does not hold four distinct build/mode pairs")
    if len(post_fix.get("runs", [])) != 6 or not all(
            r["matches_expectation"] for r in post_fix["runs"]):
        raise SystemExit("post-fix record is incomplete or contradicts its prediction")
    run2_table = source_table["run2"]
    run3_table = source_table["run3"]
    if run2_table["states"]["modified"] or run2_table["states"]["added"] or run2_table["deltas"]:
        raise SystemExit("source-table.json no longer shows run 2 as a pristine checkout")
    if len(run2_table["repaired"]) != 6:
        raise SystemExit("source-table.json does not record six repaired paths")
    if not all(delta["run_equals_later"] for delta in run3_table["deltas"]):
        raise SystemExit("source-table.json does not bind every run-3 repaired path to the commit")

    copied = []
    excluded = []
    seen = set()

    def copy_one(source, target_rel):
        with open(source, "rb") as stream:
            payload = stream.read()
        destination = os.path.join(archive, target_rel)
        os.makedirs(os.path.dirname(destination) or archive, exist_ok=True)
        note = None
        if os.path.basename(source).lower().endswith(TEXT_SUFFIXES):
            # Line endings are normalised at the byte level: 0x0d and 0x0a cannot appear inside a
            # multi-byte UTF-8 sequence or a double-byte code-page sequence, so this never rewrites a
            # character. One run-2 log carries compiler output in the local code page, which is why
            # the copy is not decoded first.
            body = payload[3:] if payload.startswith(b"\xef\xbb\xbf") else payload
            text = body.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
            with open(destination, "wb") as stream:
                stream.write(text)
            eol = "crlf"
            try:
                payload.decode("utf-8")
            except UnicodeDecodeError:
                note = "not valid UTF-8 (local code page); line endings normalised at byte level"
        else:
            with open(destination, "wb") as stream:
                stream.write(payload)
            eol = "binary"
        with open(destination, "rb") as stream:
            archived = stream.read()
        entry = {"source": source, "archive": target_rel, "bytes": len(archived),
                 "sha256": sha256_bytes(archived), "source_sha256": sha256_bytes(payload),
                 "eol": eol}
        if note:
            entry["note"] = note
        copied.append(entry)
        seen.add(os.path.normcase(os.path.abspath(source)))

    include = [
        # repro/
        (os.path.join(STAGING, "archive-src", "tiny-repro.cpp"), "repro/tiny-repro.cpp"),
        (os.path.join(STAGING, "archive-src", "control-passthrough-new.cpp"),
         "repro/control-passthrough-new.cpp"),
        (os.path.join(STAGING, "archive-src", "isolation-no-framework.cpp"),
         "repro/isolation-no-framework.cpp"),
        (os.path.join(STAGING, "repro", "pd_allocation_probe.as-run-2.hpp"),
         "repro/pd_allocation_probe.as-run-2.hpp"),
        (os.path.join(STAGING, "variants.json"), "repro/variants.json"),
        # gdb/
        (os.path.join(STAGING, "gdb", "dll-free-of-offset-payload.txt"),
         "gdb/dll-free-of-offset-payload.txt"),
        (os.path.join(STAGING, "gdb", "static-runtime-clean-free.txt"),
         "gdb/static-runtime-clean-free.txt"),
        (os.path.join(STAGING, "gdb", "payload-free.cmd"), "gdb/payload-free.cmd"),
        (os.path.join(STAGING, "gdb", "trace-sessions.json"), "gdb/trace-sessions.json"),
        # isolation/
        (os.path.join(STAGING, "isolation", "isolation-matrix.json"),
         "isolation/isolation-matrix.json"),
        # post-fix/
        (os.path.join(STAGING, "post-fix", "summary.json"), "post-fix/summary.json"),
        # scripts/
        (os.path.join(STAGING, "record_variants.py"), "scripts/record_variants.py"),
        (os.path.join(STAGING, "record_dll_free_trace.py"), "scripts/record_dll_free_trace.py"),
        (os.path.join(STAGING, "record_isolation_matrix.py"), "scripts/record_isolation_matrix.py"),
        (os.path.join(STAGING, "record_postfix_verification.py"),
         "scripts/record_postfix_verification.py"),
        (os.path.join(STAGING, "record_source_table.py"), "scripts/record_source_table.py"),
        (os.path.join(STAGING, "fill_probe_report.py"), "scripts/fill_probe_report.py"),
        (os.path.join(STAGING, "build_probe_evidence.py"), "scripts/build_probe_evidence.py"),
        (os.path.join(STAGING, "source-table.json"), "scripts/source-table.json"),
    ]
    for entry in sorted(os.listdir(os.path.join(STAGING, "isolation"))):
        if entry.endswith(".txt"):
            include.append((os.path.join(STAGING, "isolation", entry), "isolation/" + entry))
    for entry in sorted(os.listdir(os.path.join(STAGING, "post-fix"))):
        if entry.endswith(".log"):
            include.append((os.path.join(STAGING, "post-fix", entry), "post-fix/" + entry))
    for source, target in include:
        if not os.path.isfile(source):
            raise SystemExit("missing staged input: " + source)
        copy_one(source, target)

    run_manifest_path = os.path.join(run, "manifest.json")
    copy_one(run_manifest_path, "run2/source-manifest.json")
    for entry in sorted(os.listdir(run)):
        path = os.path.join(run, entry)
        if not os.path.isfile(path) or os.path.normcase(path) in seen:
            continue
        reason = None
        for pattern, text in RUN_EXCLUDED:
            if re.search(pattern, entry):
                reason = text
                break
        if reason is None and not entry.endswith((".log", ".cmd")):
            reason = "not part of the archived evidence set"
        if reason is not None:
            with open(path, "rb") as stream:
                payload = stream.read()
            excluded.append({"source": path, "bytes": len(payload),
                             "sha256": sha256_bytes(payload), "reason": reason})
            continue
        target = "run2/" + entry
        if entry.endswith((".log", ".cmd")):
            target += ".txt"
        copy_one(path, target)

    # Everything else this diagnosis produced is excluded with a hash, so a reader can see that the
    # archive did not quietly drop a contradictory artifact.
    skip_dirs = {"archive-src", "authored", "__pycache__", ARCHIVE_NAME, ARCHIVE_NAME + ".partial"}
    for root, directories, files in os.walk(STAGING):
        directories[:] = [name for name in directories if name not in skip_dirs]
        for entry in files:
            path = os.path.join(root, entry)
            if os.path.normcase(path) in seen:
                continue
            reason = "not part of the archived evidence set"
            for pattern, text in EXCLUDED_REASON:
                if re.search(pattern, os.path.basename(path)):
                    reason = text
                    break
            with open(path, "rb") as stream:
                payload = stream.read()
            excluded.append({"source": path, "bytes": len(payload),
                             "sha256": sha256_bytes(payload), "reason": reason})

    generated = {}
    authored = os.path.join(STAGING, "authored", "index.md")
    if not os.path.isfile(authored):
        raise SystemExit("authored index missing: " + authored)
    copy_one(authored, "index.md")
    generated["index.md"] = copied[-1]["sha256"]

    with open(run_manifest_path, encoding="utf-8") as stream:
        run_manifest = json.load(stream)

    # The six repaired paths are bound three ways: to the bytes run 3 actually executed, to the blob
    # the committing checkout carries, and to the blob this worktree holds today. One of the six (the
    # runner) was changed again after the run by a recorded hygiene commit, which is why the third
    # anchor exists; a path that matches none of the three stops the build instead of being published.
    head_commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=WORKTREE, capture_output=True,
                                 check=False)
    if head_commit.returncode != 0:
        raise SystemExit("cannot read HEAD: " + head_commit.stderr.decode("utf-8", "replace"))
    head = head_commit.stdout.decode("ascii").strip()

    def blob_at(commit, relative):
        result = subprocess.run(["git", "show", "{}:{}".format(commit, relative)], cwd=WORKTREE,
                                capture_output=True, check=False)
        if result.returncode != 0:
            raise SystemExit("cannot read {} at {}: {}".format(
                relative, commit, result.stderr.decode("utf-8", "replace")))
        return result.stdout

    run3_bytes = {delta["path"]: delta["run_sha256"] for delta in run3_table["deltas"]}
    gate_files = {}
    for record in run2_table["repaired"]:
        relative = record["path"]
        later_blob = blob_at(source_table["later_commit"], relative)
        head_blob = blob_at(head, relative)
        with open(os.path.join(WORKTREE, *relative.split("/")), "rb") as stream:
            disk = stream.read()
        disk_sha256 = sha256_bytes(disk)
        later_form = disk_form(disk, later_blob)
        head_form = disk_form(disk, head_blob)
        accepted = (disk_sha256 == run3_bytes.get(relative)
                    or later_form in ("blob", "blob-crlf")
                    or head_form in ("blob", "blob-crlf"))
        if not accepted:
            raise SystemExit("{} matches neither the run-3 bytes, nor the {} blob, nor HEAD"
                             .format(relative, source_table["later_commit"]))
        entry = {
            "later_commit_blob_sha256": sha256_bytes(later_blob),
            "head_blob_sha256": sha256_bytes(head_blob),
            "run2_sha256": record["run_sha256"],
            "run3_sha256": run3_bytes.get(relative),
            "worktree_sha256": disk_sha256,
            "worktree_form_vs_later_commit": later_form,
            "worktree_form_vs_head": head_form,
        }
        if head_blob != later_blob:
            entry["changed_after_later_commit"] = (
                "changed again by {}; the worktree bytes are a line-ending variant of that commit, "
                "not of {} which run 3 executed".format(head[:7], source_table["later_commit"]))
        gate_files[relative] = entry

    payload = {
        "scope": (
            "MinGW allocation-probe limit and the second local RPA acceptance-matrix run: the run that "
            "exposed the defect, the three-line reproduction that isolates it, the five-variant control "
            "matrix that separates the offset payload layout from the replaced operators, the debugger "
            "sessions that show the DLL's own free receiving an offset payload, the framework-free "
            "isolation matrix that removes the framework from that claim, and fresh-process re-runs of "
            "every suite the defect broke. Test-only diagnostic evidence for the Cuexis test "
            "instrument; it is neither hosted validation nor owner acceptance, it certifies no product "
            "behaviour, and it promotes no production threshold. Generated by "
            "out/verification-20261031/evidence-staging/r3-mingw-probe/build_probe_evidence.py."
        ),
        "source_run": run,
        "copied_files": copied,
        "attached_files": [],
        "derived_files": [],
        "superseded_attempts": [],
        "excluded": excluded,
        "generated_files": generated,
        "binding": {
            "head": variants["head"],
            "probe_header": variants["probe_header"],
            "probe_header_sha256": variants["probe_header_sha256"],
            "probe_header_as_run_sha256": variants["probe_header_as_run_sha256"],
            "compiler": variants["compiler"],
            "compiler_version": variants["compiler_version"],
            "debugger": trace["gdb"],
            "breakpoint_predicate": trace["predicate"],
            "variants_sha256": sha256_file(os.path.join(STAGING, "variants.json")),
            "variant_headers": {record["name"]: record.get("header")
                                for record in variants["variants"]},
            "variant_predictions": {record["name"]: record.get("expected_exit")
                                    for record in variants["variants"]},
            "variant_exits": {record["name"]: record.get("exit")
                              for record in variants["variants"]},
            "trace_hits": {record["build"]: record["payload_free_hits"]
                           for record in trace["runs"]},
            "trace_coverage_hits": {record["build"]: record["free_breakpoint_coverage"]
                                    for record in trace["runs"]},
            "trace_exits": {record["build"]: record["exit_code_unattended"]
                            for record in trace["runs"]},
            "isolation_expected": {"{}/{}".format(record["build"], record["mode"]):
                                   record["expected_exit"] for record in isolation["runs"]},
            "isolation_exits": {"{}/{}".format(record["build"], record["mode"]):
                                record["exit_status"] for record in isolation["runs"]},
            "post_fix_exits": {record["name"]: record["exit"] for record in post_fix["runs"]},
            "post_fix_counts": {record["name"]: [record["assertions"], record["cases"]]
                                for record in post_fix["runs"]},
            "run_manifest_source_worktree_sha256": run_manifest["source"]["worktree_sha256"],
            "run_manifest_files_hashed": len(run_manifest["source"]["files"]),
            "run_manifest_source_unchanged": run_manifest.get("source_unchanged"),
            "run2_head": run2_table["head"],
            "run2_worktree_sha256": run2_table["worktree_sha256"],
            "run2_implementation_sha256": run2_table["implementation_sha256"],
            "run2_states": run2_table["states"],
            "run3_head": run3_table["head"],
            "run3_worktree_sha256": run3_table["worktree_sha256"],
            "run3_implementation_sha256": run3_table["implementation_sha256"],
            "run3_states": run3_table["states"],
            "later_commit": source_table["later_commit"],
            "gate_files": gate_files,
        },
    }
    target = os.path.join(archive, "manifest.json")
    with open(target, "w", encoding="utf-8", newline="\r\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    if os.path.isdir(final_archive):
        raise SystemExit("archive appeared while building: " + final_archive)
    os.replace(archive, final_archive)
    print("copied {} files, excluded {}, generated {}".format(
        len(copied), len(excluded), len(generated)))
    print("manifest:", os.path.join(final_archive, "manifest.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
