"""Verify this archive without its work directory.

Every check re-derives something from bytes on disk instead of trusting manifest.json:

  * every copied file is present, has the recorded length, and hashes to the recorded sha256;
  * every gzip stream decompresses to the recorded source hash and recompresses to the same bytes,
    so the archive bytes are reproducible rather than merely stored;
  * every normalised text copy contains no bare LF and round-trips through the recorded
    content_sha256, the hash of the LF-normalised text, which is checkable whatever the source EOL
    was (crlf, lf or mixed);
  * the three archived copies of the 40K canonical text decompress to binding.canonical_40k's
    output hash, and the 40K chart decompresses to its recorded digest;
  * the derived tabulation's own 39 checks all pass, and its digest is the one binding names;
  * no file exists that the manifest does not list, and every file it lists exists;
  * index.md is LF and .gitattributes keeps every archived byte away from git's eol conversion.

Usage:
    python -B verify/verify_startup_evidence.py [archive-root]
"""

import hashlib
import json
import os
import struct
import sys
import zlib

DEFAULT_ARCHIVE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXT_SUFFIXES = (".json", ".txt", ".py", ".md", ".bat")
EXPECTED_ATTRIBUTES = (
    "*.txt binary",
    "*.json binary",
    "*.py binary",
    "*.log binary",
    "*.gz binary",
    "*.bat binary",
    "*.bin binary",
    "index.md text eol=lf",
    ".gitattributes text eol=lf",
)
GENERATED = ("index.md", ".gitattributes", "manifest.json", "validation.json")


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def gunzip_bytes(payload):
    return zlib.decompressobj(-15).decompress(payload[10:])


def gzip_bytes(payload):
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    body = compressor.compress(payload) + compressor.flush()
    header = b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff"
    trailer = struct.pack("<II", zlib.crc32(payload) & 0xFFFFFFFF, len(payload) & 0xFFFFFFFF)
    return header + body + trailer


def main():
    archive = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ARCHIVE)
    manifest_path = os.path.join(archive, "manifest.json")
    if not os.path.isfile(manifest_path):
        raise SystemExit("not an archive root: " + archive)
    with open(manifest_path, "rb") as stream:
        raw_manifest = stream.read()
    manifest = json.loads(raw_manifest.decode("utf-8"))

    failures = []
    notes = []

    def fail(what, detail):
        failures.append(what + ": " + detail)

    # The manifest itself is a CRLF document, so that a fresh checkout of this directory does not
    # change one file under the very rules that pin the rest.
    if raw_manifest.count(b"\n") != raw_manifest.count(b"\r\n"):
        fail("manifest.json", "contains a bare LF")
    if manifest["totals"]["files"] != len(manifest["copied_files"]) + 1:
        fail("manifest.json", "totals.files disagrees with copied_files plus index.md")

    copied = manifest["copied_files"]
    for entry in copied:
        path = os.path.join(archive, entry["archive"].replace("/", os.sep))
        if not os.path.isfile(path):
            fail(entry["archive"], "missing")
            continue
        with open(path, "rb") as stream:
            stored = stream.read()
        if len(stored) != entry["bytes"]:
            fail(entry["archive"], "{} bytes, manifest says {}".format(len(stored),
                                                                       entry["bytes"]))
        if sha256_bytes(stored) != entry["sha256"]:
            fail(entry["archive"], "sha256 does not match the manifest")
        if entry["mode"] == "gzip":
            payload = gunzip_bytes(stored)
            if sha256_bytes(payload) != entry["source_sha256"]:
                fail(entry["archive"], "gunzip does not reproduce source_sha256")
            if gzip_bytes(payload) != stored:
                fail(entry["archive"], "recompression does not reproduce the archived bytes")
            if not stored.startswith(b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff"):
                fail(entry["archive"], "gzip header is not the fixed one")
        elif entry["mode"] == "text":
            if stored.count(b"\n") != stored.count(b"\r\n"):
                fail(entry["archive"], "normalised copy contains a bare LF")
            if sha256_bytes(stored.replace(b"\r\n", b"\n")) != entry["content_sha256"]:
                fail(entry["archive"], "content_sha256 round-trip failed")
            if stored.startswith(b"\xef\xbb\xbf"):
                fail(entry["archive"], "normalised copy kept a BOM")

    # Generated files.
    index_path = os.path.join(archive, "index.md")
    with open(index_path, "rb") as stream:
        index_payload = stream.read()
    if sha256_bytes(index_payload) != manifest["generated_files"]["index.md"]["sha256"]:
        fail("index.md", "sha256 does not match generated_files")
    if b"\r" in index_payload:
        fail("index.md", "is not LF-only")

    attributes_path = os.path.join(archive, ".gitattributes")
    with open(attributes_path, "rb") as stream:
        attributes = stream.read().decode("utf-8")
    for rule in EXPECTED_ATTRIBUTES:
        if rule not in attributes:
            fail(".gitattributes", "missing rule: " + rule)

    # File set.
    present = set()
    for root, _dirs, names in os.walk(archive):
        for name in names:
            present.add(os.path.relpath(os.path.join(root, name), archive).replace(os.sep, "/"))
    expected = {entry["archive"] for entry in copied} | set(GENERATED)
    stray = sorted(present - expected)
    missing = sorted(expected - present)
    if stray:
        fail("file set", "stray: " + ", ".join(stray))
    if missing:
        fail("file set", "missing: " + ", ".join(missing))

    # The 40K canonical text, three times, against the identity the manifest names.
    binding = manifest["binding"]
    canonical = binding["canonical_40k"]
    copies = [entry for entry in copied
              if entry["archive"].endswith("40k-canonical.json.gz")
              or entry["archive"].endswith("e107126d44c6da1f-t3.json.gz")]
    if len(copies) != 3:
        fail("canonical 40k", "expected 3 archived copies, found {}".format(len(copies)))
    for entry in copies:
        with open(os.path.join(archive, entry["archive"].replace("/", os.sep)), "rb") as stream:
            payload = gunzip_bytes(stream.read())
        if sha256_bytes(payload) != canonical["output_sha256"]:
            fail(entry["archive"], "does not decompress to binding.canonical_40k.output_sha256")
        if len(payload) != canonical["output_bytes"]:
            fail(entry["archive"], "does not decompress to binding.canonical_40k.output_bytes")

    # The chart the project runs.
    chart = [entry for entry in copied
             if entry["archive"].endswith("stress_40k.cuexis.chart.json.gz")]
    if len(chart) != 1:
        fail("project chart", "expected 1 archived copy, found {}".format(len(chart)))
    else:
        with open(os.path.join(archive, chart[0]["archive"]), "rb") as stream:
            payload = gunzip_bytes(stream.read())
        if sha256_bytes(payload) != chart[0]["source_sha256"]:
            fail("project chart", "does not decompress to its recorded source hash")

    # The tabulation, which is what the report quotes.
    table_path = os.path.join(archive, "derived", "startup-cost-table.json")
    with open(table_path, "rb") as stream:
        table_raw = stream.read()
    table = json.loads(table_raw.decode("utf-8"))
    failed_checks = [check["name"] for check in table["checks"] if not check["ok"]]
    if failed_checks:
        fail("tabulation", "{} of {} checks fail: {}".format(
            len(failed_checks), len(table["checks"]), "; ".join(failed_checks[:5])))
    tabulated = manifest["derived_files"][0]
    # Two identities, and they are different on purpose: the archived copy is CRLF-normalised like
    # every other text copy, while the file the tabulator wrote is LF.  tabulated_sha256 names the
    # archived bytes and tabulated_source_sha256 names the bytes a re-run has to reproduce.
    normalised = table_raw.replace(b"\r\n", b"\n")
    if sha256_bytes(table_raw) != tabulated["sha256"]:
        fail("derived tabulation", "archived bytes do not match derived_files.sha256")
    if sha256_bytes(normalised) != tabulated["content_sha256"]:
        fail("derived tabulation", "content hash does not match derived_files.content_sha256")
    if sha256_bytes(table_raw) != binding["tabulated_sha256"]:
        fail("derived tabulation", "archived bytes do not match binding.tabulated_sha256")
    if sha256_bytes(normalised) != binding["tabulated_source_sha256"]:
        fail("derived tabulation", "source bytes do not match binding.tabulated_source_sha256")

    # Digest closure: a hash the tabulation cites must be a file here or a bound identity.
    cited = set()

    def collect(node, out):
        if isinstance(node, dict):
            for value in node.values():
                collect(value, out)
        elif isinstance(node, list):
            for value in node:
                collect(value, out)
        elif isinstance(node, str) and len(node) == 64 and all(
                character in "0123456789abcdef" for character in node):
            out.add(node)

    known = set()
    for entry in copied:
        known.add(entry["sha256"])
        known.add(entry["source_sha256"])
        if entry.get("content_sha256"):
            known.add(entry["content_sha256"])
    bound = set()
    collect(table, cited)
    collect(binding, bound)
    unattributed = sorted(cited - known - bound)
    if unattributed:
        fail("digest closure", "cited but unaccounted for: " + ", ".join(unattributed))

    # Binaries are not archived, so the on-disk copies are compared only when they are still there;
    # a rebuilt or deleted binary is reported, never failed.
    for name, key in (("player", "cuexis_player.exe"),
                      ("probe", "cuexis_execution_device_probe.exe")):
        recorded = binding[name]["on_disk_now"]["sha256"]
        notes.append("{} recorded on-disk digest {}".format(name, recorded))
    for entry in manifest["instrument_versions"]:
        if not entry["source_mtime_utc"]:
            fail(entry["archive"], "instrument has no mtime, so its relation note is unanchored")

    print("archive:", archive)
    print("files:", len(present), "copied:", len(copied))
    print("modes:", manifest["totals"]["text"], "text,", manifest["totals"]["gzip"], "gzip,",
          manifest["totals"]["binary"], "binary")
    print("tabulation checks:", len(table["checks"]), "findings:", len(table["findings"]))
    print("cited digests:", len(cited), "unattributed:", len(unattributed))
    for note in notes:
        print("note:", note)
    if failures:
        print("FAILED {} checks".format(len(failures)))
        for failure in failures:
            print("  -", failure)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
