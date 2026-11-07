"""Check Cuexis date-version advancement against a trusted Git baseline."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
from dataclasses import asdict, dataclass
from datetime import date
from pathlib import Path

from update_version import Version, parse_version, version_from_cmake, version_from_manifest


# Git is not on PATH in every runner environment. The hosted MinGW job runs
# ctest through the MSYS2 shell (`MSYSTEM=MINGW64`), where the Windows Git
# installation is absent from PATH, so a bare `subprocess.run(["git", ...])`
# raises FileNotFoundError instead of reporting a gate diagnostic. Resolve the
# executable explicitly so a missing git surfaces as `version.git.missing`
# rather than as an unhandled traceback.
GIT_FALLBACKS = (
    Path(r"C:\Program Files\Git\cmd\git.exe"),
    Path(r"C:\Program Files\Git\bin\git.exe"),
    Path(r"C:\Program Files (x86)\Git\cmd\git.exe"),
)


def git_executable() -> str:
    """Returns an executable git path, following PATH or a known install."""
    found = shutil.which("git")
    if found is not None:
        return found
    for candidate in GIT_FALLBACKS:
        if candidate.is_file():
            return str(candidate)
    raise GateError(
        "version.git.missing",
        "no git executable on PATH or in a known install location; "
        "the version gate cannot inspect repository history",
    )


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

    # Two hard gates that hold in both contexts: the baseline may not be dated
    # after the trusted day, and the candidate may never move backwards past the
    # baseline.
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

    # The context selects which release rule is authoritative. `live` judges the
    # candidate against the trusted UTC date supplied by the protected workflow.
    # `historical` re-verifies an already-recorded SHA against its recorded
    # release context: the candidate's date is not required to equal the current
    # day, but it must still be at or after the recorded baseline and at or
    # before the trusted upper bound (ADR 0042 S6-D07: historical SHAs are
    # re-verified with their recorded release context, not rewritten because the
    # re-run happens on another day). `live` keeps the strict same-day rule.
    if context == "live":
        if candidate_date > trusted_utc_date:
            raise GateError(
                "version.release_date.future",
                f"candidate date {candidate.version.canonical} is after trusted UTC date "
                f"{trusted_utc_date.isoformat()}",
            )
        if candidate_date < trusted_utc_date:
            raise GateError(
                "version.release_date.stale",
                f"candidate date {candidate.version.canonical} is before trusted UTC date "
                f"{trusted_utc_date.isoformat()}",
            )
    else:
        # historical: the trusted date is the recorded release upper bound. The
        # candidate may precede it (it was recorded earlier), but it may not be
        # after it, so a historical re-run cannot be used to smuggle in a
        # candidate dated after the recorded day.
        if candidate_date > trusted_utc_date:
            raise GateError(
                "version.release_date.future",
                f"candidate date {candidate.version.canonical} is after the recorded UTC date "
                f"{trusted_utc_date.isoformat()} (historical)",
            )

    if candidate_date == base_date:
        # Report "not advanced" before "exhausted": when the candidate simply
        # repeats the baseline build the real reason is that nothing moved, not
        # that the same-day build number ran out. Only a candidate that does
        # advance to an unrepresentable build is exhausted.
        if candidate.version.build == base.version.build:
            raise GateError("version.unchanged", "candidate version is unchanged from the baseline")
        expected_build = base.version.build + 1
        if expected_build > 2_147_483_647:
            raise GateError("version.build.exhausted", "same-day build number cannot be incremented")
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
        [git_executable(), "-C", str(repo_root), *arguments],
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


SDK_APPROVAL_PREFIX = "cuexis-sdk-api-approval-v1\n"
SDK_OWNERS_FILE = ".github/sdk-api-owners.json"


def _sdk_approval_body(comment: dict) -> str:
    """Accept LF and CRLF records without changing the API-observed comment."""
    body = comment.get("body", "")
    return body.replace("\r\n", "\n") if isinstance(body, str) else ""


def _github_json(repository: str, suffix: str):
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise GateError("version.sdk_api.owner_config", "invalid repository identity")
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "Cuexis-Version-Gate"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(f"https://api.github.com/repos/{repository}/{suffix}",
                                     headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            data = response.read(8 * 1024 * 1024 + 1)
        if len(data) > 8 * 1024 * 1024:
            raise ValueError("GitHub response exceeds bound")
        return json.loads(data)
    except (OSError, ValueError) as error:
        raise GateError("version.sdk_api.approval_unavailable", str(error)) from error


def validate_sdk_approval(comment: dict, owners: list[str], expected: dict,
                          event: str, approved_tree: str) -> int:
    """Validate an API-observed owner record, never a candidate-controlled JSON file."""
    actor = comment.get("user") if isinstance(comment, dict) else None
    if (not isinstance(actor, dict) or actor.get("login") not in owners or
            actor.get("type") != "User" or
            comment.get("created_at") != comment.get("updated_at") or
            str(comment.get("created_at", ""))[:10] != expected["utc_date"]):
        raise GateError("version.sdk_api.approval_invalid", "owner/date/unedited record required")
    body = comment.get("body", "")
    if not isinstance(body, str) or len(body) > 8192:
        raise GateError("version.sdk_api.approval_invalid", "invalid approval record")
    body = _sdk_approval_body(comment)
    if not body.startswith(SDK_APPROVAL_PREFIX):
        raise GateError("version.sdk_api.approval_invalid", "invalid approval record")
    try:
        record = json.loads(body[len(SDK_APPROVAL_PREFIX):])
    except ValueError as error:
        raise GateError("version.sdk_api.approval_invalid", "invalid approval JSON") from error
    if (not isinstance(record, dict) or set(record) != set(expected) or
            any(type(record[key]) is not type(value) for key, value in expected.items())):
        raise GateError("version.sdk_api.approval_invalid", "exact approval fields required")
    if not re.fullmatch(r"[0-9a-f]{40}", str(record["candidate_sha"])):
        raise GateError("version.sdk_api.approval_invalid", "full approved candidate SHA required")
    checks = dict(expected)
    if event in ("push", "merge_group"):
        # A merge or squash may have a different commit SHA; its entire tree must be identical.
        checks["candidate_sha"] = record["candidate_sha"]
    if record != checks or approved_tree != expected["candidate_tree_sha"]:
        raise GateError("version.sdk_api.approval_mismatch", "approval does not bind this change")
    comment_id = comment.get("id")
    if not isinstance(comment_id, int) or isinstance(comment_id, bool) or comment_id <= 0:
        raise GateError("version.sdk_api.approval_invalid", "GitHub comment identity required")
    return comment_id


def sdk_owner_approval(repo_root: Path, base: str, candidate: str,
                       trusted_date: date, pr: int, event: str) -> int | None:
    base = _resolve_commit(repo_root, base, "baseline")
    candidate = _resolve_commit(repo_root, candidate, "candidate")
    _require_ancestor(repo_root, base, candidate)
    from_snapshot = snapshot_from_texts(base,
        _read_git_file(repo_root, base, VERSION_FILES[0], "baseline"),
        _read_git_file(repo_root, base, VERSION_FILES[1], "baseline"))
    to_snapshot = snapshot_from_texts(candidate,
        _read_git_file(repo_root, candidate, VERSION_FILES[0], "candidate"),
        _read_git_file(repo_root, candidate, VERSION_FILES[1], "candidate"))
    if from_snapshot.sdk_api_version == to_snapshot.sdk_api_version:
        return None
    try:
        config = json.loads(_read_git_file(repo_root, base, SDK_OWNERS_FILE, "sdk_owner_config"))
    except ValueError as error:
        raise GateError("version.sdk_api.owner_config", "invalid trusted owner config") from error
    if (not isinstance(config, dict) or config.get("format") != "cuexis.sdk-api-owners" or
            type(config.get("version")) is not int or config.get("version") != 1 or not isinstance(config.get("owners"), list) or
            not config["owners"] or not all(isinstance(x, str) for x in config["owners"]) or
            not isinstance(config.get("repository"), str)):
        raise GateError("version.sdk_api.owner_config", "invalid trusted owners")
    repository = config["repository"]
    if os.environ.get("GITHUB_REPOSITORY", repository) != repository:
        raise GateError("version.sdk_api.owner_config", "workflow repository differs from trusted owner config")
    if pr == 0:
        if event == "merge_group":
            pulls = _github_json(repository, "pulls?state=open&base=master&per_page=100")
            if not isinstance(pulls, list) or len(pulls) >= 100:
                raise GateError("version.sdk_api.approval_unavailable", "queue PR lookup exceeds bound")
            matches = [item for item in pulls
                       if FULL_SHA_RE.fullmatch(str(item.get("head", {}).get("sha", "")))
                       and _run_git(repo_root, "merge-base", "--is-ancestor",
                                    item["head"]["sha"], candidate).returncode == 0]
        else:
            pulls = _github_json(repository, f"commits/{candidate}/pulls")
            if not isinstance(pulls, list):
                raise GateError("version.sdk_api.approval_unavailable", "invalid associated PR list")
            matches = [item for item in pulls if item.get("base", {}).get("ref") == "master"]
        if len(matches) != 1:
            raise GateError("version.sdk_api.approval_invalid", "one associated master PR required")
        pr = matches[0]["number"]
    if pr <= 0:
        raise GateError("version.sdk_api.approval_invalid", "positive PR number required")
    pull = _github_json(repository, f"pulls/{pr}")
    if not isinstance(pull, dict):
        raise GateError("version.sdk_api.approval_unavailable", "invalid PR response")
    head = pull.get("head", {}).get("sha")
    if event == "pull_request" and head != candidate:
        raise GateError("version.sdk_api.approval_mismatch", "PR head differs from candidate")
    if event == "merge_group":
        head = _resolve_commit(repo_root, head or "", "pr_head")
        _require_ancestor(repo_root, head, candidate)
    latest = None
    for page in range(1, 21):
        comments = _github_json(repository, f"issues/{pr}/comments?per_page=100&page={page}")
        if not isinstance(comments, list):
            raise GateError("version.sdk_api.approval_unavailable", "invalid comment list")
        for comment in comments:
            if (isinstance(comment, dict) and isinstance(comment.get("user"), dict) and
                    comment["user"].get("login") in config["owners"] and
                    _sdk_approval_body(comment).startswith(SDK_APPROVAL_PREFIX)):
                latest = comment
        if len(comments) < 100:
            break
    else:
        raise GateError("version.sdk_api.approval_unavailable", "approval history exceeds bound")
    if latest is None:
        raise GateError("version.sdk_api.approval_required", "no unedited owner approval record")
    try:
        record = json.loads(_sdk_approval_body(latest)[len(SDK_APPROVAL_PREFIX):])
        approved = record["candidate_sha"]
    except (ValueError, KeyError, TypeError) as error:
        raise GateError("version.sdk_api.approval_invalid", "latest record is invalid or revoked") from error
    if not isinstance(approved, str) or FULL_SHA_RE.fullmatch(approved) is None:
        raise GateError("version.sdk_api.approval_invalid", "full approved SHA required")
    if _run_git(repo_root, "cat-file", "-e", f"{approved}^{{commit}}").returncode != 0:
        # A squash followed by branch deletion can remove the approved head from a fresh clone.
        # Fetch its strictly validated object from the trusted repository as data only.
        fetched = _run_git(repo_root, "fetch", "--no-tags", "--no-recurse-submodules",
                           f"https://github.com/{repository}.git", approved)
        if fetched.returncode != 0:
            raise GateError("version.sdk_api.approval_unavailable", "approved commit unavailable")
    approved = _resolve_commit(repo_root, approved, "approved_candidate")
    _require_ancestor(repo_root, base, approved)
    if event == "merge_group":
        _require_ancestor(repo_root, approved, candidate)
    from_snapshot = snapshot_from_texts(base,
        _read_git_file(repo_root, base, VERSION_FILES[0], "baseline"),
        _read_git_file(repo_root, base, VERSION_FILES[1], "baseline"))
    to_snapshot = snapshot_from_texts(candidate,
        _read_git_file(repo_root, candidate, VERSION_FILES[0], "candidate"),
        _read_git_file(repo_root, candidate, VERSION_FILES[1], "candidate"))
    expected = {"repository": repository, "pr": pr, "base_sha": base,
                "candidate_sha": candidate,
                "candidate_tree_sha": _run_git(repo_root, "rev-parse", f"{candidate}^{{tree}}").stdout.decode("ascii").strip(),
                "from": from_snapshot.sdk_api_version, "to": to_snapshot.sdk_api_version,
                "utc_date": trusted_date.isoformat()}
    return validate_sdk_approval(latest, config["owners"], expected, event,
                                _run_git(repo_root, "rev-parse", f"{approved}^{{tree}}").stdout.decode("ascii").strip())


def _result_json(result: GateResult) -> str:
    return json.dumps(asdict(result), sort_keys=True)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Check Cuexis date-version advancement against a trusted Git baseline."
    )
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--check-current", action="store_true", help="check CMake and manifest consistency")
    parser.add_argument("--base-ref", help="trusted baseline commit SHA")
    parser.add_argument("--candidate-ref", help="candidate commit SHA")
    parser.add_argument("--trusted-utc-date", help="trusted release context as YYYY-MM-DD")
    parser.add_argument(
        "--context",
        choices=("live", "historical"),
        default="live",
        help="live judges against --trusted-utc-date; historical re-verifies an already-recorded "
        "SHA against the recorded baseline date (see ADR 0042 S6-D07)",
    )
    parser.add_argument(
        "--event",
        default="local",
        choices=("local", "pull_request", "merge_group", "push", "workflow_dispatch"),
        help="recorded only: event routing is decided by the workflow job conditions, not by a "
        "date rule here. Rejected if it is ever given a different rule than --context.",
    )
    parser.add_argument("--allow-sdk-api-change", action="store_true")
    parser.add_argument("--sdk-owner-approval-pr", type=int,
                        help="PR containing an exact, unedited GitHub owner approval record")
    parser.add_argument("--json", action="store_true", dest="json_output")
    return parser


def main(arguments: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(arguments)

    try:
        if args.allow_sdk_api_change:
            raise GateError("version.sdk_api.approval_required",
                            "CLI boolean is not owner authorization; use --sdk-owner-approval-pr")
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
        approval_id = None
        if args.sdk_owner_approval_pr is not None:
            approval_id = sdk_owner_approval(args.repo_root, args.base_ref, args.candidate_ref,
                                             trusted_date, args.sdk_owner_approval_pr, args.event)
        result = compare_refs(
            args.repo_root,
            args.base_ref,
            args.candidate_ref,
            trusted_date,
            args.context,
            allow_sdk_api_change=approval_id is not None,
        )
        if args.json_output:
            payload = asdict(result)
            if approval_id is not None:
                payload["sdk_owner_approval_comment_id"] = approval_id
            print(json.dumps(payload, sort_keys=True))
        else:
            print(
                f"version.gate.pass: event={args.event}(recorded) context={result.context} "
                f"base={result.base_ref}({result.base_version}) "
                f"candidate={result.candidate_ref}({result.candidate_version}) "
                f"trusted_utc_date={result.trusted_utc_date} "
                f"sdk_owner_approval_comment_id={approval_id}"
            )
        return 0
    except GateError as error:
        print(f"Version gate failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
