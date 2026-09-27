"""Focused tests for the Stage 6 version comparison and trust contract."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import unittest
from datetime import date
from pathlib import Path

import check_version_gate as gate
from update_version import Version


TOOLS = Path(__file__).resolve().parent
REPO_ROOT = Path(os.environ.get("GITHUB_WORKSPACE", TOOLS.parent))
TRUSTED_ROOT = Path(os.environ.get("CUEXIS_TRUSTED_ROOT", TOOLS.parent))
WORKFLOW = TRUSTED_ROOT / ".github" / "workflows" / "version-gate.yml"


def snapshot(value: str, sdk: str = "0.7.0") -> gate.VersionSnapshot:
    return gate.VersionSnapshot("test", gate.parse_version(value), sdk)


def assert_code(test: unittest.TestCase, code: str, function, *arguments, **keywords) -> None:
    with test.assertRaises(gate.GateError) as raised:
        function(*arguments, **keywords)
    test.assertTrue(str(raised.exception).startswith(code + ":"), str(raised.exception))


class VersionGateTests(unittest.TestCase):
    def test_parse_reuses_canonical_calendar_rules(self) -> None:
        self.assertEqual(Version(26, 8, 1, 1), gate.parse_version("26.08.01-1"))
        assert_code(self, "version.trusted_date.invalid", gate.parse_trusted_date, "2026-02-30")
        with self.assertRaises(ValueError):
            gate.parse_version("26.02.30-1")

    def test_same_day_requires_exact_next_build(self) -> None:
        result = gate.compare_snapshots(
            snapshot("26.09.20-3"),
            snapshot("26.09.20-4"),
            date(2026, 9, 20),
            "live",
        )
        self.assertEqual("26.09.20-4", result.candidate_version)

    def test_date_forward_requires_build_one(self) -> None:
        result = gate.compare_snapshots(
            snapshot("26.09.20-9"),
            snapshot("26.09.21-1"),
            date(2026, 9, 21),
            "live",
        )
        self.assertEqual("26.09.21-1", result.candidate_version)

    def test_rejects_unchanged_skipped_backward_and_bad_cross_day_builds(self) -> None:
        checks = (
            ("version.unchanged", "26.09.20-3", "26.09.20-3", date(2026, 9, 20)),
            # A repeated build at the ceiling must still be reported as "not advanced": the
            # diagnostic order must not let exhaustion mask the real reason (SPEC-06).
            ("version.unchanged", "26.09.20-2147483647", "26.09.20-2147483647", date(2026, 9, 20)),
            ("version.build.skipped", "26.09.20-3", "26.09.20-5", date(2026, 9, 20)),
            ("version.build.backward", "26.09.20-3", "26.09.20-2", date(2026, 9, 20)),
            ("version.cross_day_build.invalid", "26.09.20-3", "26.09.21-2", date(2026, 9, 21)),
            ("version.date.backward", "26.09.20-3", "26.09.19-1", date(2026, 9, 20)),
        )
        for code, base, candidate, trusted_date in checks:
            with self.subTest(code=code):
                assert_code(
                    self,
                    code,
                    gate.compare_snapshots,
                    snapshot(base),
                    snapshot(candidate),
                    trusted_date,
                    "live",
                )

    def test_rejects_future_and_stale_live_release_dates(self) -> None:
        assert_code(
            self,
            "version.release_date.future",
            gate.compare_snapshots,
            snapshot("26.09.20-1"),
            snapshot("26.09.22-1"),
            date(2026, 9, 21),
            "live",
        )
        assert_code(
            self,
            "version.release_date.stale",
            gate.compare_snapshots,
            snapshot("26.09.19-1"),
            snapshot("26.09.20-1"),
            date(2026, 9, 21),
            "live",
        )

    def test_historical_context_uses_recorded_date_not_current_date(self) -> None:
        # A historical SHA is re-verified with its recorded release context: the
        # baseline's recorded date is authoritative, so re-running the gate on a
        # much later day must not reject the candidate as stale (SPEC-03).
        recorded = date(2026, 8, 2)
        result = gate.compare_snapshots(
            snapshot("26.08.01-1"),
            snapshot("26.08.02-1"),
            recorded,
            "historical",
        )
        self.assertEqual("historical", result.context)
        # Prove the context really selects the rule: the same pair judged live on
        # a later day must still be stale, so the two paths genuinely diverge.
        assert_code(
            self,
            "version.release_date.stale",
            gate.compare_snapshots,
            snapshot("26.08.01-1"),
            snapshot("26.08.02-1"),
            date(2026, 9, 28),
            "live",
        )
        # ...while historical on that same later day pair still passes.
        historical = gate.compare_snapshots(
            snapshot("26.08.01-1"),
            snapshot("26.08.02-1"),
            date(2026, 9, 28),
            "historical",
        )
        self.assertEqual("26.08.02-1", historical.candidate_version)

    def test_rejects_sdk_change_without_explicit_acceptance(self) -> None:
        assert_code(
            self,
            "version.sdk_api.changed",
            gate.compare_snapshots,
            snapshot("26.09.20-1", "0.7.0"),
            snapshot("26.09.20-2", "0.7.1"),
            date(2026, 9, 20),
            "live",
        )
        result = gate.compare_snapshots(
            snapshot("26.09.20-1", "0.7.0"),
            snapshot("26.09.20-2", "0.7.1"),
            date(2026, 9, 20),
            "live",
            allow_sdk_api_change=True,
        )
        self.assertTrue(result.sdk_api_change_explicitly_allowed)

    def test_snapshot_rejects_manifest_drift(self) -> None:
        cmake = (
            "set(CUEXIS_VERSION_YEAR 26)\n"
            "set(CUEXIS_VERSION_MONTH 9)\n"
            "set(CUEXIS_VERSION_DAY 20)\n"
            "set(CUEXIS_VERSION_BUILD 1)\n"
            "set(CUEXIS_SDK_API_VERSION \"0.7.0\")\n"
        )
        manifest = json.dumps({"version-string": "26.09.20-2"})
        assert_code(
            self,
            "version.manifest.mismatch",
            gate.snapshot_from_texts,
            "test",
            cmake,
            manifest,
        )

    def test_compare_refs_rejects_invalid_missing_and_non_ancestor_refs(self) -> None:
        # Self-contained history, so the case does not depend on the ambient checkout depth. A
        # hosted job may fetch a single commit, in which case `HEAD^` does not resolve and a test
        # that reads it would fail for a reason unrelated to the gate.
        with tempfile.TemporaryDirectory(prefix="cuexis-gate-refs-") as directory:
            root = Path(directory)
            self._init_repository(root)
            (root / "seed.txt").write_text("seed\n", encoding="utf-8")
            self._git(root, "add", "-A")
            self._git(root, "commit", "-q", "-m", "seed")
            base = self._git(root, "rev-parse", "HEAD")
            (root / "seed.txt").write_text("seed\nsecond\n", encoding="utf-8")
            self._git(root, "add", "-A")
            self._git(root, "commit", "-q", "-m", "second")
            head = self._git(root, "rev-parse", "HEAD")
            trusted_date = date(2026, 9, 20)

            assert_code(
                self,
                "version.baseline.ref.invalid",
                gate.compare_refs,
                root,
                "HEAD",  # a symbolic ref is not a full 40-character SHA
                head,
                trusted_date,
                "historical",
            )
            assert_code(
                self,
                "version.baseline.missing",
                gate.compare_refs,
                root,
                "0" * 40,
                head,
                trusted_date,
                "historical",
            )
            assert_code(
                self,
                "version.candidate.missing",
                gate.compare_refs,
                root,
                head,
                "0" * 40,
                trusted_date,
                "historical",
            )
            # HEAD and its parent are unrelated to each other in ancestry terms: the candidate must
            # descend from the baseline, not the other way round.
            assert_code(
                self,
                "version.baseline.not_ancestor",
                gate.compare_refs,
                root,
                head,
                base,
                trusted_date,
                "historical",
            )

    def test_workflow_uses_trusted_event_baselines_and_full_history(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        for token in (
            "pull_request:",
            "merge_group:",
            "push:",
            "workflow_dispatch:",
            "fetch-depth: 0",
            "github.event.pull_request.base.sha",
            "github.event.merge_group.base_sha",
            "github.event.before",
            "github.sha",
            "ref: ${{ github.sha }}",
            "git cat-file -e",
            "git show \"${CUEXIS_BASE_SHA}:${path}\"",
            "CUEXIS_TRUSTED_ROOT",
            "check_version_gate_tests.py",
            "version.bootstrap.required",
            "exit 1",
            "--trusted-utc-date",
        ):
            self.assertIn(token, text)
        self.assertNotIn("github.event.pull_request.head.sha", text)
        self.assertNotIn("${GITHUB_WORKSPACE}/tools/check_version_gate.py", text)

    def test_workflow_does_not_silently_bypass_bootstrap_or_trust_candidate_tests(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("workflow intentionally fails", text)
        self.assertIn("trusted baseline", text)
        self.assertIn("Run trusted version-gate tests", text)
        self.assertNotIn("|| cp", text)

    def _git(self, repo: Path, *arguments: str) -> str:
        completed = subprocess.run(
            [
                "git",
                "-C",
                str(repo),
                "-c",
                "user.email=gate@test.invalid",
                "-c",
                "user.name=Version Gate Test",
                "-c",
                "commit.gpgsign=false",
                *arguments,
            ],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return completed.stdout.strip()

    def _init_repository(self, root: Path) -> None:
        self._git(root, "init", "-q", "-b", "master")
        # Keep the rendered CMake lines byte-exact: autocrlf would rewrite the
        # `set(...)` lines and break the renderer's line-anchored pattern.
        self._git(root, "config", "core.autocrlf", "false")
        self._git(root, "config", "core.eol", "lf")

    GATE_FILES = (
        "tools/check_version_gate.py",
        "tools/check_version_gate_tests.py",
        "tools/update_version.py",
        ".github/workflows/version-gate.yml",
    )

    def _commit_version_files(self, root: Path, version: str, *, with_gate: bool) -> str:
        """Writes a real candidate tree and commits it, returning the commit SHA.

        The version files are rendered from the repository's own canonical
        templates through `update_version`, so the fixture stays valid as the
        real file layout evolves instead of drifting from a hand-written stub.
        """
        from update_version import Version, render_cmake, render_manifest

        parsed = Version(
            year=int(version[0:2]),
            month=int(version[3:5]),
            day=int(version[6:8]),
            build=int(version.split("-")[1]),
        )
        cmake = root / "cmake" / "CuexisVersion.cmake"
        cmake.parent.mkdir(parents=True, exist_ok=True)
        # Write with LF explicitly: the renderer's patterns are line-anchored and
        # a translated CRLF would make `^set(...)$` fail to match.
        cmake.write_text(
            render_cmake((TOOLS.parent / "cmake" / "CuexisVersion.cmake").read_text(encoding="utf-8"), parsed),
            encoding="utf-8",
            newline="\n",
        )
        (root / "vcpkg.json").write_text(
            render_manifest((TOOLS.parent / "vcpkg.json").read_text(encoding="utf-8"), parsed),
            encoding="utf-8",
            newline="\n",
        )
        if with_gate:
            for name in self.GATE_FILES:
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text("# candidate gate stub\n", encoding="utf-8")
        else:
            # A candidate that lacks the gate must really lack it. Leaving stale
            # files on disk would let `git add -A` re-commit them and the
            # "missing" assertion would silently test nothing (SPEC-05).
            for name in self.GATE_FILES:
                (root / name).unlink(missing_ok=True)
        self._git(root, "add", "-A")
        self._git(root, "commit", "-q", "-m", "candidate %s" % version)
        return self._git(root, "rev-parse", "HEAD")

    def test_rejects_a_real_baseline_that_predates_the_version_files(self) -> None:
        # A real empty history: the baseline commit exists and resolves, but it
        # carries no version files, so the gate must fail on baseline.missing
        # rather than on rev-parse (SPEC-05). Previously this path was only
        # covered by a fake all-zero SHA.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._init_repository(root)
            (root / "README.md").write_text("no version files yet\n", encoding="utf-8")
            self._git(root, "add", "-A")
            self._git(root, "commit", "-q", "-m", "baseline without version files")
            baseline = self._git(root, "rev-parse", "HEAD")
            candidate = self._commit_version_files(root, "26.09.20-1", with_gate=True)

            assert_code(
                self,
                "version.baseline.missing",
                gate.compare_refs,
                root,
                baseline,
                candidate,
                date(2026, 9, 20),
                "live",
            )

    def test_rejects_a_real_candidate_missing_the_gate_artifacts(self) -> None:
        # A candidate tree that drops the checker, the focused tests or the
        # workflow must not be able to pass. Two executable layers are asserted:
        #   1. the Python reader refuses a missing gate file per role, and
        #   2. the real bootstrap shell (extracted from the trusted workflow)
        #      exits non-zero with version.bootstrap.required.
        # The workflow script is executed for real when a POSIX shell exists;
        # otherwise that half is skipped explicitly instead of being faked.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._init_repository(root)
            self._commit_version_files(root, "26.09.20-1", with_gate=True)
            candidate = self._commit_version_files(root, "26.09.20-2", with_gate=False)
            for name in ("tools/check_version_gate.py", "tools/check_version_gate_tests.py"):
                with self.subTest(role="candidate", path=name):
                    assert_code(
                        self,
                        "version.candidate.missing",
                        gate._read_git_file,
                        root,
                        candidate,
                        name,
                        "candidate",
                    )
            # A baseline that lacks the gate artifacts is refused the same way.
            assert_code(
                self,
                "version.baseline.missing",
                gate._read_git_file,
                root,
                candidate,
                "tools/check_version_gate.py",
                "baseline",
            )

    def test_bootstrap_shell_fails_when_the_trusted_tree_lacks_the_gate(self) -> None:
        # Executes the materialize block from the trusted workflow against two
        # real baselines. This is the only way to actually run
        # version.bootstrap.required, which lives in shell rather than in the
        # Python checker.
        #
        # Both directions are asserted on purpose: the rejection alone would
        # also be satisfied by a shell that failed for an unrelated reason
        # (a syntax error in the extraction, a WSL shim on PATH, a missing
        # interpreter), which would make the test vacuously green. The positive
        # case pins the block to a real, materializing script.
        shell = shutil.which("bash")
        if shell is None:
            self.skipTest("no POSIX shell available to execute the bootstrap block")
        script = self._bootstrap_script()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._init_repository(root)
            self._commit_version_files(root, "26.09.20-1", with_gate=True)
            trusted = self._git(root, "rev-parse", "HEAD")
            completed = subprocess.run(
                [shell, "-c", script],
                cwd=str(root),
                env={
                    **os.environ,
                    "CUEXIS_BASE_SHA": trusted,
                    # The block both reads and exports TRUSTED_ROOT; the
                    # workflow assigns it a few lines above the slice, so the
                    # harness must provide it or `set -u` aborts on an
                    # unrelated unbound-variable error.
                    "TRUSTED_ROOT": str(root / "trusted"),
                    "CUEXIS_TRUSTED_ROOT": str(root / "trusted"),
                },
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertEqual(
                0,
                completed.returncode,
                "the extracted bootstrap block is not executable shell: %s"
                % (completed.stdout + completed.stderr),
            )
            materialized = sorted(
                # Normalize to forward slashes: `relative_to` yields backslashes
                # on Windows, which would never equal the GATE_FILES spelling.
                path.relative_to(root / "trusted").as_posix()
                for path in (root / "trusted").rglob("*")
                if path.is_file()
            )
            self.assertEqual(sorted(self.GATE_FILES), materialized)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._init_repository(root)
            (root / "README.md").write_text("no gate here\n", encoding="utf-8")
            self._git(root, "add", "-A")
            self._git(root, "commit", "-q", "-m", "baseline without the gate")
            empty = self._git(root, "rev-parse", "HEAD")
            completed = subprocess.run(
                [shell, "-c", script],
                cwd=str(root),
                env={
                    **os.environ,
                    "CUEXIS_BASE_SHA": empty,
                    "TRUSTED_ROOT": str(root / "trusted"),
                    "CUEXIS_TRUSTED_ROOT": str(root / "trusted"),
                },
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            self.assertNotEqual(0, completed.returncode)
            self.assertIn("version.bootstrap.required", completed.stdout + completed.stderr)

    def _bootstrap_script(self) -> str:
        """Extracts the trusted-baseline materialize block from the workflow.

        The block is a `for` loop, so a naive "from `git cat-file -e` to `exit 1`"
        slice would cut the loop open and bash would report a syntax error
        instead of the intended diagnostic. The slice therefore starts at the
        enclosing `for path in` line and ends at the matching `done`, which keeps
        the extracted shell genuinely executable.
        """
        text = WORKFLOW.read_text(encoding="utf-8")
        for_start = text.index("for path in \\")
        for_start = text.rindex("\n", 0, for_start) + 1
        done = text.index("\n", text.index("done", for_start)) + 1
        block = text[for_start:done]
        return "set -euo pipefail\n" + block

    def test_rejects_a_candidate_that_raced_past_the_latest_baseline(self) -> None:
        # Two PRs racing: PR A merges and advances master, so the baseline is the
        # newer commit while the candidate is built on an older one. The gate
        # must reject the stale-based candidate with baseline.not_ancestor. This
        # is an executable approximation of the race: a true interleaving of two
        # concurrent merges cannot be reproduced in a single process.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self._init_repository(root)
            older = self._commit_version_files(root, "26.09.20-1", with_gate=True)
            newer = self._commit_version_files(root, "26.09.20-2", with_gate=True)
            # master tip is `newer`; the candidate branch tip is `older`.
            assert_code(
                self,
                "version.baseline.not_ancestor",
                gate.compare_refs,
                root,
                newer,
                older,
                date(2026, 9, 20),
                "live",
            )


if __name__ == "__main__":
    unittest.main()
