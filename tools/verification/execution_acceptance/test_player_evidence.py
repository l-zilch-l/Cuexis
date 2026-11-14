"""Offline Player trajectory and collector tests; no native Player is launched."""
import copy
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from . import player_evidence as player
from .binding import artifact, read, write_once


ENDPOINT = "{0.0.0.00000000}.{0218a919-eb31-46ab-bba3-4b6beabe528c}"


def audio_defaults():
    return [dict(role=role, endpoint_id=ENDPOINT, state=1, interface_name="Senary Audio")
            for role in ("console", "multimedia", "communications")]


def all_hit():
    inputs = []
    for index in range(19):
        tick, lane = 3000000 + 1000000 * index, list(player.KEYS)[index % 4]
        inputs.extend((dict(tick=tick, action="press", channel=lane),
                       dict(tick=tick + 30000, action="release", channel=lane)))
    return dict(name="all-hit", end=35000000, inputs=inputs, score=46, hits=21, misses=0)


def trace(kind, *, summary_changes=None, identity_changes=None):
    gameplay = kind == "dfjk"
    duration = (player.DFJK_CLOSE_US if gameplay else player.MUSIC_CLOSE_US) + 1000
    native = dict(pid=7, tid=8, process_affinity_error=0, process_affinity=3,
                  system_affinity=3, processor_group_error=0, processor_group=0,
                  processor_number=0, process_priority=32, thread_priority=0,
                  process_power_error=0, process_power_control=0, process_power_state=0,
                  thread_power_error=0, thread_power_control=0, thread_power_state=0)
    summary = dict(script_events=39 if gameplay else 1, dispatched=39 if gameplay else 1,
                   actions=19 if gameplay else 0, captured=38 if gameplay else 0, frames=2,
                   trajectory_duration_us=duration, active_trajectory_duration_us=duration - 1000,
                   terminal_wait_us=1000, maximum_source_position_ms=35000 if gameplay else 129031.95,
                   audio_ended=True, terminal_executed=gameplay, judgement_seen=gameplay,
                   score=46 if gameplay else 0, hits=21 if gameplay else 0, misses=0,
                   audio_reads=100, audio_sequence_max=100 if gameplay else 0,
                   audio_boundary="AudioPublication" if gameplay else "IAudioTransport.snapshot",
                   underruns=0, endpoint_id=ENDPOINT,
                   endpoint_method="CoreAudio active render session matching native PID",
                   foreground_checked=True)
    summary.update(summary_changes or {})
    identity = dict(schema="pd5.probe.v1", harness="player", mode="script",
                    metrics_compile_enabled=False, allocator="normal", smoke=False,
                    gpu=True, audio=True, gameplay=gameplay, closed=True,
                    replay_equal=True if gameplay else None,
                    counts=dict(close=1, request_to_ready=2, frame_interval=1))
    identity.update(identity_changes or {})
    rows = [("PD5_PROCESS_START ", dict(pid=7, qpc=1, frequency=1000000,
                                        boundary="before-native-CreateProcess")),
            ("PD_ENV ", dict(native, label="begin"))]
    for metric, count in dict(close=1, request_to_ready=2, frame_interval=1).items():
        for ordinal in range(count):
            rows.append(("PD5_SAMPLE ", dict(metric=metric, ordinal=ordinal,
                elapsed_us=50, start_us=1000, confirmed_us=1050, first_present_us=1050,
                complete=True)))
    rows.extend((("PD_ENV ", dict(native, label="end")),
                 ("PD5_FIRST_FRAME ", dict(qpc=2001, frequency=1000000)),
                 ("PD5_PLAYER ", summary), ("PD5_ID ", identity)))
    return "\n".join(prefix + json.dumps(value) for prefix, value in rows)


class PlayerEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bundle = self.root / "bundle"
        self.inventory = dict(schema="pd5.device-inventory.v1", audio_defaults=audio_defaults(),
                              contents={})
        for kind, (case, binding, package) in player.KINDS.items():
            case_root = self.bundle / case
            case_root.mkdir(parents=True)
            (case_root / package).write_bytes((kind + " package").encode())
            files = [case_root / package]
            if kind == "dfjk":
                write_once(case_root / "verification-cases.json", [all_hit()])
                write_once(case_root / "gameplay-configuration.json", {"testOnly": True})
                (case_root / "gameplay.packed").write_bytes(b"packed fixture")
                files.extend(case_root / name for name in
                             ("verification-cases.json", "gameplay-configuration.json",
                              "gameplay.packed"))
            settings = case_root / "settings/Cuexis"
            (settings / "audio-profiles").mkdir(parents=True)
            write_once(settings / "preferences.json", player.PREFERENCES)
            write_once(settings / "audio-profiles/system-default.json", player.AUDIO_PROFILE)
            document = dict(head="b" * 40, player_sha256="0" * 64,
                             files={path.relative_to(self.bundle).as_posix(): artifact(path)["sha256"]
                                    for path in files})
            write_once(self.bundle / binding, document)
            self.inventory["contents"][kind] = dict(
                binding_sha256=artifact(self.bundle / binding)["sha256"], files=document["files"])
        self.inventory_path = self.root / "inventory.json"
        write_once(self.inventory_path, self.inventory)

    def parse(self, kind, **changes):
        return player.parse_log(trace(kind, **changes), kind=kind,
                                 events=player.script_events(self.bundle, kind),
                                 inventory=self.inventory, affinity=3)

    def test_scripts_preserve_the_complete_independent_courses(self):
        self.assertEqual(player.script_events(self.bundle, "music"), [(130000000, 3, 0, 0)])
        events = player.script_events(self.bundle, "dfjk")
        self.assertEqual(len(events), 39)
        self.assertEqual(events[-1], (35200000, 3, 0, 0))
        self.assertEqual(events[0:2], [(3000000, 0, 7, 1), (3030000, 0, 7, 0)])
        self.assertEqual({row[2] for row in events[:-1]}, {7, 9, 13, 14})
        self.assertEqual(sum(row[3] for row in events[:-1]), 19)
        path = self.root / "script.txt"
        path.write_text(player.script_text(events), encoding="ascii")
        self.assertEqual(player.read_script(path), events)

    def test_scripts_reject_a_changed_or_incomplete_dfjk_oracle(self):
        for mutate in (lambda case: case["inputs"].pop(),
                       lambda case: case.update(score=45),
                       lambda case: case["inputs"][0].update(action="repeat"),
                       lambda case: case["inputs"][0].update(channel="lane.s"),
                       lambda case: case["inputs"][0].update(tick=True),
                       lambda case: case["inputs"][1].update(tick=1)):
            case = all_hit()
            mutate(case)
            path = self.bundle / "keyboard-release/verification-cases.json"
            path.write_text(json.dumps([case]), encoding="utf-8")
            with self.subTest(mutation=mutate), self.assertRaises(ValueError):
                player.script_events(self.bundle, "dfjk")

    def test_valid_traces_are_functional_observations_without_acceptance_claims(self):
        for kind in player.KINDS:
            result = self.parse(kind)
            self.assertIs(result["valid"], True)
            for field in ("acceptance", "formal_registration", "normal_timing_acceptance",
                          "physical_input_observed", "listening_observed", "retention_600_seconds"):
                self.assertIs(result[field], False)
            self.assertEqual(result["formal_attempts_consumed"], 0)
            self.assertEqual(result["statistics"]["load"]["median_us"], 2000)
            self.assertNotIn("absolute_gates", result)
            self.assertEqual(result["identity"]["replay_equal"], True if kind == "dfjk" else None)

    def test_dfjk_requires_dispatch_capture_judgement_terminal_audio_and_focus(self):
        for key, value in dict(script_events=38, dispatched=38, actions=38, captured=37,
                               score=45, hits=20, misses=1, terminal_executed=False,
                               judgement_seen=False, underruns=1, foreground_checked=False,
                               endpoint_id="other", audio_boundary="IAudioTransport.snapshot",
                               audio_sequence_max=0, active_trajectory_duration_us=0,
                               terminal_wait_us=-1, audio_reads=0).items():
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.parse("dfjk", summary_changes={key: value})

    def test_music_requires_full_source_and_eof_after_its_uninterrupted_script(self):
        for change in (dict(audio_ended=False), dict(maximum_source_position_ms=129030.99),
                       dict(actions=1), dict(captured=1), dict(dispatched=0),
                       dict(trajectory_duration_us=129000000), dict(underruns=1)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.parse("music", summary_changes=change)
        with self.assertRaises(ValueError):
            player.parse_log(trace("music"), kind="music", inventory=self.inventory,
                             events=[(1000000, 1, 44, 1), (130000000, 3, 0, 0)])

    def test_native_identity_and_startup_cannot_be_borrowed_from_another_probe(self):
        for change in (dict(harness="matrix"), dict(mode="stream"), dict(audio=False),
                       dict(gpu=False), dict(smoke=True), dict(closed=False),
                       dict(replay_equal=False), dict(metrics_compile_enabled=True)):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.parse("dfjk", identity_changes=change)
        text = trace("dfjk")
        events = player.script_events(self.bundle, "dfjk")
        for broken in ("\n".join(row for row in text.splitlines()
                                  if not row.startswith("PD5_FIRST_FRAME ")),
                       text + '\nPD5_PLAYER {}', text.replace('"pid": 7', '"pid": 9', 1)):
            with self.assertRaises(ValueError):
                player.parse_log(broken, kind="dfjk", events=events, inventory=self.inventory)
        with self.assertRaises(ValueError):
            player.parse_log(text, kind="dfjk", events=events, inventory=self.inventory, exit_code=1)

    def test_bundle_binds_content_and_the_exact_release_settings(self):
        bindings = player.bundle_bindings(self.bundle, "music", self.inventory)
        self.assertIn("Cuexis/preferences.json", bindings["settings"])
        preferences = self.bundle / "music-release/settings/Cuexis/preferences.json"
        preferences.write_text(json.dumps(dict(player.PREFERENCES, vsync=False)), encoding="utf-8")
        with self.assertRaises(ValueError):
            player.bundle_bindings(self.bundle, "music", self.inventory)
        with self.assertRaises(ValueError):
            player.bundle_bindings(self.bundle, "dfjk", dict(self.inventory, contents={}))
        (self.bundle / "keyboard-release/practice.cxc").write_bytes(b"changed content")
        with self.assertRaises(ValueError):
            player.bundle_bindings(self.bundle, "dfjk", self.inventory)

    def test_monitor_allows_the_new_player_foreground_but_rejects_ac_conflicts_and_power_changes(self):
        rows = [dict(monotonic=index, duration_seconds=0, conflicts=[], foreground_pid=pid,
                      ac_line_status=1, power_scheme_bytes="same")
                for index, pid in enumerate((100, 200))]
        monitor = player.PlayerEnvironmentMonitor(snapshot=iter(rows).__next__)
        monitor.sample()
        monitor.sample()
        self.assertIs(monitor.result()["passed"], True)
        self.assertEqual(monitor.result()["foreground_policy"], "native-player-focus")
        for changes in (dict(ac_line_status=0), dict(conflicts=[{"name": "ninja.exe"}]),
                        dict(power_scheme_bytes="changed")):
            snapshots = [rows[0], dict(rows[1], **changes)]
            monitor = player.PlayerEnvironmentMonitor(snapshot=iter(snapshots).__next__)
            monitor.sample()
            monitor.sample()
            with self.subTest(changes=changes):
                self.assertIs(monitor.result()["passed"], False)

    def arguments(self, kind):
        binary = self.root / "cuexis_player.exe"
        binary.write_bytes(b"synthetic Player; never executed")
        binding = self.root / (kind + "-build.json")
        write_once(binding, dict(config="Release", binary=artifact(binary)))
        return SimpleNamespace(source=self.root, bundle=self.bundle, kind=kind,
                               build_binding=binding, output=self.root / (kind + "-output"),
                               script=None, inventory=self.inventory_path)

    def collect(self, args, invoke):
        monitor_type = player.PlayerEnvironmentMonitor
        clock = iter(range(100))
        def snapshot():
            return dict(monotonic=next(clock), duration_seconds=0, conflicts=[], foreground_pid=7,
                        ac_line_status=1, power_scheme_bytes="same")
        with patch.object(player, "clean_source", return_value={"sha": "a" * 40}), \
                patch.object(player.build_binding, "verify") as verify, \
                patch.object(player.process_control, "affinity", return_value=3), \
                patch.object(player.device_inventory, "default_audio", return_value=audio_defaults()), \
                patch.object(player, "PlayerEnvironmentMonitor",
                             side_effect=lambda: monitor_type(snapshot=snapshot)), \
                patch.object(player.process_control, "run", side_effect=invoke) as child:
            result = player.run(args)
            self.assertTrue(all(call.args[2] == "player-trace" for call in verify.call_args_list))
            return result, child

    def test_collection_copies_settings_sanitizes_observers_and_runs_exactly_once(self):
        for kind in player.KINDS:
            args = self.arguments(kind)
            original = copy.deepcopy(player.bundle_bindings(self.bundle, kind, self.inventory))
            observed = {}
            def invoke(command, mask, **options):
                observed.update(command=command, mask=mask, **options)
                options["output"].write(trace(kind).encode())
                return SimpleNamespace(returncode=0)
            with patch.dict("os.environ", {"APPDATA": "user-settings",
                            "CUEXIS_PROCESS_GPU_MEMORY_LOG": "inherited-gpu-log",
                            "CUEXIS_PD5_PLAYER_SCRIPT": "inherited-script"}):
                result, child = self.collect(args, invoke)
            self.assertIs(result["valid"], True)
            self.assertEqual(child.call_count, 1)
            self.assertEqual(observed["timeout"], 180)
            self.assertEqual(observed["env"]["APPDATA"], str(args.output / "settings"))
            self.assertEqual(observed["env"]["CUEXIS_PD5_PLAYER_SCRIPT"],
                             str(args.output / "script.txt"))
            self.assertNotIn("CUEXIS_PROCESS_GPU_MEMORY_LOG", observed["env"])
            self.assertEqual(observed["env"]["SDL_WINDOW_ACTIVATE_WHEN_SHOWN"], "1")
            declaration = read(args.output / "declaration.json")
            self.assertEqual(declaration["measurement_mode"], "player-trace")
            self.assertIs(declaration["automatic_retry"], False)
            self.assertEqual(declaration["processes"], 1)
            self.assertEqual(declaration["script_events"][-1][0],
                             130000000 if kind == "music" else 35200000)
            self.assertEqual(original, player.bundle_bindings(self.bundle, kind, self.inventory))
            for name, bound in original["settings"].items():
                self.assertEqual(artifact(args.output / "settings" / name)["sha256"], bound["sha256"])
            if kind == "dfjk":
                index = observed["command"].index("--execution-tail-input-deadline")
                self.assertEqual(observed["command"][index + 1], "35100000")
            else:
                self.assertNotIn("--execution-test-only", observed["command"])

    def test_failed_or_timed_out_collection_keeps_raw_logs_without_retrying(self):
        for kind, timeout in (("music", False), ("dfjk", True)):
            args = self.arguments(kind)
            invocations = []
            def invoke(command, mask, **options):
                invocations.append(command)
                options["output"].write(b"partial failed Player log\n")
                if timeout:
                    raise subprocess.TimeoutExpired(command, 180)
                return SimpleNamespace(returncode=1)
            with self.assertRaises((ValueError, subprocess.TimeoutExpired)):
                self.collect(args, invoke)
            self.assertEqual(len(invocations), 1)
            self.assertEqual((args.output / "process.log").read_text(), "partial failed Player log\n")
            stopped = read(args.output / "stopped.json")
            self.assertEqual(stopped["formal_attempts_consumed"], 0)
            self.assertIs(stopped["automatic_retry"], False)
            self.assertFalse((args.output / "result.json").exists())
            with self.assertRaises(FileExistsError):
                player.run(args)

    def test_changed_supplied_script_is_rejected_before_a_child_can_launch(self):
        args = self.arguments("music")
        args.script = self.root / "short-script.txt"
        args.script.write_text("1000000 3 0 0\n", encoding="ascii")
        with patch.object(player, "clean_source", return_value={"sha": "a" * 40}), \
                patch.object(player.build_binding, "verify"), \
                patch.object(player.process_control, "run") as child:
            with self.assertRaises(ValueError):
                player.run(args)
            child.assert_not_called()
        self.assertFalse(args.output.exists())


if __name__ == "__main__":
    unittest.main()
