"""Offline preparation and size-audit contracts; no compiler, WPR or tool role runs."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / 'tools/verification/playback_performance'
sys.path.insert(0, str(PACKAGE))
import audit_element_sizes as sizes
import prepare_etw_canary as retired


class ToolContractTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'out/pd5-tool-contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        scratch = tempfile.TemporaryDirectory(prefix='case-', dir=parent)
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name)

    def generated(self, name):
        directory = self.root / 'out/build' / name / 'generated'
        header = directory / 'cuexis/playback/playback_export.hpp'
        header.parent.mkdir(parents=True)
        header.write_text('// contract fixture only\n', encoding='ascii')
        return directory

    def test_retired_import_call_creates_no_output_and_needs_no_wpr(self):
        output = self.root / 'never-created'
        with self.assertRaisesRegex(RuntimeError, 'retired; use prepare_etw_tools.py'):
            retired.prepare(output, self.root / 'missing-wpr.exe')
        self.assertFalse(output.exists())

    def test_retired_call_preserves_existing_package_bytes(self):
        output = self.root / 'sealed'
        output.mkdir()
        binding = output / 'binding.json'
        original = b'{"schema":"historical.v2"}\r\n'
        binding.write_bytes(original)
        with self.assertRaisesRegex(RuntimeError, 'retired'):
            retired.prepare(output, None)
        self.assertEqual(binding.read_bytes(), original)
        self.assertEqual([path.name for path in output.iterdir()], ['binding.json'])

    def test_retired_cli_rejects_before_output_creation(self):
        output = self.root / 'never-created'
        argv = ['prepare_etw_canary.py', '--output', str(output), '--wpr', 'missing.exe']
        error = io.StringIO()
        with mock.patch.object(sys, 'argv', argv), contextlib.redirect_stderr(error):
            with self.assertRaises(SystemExit) as stopped:
                retired.main()
        self.assertEqual(stopped.exception.code, 2)
        self.assertIn('retired; use prepare_etw_tools.py', error.getvalue())
        self.assertFalse(output.exists())

    def test_retired_cli_without_arguments_names_the_replacement(self):
        error = io.StringIO()
        with mock.patch.object(sys, 'argv', ['prepare_etw_canary.py']), \
                contextlib.redirect_stderr(error):
            with self.assertRaises(SystemExit) as stopped:
                retired.main()
        self.assertEqual(stopped.exception.code, 2)
        self.assertIn('retired; use prepare_etw_tools.py', error.getvalue())

    def test_explicit_generated_directory_requires_export_header(self):
        directory = self.root / 'no-headers'
        directory.mkdir()
        with self.assertRaisesRegex(SystemExit, 'generated export header not found'):
            sizes._generated_dir(directory)

    def test_missing_explicit_generated_directory_is_refused(self):
        with self.assertRaisesRegex(SystemExit, 'generated export header not found'):
            sizes._generated_dir(self.root / 'missing')

    def test_explicit_generated_directory_is_resolved(self):
        directory = self.generated('chosen')
        self.assertEqual(sizes._generated_dir(directory / '..' / 'generated'), directory.resolve())

    def test_no_configured_generated_directory_is_refused(self):
        with mock.patch.object(sizes, 'ROOT', self.root):
            with self.assertRaisesRegex(SystemExit, 'pass --generated'):
                sizes._generated_dir(None)

    def test_single_complete_generated_candidate_is_selected(self):
        directory = self.generated('only')
        (self.root / 'out/build/incomplete/generated').mkdir(parents=True)
        with mock.patch.object(sizes, 'ROOT', self.root):
            self.assertEqual(sizes._generated_dir(None), directory)

    def test_multiple_generated_candidates_require_explicit_selection(self):
        self.generated('older')
        self.generated('newer')
        with mock.patch.object(sizes, 'ROOT', self.root):
            with self.assertRaisesRegex(SystemExit, 'multiple configured build trees; pass --generated'):
                sizes._generated_dir(None)

    def test_explicit_generated_directory_selects_among_multiple_candidates(self):
        directory = self.generated('chosen')
        self.generated('other')
        with mock.patch.object(sizes, 'ROOT', self.root):
            self.assertEqual(sizes._generated_dir(directory), directory.resolve())

    def test_size_check_cli_passes_generated_to_measure(self):
        generated = self.root / 'selected-generated'
        includes = self.root / 'selected-includes'
        reference = {'schema': sizes.SCHEMA, 'elements': [], 'toolchain': {}}
        output = self.root / 'element_sizes.v1.json'
        output.write_text(json.dumps(reference), encoding='utf-8')
        argv = ['audit_element_sizes.py', '--check', '--generated', str(generated),
                '--vcpkg-include', str(includes)]
        with mock.patch.object(sys, 'argv', argv), \
                mock.patch.object(sizes, 'OUTPUT', output), \
                mock.patch.object(sizes, 'measure', return_value=reference) as measure, \
                mock.patch('builtins.print'):
            self.assertEqual(sizes.main(), 0)
        measure.assert_called_once_with(includes, generated)


if __name__ == '__main__':
    unittest.main()
