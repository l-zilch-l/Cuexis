"""Read-only input checks for the current-implementation replay.

Verifies that the split R4 case files are identical to their entries in the
package's verification-cases.json, reconstructs the PD3c expectation lines from
its case file, and writes SHA-256 values plus byte lengths to hash-check.json.
"""
import hashlib
import json
from pathlib import Path

R4 = Path(r"C:\Users\Zilch\Desktop\Cuexis-R4-P7-O2-2026-11-14\keyboard-dense-v5")
PD3C = Path(r"C:\Users\Zilch\Desktop\Cuexis-PD3c-DeviceAcceptance-2026-10-19\keyboard")
HERE = Path(__file__).resolve().parents[1]
CASES = HERE / "cases"
OUT = HERE / "outputs"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def entry_bytes(path: Path) -> bytes:
    return path.read_bytes()


def case_line(case: dict) -> str:
    return (
        f"{case['name']}: score={case['score']} hits={case['hits']} "
        f"misses={case['misses']} replay=equal\n"
    )


report = {"r4": {}, "pd3c": {}}

# --- R4 dense v5 -----------------------------------------------------------
r4_full = json.loads((R4 / "verification-cases.json").read_text(encoding="utf-8"))
r4_by_name = {case["name"]: case for case in r4_full}
for name in ("all-hit", "no-input", "early-hold-release"):
    split = json.loads((CASES / f"{name}.json").read_text(encoding="utf-8"))
    report["r4"][name] = {
        "split_bytes": (CASES / f"{name}.json").stat().st_size,
        "split_single_element": len(split) == 1,
        "matches_full_entry": split[0] == r4_by_name[name],
        "inputs": len(split[0]["inputs"]),
        "end": split[0]["end"],
        "expected": {
            "score": split[0]["score"],
            "hits": split[0]["hits"],
            "misses": split[0]["misses"],
        },
    }

r4_expected = "".join(case_line(r4_by_name[n]) for n in r4_by_name)
# Map preserves the package order (all-hit, no-input, early-hold-release).
report["r4"]["package_order"] = list(r4_by_name)
report["r4"]["verification_log"] = {
    "bytes": (R4 / "verification.log").stat().st_size,
    "sha256": sha256(entry_bytes(R4 / "verification.log")),
}
report["r4"]["full_cases"] = {
    "bytes": (R4 / "verification-cases.json").stat().st_size,
    "sha256": sha256(entry_bytes(R4 / "verification-cases.json")),
    "cases": len(r4_full),
}
report["r4"]["inputs"] = {
    name: {
        "bytes": (R4 / name).stat().st_size,
        "sha256": sha256(entry_bytes(R4 / name)),
    }
    for name in (
        "practice.cxc",
        "gameplay.packed",
        "gameplay-configuration.json",
        "verification-cases.json",
        "verification.log",
    )
}

# --- PD3c keyboard ---------------------------------------------------------
pd3c_cases = json.loads((PD3C / "verification-cases.json").read_text(encoding="utf-8"))
pd3c_expected = "".join(case_line(case) for case in pd3c_cases)
pd3c_verification = json.loads((PD3C / "verification.json").read_text(encoding="utf-8"))
report["pd3c"] = {
    "cases": [
        {
            "name": case["name"],
            "end": case["end"],
            "inputs": len(case["inputs"]),
            "expected": {
                "score": case["score"],
                "hits": case["hits"],
                "misses": case["misses"],
            },
        }
        for case in pd3c_cases
    ],
    "verification_cases": {
        "bytes": (PD3C / "verification-cases.json").stat().st_size,
        "sha256": sha256(entry_bytes(PD3C / "verification-cases.json")),
    },
    "verification_json": {
        "bytes": (PD3C / "verification.json").stat().st_size,
        "sha256": sha256(entry_bytes(PD3C / "verification.json")),
        "logicalCases_sha256": sha256(pd3c_verification["logicalCases"].encode()),
        "logicalCases_bytes": len(pd3c_verification["logicalCases"].encode()),
    },
    "reconstructed_logicalCases_equal": pd3c_expected == pd3c_verification["logicalCases"],
    "also_copied_identical_to_keyboard_release": None,
    "inputs": {
        name: {
            "bytes": (PD3C / name).stat().st_size,
            "sha256": sha256(entry_bytes(PD3C / name)),
        }
        for name in (
            "practice.cxc",
            "gameplay.packed",
            "gameplay-configuration.json",
            "verification-cases.json",
            "verification.json",
        )
    },
}

# keyboard/ and keyboard-release/ are two entry directories of the same course.
release = Path(r"C:\Users\Zilch\Desktop\Cuexis-PD3c-DeviceAcceptance-2026-10-19\keyboard-release")
identical = {}
for name in (
    "course.json",
    "entity-index.json",
    "practice.cxc",
    "gameplay.packed",
    "gameplay-configuration.json",
    "verification-cases.json",
    "verification.json",
):
    identical[name] = entry_bytes(PD3C / name) == entry_bytes(release / name)
report["pd3c"]["also_copied_identical_to_keyboard_release"] = identical

# --- stdout vs expectations ------------------------------------------------
r4_stdout = b"".join(
    (OUT / f"r4-{name}.stdout.txt").read_bytes()
    for name in ("all-hit", "no-input", "early-hold-release")
)
pd3c_stdout = (OUT / "pd3c-keyboard.stdout.txt").read_bytes()
pd3c_old_stdout = (OUT / "pd3c-keyboard.old-d195.stdout.txt").read_bytes()


def lf(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n")


report["stdout_checks"] = {
    "r4_concatenated_stdout": {
        "bytes": len(r4_stdout),
        "sha256": sha256(r4_stdout),
        "has_crlf": b"\r\n" in r4_stdout,
        "equals_verification_log_bytes": r4_stdout
        == entry_bytes(R4 / "verification.log"),
    },
    "pd3c_stdout": {
        "bytes": len(pd3c_stdout),
        "sha256": sha256(pd3c_stdout),
        "has_crlf": b"\r\n" in pd3c_stdout,
        "equals_verification_json_logicalCases_raw": pd3c_stdout
        == pd3c_verification["logicalCases"].encode(),
        "equals_verification_json_logicalCases_after_crlf_normalization": lf(
            pd3c_stdout
        )
        == pd3c_verification["logicalCases"].encode(),
        "equals_reconstructed_lines_after_crlf_normalization": lf(pd3c_stdout)
        == pd3c_expected.encode(),
        "normalized_bytes": len(lf(pd3c_stdout)),
        "normalized_sha256": sha256(lf(pd3c_stdout)),
        "logicalCases_bytes": len(pd3c_verification["logicalCases"].encode()),
        "logicalCases_sha256": sha256(pd3c_verification["logicalCases"].encode()),
    },
    "pd3c_old_d195_stdout": {
        "bytes": len(pd3c_old_stdout),
        "sha256": sha256(pd3c_old_stdout),
        "has_crlf": b"\r\n" in pd3c_old_stdout,
        "equals_current_stdout": pd3c_old_stdout == pd3c_stdout,
        "equals_verification_json_logicalCases_after_crlf_normalization": lf(
            pd3c_old_stdout
        )
        == pd3c_verification["logicalCases"].encode(),
    },
}

# --- binaries --------------------------------------------------------------
CURRENT_EXE = HERE / "compiler" / "compiler.exe"
OLD_EXE = Path(r"E:\Cuexis-replay-d195\out\build\replay\entity-practice\compiler.exe")
report["binaries"] = {
    "current_compiler.exe": {
        "path": str(CURRENT_EXE),
        "bytes": CURRENT_EXE.stat().st_size,
        "sha256": sha256(CURRENT_EXE.read_bytes()),
    },
    "old_d195_compiler.exe": {
        "path": str(OLD_EXE),
        "bytes": OLD_EXE.stat().st_size,
        "sha256": sha256(OLD_EXE.read_bytes()),
    },
}

(OUT / "hash-check.json").write_text(
    json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
)
print(json.dumps(report, indent=2, ensure_ascii=False))
