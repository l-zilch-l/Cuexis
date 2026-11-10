import json
import unittest

from tools.verification.execution_acceptance import probe_units
from tools.verification.execution_acceptance.runner import parse_result
from tools.verification.playback_performance import measurement


def probe_log(unit, frames, warmup, *, overrides=None):
    identity = probe_units.identity(unit, frames, warmup)
    identity.update(overrides or {})
    rows = ["PD5_ID " + json.dumps(identity)]
    for metric, count in identity["counts"].items():
        for ordinal in range(count):
            rows.append("PD5_SAMPLE " + json.dumps(dict(metric=metric, ordinal=ordinal,
                                                        elapsed_us=1000 + ordinal % 7,
                                                        complete=True)))
    return identity, "\n".join(rows)


def unit_record(unit, frames=64, warmup=8):
    return dict(adapter="probe", gate=unit["gate"], config="Release",
                expected_identity=probe_units.expected_fields(unit, frames, warmup),
                requests_per_process=frames, required_metrics=list(unit["metrics"]))


class ProbeUnitTests(unittest.TestCase):
    def test_registered_counters_follow_the_original_matrix_rules(self):
        self.assertEqual(probe_units.counts(probe_units.registered("p1-no-consumer")),
                         {"sdk_update": 14400})
        self.assertEqual(probe_units.counts(probe_units.registered("p2-low-delivery")),
                         {"sdk_update": 115200, "materialize": 14400, "capture_delivery": 14400})
        self.assertEqual(probe_units.counts(probe_units.registered("p4-capture-4"))["materialize"],
                         57600)
        topology = probe_units.identity(probe_units.registered("p6-topology"))
        self.assertEqual(topology["counts"]["topology_reload"], 56)
        self.assertEqual(topology["reloads"], 65)
        self.assertEqual(topology["delivery_generations"], 66)
        self.assertEqual(topology["max_entities"], 201)

    def test_identity_is_declared_before_any_observation(self):
        unit = probe_units.registered("p6-sparse")
        identity = probe_units.identity(unit)
        self.assertEqual((identity["base_entities"], identity["max_entities"]), (200, 200))
        self.assertEqual((identity["frames"], identity["warmup_frames"]), (14400, 2400))
        self.assertEqual(identity["updates"], 14400)
        self.assertNotIn("digest", probe_units.expected_fields(unit))
        self.assertNotIn("smoke", probe_units.expected_fields(unit))

    def test_selection_rejects_unknown_and_repeated_units(self):
        self.assertEqual([unit["id"] for unit in probe_units.select(["p1-no-consumer"])],
                         ["p1-no-consumer"])
        with self.assertRaises(ValueError):
            probe_units.select(["p1-no-consumer", "p1-no-consumer"])
        with self.assertRaises(ValueError):
            probe_units.select(["p3-renderer"])

    def test_parse_result_uses_frames_and_the_exact_declared_counters(self):
        unit = probe_units.registered("p4-capture-1")
        identity, text = probe_log(unit, 64, 8)
        parsed = parse_result(unit_record(unit), text, 0)
        self.assertEqual(parsed["parsed"]["identity"]["frames"], 64)
        self.assertEqual(parsed["medians"], {"capture_delivery": parsed["medians"]["capture_delivery"]})
        self.assertEqual(parsed["absolute_gates"], {})

    def test_parse_result_rejects_a_changed_counter_or_frame_count(self):
        unit = probe_units.registered("p1-no-consumer")
        record = unit_record(unit)
        _, fewer = probe_log(unit, 63, 8)
        with self.assertRaises(ValueError):
            parse_result(record, fewer, 0)
        _, changed = probe_log(unit, 64, 8, overrides={"updates": 65})
        with self.assertRaises(ValueError):
            parse_result(record, changed, 0)
        with self.assertRaises(ValueError):
            parse_result(dict(record, adapter="host-common"), changed, 0)

    def test_probe_identity_is_comparable_only_against_the_same_declared_workload(self):
        unit = probe_units.registered("p6-sparse")
        identity, text = probe_log(unit, 64, 8)
        parsed = measurement.parse_probe(text, exit_code=0)
        measurement.require_comparable(parsed, parsed, "capture_delivery")
        other = dict(parsed, identity=dict(identity, mode="capture-1"))
        with self.assertRaises(ValueError):
            measurement.require_comparable(parsed, other, "capture_delivery")
        with self.assertRaises(ValueError):
            measurement.require_comparable(parsed, parsed, "topology_reload")


    def test_registration_rejects_a_baseline_side_or_a_changed_frame_count(self):
        unit = probe_units.registered("p1-no-consumer")
        record = dict(unit_record(unit), workload="steady", comparison="B-only")
        probe_units.validate_registration(record)
        for changed in (dict(record, comparison="A/B"), dict(record, requests_per_process=63),
                        dict(record, expected_identity={"mode": "no-consumer"}),
                        dict(record, gate="p3-renderer")):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                probe_units.validate_registration(changed)


if __name__ == "__main__":
    unittest.main()
