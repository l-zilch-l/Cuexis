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
    # A gate that silently loses its inputs is worse than no gate: name every file and directory
    # the scan depends on, and stop before the scan when one of them is absent.
    required_inputs = (
        "docs/stage_reports/stages/stage-07/02-diagnostics/n1/2026-10-10-n1-branch-mapping.json",
        "engine/judgement/src/diagnostic_policy.hpp",
        "engine/judgement/src/source_codes.hpp",
        "engine/judgement/src/publication.cpp",
        "engine/playback/src/gameplay_internal.hpp",
        "schemas/cuexis.gameplay-diagnostics.v2.codes.json",
        "tests/fixtures/diagnostic_revision1/manifest.json",
    )
    for relative in required_inputs:
        if not (root / relative).is_file():
            failures.append(f"required gate input is missing: {relative}")
    for directory in ("engine", "app", "tools", "tests", "cmake", "schemas"):
        if not (root / directory).is_dir():
            failures.append(f"scanned directory is missing: {directory}")
    if failures:
        return failures
    rows = json.loads((root / "docs/stage_reports/stages/stage-07/02-diagnostics/n1/2026-10-10-n1-branch-mapping.json").read_text(encoding="utf-8"))
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
    kernel_units = sorted((root / "engine/judgement/src").glob("execution_kernel*.cpp"))
    if len(kernel_units) < 2:
        failures.append("execution kernel unit glob found fewer than two units")
    kernel_units.append(root / "engine/judgement/src/execution_profile.cpp")
    for path in kernel_units:
        producer = path.read_text(encoding="utf-8")
        helper = "rejection" if path.name == "execution_profile.cpp" else "error"
        suffixes = re.findall(r'\b' + helper + r'\(\s*"([^"]+)"', producer)
        if any("judgement." + suffix not in targets for suffix in suffixes):
            failures.append(
                f"{path.relative_to(root).as_posix()}: dynamic reason has no finite policy"
            )
    projection = (root / "engine/playback/src/gameplay_internal.hpp").read_text(encoding="utf-8")
    if "judgement::detail::classifyPublicDiagnostic(code)" not in projection:
        failures.append("explicit projection is not used")
    bridge = (root / "engine/judgement/src/publication.cpp").read_text(encoding="utf-8")
    if not re.search(r"classifyPublicDiagnostic\(std::string_view code\)\s*noexcept\s*->\s*bool\s*\{\s*return categoryProjectedDiagnostic\(code\);\s*\}", bridge):
        failures.append("publication diagnostic bridge does not delegate to the finite policy")
    if "judgement/src/diagnostic_policy.hpp" in projection:
        failures.append("Playback bypasses the internal diagnostic bridge")
    exceptions = {
        "tests/judgement/fold_recovery_diagnostic_tests.cpp": {
            '"judgement.s7a4.kernel.transaction_failed"'
        },
        "tests/playback/playback_gameplay_diagnostics_tests.cpp": {
            '"judgement.s7a2.artificial.undefined"'
        },
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
    # A wholesale deletion inside a scanned directory would otherwise leave the scan green.
    for directory, floor in (("engine", 200), ("app", 20), ("tools", 20), ("tests", 150), ("cmake", 10), ("schemas", 5)):
        found = sum(
            1
            for path in (root / directory).rglob("*")
            if path.is_file() and path.suffix in (".cpp", ".hpp", ".cmake", ".json")
        )
        if found < floor:
            failures.append(f"{directory}: only {found} files scanned, expected at least {floor}")
    manifest = json.loads((root / "tests/fixtures/diagnostic_revision1/manifest.json").read_text(encoding="utf-8"))
    for entry in manifest["files"]:
        if hashlib.sha256((root / entry["path"]).read_bytes()).hexdigest() != entry["sha256"]:
            failures.append(f"historical original modified: {entry['path']}")
    return failures


def self_test(root: Path) -> None:
    """Prove actual CTest -> CMake -> Python nonzero propagation in isolated input."""
    with tempfile.TemporaryDirectory(prefix="cuexis-diagnostic-gate-") as temporary:
        copy = Path(temporary) / "source"
        for directory in ("engine", "app", "tools", "tests", "cmake", "schemas", "docs/stage_reports/stages/stage-07/02-diagnostics"):
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
        # Vacuity guards: losing an input must fail the gate instead of silently narrowing it.
        schema = copy / "schemas/cuexis.gameplay-diagnostics.v2.codes.json"
        parked_schema = schema.with_name(schema.name + ".parked")
        schema.rename(parked_schema)
        run(False, "missing codes schema")
        parked_schema.rename(schema)
        kernels = sorted((copy / "engine/judgement/src").glob("execution_kernel*.cpp"))
        if len(kernels) < 2:
            raise RuntimeError("self-test expects several execution kernel units")
        for path in kernels:
            path.rename(path.with_name(path.name + ".parked"))
        run(False, "empty execution kernel glob")
        for path in kernels:
            path.with_name(path.name + ".parked").rename(path)
        app = copy / "app"
        parked_app = copy / "app.parked"
        app.rename(parked_app)
        run(False, "missing scanned directory")
        parked_app.rename(app)
        mutations = [
            ("engine/playback/src/gameplay_internal.hpp", lambda s: s.replace('judgement::detail::classifyPublicDiagnostic(code)', 'false', 1), "missing bridge consumer"),
            ("engine/judgement/src/publication.cpp", lambda s: s.replace('return categoryProjectedDiagnostic(code);', 'return false;', 1), "disconnected finite policy"),
            ("engine/judgement/src/source_codes.hpp", lambda s: s + '\nconstexpr auto missed = "judgement.s7a2.input.missed";\n', "old constant"),
            ("engine/judgement/src/execution_kernel_internal.hpp", lambda s: s.replace('"judgement." +', '"judgement.s7a4." +', 1), "old dynamic prefix"),
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
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
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
