import copy
import json
from pathlib import Path
import unittest

from tools.verification.execution_acceptance.budgets import derive, validate


class BudgetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.layout = json.loads(Path(__file__).with_name("element-sizes-msvc-release.v1.json")
                                .read_text(encoding="utf-8"))

    def setUp(self):
        self.proposal = derive(self.layout)

    def test_all_scales_cover_all_domains(self):
        validate(self.proposal)
        self.assertEqual([p["entities"] for p in self.proposal["profiles"]], [64, 2048, 40000])
        # Derived from the audited layout, not a coefficient. The 2026-11-24 re-audit raised the
        # n=40000 bound by 576 bytes because FramePayload, which the F135 Error change does not
        # touch, grew after commit 739b6f34 without refreshing this baseline.
        self.assertEqual(self.proposal["profiles"][-1]["frame_byte_bound"], 163842704)

    def test_missing_domain_zero_unknown_and_self_acceptance_are_rejected(self):
        mutations = [
            lambda d: d["profiles"][0]["domains"].pop(),
            lambda d: d["profiles"][0]["domains"][0].update(requested_bytes=0),
            lambda d: d.update(accepted=True),
            lambda d: d.update(formal_registration=True),
            lambda d: d["profiles"][0].update(cpu_requested_envelope=1),
            lambda d: d["profiles"][0]["domains"][0].update(lifetime=""),
        ]
        for mutate in mutations:
            document = copy.deepcopy(self.proposal)
            mutate(document)
            with self.subTest(mutation=mutate), self.assertRaises(ValueError):
                validate(document)

    def test_missing_or_boolean_layout_size_is_rejected(self):
        for bad in (None, True, 0, -1):
            document = copy.deepcopy(self.layout)
            document["sizes"]["SessionChannelSlot"] = bad
            with self.subTest(value=bad), self.assertRaises(ValueError):
                derive(document)


if __name__ == "__main__":
    unittest.main()
