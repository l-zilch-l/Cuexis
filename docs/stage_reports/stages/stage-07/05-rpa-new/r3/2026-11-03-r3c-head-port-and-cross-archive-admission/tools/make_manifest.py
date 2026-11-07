"""Record the byte identity of one evidence directory.

    python -B make_manifest.py <evidence-dir> --scope "one line" --source-run "where it was produced"

Writes <evidence-dir>/manifest.json listing every regular file with bytes, sha256 and EOL, plus a
content hash of the list itself, so a later reader can tell a copied file from an edited one.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=pathlib.Path)
    parser.add_argument("--scope", required=True)
    parser.add_argument("--source-run", required=True)
    args = parser.parse_args()

    root: pathlib.Path = args.directory.resolve()
    entries = []
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.name == "manifest.json":
            continue
        data = path.read_bytes()
        entries.append(
            {
                "path": path.relative_to(root).as_posix(),
                "bytes": len(data),
                "sha256": sha256_bytes(data),
                "eol": "crlf" if b"\r\n" in data else "lf",
            }
        )
    document = {
        "scope": args.scope,
        "source_run": args.source_run,
        "content_sha256": sha256_bytes(
            json.dumps(entries, sort_keys=True).encode("utf-8")
        ),
        "files": entries,
    }
    (root / "manifest.json").write_text(
        json.dumps(document, indent=2) + "\n", encoding="utf-8"
    )
    print(f"{len(entries)} files -> {root / 'manifest.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
