"""Control budgets use every operation and the native process-start boundary."""
import json
import unittest

from . import prepare_controls, runner


def control_log(*, elapsed=None, startup_us=1000, startup=True):
    elapsed = elapsed or {}
    identity = dict(mode="controls", frames=512, warmup_frames=32, cycles=10, rate=240,
                    source_step_ticks=7000, source_tick_rate=1000000, audio=False,
                    closed=True, replay_equal=True, counts=prepare_controls.counts(),
                    schema="pd5.probe.v1", metrics_compile_enabled=False, allocator="normal")
    rows = ["PD5_ID " + json.dumps(identity)]
    for metric, count in identity["counts"].items():
        for ordinal in range(count):
            value = elapsed.get(metric, 1000)
            value = value[ordinal] if isinstance(value, list) else value
            row = dict(metric=metric, ordinal=ordinal, elapsed_us=value, complete=True)
            if metric in prepare_controls.CONTROL_METRICS:
                row.update(start_us=1, confirmed_us=1, first_present_us=1 + value)
            rows.append("PD5_SAMPLE " + json.dumps(row))
    if startup:
        rows.extend((
            'PD5_PROCESS_START {"frequency":1000000,"qpc":1,"pid":1,'
            '"boundary":"before-native-CreateProcess"}',
            "PD5_FIRST_FRAME " + json.dumps(dict(frequency=1000000, qpc=1 + startup_us))))
    return identity, "\n".join(rows)


def record(identity):
    return dict(adapter="controls", gate="p5-controls", expected_identity=identity,
                requests_per_process=prepare_controls.sample_count(),
                required_metrics=list(prepare_controls.METRICS), config="Release")


class ControlsAbsoluteTests(unittest.TestCase):
    def test_each_absolute_budget_accepts_its_exact_maximum(self):
        values = dict(pause=50000, resume=50000, resize=50000, reset_paused=1000000,
                      reset_running=1000000, close=1000000)
        identity, text = control_log(elapsed=values, startup_us=2000000)
        absolute = runner.parse_result(record(identity), text, 0)["absolute_gates"]
        self.assertEqual(set(absolute), {*values, "load"})
        self.assertTrue(all(value["status"] == "pass" for value in absolute.values()))
        self.assertTrue(all(value["statistic"] == "max_us" for value in absolute.values()))

    def test_a_single_slow_control_is_preserved_as_over_budget(self):
        for metric, limit in dict(pause=50000, resume=50000, resize=50000,
                                  reset_paused=1000000, reset_running=1000000,
                                  close=1000000).items():
            values = [1000] * prepare_controls.counts()[metric]
            values[-1] = limit + 1
            identity, text = control_log(elapsed={metric: values})
            with self.subTest(metric=metric):
                parsed = runner.parse_result(record(identity), text, 0)
                self.assertEqual(parsed["absolute_gates"][metric]["status"], "over_budget")
                self.assertEqual(parsed["absolute_gates"][metric]["statistics"]["max_us"],
                                 limit + 1)

    def test_load_host_never_substitutes_for_process_start_to_first_frame(self):
        identity, text = control_log(elapsed={"load_host": 1}, startup_us=2000001)
        parsed = runner.parse_result(record(identity), text, 0)
        self.assertEqual(parsed["absolute_gates"]["load"]["status"], "over_budget")
        self.assertNotIn("load_host", parsed["absolute_gates"])
        identity, text = control_log(elapsed={"load_host": 3000000}, startup_us=1)
        self.assertEqual(runner.parse_result(record(identity), text, 0)
                         ["absolute_gates"]["load"]["status"], "pass")

    def test_missing_startup_cannot_be_hidden_by_a_changed_registration_count(self):
        identity, text = control_log(startup=False)
        unit = dict(record(identity), requests_per_process=sum(identity["counts"].values()))
        with self.assertRaises(ValueError):
            runner.parse_result(unit, text, 0)


if __name__ == "__main__":
    unittest.main()
