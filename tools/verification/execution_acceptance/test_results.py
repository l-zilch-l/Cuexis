import copy
import unittest

from tools.verification.execution_acceptance.results import verdict


class ResultTests(unittest.TestCase):
    def group(self):
        return [[{"A": {"read": 100}, "B": {"read": 105}} for _ in range(3)] for _ in range(3)]

    def test_equal_ratios_and_window_sizes_use_original_t_intervals(self):
        result = verdict(self.group(), "A/B")["metrics"]["read"]
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["full_interval"]["n_pairs"], 9)
        self.assertEqual([row["n_pairs"] for row in result["window_intervals"]], [3] * 3)

    def test_slow_samples_remain_and_noise_cannot_be_accepted(self):
        group = self.group()
        group[2][2]["B"]["read"] = 250
        result = verdict(group, "A/B")["metrics"]["read"]
        self.assertEqual(result["status"], "inconclusive")
        self.assertEqual(result["full_interval"]["ratios"][-1], 2.5)

    def test_incomplete_invalid_or_mismatched_results_are_rejected(self):
        changes = [lambda group: group.pop(), lambda group: group[0].pop(),
                   lambda group: group[0][0]["B"].update(read=float("nan")),
                   lambda group: group[0][0]["A"].update(read=0),
                   lambda group: group[0][0]["B"].update(other=1)]
        for change in changes:
            group = copy.deepcopy(self.group())
            change(group)
            with self.subTest(change=change), self.assertRaises(ValueError):
                verdict(group, "A/B")
        with self.assertRaises(ValueError):
            verdict(self.group(), "P8-single-sided")


if __name__ == "__main__":
    unittest.main()
