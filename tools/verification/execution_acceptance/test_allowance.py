import unittest

from tools.verification.execution_acceptance import admission
from tools.verification.execution_acceptance.binding import write_once


def waiver(**overrides):
    value = {"schema": "rpa.execution.attempt-waiver.v1", "granted_by": "owner",
             "stable_protocol_ids": ["p1-no-consumer-release"], "maximum_attempts": 8}
    value.update(overrides)
    return value


class AllowanceTests(unittest.TestCase):
    def setUp(self):
        self.unit = {"id": "p1-no-consumer", "stable_protocol_id": "p1-no-consumer-release",
                     "workload": "steady", "config": "Release", "gate": "p1-no-consumer"}
        self.p8 = {"id": "p8-40000-low", "stable_protocol_id": "p8-40000-low-release",
                   "workload": "p8", "config": "Release", "entities": 40000, "reuse": "low"}

    def test_default_allowance_is_two_and_one_for_p8(self):
        self.assertEqual(admission.allowance(self.unit), 2)
        self.assertEqual(admission.allowance(self.p8), 1)

    def test_owner_waiver_widens_only_named_units(self):
        self.assertEqual(admission.allowance(self.unit, waiver()), 8)
        self.assertEqual(admission.allowance(
            self.p8, waiver(stable_protocol_ids=["p8-40000-low-release"])), 8)
        with self.assertRaisesRegex(ValueError, "does not authorize"):
            admission.allowance(self.unit, waiver(stable_protocol_ids=["p6-sparse-release"]))

    def test_a_waiver_cannot_shrink_the_default_or_skip_owner_attribution(self):
        for broken in (waiver(maximum_attempts=2), waiver(granted_by="agent"),
                       waiver(schema="other"), waiver(maximum_attempts="many")):
            with self.subTest(broken=broken), self.assertRaisesRegex(ValueError, "does not authorize"):
                admission.allowance(self.unit, broken)

    def test_attempts_are_ordered_numerically(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            for name in ("attempt-1.json", "attempt-2.json", "attempt-10.json",
                         "attempt-9.json", "notes.json"):
                write_once(directory / name, {"name": name})
            ordered = [path.name for path in admission.attempts_of(directory)]
            self.assertEqual(ordered, ["attempt-1.json", "attempt-2.json",
                                       "attempt-9.json", "attempt-10.json"])


if __name__ == "__main__":
    unittest.main()
