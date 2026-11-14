"""Backend allowances are independent while existing CPU claims remain authoritative."""
import copy
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from . import admission, prepare_probe, probe_units
from .binding import artifact, write_once


class BackendIdentityTests(unittest.TestCase):
    def record(self, device=False):
        return prepare_probe.unit_record(
            probe_units.registered("p4-capture-1"), Path("probe.exe"), Path("warmup.exe"),
            Path("."), 1, {}, device=device)

    def test_cpu_key_is_unchanged_and_device_identity_cannot_borrow_it(self):
        cpu, device = self.record(), self.record(True)
        self.assertEqual(admission.stable_id(cpu), "p4-capture-1-release")
        self.assertEqual(admission.stable_id(device), "p4-capture-1-device-release")
        for record in (cpu, device):
            probe_units.validate_registration(record)
            changed = dict(record, backend="device" if record["backend"] == "cpu" else "cpu")
            with self.assertRaises(ValueError):
                admission.stable_id(changed)
            with self.assertRaises(ValueError):
                probe_units.validate_registration(changed)
        legacy = dict(cpu)
        legacy.pop("backend")
        self.assertEqual(admission.stable_id(legacy), admission.stable_id(cpu))
        with self.assertRaises(ValueError):
            admission.stable_id(dict(cpu, backend="renamed-device"))

    def test_exhausted_cpu_ledger_does_not_block_device_or_get_reset(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ledger = root / "ledger"
            cpu = ledger / "p4-capture-1-release"
            cpu.mkdir(parents=True)
            for attempt in range(1, 4):
                write_once(cpu / f"attempt-{attempt}.json", {"attempt": attempt})
            before = {path: path.read_bytes() for path in cpu.iterdir()}
            authorization = root / "authorization.json"
            write_once(authorization, {"attempt_ledger": str(ledger)})
            preparation = {"attempt_ledger": str(ledger),
                           "authorization_binding": artifact(authorization),
                           "source_binding": {"sha": "a" * 40}, "units": [self.record(True)]}
            # This isolates ledger accounting; full proof/build admission has its own tests.
            with patch.object(admission, "validate_preparation", side_effect=lambda value: value):
                claims = admission.claim(preparation, ledger)
                self.assertEqual(claims[0].parent.name, "p4-capture-1-device-release")
                self.assertEqual(claims[0].name, "attempt-1.json")
                changed = copy.deepcopy(preparation)
                changed["units"] = [self.record()]
                with self.assertRaisesRegex(ValueError, "allowance exhausted"):
                    admission.claim(changed, ledger)
            self.assertEqual(before, {path: path.read_bytes() for path in cpu.iterdir()})
            self.assertEqual(admission.allowance(preparation["units"][0]), 2)


if __name__ == "__main__":
    unittest.main()
