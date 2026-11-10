"""Prove that each split header can be included on its own.

A translation unit that includes one header before anything else must compile: if it does not, the
header depends on a specific include order and every consumer has to guess that order. The build
itself cannot show this, because it only proves that some translation unit compiles with the
include order that translation unit happens to use.

This check is deliberately not a CTest: on MSVC the system include paths come from the Developer
Shell's ``INCLUDE`` variable, so a registered test would fail on any machine that did not configure
from such a shell. Run it by hand after touching a public header or the include graph:

    python -B tools/verification/check_header_self_containment.py

Each probe reuses the compile command of a real translation unit from the same module, so the
include paths, defines and language standard match the build, and asks the compiler for a syntax
only pass (``/Zs`` for MSVC, ``-fsyntax-only`` elsewhere).
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# One reference translation unit per module. Its flags decide the include paths and defines, so the
# probe sees exactly what a real file of that module sees.
REFERENCE_TUS = {
    "judgement": "engine/judgement/src/timebase.cpp",
    "assets": "engine/assets/src/resource_manager.cpp",
    "chart": "engine/chart/src/packed_chart_tables_decoder.cpp",
}

# The headers split out of one oversized original. `pattern_dfa_node.hpp` and its siblings are
# excluded on purpose: they are fragments included by `pattern_dfa.hpp` and are not self-contained
# by construction, which the fragment headers state themselves.
HEADERS = {
    "judgement": [
        "engine/judgement/include/cuexis/judgement/timebase.hpp",
        "engine/judgement/include/cuexis/judgement/timebase_tick.hpp",
        "engine/judgement/include/cuexis/judgement/timebase_rational.hpp",
        "engine/judgement/include/cuexis/judgement/timebase_profile.hpp",
        "engine/judgement/include/cuexis/judgement/input_boundary.hpp",
        "engine/judgement/include/cuexis/judgement/input_boundary_amount.hpp",
        "engine/judgement/include/cuexis/judgement/input_boundary_mapping.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_assembler.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_assembler_identity.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_assembler_publication.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_assembler_compiled.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_graph.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_graph_reference.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_graph_pattern.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_graph_phase.hpp",
        "engine/judgement/include/cuexis/judgement/gameplay_graph_record.hpp",
        "engine/judgement/src/recovery_wire.hpp",
        "engine/judgement/src/recovery_wire_fields.hpp",
        "engine/judgement/src/source_codes.hpp",
        "engine/judgement/src/source_codes_paths.hpp",
        "engine/judgement/src/pattern_dfa.hpp",
        "engine/core/internal/cuexis_internal/execution_diagnostics_rpa.hpp",
        "engine/core/internal/cuexis_internal/execution_diagnostics_rpa_close.hpp",
        "engine/core/internal/cuexis_internal/execution_diagnostics_rpa_seek.hpp",
    ],
    "assets": [
        "engine/assets/src/resource_manager_resource_detail.hpp",
        "engine/assets/src/resource_manager_state_internal.hpp",
    ],
    "chart": [
        "engine/chart/src/packed_chart_tables_decoder_internal.hpp",
    ],
}


def load_commands(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise SystemExit(
            f"compile_commands.json not found: {path}\n"
            "Configure with CMAKE_EXPORT_COMPILE_COMMANDS=ON (the debug and release presets do), "
            "or pass --compile-commands."
        )
    entries = json.loads(path.read_text(encoding="utf-8"))
    return {entry["file"].replace("\\", "/"): entry["command"] for entry in entries}


def tokens(command: str) -> list[str]:
    """Split a command line, keeping quoted paths (which may contain spaces) in one piece."""
    return re.findall(r'"[^"]*"|\S+', command)


def syntax_only(command: str, probe: Path, drop_directory: Path) -> str:
    """Turn a real compile command into a syntax-only compile of the probe file.

    The original translation unit is removed from the command and the probe takes its place; every
    other flag, in particular the include paths and defines, is kept verbatim.
    """
    is_msvc = "cl.exe" in command.lower()
    takes_argument = ("-o", "-MF", "-MT", "-MQ", "-MD", "-MMD") if not is_msvc else ()
    dropped_flags = ("-c",) if not is_msvc else ()
    cleaned: list[str] = []
    index = 0
    parts = tokens(command)
    while index < len(parts):
        token = parts[index]
        bare = token.strip('"')
        if any(token.startswith(prefix) for prefix in ("/Fo", "/Fd")) and is_msvc:
            index += 1
            continue
        if token in takes_argument:
            index += 2
            continue
        if token in dropped_flags or token in ("-MD", "-MMD"):
            index += 1
            continue
        if bare.endswith((".cpp", ".cc", ".cxx", ".c")):
            index += 1
            continue
        cleaned.append(token)
        index += 1
    if is_msvc:
        return " ".join(cleaned) + f' /TP "{probe}" /Zs /Fd"{drop_directory / "probe.pdb"}"'
    return " ".join(cleaned) + f' -fsyntax-only "{probe}"'


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-dir", type=Path, default=Path(__file__).resolve().parents[2]
    )
    parser.add_argument("--build-dir", type=Path, default=None)
    parser.add_argument("--compile-commands", type=Path, default=None)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    source_dir: Path = args.source_dir.resolve()
    build_dir = args.build_dir or source_dir / "out/build/debug"
    commands_path = args.compile_commands or build_dir / "compile_commands.json"
    commands = load_commands(commands_path)

    failures = 0
    checked = 0
    with tempfile.TemporaryDirectory(prefix="cuexis-header-probe-") as directory:
        drop = Path(directory)
        for module, headers in HEADERS.items():
            reference = f"{(source_dir / REFERENCE_TUS[module]).as_posix()}"
            command = commands.get(reference)
            if command is None:
                raise SystemExit(f"reference translation unit not in compile_commands.json: {reference}")
            for header in headers:
                checked += 1
                probe = drop / "probe.cpp"
                probe.write_text(
                    f'#include "{(source_dir / header).as_posix()}"\nint main() {{ return 0; }}\n',
                    encoding="utf-8",
                )
                built = syntax_only(command, probe, drop)
                if args.verbose:
                    print(built)
                result = subprocess.run(
                    built,
                    cwd=source_dir,
                    shell=True,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                )
                if result.returncode == 0:
                    print(f"{header:<62} OK")
                else:
                    failures += 1
                    print(f"{header:<62} FAILED")
                    text = (result.stdout or "") + (result.stderr or "")
                    print("\n".join(f"    {line}" for line in text.strip().splitlines()[-6:]))
    print(f"headers={checked} failures={failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
