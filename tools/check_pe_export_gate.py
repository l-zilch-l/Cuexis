"""Exercise the PE export gate against a real DLL and deliberately invalid export tables."""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--objdump", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--candidate", choices=["ON", "OFF"], required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    raw = subprocess.check_output([str(args.objdump), "-p", str(args.dll)], text=True)
    table_row = re.compile(
        r"^\s*\[\s*\d+\]\s+(?:\+base\[\s*\d+\]\s+[0-9A-Fa-f]+\s+)?\S+\s*$")
    with tempfile.TemporaryDirectory(prefix="cuexis-pe-export-") as directory:
        temporary = Path(directory).resolve()
        output = temporary / "exports.txt"
        tool = temporary / ("symbols.cmd" if os.name == "nt" else "symbols.sh")
        # The gate invokes an inspection tool; this fixture emits the captured table verbatim.
        if os.name == "nt":
            tool.write_text('@echo off\ntype "' + str(output) + '"\n', encoding="utf-8")
        else:
            tool.write_text("#!/bin/sh\ncat " + shlex.quote(str(output)) + "\n", encoding="utf-8")
            tool.chmod(0o700)
        command = [
            str(args.cmake),
            "-DCUEXIS_SHARED_LIBRARY=" + str(args.dll.resolve()),
            "-DCUEXIS_SYMBOL_TOOL=" + str(tool),
            "-DCUEXIS_SYMBOL_TOOL_KIND=objdump",
            "-DCUEXIS_GAMEPLAY_CANDIDATE=" + args.candidate,
            "-P",
            str(root / "cmake/VerifySharedExports.cmake"),
        ]
        cases = [
            ("real exports", raw, None),
            ("private export", raw + "\n[99998] _ZN6cuexis8playback6detail10AudioOwner6createEv\n",
             "exported internal symbol"),
            ("unapproved export", raw + "\n[99999] _ZN6cuexis8playback17UnapprovedSurface4stepEv\n",
             "outside the public allowlist"),
            # Keeping names in import/debug-like rows must not satisfy required exports.
            ("required exports absent", "\n".join(
                "import/debug " + line if table_row.match(line) else line
                for line in raw.splitlines()), "does not export"),
            ("private import is not an export",
             raw + "\nimport/debug _ZN6cuexis8playback6detail10AudioOwner6createEv\n", None),
        ]
        for name, text, rejection in cases:
            output.write_text(text, encoding="utf-8")
            result = subprocess.run(command, capture_output=True, text=True)
            diagnostics = result.stdout + result.stderr
            if (rejection is None and result.returncode != 0) or (
                rejection is not None and
                (result.returncode == 0 or rejection not in diagnostics)
            ):
                raise RuntimeError(f"{name}: exit={result.returncode}\n{diagnostics}")
            print(f"{name}: expected exit={result.returncode}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
