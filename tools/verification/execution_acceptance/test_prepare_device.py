"""Device probe preparation: test-case selection and fail-closed identity checks."""
import json
from pathlib import Path
import unittest

from tools.verification.execution_acceptance import prepare_probe, probe_units
from tools.verification.execution_acceptance.runner import parse_result

SMALL_FRAMES = 64
SMALL_WARMUP = 8


def device_log(unit, frames, warmup):
    identity = probe_units.identity(unit, frames, warmup, device=True)
    rows = ["PD5_ID " + json.dumps(identity)]
    for metric, count in identity["counts"].items():
        rows.extend("PD5_SAMPLE " + json.dumps(dict(metric=metric, ordinal=ordinal,
                                                    elapsed_us=1000 + ordinal % 7,
                                                    complete=True))
                    for ordinal in range(count))
    return identity, "\n".join(rows)


class PrepareDeviceTests(unittest.TestCase):
    def test_required_test_case_follows_the_selected_backend(self):
        self.assertEqual(prepare_probe.required_test_case(prepare_probe.CPU_EXECUTABLE, False),
                         probe_units.PROBE_PREFIX)
        self.assertEqual(
            prepare_probe.required_test_case(Path("bin") / prepare_probe.DEVICE_EXECUTABLE, True),
            "PD5 renderer and capture workload matrix")
        with self.assertRaises(ValueError):
            prepare_probe.required_test_case(prepare_probe.CPU_EXECUTABLE, True)
        with self.assertRaises(ValueError):
            prepare_probe.required_test_case(prepare_probe.DEVICE_EXECUTABLE, False)
        spec = prepare_probe.launch(Path("probe.exe"), Path("warmup.exe"), Path("."), 1,
                                    probe_units.registered("p4-capture-1"),
                                    prepare_probe.DEVICE_TEST)
        self.assertEqual(spec["command"], ["probe.exe", prepare_probe.DEVICE_TEST])

    def test_device_identity_declares_the_renderer_consumer_and_its_counts(self):
        unit = probe_units.registered("p4-capture-1")
        expected = prepare_probe.expected_identity(unit, device=True)
        self.assertIs(expected["gpu"], True)
        self.assertEqual((expected["harness"], expected["renderer_consumers"]),
                         (probe_units.DEVICE_HARNESS, 1))
        self.assertEqual(expected["materializations"], probe_units.FRAMES * 2)
        self.assertEqual(probe_units.counts(unit, SMALL_FRAMES, SMALL_WARMUP, device=True),
                         {"sdk_update": SMALL_FRAMES, "materialize": SMALL_FRAMES * 2,
                          "capture_delivery": SMALL_FRAMES,
                          "renderer_delivery": SMALL_FRAMES, "cpu_submit": SMALL_FRAMES,
                          "present_call": SMALL_FRAMES})
        four = probe_units.registered("p4-capture-4")
        self.assertEqual(
            probe_units.counts(four, SMALL_FRAMES, SMALL_WARMUP, device=True)["materialize"],
            SMALL_FRAMES * 5)
        self.assertEqual(
            probe_units.counts(four, SMALL_FRAMES, SMALL_WARMUP,
                               device=True)["capture_delivery"], SMALL_FRAMES * 4)

    def test_device_preparation_rejects_a_gpu_false_observation(self):
        unit = probe_units.registered("p4-capture-1")
        observed = dict(probe_units.identity(unit, device=True), gpu=False)
        with self.assertRaises(ValueError):
            prepare_probe.verify_identity(unit, {"identity": observed}, device=True)
        # The CPU harness is not a device observation even if its key set is the same.
        with self.assertRaises(ValueError):
            prepare_probe.verify_identity(unit, {"identity": probe_units.identity(unit)},
                                          device=True)

    def test_non_device_preparation_is_unaffected(self):
        unit = probe_units.registered("p4-capture-1")
        expected = prepare_probe.expected_identity(unit)
        self.assertIs(expected["gpu"], False)
        self.assertEqual((expected["harness"], expected["renderer_consumers"]),
                         (probe_units.HARNESS, 0))
        self.assertEqual(expected["materializations"], probe_units.FRAMES)
        self.assertEqual(probe_units.counts(unit, SMALL_FRAMES, SMALL_WARMUP),
                         {"sdk_update": SMALL_FRAMES, "materialize": SMALL_FRAMES,
                          "capture_delivery": SMALL_FRAMES})
        parsed = {"identity": probe_units.identity(unit)}
        self.assertEqual(prepare_probe.verify_identity(unit, parsed), expected)
        with self.assertRaises(ValueError):
            prepare_probe.verify_identity(unit, {"identity": probe_units.identity(
                unit, device=True)})

    def test_device_identity_rejects_a_cpu_only_workload(self):
        unit = probe_units.registered("p1-no-consumer")
        with self.assertRaises(ValueError):
            prepare_probe.expected_identity(unit, device=True)
        self.assertIs(prepare_probe.expected_identity(unit)["gpu"], False)

    def test_device_log_passes_the_unchanged_runner_identity_check(self):
        unit = probe_units.registered("p4-capture-1")
        identity, text = device_log(unit, SMALL_FRAMES, SMALL_WARMUP)
        record = {"adapter": "probe", "gate": unit["gate"], "config": "Release",
                  "expected_identity": probe_units.expected_fields(
                      unit, SMALL_FRAMES, SMALL_WARMUP, device=True),
                  "requests_per_process": SMALL_FRAMES,
                  "required_metrics": ["capture_delivery"], "absolute_gate_metrics": []}
        parsed = parse_result(record, text, 0)
        self.assertEqual(set(parsed["medians"]), {"capture_delivery"})
        self.assertEqual(identity["counts"], record["expected_identity"]["counts"])
        # The CPU identity rejects the same device log, so gpu is not the only fail-closed field.
        record["expected_identity"] = probe_units.expected_fields(
            unit, SMALL_FRAMES, SMALL_WARMUP)
        with self.assertRaises(ValueError):
            parse_result(record, text, 0)


if __name__ == "__main__":
    unittest.main()
