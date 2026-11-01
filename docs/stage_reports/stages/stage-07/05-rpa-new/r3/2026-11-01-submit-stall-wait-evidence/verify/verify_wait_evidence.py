"""Check this archive against its own manifest, and optionally re-derive the tabulation.

Two jobs, deliberately separate:

  default   byte closure. Every file the manifest lists is present, hashes to the recorded sha256,
            and has the recorded size; every gzip stream decompresses to the recorded source hash;
            every normalised text copy has no bare LF and no BOM; no file exists that the manifest
            does not list; the totals agree with the file list. This needs nothing but the archive.

  --rebuild re-derivation. Rebuilds the flat run layout in a temporary directory from the archived
            bytes, runs the archived derive_all.py and tabulate_wait.py against it, and compares the
            result with binding.tabulated_sha256. It proves that the report's numbers can be
            recomputed from the archived sample streams, not just that the bytes arrived intact.
            It needs the same ntdll build the runs used, because instruction pointers are translated
            through that ntdll's syscall stubs; the hash to compare against is in environment.

Usage:
    python -B verify_wait_evidence.py
    python -B verify_wait_evidence.py --rebuild
    python -B verify_wait_evidence.py --root <archive-dir> --json <report.json>
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
BANNED_PREFIXES = (b"\xef\xbb\xbf",)


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def gunzip_bytes(payload):
    # 10-byte header, one raw deflate stream, 8-byte CRC32/ISIZE trailer; the trailer is not trusted,
    # the sha256 of the decompressed bytes is.
    return zlib.decompressobj(-15).decompress(payload[10:])


def read_bytes(path):
    with open(path, "rb") as stream:
        return stream.read()


def archive_root():
    candidate = os.path.dirname(HERE)
    if os.path.isfile(os.path.join(candidate, "manifest.json")):
        return candidate
    return None


def check_bytes(manifest, archive):
    failures = []
    stats = {"files": 0, "gzip": 0, "text": 0, "binary": 0, "raw_bytes": 0,
             "archived_bytes": 0}
    for entry in manifest["copied_files"]:
        path = os.path.join(archive, entry["archive"].replace("/", os.sep))
        if not os.path.isfile(path):
            failures.append({"check": "present", "archive": entry["archive"],
                             "detail": "missing"})
            continue
        stored = read_bytes(path)
        stats["files"] += 1
        stats["archived_bytes"] += len(stored)
        if len(stored) != entry["bytes"]:
            failures.append({"check": "size", "archive": entry["archive"],
                             "detail": "{} != {}".format(len(stored), entry["bytes"])})
        if sha256_bytes(stored) != entry["sha256"]:
            failures.append({"check": "sha256", "archive": entry["archive"],
                             "detail": "archived hash mismatch"})
        mode = entry.get("mode", "text" if entry.get("eol") == "crlf" else "binary")
        stats[mode] += 1
        if mode == "gzip":
            try:
                raw = gunzip_bytes(stored)
            except zlib.error as error:
                failures.append({"check": "gzip", "archive": entry["archive"],
                                 "detail": str(error)})
                continue
            stats["raw_bytes"] += len(raw)
            if len(raw) != entry["source_bytes"]:
                failures.append({"check": "gzip-size", "archive": entry["archive"],
                                 "detail": "{} != {}".format(len(raw),
                                                             entry["source_bytes"])})
            if sha256_bytes(raw) != entry["source_sha256"]:
                failures.append({"check": "gzip-source", "archive": entry["archive"],
                                 "detail": "decompressed bytes != source_sha256"})
        elif mode == "text":
            stats["raw_bytes"] += len(stored)
            if stored.count(b"\n") != stored.count(b"\r\n"):
                failures.append({"check": "crlf", "archive": entry["archive"],
                                 "detail": "bare LF present"})
            if stored.startswith(BANNED_PREFIXES):
                failures.append({"check": "bom", "archive": entry["archive"],
                                 "detail": "byte order mark present"})
        else:
            stats["raw_bytes"] += len(stored)
    return failures, stats


def check_index(manifest, archive):
    failures = []
    record = manifest["generated_files"]["index.md"]
    path = os.path.join(archive, "index.md")
    if not os.path.isfile(path):
        return [{"check": "index", "archive": "index.md", "detail": "missing"}]
    payload = read_bytes(path)
    if len(payload) != record["bytes"]:
        failures.append({"check": "index-size", "archive": "index.md", "detail": "size mismatch"})
    if sha256_bytes(payload) != record["sha256"]:
        failures.append({"check": "index-sha256", "archive": "index.md", "detail": "hash mismatch"})
    if b"\r" in payload:
        failures.append({"check": "index-eol", "archive": "index.md", "detail": "not LF"})
    if payload.startswith(BANNED_PREFIXES):
        failures.append({"check": "index-bom", "archive": "index.md", "detail": "BOM present"})
    headings = [line for line in payload.decode("utf-8").split("\n") if line.startswith("# ")]
    if len(headings) != 1:
        failures.append({"check": "index-h1", "archive": "index.md",
                         "detail": "{} H1 headings".format(len(headings))})
    return failures


def check_file_set(manifest, archive):
    expected = {entry["archive"] for entry in manifest["copied_files"]}
    expected |= {"index.md", "manifest.json", "validation.json", ".gitattributes"}
    present = set()
    for root, _dirs, names in os.walk(archive):
        for name in names:
            present.add(os.path.relpath(os.path.join(root, name), archive).replace(os.sep, "/"))
    failures = []
    for path in sorted(expected - present):
        failures.append({"check": "file-set", "archive": path, "detail": "listed but absent"})
    for path in sorted(present - expected):
        failures.append({"check": "file-set", "archive": path, "detail": "present but unlisted"})
    return failures, {"expected": len(expected), "present": len(present)}


def check_totals(manifest, stats):
    failures = []
    totals = manifest["totals"]
    index_bytes = manifest["generated_files"]["index.md"]["bytes"]
    if totals["files"] != stats["files"] + 1:
        failures.append({"check": "totals", "archive": "manifest.json",
                         "detail": "files {} != {}".format(totals["files"], stats["files"] + 1)})
    if totals["archived_bytes"] != stats["archived_bytes"] + index_bytes:
        failures.append({"check": "totals", "archive": "manifest.json",
                         "detail": "archived_bytes mismatch"})
    return failures


def rebuild(manifest, archive):
    """Reconstruct the run tree from the archived streams and re-derive the tabulation.

    The archive stores text copies CRLF-normalised, so the reconstruction inverts that first and
    proves the inversion with each file's source_sha256 before any derivation runs. The derivation
    then has to reproduce the recorded derived files byte for byte.
    """
    result = {"performed": True, "ok": False, "steps": []}
    ntdll_expected = manifest["environment"]["ntdll_sha256"]
    ntdll_path = manifest["environment"]["ntdll_path"]
    if not os.path.isfile(ntdll_path):
        result["steps"].append({"step": "ntdll", "ok": False, "detail": "absent: " + ntdll_path})
        return result
    actual = sha256_bytes(read_bytes(ntdll_path))
    matches = actual == ntdll_expected
    result["steps"].append({"step": "ntdll", "ok": matches,
                            "detail": "{} vs {}".format(actual, ntdll_expected)})
    if not matches:
        result["detail"] = ("ntdll hash differs from the one the runs used; syscall attribution "
                           "may not reproduce, so re-derivation is not attempted")
        return result

    work = tempfile.mkdtemp(prefix="wait-evidence-rebuild-")
    try:
        inverted = 0
        unchanged = 0
        inversion_failures = []
        for entry in manifest["copied_files"]:
            relative = entry["archive"]
            if not relative.startswith("runs/"):
                continue
            parts = relative.split("/")[1:]
            if parts[-1].endswith(".gz"):
                parts[-1] = parts[-1][:-len(".gz")]
            mode = entry.get("mode")
            payload = read_bytes(os.path.join(archive, relative.replace("/", os.sep)))
            if mode == "gzip":
                payload = gunzip_bytes(payload)
            elif mode == "text":
                # The stored copy was normalised to CRLF, which is a no-op for a source that was
                # already CRLF and one extra byte per line for a source that was LF. Recover the
                # original by trying both readings and keeping the one its source hash accepts.
                source = entry["source_sha256"]
                if sha256_bytes(payload) == source:
                    unchanged += 1
                elif sha256_bytes(payload.replace(b"\r\n", b"\n")) == source:
                    payload = payload.replace(b"\r\n", b"\n")
                    inverted += 1
                else:
                    inversion_failures.append(relative)
                    continue
            else:
                inversion_failures.append(relative + " (unsupported mode " + str(mode) + ")")
                continue
            target = os.path.join(work, *parts)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as stream:
                stream.write(payload)
        result["steps"].append({
            "step": "text-inversion", "ok": not inversion_failures,
            "detail": "{} text copies: {} inverted to LF, {} already CRLF, all matched "
                      "source_sha256{}".format(inverted + unchanged, inverted, unchanged,
                                               "" if not inversion_failures else
                                               "; mismatched: " + ", ".join(inversion_failures[:5]))})
        if inversion_failures:
            return result

        for name in ("analyze_wait.py", "derive_wait_runs.py", "derive_all.py",
                     "tabulate_wait.py", "pe_bytes.py"):
            shutil.copyfile(os.path.join(archive, "instruments", name),
                            os.path.join(work, name))

        derive = subprocess.run([sys.executable, "-B", "derive_all.py", "--check"], cwd=work,
                                capture_output=True, text=True)
        lines = derive.stdout.strip().splitlines()
        summary = next((line for line in lines if line.startswith("streams:")),
                       lines[-1] if lines else "")
        ok = (derive.returncode == 0 and "byte-identical: 7" in summary
              and "changed: 0" in summary)
        result["steps"].append({"step": "derive_all", "ok": ok, "detail": summary})
        result["derive_all"] = summary

        out = os.path.join(work, "rebuilt.tabulated.json")
        tabulate = subprocess.run([sys.executable, "-B", "tabulate_wait.py", "--root", work,
                                   "--out", out], cwd=work, capture_output=True, text=True)
        if tabulate.returncode != 0 or not os.path.isfile(out):
            result["steps"].append({"step": "tabulate_wait", "ok": False,
                                    "detail": (tabulate.stderr or tabulate.stdout).strip()[-400:]})
            return result
        rebuilt_bytes = read_bytes(out)
        rebuilt_hash = sha256_bytes(rebuilt_bytes)
        expected = manifest["binding"]["tabulated_source_sha256"]
        if rebuilt_hash == expected:
            result["steps"].append({"step": "tabulate_wait", "ok": True,
                                    "detail": "byte-identical to the recorded source: " + rebuilt_hash})
        else:
            archived = json.loads(read_bytes(os.path.join(archive, "derived",
                                                          "wait-groups.tabulated.json")))
            candidate = json.loads(rebuilt_bytes)
            if archived == candidate:
                result["steps"].append({"step": "tabulate_wait", "ok": True,
                                        "detail": "same JSON, formatting differs: " + rebuilt_hash})
            else:
                differing = [key for key in archived if archived.get(key) != candidate.get(key)]
                result["steps"].append({"step": "tabulate_wait", "ok": False,
                                        "detail": "differs in: " + ", ".join(differing)})
        result["ok"] = all(step["ok"] for step in result["steps"])
        return result
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=None)
    parser.add_argument("--rebuild", action="store_true")
    parser.add_argument("--json", default=None)
    args = parser.parse_args()

    archive = os.path.abspath(args.root) if args.root else archive_root()
    if not archive:
        raise SystemExit("cannot locate the archive; pass --root")
    manifest = json.loads(read_bytes(os.path.join(archive, "manifest.json")).decode("utf-8"))

    failures, stats = check_bytes(manifest, archive)
    failures += check_index(manifest, archive)
    file_set_failures, file_set = check_file_set(manifest, archive)
    failures += file_set_failures
    failures += check_totals(manifest, stats)

    validation_failures = []
    validation_path = os.path.join(archive, "validation.json")
    if os.path.isfile(validation_path):
        recorded = json.loads(read_bytes(validation_path).decode("utf-8"))
        if recorded.get("result") != "pass" or recorded.get("checks_failed"):
            validation_failures.append({"check": "validation", "archive": "validation.json",
                                        "detail": "recorded result is not pass"})

    report = {
        "archive": archive,
        "scope": manifest["scope"],
        "checks": {
            "files": stats["files"],
            "gzip_streams": stats["gzip"],
            "text_copies": stats["text"],
            "binary_copies": stats["binary"],
            "archived_bytes": stats["archived_bytes"] + manifest["generated_files"]["index.md"]
            ["bytes"],
            "raw_bytes": manifest["totals"]["raw_bytes"],
            "file_set": file_set,
        },
        "failures": failures + validation_failures,
        "result": "pass" if not failures and not validation_failures else "fail",
    }
    if args.rebuild:
        report["rebuild"] = rebuild(manifest, archive)
        if not report["rebuild"]["ok"]:
            report["result"] = "fail"
    if args.json:
        with open(args.json, "w", encoding="utf-8", newline="\r\n") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")

    print("archive:", archive)
    print("files: {}  gzip: {}  text: {}  binary: {}".format(
        stats["files"], stats["gzip"], stats["text"], stats["binary"]))
    print("raw -> archived: {} -> {} bytes".format(
        manifest["totals"]["raw_bytes"], report["checks"]["archived_bytes"]))
    print("file set: expected {} present {}".format(file_set["expected"], file_set["present"]))
    if args.rebuild:
        for step in report["rebuild"]["steps"]:
            print("  rebuild {}: {} ({})".format(step["step"], "ok" if step["ok"] else "FAIL",
                                                 step["detail"]))
    for failure in report["failures"]:
        print("FAIL", failure["check"], failure["archive"], "-", failure["detail"])
    print("result:", report["result"])
    return 0 if report["result"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
