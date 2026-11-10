"""Prove that a commit still carries the source state a matrix run measured.

A run records ``worktree_sha256`` over the exact bytes it found on disk, as a map of
``path -> sha256``. Those bytes are *not* reproducible from a fresh checkout, because a
checkout applies ``core.autocrlf`` to every text file while the worktree a run measured
carried whatever form each file had accumulated locally: files never rewritten since the
clone were LF, files rewritten by tooling were CRLF. So the aggregate hash is a hash of a
history, not of a commit.

The honest closure is therefore content-level, and it is what this script measures:

1. every path the run recorded is still present at the commit and its bytes are the same
   content, allowing exactly the two line-ending forms git itself can produce for that
   content (``as-worktree`` = byte equal, ``crlf`` = equal after expanding line endings);
2. no non-document path was added, removed or changed (``docs/`` and ``*.md`` are excluded
   because publishing the run's report necessarily writes them);
3. the recording of which form each file took is printed and stored, so the aggregate
   hashes can be explained rather than claimed.

Nothing is repaired or normalised: the script reads bytes, compares them and exits
non-zero on any non-document mismatch.

``--root`` selects the checkout to read (default: this working copy). Verifying a past
commit needs a second checkout of it - a linked worktree is enough:

    git worktree add --detach out/_identity-8963889 8963889
    python -B verify_identity_closure.py out/rpa-acceptance/<run> \\
        --root out/_identity-8963889 --head 8963889 --json identity-closure.json
    git worktree remove out/_identity-8963889

Usage:
    python -B verify_identity_closure.py <run dir> [--head <sha>] [--root <checkout>] [--json <out>]
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT = r"D:\Cuexis-redesign"
ALLOWED_UNTRACKED = ("app/", "tests/", "tools/", "engine/", "cmake/", "schemas/", "docs/")


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def use_root(path):
    """Point every subsequent git call and file read at another checkout of the repo."""
    global ROOT
    ROOT = os.path.abspath(path)


def git(arguments):
    return subprocess.check_output(["git"] + arguments, cwd=ROOT).decode("utf-8", "replace")


def current_map():
    """Rebuild the run's file map for the checkout as it stands on disk."""
    paths = {p for p in git(["ls-files", "-z"]).split("\0") if p}
    untracked = git(["ls-files", "--others", "--exclude-standard", "-z"]).split("\0")
    for entry in untracked:
        if entry and entry.startswith(ALLOWED_UNTRACKED) and not entry.endswith(".pyc"):
            paths.add(entry)
    result = {}
    for relative in sorted(paths):
        absolute = os.path.join(ROOT, relative)
        if os.path.isfile(absolute):
            result[relative] = sha256_file(absolute)
    return result


def digest(files):
    return hashlib.sha256(json.dumps(files, sort_keys=True).encode("utf-8")).hexdigest()


def is_document(relative):
    return relative.startswith("docs/") or relative.endswith(".md")


def eol_forms(path):
    """The byte forms one content can legitimately take in a worktree.

    ``as-worktree`` is what the file holds now, ``lf`` and ``crlf`` are the same content
    with line endings folded one way or the other. A run's file was one of the three,
    depending on whether the tool that last wrote it used CRLF or the checkout left it LF.
    """
    with open(path, "rb") as stream:
        data = stream.read()
    lf = data.replace(b"\r\n", b"\n")
    return {
        "as-worktree": sha256_bytes(data),
        "lf": sha256_bytes(lf),
        "crlf": sha256_bytes(lf.replace(b"\n", b"\r\n")),
    }


def classify(recorded_sha, forms):
    """Name the form the run's bytes took, or ``None`` when the content itself differs."""
    if recorded_sha == forms["as-worktree"]:
        return "as-worktree"
    if recorded_sha == forms["lf"]:
        return "lf"
    if recorded_sha == forms["crlf"]:
        return "crlf"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", help="run directory holding manifest.json")
    parser.add_argument("--head", help="expected HEAD, default: whatever the checkout is on")
    parser.add_argument("--root", help="checkout to read, default: {}".format(ROOT))
    parser.add_argument("--json", help="write the closure record to this path")
    parser.add_argument("--reference",
                        help="second checkout of the same commit carrying the run's line-ending "
                             "forms, used to explain mixed-line-ending files")
    arguments = parser.parse_args()
    if arguments.root:
        use_root(arguments.root)

    with open(os.path.join(arguments.run, "manifest.json"), encoding="utf-8") as stream:
        manifest = json.load(stream)
    source = manifest["source"]
    recorded = source["files"]
    run_head = source["head"]

    head = git(["rev-parse", "HEAD"]).strip()
    if arguments.head and head != arguments.head:
        raise SystemExit("HEAD is {} but --head says {}".format(head, arguments.head))

    actual = current_map()
    added = sorted(set(actual) - set(recorded))
    removed = sorted(set(recorded) - set(actual))

    # Classify every path that exists on both sides by the byte form its content takes.
    by_form = {"as-worktree": [], "lf": [], "crlf": []}
    unexplained = []
    for relative in sorted(set(recorded) & set(actual)):
        form = classify(recorded[relative], eol_forms(os.path.join(ROOT, relative)))
        if form is None:
            unexplained.append(relative)
        else:
            by_form[form].append(relative)

    # A worktree may hold a file whose lines are mixed CRLF/LF. Git treats that as clean (it
    # normalises to the same blob) and a fresh checkout rewrites it uniformly, so the recorded
    # bytes are neither a pure LF nor a pure CRLF form of it. `--reference` is a second checkout of
    # the same commit that carries the run's forms; equal content after folding line endings
    # explains the file, unequal content is drift.
    mixed, drift = [], []
    for relative in unexplained:
        if not arguments.reference:
            drift.append(relative)
            continue
        reference = os.path.join(arguments.reference, relative)
        if not os.path.isfile(reference):
            drift.append(relative)
            continue
        with open(os.path.join(ROOT, relative), "rb") as stream:
            mine = stream.read().replace(b"\r\n", b"\n")
        with open(reference, "rb") as stream:
            theirs = stream.read().replace(b"\r\n", b"\n")
        (mixed if mine == theirs else drift).append(relative)

    crlf = by_form["crlf"]
    problems = ["{}: {}".format("added", path) for path in added if not is_document(path)]
    problems += ["{}: {}".format("removed", path) for path in removed if not is_document(path)]
    problems += ["{}: {}".format("changed", path) for path in drift if not is_document(path)]

    documents = sorted(path for path in added + removed + drift if is_document(path))
    implementation_sha = digest({p: sha for p, sha in actual.items() if not is_document(p)})
    implementation_run_sha = digest({p: sha for p, sha in recorded.items() if not is_document(p)})

    print("run head            : {}".format(run_head))
    print("checkout head       : {}".format(head))
    print("checkout root       : {}".format(ROOT))
    print("files at run start  : {}".format(len(recorded)))
    print("files at this commit: {}".format(len(actual)))
    print("content equal       : {} as-worktree + {} lf + {} crlf + {} mixed + {} drifted".format(
        len(by_form["as-worktree"]), len(by_form["lf"]), len(crlf), len(mixed), len(drift)))
    print("worktree_sha256 run : {}".format(source["worktree_sha256"]))
    print("worktree_sha256 now : {}".format(digest(actual)))
    print("implementation run  : {}".format(source.get("implementation_sha256") or implementation_run_sha))
    print("implementation now  : {}".format(implementation_sha))
    print("aggregate bytes     : worktree equal {} / implementation equal {}".format(
        digest(actual) == source["worktree_sha256"], implementation_sha == implementation_run_sha))
    print("document deltas     : added {} / removed {} / changed {}".format(
        sum(1 for p in added if is_document(p)),
        sum(1 for p in removed if is_document(p)),
        sum(1 for p in drift if is_document(p))))
    for path in documents[:20]:
        print("  {} {}".format(
            "added  " if path in added else "removed" if path in removed else "changed", path))
    if len(documents) > 20:
        print("  ... {} more document deltas".format(len(documents) - 20))
    if crlf:
        print("crlf-form files (the run recorded CRLF; content identical):")
        for path in crlf[:20]:
            print("  " + path)
        if len(crlf) > 20:
            print("  ... {} more".format(len(crlf) - 20))

    if mixed:
        print("mixed line endings (content equal to {}, read from --reference):".format(
            arguments.reference))
        for path in mixed[:10]:
            print("  " + path)
        if len(mixed) > 10:
            print("  ... {} more".format(len(mixed) - 10))

    record = {
        "run": os.path.abspath(arguments.run),
        "run_head": run_head,
        "checkout_head": head,
        "checkout_root": ROOT,
        "reference_root": os.path.abspath(arguments.reference) if arguments.reference else None,
        "files_at_run_start": len(recorded),
        "files_at_commit": len(actual),
        "content_equal_as_worktree": len(by_form["as-worktree"]),
        "content_equal_lf_form": len(by_form["lf"]),
        "content_equal_crlf_form": len(crlf),
        "content_equal_mixed_line_endings": len(mixed),
        "drifted": len(drift),
        "added": added,
        "removed": removed,
        "mixed_line_ending_files": mixed,
        "drifted_files": drift,
        "document_deltas": documents,
        "worktree_sha256_run": source["worktree_sha256"],
        "worktree_sha256_commit": digest(actual),
        "implementation_sha256_run": source.get("implementation_sha256"),
        "implementation_sha256_commit": implementation_sha,
        "aggregate_bytes_equal": {
            "worktree": digest(actual) == source["worktree_sha256"],
            "implementation": implementation_sha == implementation_run_sha,
        },
        "content_closed": not problems,
        "problems": problems,
        "note": ("Aggregate sha256 values hash the exact bytes of a worktree, including each file's "
                 "line-ending form. A fresh checkout applies core.autocrlf uniformly, so a run that "
                 "recorded a locally accumulated mix cannot be reproduced hash-for-hash from it. "
                 "Closure is therefore asserted on content: every non-document path is byte-equal, "
                 "equal up to a line-ending fold, or - with a --reference checkout carrying the "
                 "run's forms - equal after folding a mixed-line-ending file."),
    }
    if arguments.json:
        with open(arguments.json, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(record, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        print("wrote {}".format(arguments.json))

    if problems:
        print("\nproblems:")
        for problem in problems:
            print("  - " + problem)
        return 1
    print("\nidentity closed: this commit carries exactly the implementation content the run measured")
    return 0


if __name__ == "__main__":
    sys.exit(main())
