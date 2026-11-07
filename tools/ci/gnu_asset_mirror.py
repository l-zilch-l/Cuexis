"""Fetch only GNU gperf/automake through a SHA512-verified vcpkg asset mirror."""

import hashlib
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit


def fetch(url, expected, destination):
    parsed = urlsplit(url)
    if (parsed.scheme != "https" or parsed.netloc not in {"ftpmirror.gnu.org", "ftp.gnu.org"}
            or parsed.query or parsed.fragment
            or not re.fullmatch(r"[0-9a-fA-F]{128}", expected)):
        return 1
    match = re.fullmatch(r"/(?:pub/)?gnu/(gperf|automake)/([a-z]+-[0-9.]+\.tar\.gz)", parsed.path)
    if not match or not match[2].startswith(match[1] + "-"):
        return 1
    mirror = "https://mirrors.kernel.org/gnu/" + match[1] + "/" + match[2]
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gnu-asset-", dir=destination.parent) as scratch:
        archive = Path(scratch) / "archive"
        try:
            result = subprocess.run(
                ["curl", "--fail", "--location", "--silent", "--show-error",
                 "--proto", "=https", "--proto-redir", "=https", "--connect-timeout", "8",
                 "--max-time", "35", "--output", str(archive), mirror], timeout=40,
                check=False)
            if result.returncode != 0:
                return 1
            with archive.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha512").hexdigest()
            if actual != expected.lower():
                print("GNU mirror SHA512 mismatch; using normal vcpkg fallback", file=sys.stderr)
                return 1
            archive.replace(destination)
        except (OSError, subprocess.TimeoutExpired) as error:
            print("GNU mirror unavailable: " + str(error), file=sys.stderr)
            return 1
    print("Verified GNU mirror asset: " + match[2])
    return 0


if __name__ == "__main__":
    sys.exit(fetch(*sys.argv[1:]) if len(sys.argv) == 4 else 1)
