import json
import unittest

from tools.verification.execution_acceptance.runner import parse_result, process_order


def owner_log():
    identity = dict(profile="legacy", metrics_compile_enabled=False, allocator="normal",
                    digest=1, replay_sha256="a" * 64, snapshot_sha256="b" * 64,
                    packed_sha256="c" * 64)
    rows = ["PD_FORMAL_ID " + json.dumps(identity)]
    for kind in ("validated_trace", "playback_only", "read_long"):
        for group in range(8):
            rows.append("PD_V2 " + json.dumps(dict(kind=kind, group=group, active=False,
                                                   operations=1024, sample={"wall_us": 100001})))
    return identity, "\n".join(rows)


class RunnerTests(unittest.TestCase):
    def test_fixed_pair_orders_and_b_only_process_counts(self):
        self.assertEqual("".join(side for _, side in process_order("A/B", 0)), "ABBAAB")
        self.assertEqual("".join(side for _, side in process_order("A/B", 1)), "BAABBA")
        self.assertEqual(process_order("B-only", 2), [(0, "B"), (1, "B"), (2, "B")])
        with self.assertRaises(ValueError):
            process_order("P8-single-sided", 0)

    def test_owner_requires_full_semantics_and_exact_block_count(self):
        identity, text = owner_log()
        unit = dict(adapter="owner", gate="p0-legacy", expected_identity=identity, requests_per_process=8,
                    required_metrics=["validated_trace", "playback_only", "read_long"], config="Release")
        parsed = parse_result(unit, text, 0)
        self.assertEqual(parsed["parsed"]["statistics"]["read_long"]["n"], 8)
        self.assertIsNone(parsed["parsed"]["statistics"]["read_long"]["p99_us"])
        for changed in (dict(unit, expected_identity={"allocator": "normal"}),
                        dict(unit, requests_per_process=3),
                        dict(unit, expected_identity=dict(identity, digest=2))):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                parse_result(changed, text, 0)

    def test_host_absolute_gate_preserves_insufficient_and_over_budget_results(self):
        for count, status in ((32, "insufficient_samples"), (1000, "over_budget")):
            identity = dict(measurement_schema="pd4.present-return.v1", receipt_mode="asynchronous",
                            digest=1, source="fixture", viewport=[1280, 720], objects=1,
                            warmup=64, measured=count, audio=False, gpu=False, recording=False, closed=True)
            rows = ["PD_HOST_ID " + json.dumps(identity)]
            for ordinal in range(count):
                rows.append("PD_HOST_SAMPLE " + json.dumps(dict(ordinal=ordinal,
                    request_to_ready_us=10000, ready_to_present_return_us=1,
                    request_to_present_return_us=10001, present_call_to_return_us=1)))
            unit = dict(adapter="host-common", expected_identity=identity,
                        requests_per_process=count, required_metrics=["request_to_ready_us"], config="Release")
            parsed = parse_result(unit, "\n".join(rows), 0)
            self.assertEqual(parsed["absolute_gates"]["request_to_ready"]["status"], status)
            self.assertEqual(parsed["parsed"]["statistics"]["request_to_ready_us"]["n"], count)


if __name__ == "__main__":
    unittest.main()
