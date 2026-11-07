"""Stdlib-only gate: no command is spawned until one complete JSON go line arrives."""
from __future__ import annotations

import json
import subprocess
import sys


_MAX_LINE_BYTES = 1024 * 1024


def _unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("duplicate JSON key")
        value[key] = item
    return value


def _command_from_stdin():
    line = sys.stdin.buffer.readline(_MAX_LINE_BYTES + 1)
    if not line:
        return None
    if len(line) > _MAX_LINE_BYTES or not line.endswith(b"\n"):
        raise ValueError("missing or oversized go line")
    try:
        message = json.loads(line[:-1].decode("utf-8"), object_pairs_hook=_unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
        raise ValueError("invalid go JSON") from exc
    if not isinstance(message, dict) or set(message) != {"go", "command"}:
        raise ValueError("go message has unexpected fields")
    if message["go"] is not True:
        raise ValueError("go flag must be true")
    command = message["command"]
    if (
        not isinstance(command, list)
        or not command
        or any(not isinstance(part, str) or "\x00" in part for part in command)
        or not command[0]
    ):
        raise ValueError("command must be a nonempty argv list")
    return command


def main() -> int:
    try:
        command = _command_from_stdin()
    except ValueError as exc:
        print(f"worker_gate: {exc}", file=sys.stderr, flush=True)
        return 2
    if command is None:
        return 0
    try:
        child = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=None,
            stderr=subprocess.STDOUT,
            shell=False,
            close_fds=True,
        )
    except OSError as exc:
        print(f"worker_gate: command launch failed: {exc}", file=sys.stderr, flush=True)
        return 127
    return child.wait()


if __name__ == "__main__":
    raise SystemExit(main())
