"""Finite diagnostic migration gate; no directory-wide legacy-token exceptions."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def check(root: Path) -> list[str]:
    failures: list[str] = []
    rows = json.loads((root / "docs/stage_reports/stages/stage-07/diagnostic-formalization/2026-10-10-n1-branch-mapping.json").read_text(encoding="utf-8"))
    targets = [r["proposed_name"] for r in rows if r["proposed_name"]] + ["judgement.grace.value_source_conflict", "judgement.input.occurrence_conflict", "judgement.control.reconciliation_invalid"]
    if len(targets) != len(set(targets)):
        failures.append("mapping target collision")
    policy_path = root / "engine/judgement/src/diagnostic_policy.hpp"
    policy = policy_path.read_text(encoding="utf-8")
    expected_old = {r["code"] for r in rows}
    expected_projected = {r["proposed_name"] for r in rows if r["proposed_name"] and r.get("projection") == "category"} | {"judgement.grace.value_source_conflict"}
    for name, expected in (("legacyDiagnosticCodes", expected_old), ("categoryProjectedDiagnosticCodes", expected_projected), ("coordinationProjectedDiagnosticCodes", {"judgement.input.occurrence_conflict", "judgement.control.reconciliation_invalid"})):
        match = re.search(name + r"\{\{(.*?)\}\};", policy, re.S)
        values = re.findall(r'"([^"]+)"', match[1]) if match else []
        if set(values) != expected or len(values) != len(set(values)):
            failures.append(f"{name}: finite membership missing, extra or duplicate")
    header = (root / "engine/judgement/src/source_codes.hpp").read_text(encoding="utf-8")
    for row in rows:
        if not row.get("symbol") or not row["proposed_name"]:
            continue
        if not re.search(re.escape(row["symbol"]) + r'\s*\{\s*"' + re.escape(row["proposed_name"]) + r'"\s*\}', header):
            failures.append(f"constant mapping missing: {row['symbol']}")
    live_constants = re.findall(r'k\w+Code\s*\{\s*"(judgement\.[^"]+)"', header)
    if len(live_constants) != len(set(live_constants)) or not set(live_constants) <= set(targets):
        failures.append("live private diagnostic collision or unregistered projection policy")
    for file, helper in (("execution_kernel.cpp", "error"), ("execution_profile.cpp", "rejection")):
        producer = (root / "engine/judgement/src" / file).read_text(encoding="utf-8")
        suffixes = re.findall(r'\b' + helper + r'\(\s*"([^"]+)"', producer)
        if any("judgement." + suffix not in targets for suffix in suffixes):
            failures.append(f"{file}: dynamic reason has no finite policy")
    projection = (root / "engine/playback/src/gameplay_internal.hpp").read_text(encoding="utf-8")
    if "judgement::detail::categoryProjectedDiagnostic(code)" not in projection:
        failures.append("explicit projection is not used")
    exceptions = {
        "tests/judgement/fold_recovery_tests.cpp": {'"judgement.s7a4.kernel.transaction_failed"'},
        "tests/playback/playback_gameplay_tests.cpp": {'"judgement.s7a2.artificial.undefined"'},
    }
    for directory in ("engine", "app", "tools", "tests", "cmake", "schemas"):
        for path in (root / directory).rglob("*"):
            if not path.is_file() or path.suffix not in (".cpp", ".hpp", ".cmake", ".json"):
                continue
            relative = path.relative_to(root).as_posix()
            if relative == "tests/fixtures/diagnostic_revision1/manifest.json":
                continue  # The exact immutable fixture manifest, not a directory exception.
            text = path.read_text(encoding="utf-8-sig")
            if relative == "schemas/cuexis.gameplay-diagnostics.v2.codes.json":
                document = json.loads(text)
                document.pop("rulingProvenance")  # Exact dated decision field; active fields still scanned.
                text = json.dumps(document)
            if path.suffix in (".cpp", ".hpp"):
                # Preserve quoted text: an http:// literal cannot hide a later code on its line.
                pattern = r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*'
                text = re.sub(pattern, lambda m: "" if m[0].startswith(("//", "/*")) else m[0], text, flags=re.S)
            if path == policy_path:
                text = re.sub(r"legacyDiagnosticCodes\{\{.*?\}\};", "", text, flags=re.S)
            for token in exceptions.get(relative, ()):
                if text.count(token) != 1:
                    failures.append(f"{relative}: negative-fixture exception count changed")
                text = text.replace(token, "")
            if re.search(r"judgement\.(?:s7a[234]|provisional)\.", text):
                failures.append(f"{relative}: active legacy spelling or dynamic prefix")
            if "kSecondHalf" in text or "secondHalfError(" in text:
                failures.append(f"{relative}: deleted placeholder consumer")
    manifest = json.loads((root / "tests/fixtures/diagnostic_revision1/manifest.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        if hashlib.sha256((root / entry["path"]).read_bytes()).hexdigest() != entry["sha256"]:
            failures.append(f"historical original modified: {entry['path']}")
    return failures


def self_test(root: Path) -> None:
    """Prove actual CTest -> CMake -> Python nonzero propagation in isolated input."""
    with tempfile.TemporaryDirectory(prefix="cuexis-diagnostic-gate-") as temporary:
        copy = Path(temporary) / "source"
        for directory in ("engine", "app", "tools", "tests", "cmake", "schemas", "docs/stage_reports/stages/stage-07/diagnostic-formalization"):
            shutil.copytree(root / directory, copy / directory)
        assert not check(copy), check(copy)
        build = Path(temporary) / "test"
        build.mkdir()
        (build / "CTestTestfile.cmake").write_text(
            f'add_test(diagnostic "cmake" "-DCUEXIS_SOURCE_DIR={copy.as_posix()}" "-P" "{(copy / "cmake/VerifyGameplayDiagnosticFormalization.cmake").as_posix()}")\n',
            encoding="utf-8",
        )
        def run(expect_success: bool, reason: str) -> None:
            result = subprocess.run(["ctest", "--test-dir", str(build), "--no-tests=error", "--output-on-failure"], capture_output=True, text=True)
            if (result.returncode == 0) != expect_success:
                raise RuntimeError(f"{reason}: {result.stdout}\n{result.stderr}")
            print(f"{reason}: CTest exit={result.returncode}")
        run(True, "legal exact exceptions")
        mutations = [
            ("engine/judgement/src/source_codes.hpp", lambda s: s + '\nconstexpr auto missed = "judgement.s7a2.input.missed";\n', "old constant"),
            ("engine/judgement/src/execution_kernel.cpp", lambda s: s.replace('"judgement." +', '"judgement.s7a4." +', 1), "old dynamic prefix"),
            ("engine/judgement/src/diagnostic_policy.hpp", lambda s: s.replace('    "judgement.grace.value_source_conflict",', '', 1), "missing projection"),
            ("engine/judgement/src/diagnostic_policy.hpp", lambda s: s.replace('    "judgement.grace.value_source_conflict",', '    "judgement.grace.value_source_conflict",\n    "judgement.grace.value_source_conflict",', 1), "collision"),
        ]
        for name, mutate, reason in mutations:
            path = copy / name
            original = path.read_text(encoding="utf-8")
            modified = mutate(original)
            assert modified != original
            path.write_text(modified, encoding="utf-8")
            run(False, reason)
            path.write_text(original, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    failures = check(args.root)
    if failures:
        print("\n".join(failures))
        return 1
    if args.self_test:
        self_test(args.root)
    print("diagnostic formalization gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
