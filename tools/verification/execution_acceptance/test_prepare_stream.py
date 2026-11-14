"""P3 admits only the fixed device host stream, with complete startup and frame samples."""
import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from . import admission, prepare_controls, prepare_stream, runner
from .binding import artifact, read, write_once
from .runtime_monitor import POLICY
from .test_controls_absolute import control_log


def record(identity=None):
    spec = prepare_stream.launch(Path(prepare_stream.PROBE_EXECUTABLE), Path("warmup.exe"),
                                 Path("."), 1)
    return prepare_controls.unit_record(prepare_stream, spec,
                                        identity or prepare_stream.expected_fields(123), {})


def parsed():
    return {"identity": prepare_stream.expected_fields(123),
            "samples": [None] * prepare_stream.sample_count(),
            "statistics": {metric: {"median_us": 1}
                           for metric in (*prepare_stream.METRICS, "load")}}


def stream_log(*, metric_values=None, startup_us=1000):
    metric_values = metric_values or {}
    value = prepare_stream.expected_fields(123)
    rows = ["PD5_ID " + json.dumps(value)]
    for metric, count in value["counts"].items():
        values = metric_values.get(metric, 1000)
        for ordinal in range(count):
            elapsed = values[ordinal] if isinstance(values, list) else values
            rows.append(f'PD5_SAMPLE {{"metric":"{metric}","ordinal":{ordinal},'
                        f'"elapsed_us":{elapsed},"complete":true}}')
    rows.extend((
        'PD5_PROCESS_START {"frequency":1000000,"qpc":1,"pid":1,'
        '"boundary":"before-native-CreateProcess"}',
        "PD5_FIRST_FRAME " + json.dumps(dict(frequency=1000000, qpc=1 + startup_us))))
    return "\n".join(rows)


class StreamPreparationTests(unittest.TestCase):
    def test_counts_include_each_renderer_endpoint_and_the_separate_startup_sample(self):
        self.assertEqual(prepare_stream.counts(), {
            "load_host": 1, "close": 1, "request_to_ready": 14400,
            "request_to_present_return": 14400, "cpu_submit": 14400, "present_call": 14400})
        self.assertEqual(prepare_stream.sample_count(), 57603)
        self.assertEqual(prepare_controls.sample_count(), 73)
        self.assertEqual(prepare_stream.PROFILE, "gpu-stream-v1")
        self.assertEqual(prepare_stream.PILOT_FIELDS["entities"], 1)

    def test_launch_keeps_the_fixed_pacing_and_declares_inactive_window_creation(self):
        unit = record()
        spec = unit["launch"]["B"]
        env = spec["environment"]
        self.assertEqual(spec["command"], [prepare_stream.PROBE_EXECUTABLE,
                                             "PD5 scripted host timing probe"])
        self.assertEqual((env["CUEXIS_PD5_FRAMES"], env["CUEXIS_PD5_WARMUP"],
                          env["CUEXIS_PD5_RATE"]), ("14400", "2400", "240"))
        self.assertEqual((env["CUEXIS_PD5_SMOKE"], env["CUEXIS_PD5_GPU_TIMER"]), ("0", "0"))
        self.assertEqual(env["SDL_WINDOW_ACTIVATE_WHEN_SHOWN"], "0")
        self.assertEqual(env["SDL_WINDOW_ACTIVATE_WHEN_RAISED"], "0")
        self.assertEqual(admission.stable_id(unit), "p3-renderer-device-release")
        prepare_stream.validate_registration(unit)
        controls = prepare_controls.unit_record(prepare_controls,
            prepare_controls.launch(Path("host.exe"), Path("warmup.exe"), Path("."), 1), {}, {})
        self.assertEqual(controls["adapter"], "controls")
        self.assertEqual(controls["stable_protocol_id"], "p5-controls-release")
        self.assertNotIn("backend", controls)
        self.assertEqual(controls["launch"]["B"]["environment"]["CUEXIS_PD5_FRAMES"], "512")

    def test_identity_rejects_changed_workload_instrumentation_or_incomplete_release(self):
        self.assertEqual(prepare_stream.identity(parsed()), prepare_stream.expected_fields(123))
        changes = {"mode": "capture-1", "frames": 64, "warmup_frames": 32, "rate": 120,
                   "gpu": False, "gpu_timer": True, "audio": True, "smoke": True,
                   "allocator": "lifetime-observer", "metrics_compile_enabled": True,
                   "closed": False, "replay_equal": False, "payload_after_close": 1,
                   "digest": True}
        for key, value in changes.items():
            data = parsed()
            data["identity"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                prepare_stream.identity(data)
        for mutate in (lambda value: value["identity"]["counts"].pop("cpu_submit"),
                       lambda value: value["samples"].pop(),
                       lambda value: value["statistics"].pop("load")):
            data = parsed()
            mutate(data)
            with self.assertRaises(ValueError):
                prepare_stream.identity(data)

    def test_registration_cannot_rename_capture_or_cpu_work_as_the_stream_unit(self):
        unit = record()
        for change in (dict(backend="cpu"), dict(comparison="A/B"),
                       dict(gate="p4-capture-1"), dict(requests_per_process=57602),
                       dict(required_metrics=["request_to_ready"]),
                       dict(expected_identity={"gpu": True})):
            with self.subTest(change=change), self.assertRaises(ValueError):
                prepare_stream.validate_registration(dict(unit, **change))
        for change in ("command", "environment"):
            broken = copy.deepcopy(unit)
            if change == "command":
                broken["launch"]["B"]["command"][0] = "cuexis_pd_host_probe.exe"
            else:
                broken["launch"]["B"]["environment"]["CUEXIS_PD5_GPU_TIMER"] = "1"
            with self.subTest(change=change), self.assertRaises(ValueError):
                prepare_stream.validate_registration(broken)

    def test_preparation_rejects_the_cpu_executable_before_launching_a_pilot(self):
        metadata = {"config": "Release", "toolchain_signature": {"compiler": "same"},
                    "binary": {"path": "cuexis_pd_host_probe.exe"}}
        warmup = dict(metadata, binary={"path": "cuexis_pd_baseline_probe.exe"})
        args = SimpleNamespace(source=Path("."), output=Path("unused-stream-output"),
                               build_binding=Path("candidate.json"),
                               warmup_build_binding=Path("warmup.json"))
        with patch.object(prepare_controls, "clean_source", return_value={}), \
                patch.object(prepare_controls, "read", side_effect=[metadata, warmup]), \
                patch.object(prepare_controls.build_binding, "verify"), \
                patch.object(prepare_controls, "verify_artifact",
                             side_effect=lambda row: row["path"]), \
                patch.object(prepare_controls, "child") as child:
            with self.assertRaisesRegex(ValueError, prepare_stream.PROBE_EXECUTABLE):
                prepare_stream.run(args)
            child.assert_not_called()

    def test_shared_preparation_binds_each_profile_without_claiming_an_attempt(self):
        root = Path(__file__).resolve().parents[3]
        source = {"sha": "a" * 40}
        validator = admission.validate_preparation
        budget = artifact(root / "tools/verification/execution_acceptance/budget-proposal.v1.json")
        for profile in (prepare_controls, prepare_stream):
            with self.subTest(profile=profile.ADAPTER), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                bindings = []
                for executable in (profile.PROBE_EXECUTABLE, "cuexis_pd_baseline_probe.exe"):
                    binary = directory / executable
                    binary.write_bytes(b"synthetic executable; never launched")
                    metadata = dict(config="Release", toolchain_signature={"compiler": "same"},
                                    binary=artifact(binary))
                    binding = directory / (executable + ".json")
                    write_once(binding, metadata)
                    bindings.append(binding)
                acceptance = directory / "acceptance.json"
                authorization = directory / "authorization.json"
                ledger = directory / "ledger"
                write_once(acceptance, {
                    "schema": "rpa.r4.o1-acceptance.v1", "accepted": True,
                    "accepted_input_sha256": {
                        "tools/verification/execution_acceptance/budget-proposal.v1.json":
                        budget["sha256"]}})
                write_once(authorization, {
                    "schema": "rpa.r4.execution-authorization.v1",
                    "automatic_work_authorized": True, "attempt_ledger": str(ledger)})
                warm_log, sample_log = directory / "warmup.log", directory / "sample.log"
                warm_log.write_text("synthetic warmup", encoding="utf-8")
                text = stream_log() if profile is prepare_stream else control_log()[1]
                sample_log.write_text(text, encoding="utf-8")
                args = SimpleNamespace(source=root, output=directory / "output",
                                       build_binding=bindings[0], warmup_build_binding=bindings[1],
                                       acceptance=acceptance, authorization=authorization)
                with patch.object(prepare_controls, "clean_source", return_value=source), \
                        patch.object(prepare_controls.build_binding, "verify"), \
                        patch.object(prepare_controls, "capture_cpu",
                                     return_value={"machine": {"parent_affinity": 1}}), \
                        patch.object(prepare_controls, "verify_cpu"), \
                        patch.object(prepare_controls, "verify_native", return_value={}), \
                        patch.object(prepare_controls, "verify_warmup"), \
                        patch.object(prepare_controls, "child", side_effect=[
                            (warm_log, [artifact(warm_log)]),
                            (sample_log, [artifact(sample_log)])]) as child, \
                        patch.object(admission, "validate_preparation", side_effect=lambda value:
                                     validator(value, source_verifier=lambda bound: None)), \
                        redirect_stdout(io.StringIO()):
                    profile.run(args)
                preparation = read(args.output / "preparation.json")
                declaration = read(args.output / "pilot-declaration.json")
                unit = preparation["units"][0]
                self.assertEqual(unit["adapter"], profile.ADAPTER)
                self.assertEqual(unit["expected_identity"]["counts"], profile.counts())
                self.assertEqual(unit["requests_per_process"], profile.sample_count())
                self.assertEqual(declaration["declared_counts"], profile.counts())
                self.assertIs(preparation["formal_registration"], False)
                self.assertIs(declaration["formal_sampling"], False)
                self.assertEqual(child.call_count, 2)
                self.assertEqual(child.call_args.args[2], unit["launch"]["B"])
                self.assertFalse(ledger.exists())
                self.assertFalse((args.output / "stopped.json").exists())
                if profile is prepare_stream:
                    prepare_stream.validate_registration(unit)
                    self.assertIs(declaration["gpu_timer"], False)
                    self.assertIn("device-host-probe", preparation["fixture_bindings"])
                else:
                    self.assertNotIn("backend", unit)

    def test_runner_rejects_an_invalid_backend_before_creating_a_running_marker(self):
        with tempfile.TemporaryDirectory() as temporary:
            seal = Path(temporary) / "seal.json"
            unit = dict(record(), backend="cpu", runtime_monitor_policy=POLICY)
            with patch.object(runner, "read_sealed", return_value={"units": [unit]}):
                with self.assertRaises(ValueError):
                    runner.run(seal, Path(temporary) / "run")
            self.assertFalse(seal.with_suffix(".running.json").exists())


class StreamResultTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = stream_log()

    def test_full_stream_log_keeps_cpu_endpoints_and_judges_only_registered_budgets(self):
        parsed_value = runner.parse_result(record(), self.text, 0)
        self.assertEqual(len(parsed_value["parsed"]["samples"]), 57603)
        self.assertEqual(set(parsed_value["medians"]), set(prepare_stream.METRICS))
        absolute = parsed_value["absolute_gates"]
        self.assertEqual(set(absolute), {"request_to_ready", "load", "close"})
        self.assertTrue(all(value["status"] == "pass" for value in absolute.values()))
        self.assertEqual(absolute["request_to_ready"]["statistic"], "p99_us")
        self.assertEqual(absolute["request_to_ready"]["limit_us"], 1000000 / 240)

    def test_request_to_ready_uses_p99_while_load_and_close_use_maximum(self):
        values = [1000] * prepare_stream.FRAMES
        values[-1] = 100000
        text = stream_log(metric_values={"request_to_ready": values, "close": 1000001,
                                         "load_host": 1}, startup_us=2000001)
        absolute = runner.parse_result(record(), text, 0)["absolute_gates"]
        self.assertEqual(absolute["request_to_ready"]["status"], "pass")
        self.assertEqual(absolute["load"]["status"], "over_budget")
        self.assertEqual(absolute["close"]["status"], "over_budget")
        text = stream_log(metric_values={"request_to_ready": 4167})
        self.assertEqual(runner.parse_result(record(), text, 0)
                         ["absolute_gates"]["request_to_ready"]["status"], "over_budget")

    def test_missing_startup_or_changed_digest_rejects_the_formal_result(self):
        missing = "\n".join(row for row in self.text.splitlines()
                            if not row.startswith("PD5_FIRST_FRAME "))
        with self.assertRaises(ValueError):
            runner.parse_result(record(), missing, 0)
        with self.assertRaises(ValueError):
            runner.parse_result(record(prepare_stream.expected_fields(124)), self.text, 0)
        reference = {"identity": prepare_stream.expected_fields(123)}
        runner.require_same_workload(record(), reference, reference)
        with self.assertRaises(ValueError):
            runner.require_same_workload(record(), reference,
                                         {"identity": prepare_stream.expected_fields(124)})


if __name__ == "__main__":
    unittest.main()
