"""Focused tests for the Stage 6 version comparison and trust contract."""

from __future__ import annotations

import json
import os
import subprocess
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
        result = gate.compare_snapshots(
            snapshot("26.08.01-1"),
            snapshot("26.08.02-1"),
            date(2026, 8, 2),
            "historical",
        )
        self.assertEqual("historical", result.context)

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
        head = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        ).stdout.strip()
        parent = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD^"],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        ).stdout.strip()
        trusted_date = date(2026, 9, 20)

        assert_code(
            self,
            "version.baseline.ref.invalid",
            gate.compare_refs,
            REPO_ROOT,
            "HEAD",
            head,
            trusted_date,
            "historical",
        )
        assert_code(
            self,
            "version.baseline.missing",
            gate.compare_refs,
            REPO_ROOT,
            "0" * 40,
            head,
            trusted_date,
            "historical",
        )
        assert_code(
            self,
            "version.candidate.missing",
            gate.compare_refs,
            REPO_ROOT,
            head,
            "0" * 40,
            trusted_date,
            "historical",
        )
        assert_code(
            self,
            "version.baseline.not_ancestor",
            gate.compare_refs,
            REPO_ROOT,
            head,
            parent,
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


if __name__ == "__main__":
    unittest.main()
