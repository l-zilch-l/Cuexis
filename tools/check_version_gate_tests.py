"""Focused tests for the Stage 6 version comparison and trust contract."""

from __future__ import annotations

import ast
import json
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest
from datetime import date
from pathlib import Path
from unittest.mock import patch

import check_version_gate as gate
from update_version import Version


TOOLS = Path(__file__).resolve().parent
REPO_ROOT = Path(os.environ.get("GITHUB_WORKSPACE", TOOLS.parent))
TRUSTED_ROOT = Path(os.environ.get("CUEXIS_TRUSTED_ROOT", TOOLS.parent))
WORKFLOW = TRUSTED_ROOT / ".github" / "workflows" / "version-gate.yml"

# Git is not on PATH in every runner environment. The hosted MinGW job runs
# ctest through the MSYS2 shell (`MSYSTEM=MINGW64`), where the Windows Git
# installation is absent from PATH, so a bare `subprocess.run(["git", ...])`
# raises FileNotFoundError and the whole self-test dies for a reason unrelated
# to the gate. Resolve the executable explicitly instead of relying on PATH.
GIT_FALLBACKS = (
    Path(r"C:\Program Files\Git\cmd\git.exe"),
    Path(r"C:\Program Files\Git\bin\git.exe"),
    Path(r"C:\Program Files (x86)\Git\cmd\git.exe"),
)


def resolve_git() -> str | None:
    """Returns an executable git path, following PATH or a known install.

    A visible diagnostic on failure matters more than a clever search: if git
    genuinely cannot be found, the caller must be able to tell that apart from
    a version-gate rejection.
    """
    found = shutil.which("git")
    if found is not None:
        return found
    for candidate in GIT_FALLBACKS:
        if candidate.is_file():
            return str(candidate)
    return None


# A shell can answer `exit 0` and `git --version` while living in a different
# filesystem view. `C:\Windows\System32\bash.exe` is the WSL launcher: its
# filename contains no "wsl" and inside WSL `git` resolves, so a name-only check
# selects it even though it cannot read the Windows workspace the bootstrap
# block materializes into. The guard therefore has to test the invariant the
# block actually depends on.
SYSTEM_BASH = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "bash.exe"
SHELL_VIEW_MARKER = "cuexis-shell-view-probe"


def is_system_bash(shell: str) -> bool:
    """True when `shell` resolves to the Windows WSL launcher under System32."""
    try:
        return os.path.normcase(os.path.abspath(shell)) == os.path.normcase(
            os.path.abspath(SYSTEM_BASH)
        )
    except (OSError, ValueError):
        return False


def reads_marker_through_native_path(shell: str) -> bool:
    """True when `shell` can read a marker through a native absolute path.

    Git Bash and MSYS2 translate `C:\\...`-style paths and a POSIX shell on
    Linux reads the POSIX path unchanged, so both keep working. A shell in a
    different filesystem view cannot resolve the string at all, which is the
    case that must never be selected for the bootstrap block.
    """
    try:
        with tempfile.TemporaryDirectory() as directory:
            marker = Path(directory) / SHELL_VIEW_MARKER
            marker.write_text("ok", encoding="ascii")
            probe = subprocess.run(
                [shell, "-c", "test -f %s" % shlex.quote(str(marker))],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
            )
    except (OSError, subprocess.SubprocessError):
        return False
    return probe.returncode == 0


def usable_posix_shell() -> str | None:
    """Returns a shell that can actually execute the bootstrap block, or None.

    `shutil.which("bash")` is not enough on Windows: `C:\\Windows\\System32\\bash.exe`
    is the WSL launcher, which either fails outright or runs inside a different
    filesystem view. Either way the bootstrap block would be tested against the
    shim instead of the workflow. The only reliable check is to run something.

    The bootstrap block is extracted verbatim from the trusted workflow, and that
    script shells out to `git` by bare name. A shell whose PATH cannot resolve
    git (the hosted MinGW job runs ctest under the MSYS2 shell, where the Windows
    Git install under `C:\\Program Files\\Git` is absent from PATH) would make the
    positive case fail with a false "lacks <file>" and the negative case pass
    vacuously, so git must be resolvable by the same shell too.

    Non-UTF-8 bytes are tolerated here precisely because a broken shim emits
    localized text; the caller only needs to know whether the shell is usable.

    The last probe is the invariant the bootstrap block actually depends on: the
    shell must read a marker through the native absolute path spelling of the
    workspace. A shell in another filesystem view -- the WSL launcher reached
    through `C:\\Windows\\System32\\bash.exe` -- answers `exit 0` and even
    `git --version` but cannot resolve that path, so a name-only check would
    select it and turn the positive bootstrap case into a false "lacks <file>"
    failure.
    """
    shell = shutil.which("bash")
    if shell is None:
        return None
    try:
        probe = subprocess.run(
            [shell, "-c", "exit 0"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if probe.returncode != 0:
        return None
    # The WSL shim identifies itself in its banner; reject it even when the
    # probe happens to succeed on a machine with a working WSL distribution.
    # The launcher itself is named `bash.exe`, so the banner check alone never
    # matches it; the resolved System32 path is rejected as well.
    if "wsl" in Path(shell).name.lower() or is_system_bash(shell):
        return None
    try:
        git_probe = subprocess.run(
            [shell, "-c", "git --version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=30,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if git_probe.returncode != 0:
        return None
    if not reads_marker_through_native_path(shell):
        return None
    return shell


def captured(result: subprocess.CompletedProcess[str]) -> str:
    """Safely renders a completed process's streams for a failure message.

    `stdout`/`stderr` stay None when the process never started (for example a
    blacklisted or missing interpreter), so a naive concatenation raises
    TypeError and replaces the real diagnostic with a confusing one.
    """
    return (result.stdout or "") + (result.stderr or "")


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
            "pull_request_target:",
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
        self.assertIn("github.event.pull_request.head.sha", text)
        self.assertIn("Never check out or execute PR code", text)
        self.assertNotIn("${GITHUB_WORKSPACE}/tools/check_version_gate.py", text)

    def test_pre_merge_pr_trigger_and_job_mapping_stay_reachable(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        # The target event uses the base workflow. Removing pull_request from the
        # candidate before master has the target trigger leaves the PR with no run.
        events, jobs = text.split("\njobs:\n", 1)
        for event in ("pull_request", "pull_request_target"):
            # Extract the complete indented event body, rather than only its first line.
            block = re.search(r"(?m)^  " + event + r":\n((?:    .*\n)+)", events)
            self.assertIsNotNone(block)
            self.assertIn("- master", block.group(1))
            self.assertIn("synchronize", block.group(1))
        pre_merge = jobs.split("  post-merge-audit:\n", 1)[0]
        pr = "startsWith(github.event_name, 'pull_request')"
        self.assertIn("if: " + pr + " || github.event_name == 'merge_group'", pre_merge)
        self.assertIn("ref: ${{ " + pr + " && github.event.pull_request.base.sha || github.sha }}",
                      pre_merge)
        for variable, expression in (
            ("CUEXIS_BASE_SHA", "github.event.pull_request.base.sha || github.event.merge_group.base_sha"),
            ("CUEXIS_CANDIDATE_SHA", "github.event.pull_request.head.sha || github.sha"),
            ("CUEXIS_EVENT", "'pull_request' || github.event_name"),
        ):
            self.assertIn(variable + ": ${{ " + pr + " && " + expression + " }}", pre_merge)
        self.assertIn("git fetch --no-tags origin", pre_merge)
        self.assertNotIn("ref: ${{ github.event.pull_request.head.sha }}", pre_merge)

    def test_workflow_does_not_silently_bypass_bootstrap_or_trust_candidate_tests(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("workflow intentionally fails", text)
        self.assertIn("trusted baseline", text)
        self.assertIn("Run trusted version-gate tests", text)
        self.assertNotIn("|| cp", text)

    def test_git_is_never_invoked_by_bare_name(self) -> None:
        # Regression: the hosted MinGW job runs ctest under the MSYS2 shell
        # (`MSYSTEM=MINGW64`), where git is absent from PATH. A bare
        # `subprocess.run(["git", ...])` raised FileNotFoundError there and took
        # the whole self-test down with it. Every git invocation must go through
        # an explicitly resolved executable.
        #
        # The check inspects real argument lists rather than raw text, because
        # the modules legitimately quote the old broken call in prose comments.
        for module in ("check_version_gate.py", "check_version_gate_tests.py"):
            source = (TOOLS / module).read_text(encoding="utf-8")
            tree = ast.parse(source)
            for node in ast.walk(tree):
                if not isinstance(node, ast.List) or not node.elts:
                    continue
                first = node.elts[0]
                if not isinstance(first, ast.Constant) or not isinstance(first.value, str):
                    continue
                self.assertNotEqual(
                    "git",
                    first.value,
                    "%s passes the bare name 'git' as argv[0] at line %s; "
                    "resolve the executable instead" % (module, node.lineno),
                )
            self.assertIn(
                "resolve_git" if module.endswith("_tests.py") else "git_executable",
                source,
                "%s no longer resolves a git executable" % module,
            )

    def test_resolved_git_is_an_absolute_executable_when_available(self) -> None:
        # The resolver must never hand back a bare name, which would put the
        # PATH dependency straight back. When nothing resolves, it returns None
        # and the callers skip with a readable reason instead of crashing.
        executable = resolve_git()
        if executable is None:
            self.skipTest("no git executable available to resolve on this machine")
        self.assertTrue(Path(executable).is_absolute(), executable)
        self.assertTrue(Path(executable).is_file(), executable)
        completed = subprocess.run(
            [executable, "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
        )
        self.assertEqual(0, completed.returncode, captured(completed))
        self.assertIn("git version", completed.stdout)

    def test_usable_posix_shell_rejects_a_shell_that_cannot_run(self) -> None:
        # Regression: a bootstrap block executed through the WSL shim reported
        # success or failed for reasons unrelated to the workflow, and a naive
        # `completed.stdout + completed.stderr` then raised TypeError because
        # both streams are None when the process never started.
        shell = usable_posix_shell()
        if shell is None:
            self.skipTest("no usable POSIX shell available on this machine")
        self.assertTrue(Path(shell).is_absolute(), shell)
        self.assertNotIn("wsl", Path(shell).name.lower())
        completed = subprocess.run(
            [shell, "-c", "exit 0"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            errors="replace",
        )
        self.assertEqual(0, completed.returncode, captured(completed))

    def test_failure_messages_survive_a_process_that_never_started(self) -> None:
        # A blacklisted or missing interpreter leaves stdout and stderr as None.
        # The diagnostic helper must still produce a usable message rather than
        # raising TypeError and hiding the real failure.
        never_started = subprocess.CompletedProcess(args=["missing"], returncode=1)
        self.assertEqual("", captured(never_started))
        self.assertEqual(
            "out\nerr\n",
            captured(subprocess.CompletedProcess(args=["ok"], returncode=0, stdout="out\n", stderr="err\n")),
        )

    def _git(self, repo: Path, *arguments: str) -> str:
        executable = resolve_git()
        if executable is None:
            # Skipping is honest; a FileNotFoundError traceback would look like a
            # gate rejection and a silently passing test would be worse still.
            self.skipTest(
                "no git executable on PATH (%s) or in a known install location; "
                "the repository-building cases cannot run" % os.environ.get("PATH", "")
            )
        completed = subprocess.run(
            [
                executable,
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
        ".github/sdk-api-owners.json",
    )
    # The bootstrap block materializes the gate files plus the two canonical
    # templates it renders its fixtures from, so the materialize assertion must
    # not reuse GATE_FILES. Adding the templates to GATE_FILES would instead make
    # the with_gate stub loop overwrite the real version files and the
    # without_gate branch delete them, which is exactly the fixture the version
    # tests depend on.
    MATERIALIZED_FILES = GATE_FILES + (
        "cmake/CuexisVersion.cmake",
        "vcpkg.json",
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
        shell = usable_posix_shell()
        if shell is None:
            self.skipTest(
                "no usable POSIX shell available to execute the bootstrap block; "
                "`bash` is absent, resolves to the WSL shim, its PATH cannot "
                "resolve `git` (the block shells out to git by bare name), or it "
                "cannot read a marker through the native workspace path"
            )
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
                # A shell started outside MSYS may emit localized bytes; an
                # undecodable stream must not turn into a decode traceback that
                # hides which step actually failed.
                errors="replace",
            )
            self.assertEqual(
                0,
                completed.returncode,
                "the extracted bootstrap block is not executable shell: %s"
                % captured(completed),
            )
            materialized = sorted(
                # Normalize to forward slashes: `relative_to` yields backslashes
                # on Windows, which would never equal the GATE_FILES spelling.
                path.relative_to(root / "trusted").as_posix()
                for path in (root / "trusted").rglob("*")
                if path.is_file()
            )
            self.assertEqual(sorted(self.MATERIALIZED_FILES), materialized)
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
                errors="replace",
            )
            self.assertNotEqual(0, completed.returncode)
            self.assertIn("version.bootstrap.required", captured(completed))

    def test_rejects_a_shell_that_cannot_read_the_workspace_path(self) -> None:
        # A shell can answer `exit 0` and `git --version` while living in a
        # different filesystem view: that is exactly how the WSL launcher
        # (`C:\Windows\System32\bash.exe`, a filename that contains no "wsl")
        # slips past a name-only check. If such a shell were selected, the
        # positive bootstrap case would fail with a false "lacks <file>" and
        # the negative case could pass vacuously, so it must be rejected.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake = root / ("bash.cmd" if os.name == "nt" else "bash")
            if os.name == "nt":
                # `findstr` is addressed through %SystemRoot% on purpose: this
                # stand-in must not depend on the caller's PATH resolving any
                # external tool, or a trimmed PATH would turn the marker probe
                # into a false pass and the test into a false failure.
                content = (
                    "@echo off\r\n"
                    'echo %* | "%SystemRoot%\\System32\\findstr.exe" /C:"MARKER" >nul\r\n'
                    "if %errorlevel%==0 exit /b 1\r\n"
                    "exit /b 0\r\n"
                )
            else:
                content = (
                    "#!/bin/sh\n"
                    'case "$*" in\n'
                    "  *MARKER*) exit 1 ;;\n"
                    "  *) exit 0 ;;\n"
                    "esac\n"
                )
            fake.write_text(content.replace("MARKER", SHELL_VIEW_MARKER), encoding="ascii")
            if os.name != "nt":
                fake.chmod(0o755)

            original = os.environ.get("PATH", "")
            os.environ["PATH"] = str(root) + os.pathsep + original
            try:
                resolved = shutil.which("bash")
                if resolved is None or os.path.normcase(str(resolved)) != os.path.normcase(
                    str(fake)
                ):
                    self.skipTest(
                        "this platform cannot resolve a `bash` stand-in on PATH, so the "
                        "view-mismatch rejection cannot be exercised here"
                    )
                # The stand-in passes the trivial probe the guard used to rely on.
                trivial = subprocess.run(
                    [str(fake), "-c", "exit 0"],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    timeout=30,
                )
                self.assertEqual(0, trivial.returncode)
                # It cannot read the marker through the native path, so it must
                # never be selected for the bootstrap block.
                self.assertFalse(reads_marker_through_native_path(str(fake)))
                self.assertIsNone(usable_posix_shell())
            finally:
                os.environ["PATH"] = original

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


class SdkOwnerApprovalTests(unittest.TestCase):
    def test_real_git_tree_and_api_owner_record(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output([gate.git_executable(), "-C", str(root), *args]).decode().strip()
            git("init", "-q")
            git("config", "user.name", "Gate Test")
            git("config", "user.email", "gate@example.invalid")
            (root / "cmake").mkdir()
            (root / ".github").mkdir()
            version = (TOOLS.parent / "cmake/CuexisVersion.cmake").read_text()
            version = re.sub(r'set\(CUEXIS_SDK_API_VERSION "[^"]+"\)', 'set(CUEXIS_SDK_API_VERSION "0.7.0")', version)
            (root / "cmake/CuexisVersion.cmake").write_text(version)
            (root / "vcpkg.json").write_text((TOOLS.parent / "vcpkg.json").read_text())
            (root / gate.SDK_OWNERS_FILE).write_text(json.dumps({"format": "cuexis.sdk-api-owners",
                "version": 1, "owners": ["l-zilch-l"], "repository": "l-zilch-l/Cuexis"}))
            git("add", ".")
            git("commit", "-qm", "trusted baseline")
            base = git("rev-parse", "HEAD")
            (root / "cmake/CuexisVersion.cmake").write_text(version.replace('"0.7.0"', '"0.7.1"'))
            git("add", ".")
            git("commit", "-qm", "candidate")
            candidate = git("rev-parse", "HEAD")
            expected, comment = self.record()
            expected.update(base_sha=base, candidate_sha=candidate,
                            candidate_tree_sha=git("rev-parse", "HEAD^{tree}"))
            comment["body"] = gate.SDK_APPROVAL_PREFIX + json.dumps(expected)
            def api(repository, suffix):
                return {"head": {"sha": candidate}} if suffix == "pulls/32" else [comment]
            with patch.object(gate, "_github_json", side_effect=api), patch.dict(os.environ, {"GITHUB_REPOSITORY": "l-zilch-l/Cuexis"}):
                self.assertEqual(gate.sdk_owner_approval(root, base, candidate, date(2026, 10, 6), 32,
                                                         "pull_request"), 123)
                git("branch", "topic", candidate)
                git("checkout", "-q", "-B", "master", base)
                git("merge", "--squash", "topic")
                git("commit", "-qm", "squash candidate")
                merged = git("rev-parse", "HEAD")
                git("branch", "-D", "topic")
                fresh = root / "fresh"
                subprocess.check_call([gate.git_executable(), "clone", "-q", "--no-local",
                                       str(root), str(fresh)])
                self.assertNotEqual(gate._run_git(fresh, "cat-file", "-e",
                                                  candidate + "^{commit}").returncode, 0)
                original_run = gate._run_git
                def fetching(repo, *arguments):
                    if arguments and arguments[0] == "fetch":
                        self.assertEqual(arguments[-2], "https://github.com/l-zilch-l/Cuexis.git")
                        self.assertEqual(arguments[-1], candidate)
                        return original_run(repo, *arguments[:-2], str(root), candidate)
                    return original_run(repo, *arguments)
                with patch.object(gate, "_run_git", side_effect=fetching):
                    self.assertEqual(gate.sdk_owner_approval(fresh, base, merged,
                        date(2026, 10, 6), 32, "push"), 123)
                comment["user"]["login"] = "intruder"
                with self.assertRaises(gate.GateError):
                    gate.sdk_owner_approval(root, base, candidate, date(2026, 10, 6), 32, "pull_request")

    def record(self):
        expected = {"repository": "l-zilch-l/Cuexis", "pr": 32, "base_sha": "a" * 40,
                    "candidate_sha": "b" * 40, "candidate_tree_sha": "c" * 40,
                    "from": "0.7.0", "to": "0.7.1", "utc_date": "2026-10-06"}
        comment = {"id": 123, "user": {"login": "l-zilch-l", "type": "User"},
                   "created_at": "2026-10-06T12:00:00Z", "updated_at": "2026-10-06T12:00:00Z",
                   "body": gate.SDK_APPROVAL_PREFIX + json.dumps(expected)}
        return expected, comment

    def test_exact_owner_record_and_merge_tree(self):
        expected, comment = self.record()
        self.assertEqual(gate.validate_sdk_approval(comment, ["l-zilch-l"], expected,
                                                   "pull_request", "c" * 40), 123)
        merged = dict(expected, candidate_sha="d" * 40)
        self.assertEqual(gate.validate_sdk_approval(comment, ["l-zilch-l"], merged,
                                                   "push", "c" * 40), 123)
        with self.assertRaises(gate.GateError):
            gate.validate_sdk_approval(comment, ["l-zilch-l"], merged, "push", "e" * 40)

    def test_stale_sha_base_sdk_date_or_actor_never_grants_permission(self):
        for field, value in (("candidate_sha", "e" * 40), ("base_sha", "f" * 40),
                             ("candidate_tree_sha", "e" * 40), ("to", "0.8.0"),
                             ("utc_date", "2026-10-07"), ("pr", True)):
            with self.subTest(field=field):
                expected, comment = self.record()
                wrong = dict(expected, **{field: value})
                comment["body"] = gate.SDK_APPROVAL_PREFIX + json.dumps(wrong)
                with self.assertRaises(gate.GateError):
                    gate.validate_sdk_approval(comment, ["l-zilch-l"], expected,
                                               "pull_request", "c" * 40)
        for user in ({"login": "intruder", "type": "User"},
                     {"login": "l-zilch-l", "type": "Bot"}):
            expected, comment = self.record()
            comment["user"] = user
            with self.assertRaises(gate.GateError):
                gate.validate_sdk_approval(comment, ["l-zilch-l"], expected,
                                           "pull_request", "c" * 40)
        expected, comment = self.record()
        comment["updated_at"] = "2026-10-06T13:00:00Z"
        with self.assertRaises(gate.GateError):
            gate.validate_sdk_approval(comment, ["l-zilch-l"], expected,
                                       "pull_request", "c" * 40)

    def test_cli_boolean_cannot_grant_authorization(self):
        self.assertEqual(gate.main(["--check-current", "--allow-sdk-api-change"]), 1)


if __name__ == "__main__":
    unittest.main()
