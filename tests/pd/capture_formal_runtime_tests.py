"""Recording ownership and failed-work cleanup are part of the diagnostic contract."""
import argparse
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch

from capture_formal_runtime import capture, capture_transaction


class RuntimeCaptureTests(unittest.TestCase):
    def command(self, responses):
        calls = []
        def invoke(label, args):
            calls.append((label, args))
            return responses[label]
        return invoke, calls

    def replies(self, start=0, stop=0, before=b'WPR is not recording'):
        return {label: subprocess.CompletedProcess([], code, data)
                for label, code, data in [('wpr-before', 0, before),
                                          ('wpr-start', start, b''), ('wpr-stop', stop, b'')]}

    def test_active_recording_is_not_started_stopped_or_canceled(self):
        invoke, calls = self.command(self.replies(before=b'WPR recording is in progress'))
        with self.assertRaises(RuntimeError):
            capture_transaction(invoke, lambda: self.fail('work must not run'), Path('x.etl'), 'owned')
        self.assertEqual([label for label, _ in calls], ['wpr-before'])

    def test_failed_start_does_not_stop_an_unowned_recording(self):
        invoke, calls = self.command(self.replies(start=5))
        with self.assertRaises(RuntimeError):
            capture_transaction(invoke, lambda: self.fail('work must not run'), Path('x.etl'), 'owned')
        self.assertEqual([label for label, _ in calls], ['wpr-before', 'wpr-start'])

    def test_work_failure_still_saves_only_the_named_recording(self):
        invoke, calls = self.command(self.replies())
        def fail():
            raise ValueError('probe failure')
        with self.assertRaisesRegex(ValueError, 'probe failure'):
            capture_transaction(invoke, fail, Path('x.etl'), 'owned')
        self.assertEqual([label for label, _ in calls], ['wpr-before', 'wpr-start', 'wpr-stop'])
        self.assertEqual(calls[-1][1][-2:], ['-instancename', 'owned'])

    def test_stop_failure_is_not_reported_as_success(self):
        invoke, calls = self.command(self.replies(stop=5))
        with self.assertRaisesRegex(RuntimeError, 'stop failed'):
            capture_transaction(invoke, lambda: 42, Path('x.etl'), 'owned')
        self.assertEqual(len(calls), 3)

    def test_success_uses_the_same_named_session_and_returns_work(self):
        invoke, calls = self.command(self.replies())
        self.assertEqual(capture_transaction(invoke, lambda: 42, Path('x.etl'), 'owned'), 42)
        for _, args in calls[1:]:
            self.assertEqual(args[-2:], ['-instancename', 'owned'])
            self.assertNotIn('-cancel', args)

    def test_nonadmin_does_not_read_registration_claim_or_touch_wpr(self):
        with patch('capture_formal_runtime.ctypes.windll.shell32.IsUserAnAdmin', return_value=0), \
             patch('capture_formal_runtime.formal.load') as load, \
             patch('capture_formal_runtime.formal.no_trace') as no_trace:
            with self.assertRaises(PermissionError):
                capture(argparse.Namespace(registration=Path('absent-registration.json')))
            load.assert_not_called()
            no_trace.assert_not_called()

    def test_frozen_export_replaces_dynamic_builtin_profiles(self):
        invoke, calls = self.command(self.replies())
        capture_transaction(invoke, lambda: 42, Path('x.etl'), 'owned', 'frozen.wprp!CPUPower')
        args = calls[1][1]
        self.assertIn('frozen.wprp!CPUPower', args)
        self.assertEqual(args.count('-start'), 1)
        self.assertNotIn('CPU', args)


if __name__ == '__main__':
    unittest.main()
