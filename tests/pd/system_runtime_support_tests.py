import copy
import unittest
from system_runtime_support import require_same_power, validate_alignment


class RuntimeSupportTests(unittest.TestCase):
    def fixture(self):
        anchors = [dict(label='begin', pid=8, tid=9, qpc_frequency=10000000,
                        qpc_before=1000, qpc_after=1002, utc_filetime_100ns=130000000000000001),
                   dict(label='end', pid=8, tid=9, qpc_frequency=10000000,
                        qpc_before=1001000, qpc_after=1001002, utc_filetime_100ns=130000000001000001)]
        sample = dict(qpc_start=2000, qpc_end=100200, wall_us=9820)
        return anchors, sample

    def test_preserves_large_filetime_and_anchor_uncertainty(self):
        anchors, sample = self.fixture()
        result = validate_alignment([sample], anchors)
        self.assertEqual(result['intervals'][0]['utc_filetime_start_bounds'],
                         [130000000000000999, 130000000000001001])
        self.assertEqual(result['endpoint_mapping_drift_us'], 0)

    def test_rejects_missing_changed_reversed_or_inconsistent_clocks(self):
        anchors, sample = self.fixture()
        for field, value in [('qpc_end', 1999), ('wall_us', 8000), ('qpc_start', 0)]:
            bad = dict(sample, **{field: value})
            with self.assertRaises(ValueError):
                validate_alignment([bad], anchors)
        bad = copy.deepcopy(anchors)
        bad[1]['utc_filetime_100ns'] += 20000
        with self.assertRaises(ValueError):
            validate_alignment([sample], bad)
        with self.assertRaises(ValueError):
            validate_alignment([sample, sample], anchors)

    def test_same_scheme_does_not_hide_changed_policy_or_ac(self):
        original = {'identity': {'scheme': 'same', 'policy_sha': 'a', 'ac': 1}}
        require_same_power(original, copy.deepcopy(original))
        for update in [{'policy_sha': 'b'}, {'ac': 0}]:
            with self.assertRaises(ValueError):
                require_same_power(original, {'identity': dict(original['identity'], **update)})


if __name__ == '__main__':
    unittest.main()
