"""Observer gates require exact output and never invent an accepted perturbation limit."""
import copy
import unittest

from . import prepare_stream, stream_observers


def rows():
    result = []
    for pair in range(3):
        for kind in stream_observers.KINDS:
            identity = prepare_stream.expected_fields(123)
            if kind == "gpu":
                identity["gpu_timer"] = True
                identity["counts"]["gpu_render"] = prepare_stream.FRAMES
            if kind == "memory":
                identity["allocator"] = "lifetime-observer"
            if kind == "stage":
                identity["metrics_compile_enabled"] = True
            parsed = {"identity": identity,
                      "statistics": {metric: {"median_us": 100} for metric in
                                     ("request_to_ready", "request_to_present_return", "cpu_submit")},
                      "samples": [{"metric": "gpu_render", "elapsed_us": 100}]
                                 * prepare_stream.FRAMES if kind == "gpu" else []}
            row = {"pair": pair, "kind": kind, "parsed": parsed, "elapsed_seconds": 70}
            if kind == "stage":
                row["stages"] = {stage: {"inclusive_us": [100] * prepare_stream.FRAMES}
                                 for stage in stream_observers.STAGES}
            result.append(row)
    return result


class StreamObserverTests(unittest.TestCase):
    def test_high_submit_perturbation_does_not_grant_gpu_budget(self):
        observed = rows()
        self.assertFalse(stream_observers.summarize(observed)["observer_admitted"]["gpu"])
        observed[1]["parsed"]["statistics"]["cpu_submit"]["median_us"] = 130
        summary = stream_observers.summarize(observed)
        self.assertFalse(summary["observer_admitted"]["gpu"])
        self.assertEqual(summary["stage_and_gpu_gates"][0]["status"], "observer_unvalidated")

    def test_four_stage_exception_discloses_cost_without_a_new_numerical_limit(self):
        observed = rows()
        observed[2]["parsed"]["statistics"]["cpu_submit"]["median_us"] = 200
        summary = stream_observers.summarize(observed)
        self.assertTrue(summary["observer_admitted"]["stage"])
        self.assertEqual(summary["overhead"]["stage"][0]["endpoint_median_ratios"]["cpu_submit"], 2)
        self.assertFalse(summary["formal_acceptance"])

    def test_missing_query_or_changed_output_is_rejected(self):
        for change in (lambda value: value[1]["parsed"]["samples"].pop(),
                       lambda value: value[1]["parsed"]["identity"].update(digest=456),
                       lambda value: value.pop()):
            observed = copy.deepcopy(rows())
            change(observed)
            with self.assertRaises(ValueError):
                stream_observers.summarize(observed)


if __name__ == "__main__":
    unittest.main()
