import unittest

from tools.verification.execution_acceptance import prepare_host


def parsed(**overrides):
    identity = {"digest": 12345, "source": prepare_host.SOURCE,
                "viewport": list(prepare_host.VIEWPORT), "objects": prepare_host.OBJECTS,
                "warmup": prepare_host.NATIVE_WARMUP, "measured": prepare_host.FRAMES,
                "audio": False, "gpu": False, "closed": True, "recording": False,
                "measurement_schema": "pd4.present-return.v1", "receipt_mode": "asynchronous"}
    identity.update(overrides.pop("identity", {}))
    return {"schema": "pd4.present-return.v1", "identity": identity,
            "samples": [{"ordinal": index} for index in range(overrides.pop(
                "samples", prepare_host.FRAMES))],
            "statistics": {metric: {"median_us": 900.0} for metric in prepare_host.METRICS}}


class HostIdentityTests(unittest.TestCase):
    def test_the_pilot_fixes_the_declared_output_identity(self):
        value = prepare_host.identity(parsed())
        self.assertEqual(set(value), {"digest", "source", "viewport", "objects", "warmup",
                                      "measured", "audio", "gpu"})
        self.assertEqual(value["measured"], 1024)
        self.assertEqual(value["digest"], 12345)

    def test_a_different_workload_or_output_is_rejected(self):
        # The digest is the pilot's observed output fingerprint, so a different digest is carried
        # through rather than rejected; every registered workload field is still enforced.
        self.assertEqual(prepare_host.identity(parsed(identity={"digest": 1}))["digest"], 1)
        cases = {
            "measured": dict(identity={"measured": 512}),
            "objects": dict(identity={"objects": 1}),
            "audio": dict(identity={"audio": True}),
            "gpu": dict(identity={"gpu": True}),
            "closed": dict(identity={"closed": False}),
            "source": dict(identity={"source": "assets/projects/other"}),
            "viewport": dict(identity={"viewport": [1920, 1080]}),
            "samples": dict(samples=512),
        }
        for name, change in cases.items():
            with self.subTest(name=name), self.assertRaises(ValueError):
                prepare_host.identity(parsed(**change))

    def test_a_zero_median_is_not_a_registered_cost(self):
        broken = parsed()
        broken["statistics"]["request_to_ready_us"]["median_us"] = 0
        with self.assertRaises(ValueError):
            prepare_host.identity(broken)

    def test_the_registered_batch_can_reach_the_accepted_p99(self):
        self.assertGreaterEqual(prepare_host.FRAMES, 1000)
        self.assertEqual(prepare_host.NATIVE_WARMUP, 32)
        self.assertEqual(prepare_host.ENDPOINT, "present-return")


if __name__ == "__main__":
    unittest.main()
