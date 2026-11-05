"""Regression tests for destructive configure-directory selection."""
from pathlib import Path
import tempfile
import unittest

from tools.verification.run_rpa_acceptance_matrix import isolated_build_paths


class BuildIsolationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build_root = self.root / "out/build/r4"

    def test_rows_and_invocations_have_distinct_new_directories(self):
        first = isolated_build_paths(self.build_root, "first", ["on", "off"], self.root)
        second = isolated_build_paths(self.build_root, "second", ["on"], self.root)
        self.assertEqual(len(set([*first.values(), *second.values()])), 3)
        self.assertTrue(all(not path.exists() for path in first.values()))

    def test_checkout_and_foreign_paths_are_rejected(self):
        for path in (self.root, self.root.parent / "foreign"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                isolated_build_paths(path, "run", ["on"], self.root)

    def test_existing_invocation_is_not_reused(self):
        (self.build_root / "run").mkdir(parents=True)
        with self.assertRaises(ValueError):
            isolated_build_paths(self.build_root, "run", ["on"], self.root)

    def test_build_tree_and_its_descendants_are_protected(self):
        existing = self.root / "out/build/frozen"
        existing.mkdir(parents=True)
        (existing / "CMakeCache.txt").write_text("frozen", encoding="utf-8")
        for path in (existing, existing / "child"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                isolated_build_paths(path, "run", ["on"], self.root)
        self.assertEqual((existing / "CMakeCache.txt").read_text(encoding="utf-8"), "frozen")


if __name__ == "__main__":
    unittest.main()
