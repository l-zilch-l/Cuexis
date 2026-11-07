"""Reconstruct which files the two runs measured, from their own manifests.

The report needs to say what source each run actually built, and a hand-written table is exactly the
kind of claim that drifts from the bytes. This script derives it instead. For every path in a run's
`source.files` it compares the recorded sha256 with the blob of that path in the run's `head`:

  * a file counts as unmodified when the recorded hash equals the blob bytes, the blob expanded the
    way `core.autocrlf` materialises it, or today's worktree bytes for that path — provided those
    bytes differ from the blob only by line endings. The third form exists because the run hashed
    raw bytes: this repository stores LF, some checkouts are CRLF and a few files are mixed, and git
    calls all of them clean. Requiring the worktree bytes to hash to the run's own value keeps a real
    content edit out of that case;
  * a path with no blob in the tree was added at run time; one matching no form was modified.

For run 3 it also asks whether the run-time bytes are exactly what `8963889` later committed, which
is what lets the archive claim that the closure anchor covers the bytes the run measured.

Outputs `source-table.json` next to this script; `fill_probe_report.py` reads it.

Usage:
    python -B record_source_table.py [--run2 <dir>] [--run3 <dir>] [--json <file>]
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(r"D:\Cuexis-redesign")
HERE = Path(__file__).resolve().parent
ACCEPTANCE = REPO / "out" / "rpa-acceptance"
DEFAULT_RUN2 = ACCEPTANCE / "20261101-021741-4daf2bb2"
DEFAULT_RUN3 = ACCEPTANCE / "20261101-051925-efdc7b6e"
LATER_COMMIT = "8963889"
LF_TO_CRLF = re.compile(rb"(?<!\r)\n")
CRLF_TO_LF = re.compile(rb"\r\n")
# The files whose repair went in between the two runs; the table names them explicitly.
REPAIRED = (
    "tests/CMakeLists.txt",
    "tests/gameplay_packed/CMakeLists.txt",
    "tests/pd/CMakeLists.txt",
    "tests/pd/README.md",
    "tests/pd/pd_allocation_probe.hpp",
    "tools/verification/run_rpa_acceptance_matrix.py",
)


def git_tree_blobs(rev):
    """path -> blob id for every file in `rev`."""
    out = subprocess.run(["git", "-C", str(REPO), "ls-tree", "-r", "-z", "--full-tree", rev],
                         capture_output=True, check=True).stdout
    blobs = {}
    for entry in out.split(b"\0"):
        if not entry:
            continue
        meta, path = entry.split(b"\t", 1)
        _mode, kind, oid = meta.split(b" ")
        if kind == b"blob":
            blobs[path.decode("utf-8")] = oid.decode("ascii")
    return blobs


def read_blobs(oids):
    """oid -> bytes, in one `git cat-file --batch` process."""
    ordered = sorted(set(oids))
    if not ordered:
        return {}
    out = subprocess.run(["git", "-C", str(REPO), "cat-file", "--batch"],
                         input=("\n".join(ordered) + "\n").encode("ascii"),
                         capture_output=True, check=True).stdout
    contents = {}
    offset = 0
    for oid in ordered:
        end = out.index(b"\n", offset)
        header = out[offset:end].split()
        if len(header) == 2 and header[1] == b"missing":
            offset = end + 1
            continue
        size = int(header[2])
        contents[oid] = out[end + 1:end + 1 + size]
        offset = end + 1 + size + 1
    return contents


def digest_of(blob):
    return (hashlib.sha256(blob).hexdigest(),
            hashlib.sha256(LF_TO_CRLF.sub(b"\r\n", blob)).hexdigest())


FORMS = ("blob", "blob-crlf")


def forms_match(recorded, blob):
    for name, digest in zip(FORMS, digest_of(blob)):
        if recorded == digest:
            return name
    return None


def worktree_match(recorded, blob, path):
    """Recognise a hash that only differs from the blob by line endings.

    The run hashed raw on-disk bytes, so a checkout whose endings are CRLF, or even mixed, produces a
    hash the blob cannot reproduce: git stores LF and un-mixes endings only when it stages. It also
    makes `git status` report such a file as clean. The missing direction therefore has to be checked
    against the bytes themselves, and the only copy available is today's worktree. This returns a form
    only when the worktree file still hashes to exactly what the run recorded *and* git would see its
    content as the blob's content, so a real content edit can never be absorbed here.
    """
    full = REPO / path
    if not full.is_file():
        return None
    disk = full.read_bytes()
    if CRLF_TO_LF.sub(b"\n", disk) != blob:
        return None
    return "worktree-eol" if hashlib.sha256(disk).hexdigest() == recorded else None


def inspect(run):
    manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8"))
    source = manifest["source"]
    head = source["head"]
    recorded = source["files"]
    blobs = git_tree_blobs(head)
    later = git_tree_blobs(LATER_COMMIT)
    contents = read_blobs(list(blobs.values()))
    later_contents = read_blobs([later[path] for path in REPAIRED if path in later])

    states = {"unmodified": 0, "modified": 0, "added": 0}
    forms = {"blob": 0, "blob-crlf": 0, "worktree-eol": 0}
    deltas = []
    for path, digest in recorded.items():
        oid = blobs.get(path)
        blob = contents.get(oid) if oid else None
        if blob is None:
            states["added"] += 1
            deltas.append({"path": path, "run_sha256": digest, "head_sha256": None,
                           "later_sha256": None, "state": "added"})
            continue
        form = forms_match(digest, blob) or worktree_match(digest, blob, path)
        if form:
            states["unmodified"] += 1
            forms[form] += 1
            continue
        states["modified"] += 1
        later_blob = later_contents.get(later.get(path))
        deltas.append({
            "path": path,
            "run_sha256": digest,
            "head_sha256": hashlib.sha256(blob).hexdigest(),
            "later_sha256": hashlib.sha256(later_blob).hexdigest() if later_blob else None,
            "state": "modified",
            "form": None,
            "run_equals_later": bool(later_blob) and (
                digest in digest_of(later_blob)),
        })

    return {
        "run": str(run.relative_to(REPO)),
        "head": head,
        "worktree_sha256": source["worktree_sha256"],
        "implementation_sha256": source["implementation_sha256"],
        "files_recorded": len(recorded),
        "states": states,
        "forms": forms,
        "deltas": sorted(deltas, key=lambda record: record["path"]),
        "repaired": [{"path": path,
                      "run_sha256": recorded.get(path),
                      "later_sha256": hashlib.sha256(later_contents[later[path]]).hexdigest()
                      if path in later and later[path] in later_contents else None}
                     for path in REPAIRED],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run2", type=Path, default=DEFAULT_RUN2)
    parser.add_argument("--run3", type=Path, default=DEFAULT_RUN3)
    parser.add_argument("--json", type=Path, default=HERE / "source-table.json")
    arguments = parser.parse_args()

    payload = {"later_commit": LATER_COMMIT,
               "run2": inspect(arguments.run2),
               "run3": inspect(arguments.run3)}
    with open(arguments.json, "w", encoding="utf-8", newline="\r\n") as stream:
        json.dump(payload, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    for name in ("run2", "run3"):
        record = payload[name]
        print("{}: head {} files {} {}".format(name, record["head"][:8],
                                              record["files_recorded"], record["states"]))
        print("  forms {}".format(record["forms"]))
        for delta in record["deltas"]:
            print("  {} {} equals_8963889={}".format(
                delta["state"], delta["path"], delta.get("run_equals_later")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
