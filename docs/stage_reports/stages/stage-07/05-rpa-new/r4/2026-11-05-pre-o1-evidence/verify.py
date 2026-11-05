"""Verify captured bytes and recompute both budget documents without running workloads."""
import hashlib
import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    manifest = json.loads((ROOT / "manifest.json").read_text(encoding="utf-8"))
    for item in manifest["files"]:
        path = ROOT / item["path"]
        assert path.resolve().is_relative_to(ROOT.resolve())
        assert path.stat().st_size == item["bytes"], item["path"]
        assert digest(path) == item["sha256"], item["path"]
    assert digest(ROOT / "summary.json") == manifest["summary_sha256"]
    prepared = ROOT / "raw/preparation-tools"
    layout_path = prepared / "element-sizes-msvc-release.v1.json"
    budget_path = prepared / "budget-proposal.v1.json"
    module = load_module("archived_budget", prepared / "budgets.py")
    rebuilt = module.validate(module.derive(json.loads(layout_path.read_text(encoding="utf-8"))))
    rebuilt["layout_sha256"] = digest(layout_path)
    rebuilt["derivation_sha256"] = digest(prepared / "budgets.py")
    assert rebuilt == json.loads(budget_path.read_text(encoding="utf-8"))
    frozen = ROOT / "raw/frozen-b0/tools/verification/playback_performance"
    old = load_module("archived_b0", frozen / "memory_caps.py")
    normalized = {old.PROFILES.resolve(), (frozen / "memory_caps.py").resolve()}
    old.sha256 = lambda path: hashlib.sha256(
        path.read_bytes().replace(b"\r\n", b"\n") if path.resolve() in normalized
        else path.read_bytes()).hexdigest()
    assert old.render(old.build()) == old.OUTPUT.read_text(encoding="utf-8")
    for platform in ("msvc", "mingw"):
        directory = ROOT / "raw/after-audio-fix" / platform
        result = json.loads((directory / "ctest.json").read_text(encoding="utf-8"))
        assert result["exit_code"] == 0
        assert digest(directory / "ctest.log") == result["log_sha256"]
    print(f"PASS: {len(manifest['files'])} raw files; R4 budget; canonical-LF B0; two CTest logs")


if __name__ == "__main__":
    main()
