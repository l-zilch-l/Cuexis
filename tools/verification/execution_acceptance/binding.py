"""Immutable file and clean-source bindings used before execution acceptance runs."""
import hashlib
import json
from pathlib import Path
import re
import subprocess


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_once(path, document):
    with Path(path).open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(document, ensure_ascii=False, indent=2) + "\n")


def artifact(path):
    path = Path(path).resolve(strict=True)
    if not path.is_file():
        raise ValueError("artifact is not a file")
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": sha256(path)}


def verify_artifact(item):
    if not isinstance(item, dict) or not re.fullmatch("[a-f0-9]{64}", item.get("sha256", "")):
        raise ValueError("invalid artifact binding")
    path = Path(item["path"])
    if not path.is_absolute() or not path.is_file():
        raise ValueError("missing absolute artifact path")
    if type(item.get("bytes")) is not int or path.stat().st_size != item["bytes"]:
        raise ValueError("artifact size changed: " + str(path))
    if sha256(path) != item["sha256"]:
        raise ValueError("artifact hash changed: " + str(path))
    return path


def clean_source(root):
    root = Path(root).resolve()
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root).decode("utf-8").strip()
    if git("status", "--porcelain", "--untracked-files=all"):
        raise ValueError("candidate source must be committed and clean")
    files = git("ls-files", "-z").split("\x00")
    bound = {name: artifact(root / name) for name in files if name}
    return {"schema": "rpa.execution.source-binding.v1", "root": str(root),
            "sha": git("rev-parse", "HEAD"), "tree": git("rev-parse", "HEAD^{tree}"),
            "files": bound}


def verify_source(binding):
    if binding.get("schema") != "rpa.execution.source-binding.v1" or not binding.get("files"):
        raise ValueError("missing source binding")
    actual = clean_source(binding["root"])
    if actual != binding:
        raise ValueError("candidate source identity changed")


def accepted_budget(acceptance, budget):
    """Accept external O1 evidence without changing the frozen proposal itself."""
    record = read(verify_artifact(acceptance))
    path = verify_artifact(budget)
    if record.get("schema") != "rpa.r4.o1-acceptance.v1" or record.get("accepted") is not True:
        raise ValueError("O1 reference budget has not been accepted")
    key = "tools/verification/execution_acceptance/budget-proposal.v1.json"
    if record.get("accepted_input_sha256", {}).get(key) != budget["sha256"]:
        raise ValueError("budget differs from the exact O1 accepted artifact")
    document = read(path)
    if document.get("schema") != "rpa.r4.budget-proposal.v1":
        raise ValueError("unsupported reference budget")
    return document
