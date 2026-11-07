"""Small, append-only evidence helpers for the tool-only ETW lifecycle canary."""
import hashlib
import json
import os
from pathlib import Path
import time
import uuid


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path, value):
    path = Path(path)
    temporary = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    with temporary.open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2, ensure_ascii=True)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    # On Windows rename refuses replacement. No historical receipt is overwritten.
    temporary.rename(path)


def say(value):
    print('[PD5 tools] ' + str(value), flush=True)


def remaining(deadline, cap):
    value = min(cap, deadline - time.monotonic())
    if value <= 0:
        raise TimeoutError('phase budget exhausted')
    return value
