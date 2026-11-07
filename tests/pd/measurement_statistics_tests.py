import math
import unittest
from measurement_statistics import paired_effect, per_operation


class MeasurementStatisticsTests(unittest.TestCase):
    def rows(self, effect=1):
        return [{"group": 0, "active": active, "operations": 1024,
                 "sample": {"wall_us": 100 * 1.1**i * (effect if active else 1)}}
                for i, active in enumerate([False, True, True, False])]

    def test_null_comparison_cancels_log_linear_drift(self):
        self.assertAlmostEqual(paired_effect(self.rows()), 0)

    def test_known_cost_survives_drift_cancellation(self):
        self.assertAlmostEqual(paired_effect(self.rows(1.05)), .05)

    def test_slow_sample_is_not_discarded(self):
        rows = self.rows()
        rows[1]["sample"]["wall_us"] *= 10
        self.assertAlmostEqual(paired_effect(rows), math.sqrt(10) - 1)

    def test_incomplete_or_nonsymmetric_quartet_is_rejected(self):
        with self.assertRaises(ValueError):
            paired_effect(self.rows()[:3])
        rows = self.rows()
        rows[0]["active"], rows[1]["active"] = True, False
        with self.assertRaises(ValueError):
            paired_effect(rows)

    def test_normalization_retains_different_batch_lengths(self):
        self.assertEqual(per_operation({"operations": 4096, "sample": {"wall_us": 819.2}}),
                         per_operation({"operations": 1048576, "sample": {"wall_us": 209715.2}}))


if __name__ == "__main__":
    unittest.main()
