"""Check Cuexis date-version advancement against a trusted Git baseline."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from datetime import date
from pathlib import Path

from update_version import Version, parse_version, version_from_cmake, version_from_manifest


VERSION_FILES = ("cmake/CuexisVersion.cmake", "vcpkg.json")
FULL_SHA_RE = re.compile(r"^[0-9a-fA-F]{40}$")
SDK_API_RE = re.compile(
    r"^set\(CUEXIS_SDK_API_VERSION \"(?P<version>[0-9]+\.[0-9]+\.[0-9]+)\"\)$",
    re.MULTILINE,
)
DATE_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")


class GateError(RuntimeError):
    """A stable, user-facing version gate failure."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message


@dataclass(frozen=True)
class VersionSnapshot:
    ref: str
    version: Version
    sdk_api_version: str


@dataclass(frozen=True)
class GateResult:
    base_ref: str
    candidate_ref: str
    base_version: str
    candidate_version: str
    base_sdk_api_version: str
    candidate_sdk_api_version: str
    trusted_utc_date: str
    context: str
    sdk_api_change_explicitly_allowed: bool


def parse_trusted_date(value: str) -> date:
    if DATE_RE.fullmatch(value) is None:
        raise GateError(
            "version.trusted_date.invalid",
            "trusted UTC date must use YYYY-MM-DD",
        )
    try:
        return date.fromisoformat(value)
    except ValueError as error:
        raise GateError("version.trusted_date.invalid", str(error)) from error


def _sdk_api_version(cmake_text: str, ref: str) -> str:
    matches = SDK_API_RE.findall(cmake_text)
    if len(matches) != 1:
        raise GateError(
            "version.sdk_api.invalid",
            f"{ref}: expected exactly one CUEXIS_SDK_API_VERSION assignment",
        )
    return matches[0]


def snapshot_from_texts(ref: str, cmake_text: str, manifest_text: str) -> VersionSnapshot:
    try:
        cmake_version = version_from_cmake(cmake_text)
    except (UnicodeError, ValueError) as error:
        raise GateError("version.cmake.invalid", f"{ref}: {error}") from error

    try:
        manifest_version = version_from_manifest(manifest_text)
    except (UnicodeError, ValueError, json.JSONDecodeError) as error:
        raise GateError("version.manifest.invalid", f"{ref}: {error}") from error

    if cmake_version != manifest_version:
        raise GateError(
            "version.manifest.mismatch",
            f"{ref}: CMake is {cmake_version.canonical}, manifest is {manifest_version.canonical}",
        )

    return VersionSnapshot(ref, cmake_version, _sdk_api_version(cmake_text, ref))


def compare_snapshots(
    base: VersionSnapshot,
    candidate: VersionSnapshot,
    trusted_utc_date: date,
    context: str,
    *,
    allow_sdk_api_change: bool = False,
) -> GateResult:
    if context not in {"live", "historical"}:
        raise GateError("version.context.invalid", f"unsupported context: {context}")

    base_date = date(2000 + base.version.year, base.version.month, base.version.day)
    candidate_date = date(2000 + candidate.version.year, candidate.version.month, candidate.version.day)

    if base_date > trusted_utc_date:
        raise GateError(
            "version.baseline.future",
            f"baseline date {base.version.canonical} is after trusted UTC date {trusted_utc_date.isoformat()}",
        )
    if candidate_date < base_date:
        raise GateError(
            "version.date.backward",
            f"candidate {candidate.version.canonical} is before baseline {base.version.canonical}",
        )
    if candidate_date > trusted_utc_date:
        raise GateError(
            "version.release_date.future",
            f"candidate date {candidate.version.canonical} is after trusted UTC date {trusted_utc_date.isoformat()}",
        )
    if candidate_date < trusted_utc_date:
        raise GateError(
            "version.release_date.stale",
            f"candidate date {candidate.version.canonical} is before trusted UTC date {trusted_utc_date.isoformat()}",
        )

    if candidate_date == base_date:
        expected_build = base.version.build + 1
        if expected_build > 2_147_483_647:
            raise GateError("version.build.exhausted", "same-day build number cannot be incremented")
        if candidate.version.build == base.version.build:
            raise GateError("version.unchanged", "candidate version is unchanged from the baseline")
        if candidate.version.build < expected_build:
            raise GateError(
                "version.build.backward",
                f"same-day candidate build must be {expected_build}, found {candidate.version.build}",
            )
        if candidate.version.build > expected_build:
            raise GateError(
                "version.build.skipped",
                f"same-day candidate build must be {expected_build}, found {candidate.version.build}",
            )
    elif candidate.version.build != 1:
        raise GateError(
            "version.cross_day_build.invalid",
            f"date-forward candidate {candidate.version.canonical} must use build 1",
        )

    sdk_change = base.sdk_api_version != candidate.sdk_api_version
    if sdk_change and not allow_sdk_api_change:
        raise GateError(
            "version.sdk_api.changed",
            f"SDK API changed from {base.sdk_api_version} to {candidate.sdk_api_version} without explicit acceptance",
        )

    return GateResult(
        base.ref,
        candidate.ref,
        base.version.canonical,
        candidate.version.canonical,
        base.sdk_api_version,
        candidate.sdk_api_version,
        trusted_utc_date.isoformat(),
        context,
        allow_sdk_api_change,
    )


def _run_git(repo_root: Path, *arguments: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["git", "-C", str(repo_root), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def _resolve_commit(repo_root: Path, ref: str, role: str) -> str:
    if FULL_SHA_RE.fullmatch(ref) is None:
        raise GateError(
            f"version.{role}.ref.invalid",
            f"{role} ref must be a full Git commit SHA, found {ref!r}",
        )
    result = _run_git(repo_root, "rev-parse", "--verify", "--end-of-options", f"{ref}^{{commit}}")
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise GateError(
            f"version.{role}.missing",
            f"cannot resolve {ref}: {detail or 'commit is unavailable'}",
        )
    resolved = result.stdout.decode("ascii", errors="replace").strip()
    if resolved.lower() != ref.lower():
        raise GateError(
            f"version.{role}.ref.invalid",
            f"Git resolved {ref} to unexpected commit {resolved}",
        )
    return resolved


def _require_ancestor(repo_root: Path, base_sha: str, candidate_sha: str) -> None:
    result = _run_git(repo_root, "merge-base", "--is-ancestor", base_sha, candidate_sha)
    if result.returncode == 0:
        return
    if result.returncode == 1:
        raise GateError(
            "version.baseline.not_ancestor",
            f"candidate {candidate_sha} is not based on trusted baseline {base_sha}",
        )
    detail = result.stderr.decode("utf-8", errors="replace").strip()
    raise GateError("version.git.error", detail or "git merge-base failed")


def _read_git_file(repo_root: Path, commit_sha: str, path: str, role: str) -> str:
    result = _run_git(repo_root, "show", f"{commit_sha}:{path}")
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise GateError(
            f"version.{role}.missing",
            f"{path} is unavailable at {commit_sha}: {detail or 'file is missing'}",
        )
    try:
        return result.stdout.decode("utf-8")
    except UnicodeDecodeError as error:
        raise GateError(f"version.{role}.invalid", f"{path} is not UTF-8") from error


def compare_refs(
    repo_root: Path,
    base_ref: str,
    candidate_ref: str,
    trusted_utc_date: date,
    context: str,
    *,
    allow_sdk_api_change: bool = False,
) -> GateResult:
    repo_root = repo_root.resolve()
    base_sha = _resolve_commit(repo_root, base_ref, "baseline")
    candidate_sha = _resolve_commit(repo_root, candidate_ref, "candidate")
    _require_ancestor(repo_root, base_sha, candidate_sha)

    base_snapshot = snapshot_from_texts(
        base_sha,
        _read_git_file(repo_root, base_sha, VERSION_FILES[0], "baseline"),
        _read_git_file(repo_root, base_sha, VERSION_FILES[1], "baseline"),
    )
    candidate_snapshot = snapshot_from_texts(
        candidate_sha,
        _read_git_file(repo_root, candidate_sha, VERSION_FILES[0], "candidate"),
        _read_git_file(repo_root, candidate_sha, VERSION_FILES[1], "candidate"),
    )
    return compare_snapshots(
        base_snapshot,
        candidate_snapshot,
        trusted_utc_date,
        context,
        allow_sdk_api_change=allow_sdk_api_change,
    )


def current_snapshot(repo_root: Path) -> VersionSnapshot:
    repo_root = repo_root.resolve()
    try:
        cmake_text = (repo_root / VERSION_FILES[0]).read_text(encoding="utf-8")
        manifest_text = (repo_root / VERSION_FILES[1]).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        raise GateError("version.current.missing", str(error)) from error
    return snapshot_from_texts("working-tree", cmake_text, manifest_text)


def _result_json(result: GateResult) -> str:
    value = asdict(result)
    value["base_version"] = result.base_version
    value["candidate_version"] = result.candidate_version
    return json.dumps(value, sort_keys=True)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Check Cuexis date-version advancement against a trusted Git baseline."
    )
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check-current", action="store_true", help="check CMake and manifest consistency")
    parser.add_argument("--base-ref", help="trusted baseline commit SHA")
    parser.add_argument("--candidate-ref", help="candidate commit SHA")
    parser.add_argument("--trusted-utc-date", help="trusted release context as YYYY-MM-DD")
    parser.add_argument("--context", choices=("live", "historical"), default="live")
    parser.add_argument("--event", default="local")
    parser.add_argument("--allow-sdk-api-change", action="store_true")
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser


def main(arguments: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(arguments)

    try:
        if args.check_current:
            if any(value is not None for value in (args.base_ref, args.candidate_ref, args.trusted_utc_date)):
                parser.error("--check-current cannot be combined with baseline comparison arguments")
            snapshot = current_snapshot(args.repo_root)
            if args.json_output:
                print(json.dumps({"version": snapshot.version.canonical, "sdk_api_version": snapshot.sdk_api_version}))
            else:
                print(
                    f"version.current.pass: version={snapshot.version.canonical} "
                    f"sdk_api={snapshot.sdk_api_version}"
                )
            return 0

        if args.base_ref is None or args.candidate_ref is None or args.trusted_utc_date is None:
            parser.error("baseline comparison requires --base-ref, --candidate-ref and --trusted-utc-date")
        trusted_date = parse_trusted_date(args.trusted_utc_date)
        result = compare_refs(
            args.repo_root,
            args.base_ref,
            args.candidate_ref,
            trusted_date,
            args.context,
            allow_sdk_api_change=args.allow_sdk_api_change,
        )
        if args.json_output:
            print(_result_json(result))
        else:
            print(
                f"version.gate.pass: event={args.event} context={result.context} "
                f"base={result.base_ref}({result.base_version}) "
                f"candidate={result.candidate_ref}({result.candidate_version}) "
                f"trusted_utc_date={result.trusted_utc_date}"
            )
        return 0
    except GateError as error:
        print(f"Version gate failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
