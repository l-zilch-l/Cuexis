"""Offline failure and integrity checks for the GNU vcpkg asset provider."""

import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from gnu_asset_mirror import fetch


class MirrorTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.destination = Path(self.scratch.name) / "asset"
        self.destination.write_bytes(b"existing")
        self.url = "https://ftpmirror.gnu.org/gnu/gperf/gperf-3.3.tar.gz"
        self.content = b"verified archive"
        self.digest = hashlib.sha512(self.content).hexdigest()

    def download(self, command, **kwargs):
        Path(command[command.index("--output") + 1]).write_bytes(self.content)
        return subprocess.CompletedProcess(command, 0)

    def test_verified_publication(self):
        with patch("gnu_asset_mirror.subprocess.run", side_effect=self.download) as run:
            self.assertEqual(fetch(self.url, self.digest, self.destination), 0)
            self.assertEqual(self.destination.read_bytes(), self.content)
            self.assertEqual(run.call_args.args[0][-1],
                             "https://mirrors.kernel.org/gnu/gperf/gperf-3.3.tar.gz")

    def test_wrong_hash_preserves_destination(self):
        with patch("gnu_asset_mirror.subprocess.run", side_effect=self.download):
            self.assertEqual(fetch(self.url, "0" * 128, self.destination), 1)
        self.assertEqual(self.destination.read_bytes(), b"existing")
        self.assertEqual(list(self.destination.parent.iterdir()), [self.destination])

    def test_timeout_preserves_destination(self):
        with patch("gnu_asset_mirror.subprocess.run",
                   side_effect=subprocess.TimeoutExpired("curl", 40)):
            self.assertEqual(fetch(self.url, self.digest, self.destination), 1)
        self.assertEqual(self.destination.read_bytes(), b"existing")

    def test_non_gnu_assets_do_not_download(self):
        with patch("gnu_asset_mirror.subprocess.run") as run:
            for url in ["http://ftp.gnu.org/gnu/gperf/gperf-3.3.tar.gz",
                        "https://ftp.gnu.org.evil/gnu/gperf/gperf-3.3.tar.gz",
                        "https://ftp.gnu.org/gnu/gperf/../../other.tar.gz",
                        "https://ftp.gnu.org/gnu/gperf/automake-1.17.tar.gz",
                        "https://ftp.gnu.org/gnu/gperf/gperf-3.3.tar.gz?extra=1",
                        "https://github.com/some/archive.tar.gz"]:
                self.assertEqual(fetch(url, self.digest, self.destination), 1)
            self.assertEqual(fetch(self.url, "invalid", self.destination), 1)
            run.assert_not_called()

    def test_automake_pub_url(self):
        with patch("gnu_asset_mirror.subprocess.run", side_effect=self.download) as run:
            self.assertEqual(fetch("https://ftp.gnu.org/pub/gnu/automake/automake-1.17.tar.gz",
                                   self.digest, self.destination), 0)
            self.assertEqual(run.call_args.args[0][-1],
                             "https://mirrors.kernel.org/gnu/automake/automake-1.17.tar.gz")


if __name__ == "__main__":
    unittest.main()
