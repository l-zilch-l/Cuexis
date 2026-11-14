"""One Player functional capture and strict offline trace validation.

The opt-in player-trace build is bound separately from normal performance evidence. The collector
never claims an execution allowance; it preserves a failed invocation without retrying it.
"""
import argparse
import datetime as dt
from pathlib import Path
import shutil
import time

from tools.verification.playback_performance import device_inventory, measurement, process_control

from . import build_binding
from .binding import artifact, clean_source, read, verify_artifact, write_once
from .environment import process_environment, verify_native
from .runtime_monitor import POLICY, RuntimeMonitor


TIMEOUT = 180
MUSIC_CLOSE_US = 130000000
DFJK_CLOSE_US = 35200000
KEYS = {"lane.d": 7, "lane.f": 9, "lane.j": 13, "lane.k": 14}
KINDS = {"music": ("music-release", "binding-release.json", "music.cxc"),
         "dfjk": ("keyboard-release", "binding-keyboard-release.json", "practice.cxc")}
PREFERENCES = {"format": "cuexis.player-preferences", "version": 1,
               "audioDeviceProfileId": "system-default", "vsync": True,
               "windowWidth": 1280, "windowHeight": 720, "gain": .35, "fullscreen": False}
AUDIO_PROFILE = {"format": "cuexis.audio-device-profile", "version": 1,
                 "id": "system-default", "selector": {"kind": "system-default"},
                 "outputCorrectionUs": 0}


def integer(value, name, minimum=0):
    if type(value) is not int or value < minimum:
        raise ValueError("invalid Player integer: " + name)
    return value


def endpoint(inventory):
    if inventory.get("schema") != "pd5.device-inventory.v1":
        raise ValueError("Player requires the bound device inventory")
    rows = inventory.get("audio_defaults", [])
    roles = {row.get("role") for row in rows}
    ids = {row.get("endpoint_id") for row in rows}
    if (len(rows) != 3 or roles != {"console", "multimedia", "communications"} or
            len(ids) != 1 or not isinstance(next(iter(ids)), str) or not next(iter(ids)) or
            any(row.get("state") != 1 or row.get("interface_name") != "Senary Audio"
                for row in rows)):
        raise ValueError("Player requires the inventory's active Senary system-default endpoint")
    return next(iter(ids))


def script_events(bundle, kind):
    """Convert the actual verification-cases list, preserving simultaneous event order."""
    if kind == "music":
        return [(MUSIC_CLOSE_US, 3, 0, 0)]
    if kind != "dfjk":
        raise ValueError("unknown Player functional kind")
    cases = read(Path(bundle) / "keyboard-release/verification-cases.json")
    if not isinstance(cases, list):
        raise ValueError("DFJK verification-cases must be a list")
    selected = [case for case in cases if case.get("name") == "all-hit"]
    if len(selected) != 1:
        raise ValueError("DFJK requires exactly one all-hit case")
    case = selected[0]
    if any(type(case.get(key)) is not int or case[key] != value
           for key, value in dict(end=35000000, score=46, hits=21, misses=0).items()):
        raise ValueError("DFJK all-hit oracle changed")
    inputs = case.get("inputs", [])
    if not isinstance(inputs, list) or len(inputs) != 38:
        raise ValueError("DFJK requires all 38 press/release events")
    events, held, previous, presses = [], set(), 0, 0
    for row in inputs:
        tick = integer(row.get("tick"), "input tick")
        channel, action = row.get("channel"), row.get("action")
        if tick < previous or tick > case["end"] or channel not in KEYS:
            raise ValueError("invalid DFJK input order, channel or timebase")
        if action == "press" and channel not in held:
            held.add(channel)
            presses += 1
        elif action == "release" and channel in held:
            held.remove(channel)
        else:
            raise ValueError("DFJK input is not a balanced press/release course")
        events.append((tick, 0, KEYS[channel], int(action == "press")))
        previous = tick
    if held or presses != 19:
        raise ValueError("DFJK requires 19 complete key actions")
    return [*events, (DFJK_CLOSE_US, 3, 0, 0)]


def script_text(events):
    return "".join(" ".join(str(value) for value in event) + "\n" for event in events)


def read_script(path):
    rows = Path(path).read_text(encoding="ascii").splitlines()
    try:
        events = [tuple(int(value) for value in row.split()) for row in rows if row.strip()]
    except ValueError as error:
        raise ValueError("Player script requires four integer columns") from error
    if not events or any(len(row) != 4 for row in events):
        raise ValueError("Player script requires four integer columns")
    return events


def bundle_bindings(bundle, kind, inventory):
    bundle = Path(bundle).resolve(strict=True)
    case, binding_name, package = KINDS[kind]
    historical = artifact(bundle / binding_name)
    document = read(historical["path"])
    reference = inventory.get("contents", {}).get(kind, {})
    if (reference.get("binding_sha256") != historical["sha256"] or
            reference.get("files") != document.get("files")):
        raise ValueError("Player content differs from the bound device inventory")
    files = {}
    for name, expected in document["files"].items():
        path = (bundle / name).resolve(strict=True)
        if not path.is_relative_to(bundle):
            raise ValueError("bundle input escapes its bound directory")
        files[name] = artifact(path)
        if files[name]["sha256"] != expected:
            raise ValueError("Player content changed: " + name)
    required = {case + "/" + package}
    if kind == "dfjk":
        required.update(case + "/" + name for name in
                        ("verification-cases.json", "gameplay.packed", "gameplay-configuration.json"))
    if not required.issubset(files):
        raise ValueError("bundle omits required Player inputs")
    settings_root = bundle / case / "settings"
    settings = {path.relative_to(settings_root).as_posix(): artifact(path)
                for path in sorted(settings_root.rglob("*")) if path.is_file()}
    expected_settings = {"Cuexis/preferences.json": PREFERENCES,
                         "Cuexis/audio-profiles/system-default.json": AUDIO_PROFILE}
    for name, expected in expected_settings.items():
        if name not in settings or read(settings[name]["path"]) != expected:
            raise ValueError("Player settings differ from the release fixture: " + name)
    return {"historical_binding": historical, "content": files, "settings": settings}


def command(binary, bundle, kind):
    case, _, package = KINDS[kind]
    root = Path(bundle) / case
    result = [str(binary), "--mode", "audio", "--cxc", str(root / package)]
    if kind == "dfjk":
        result.extend(("--candidate-entry", "compiled/chart.v5.json", "--execution-test-only",
                       "--gameplay-packed", str(root / "gameplay.packed"),
                       "--gameplay-configuration", str(root / "gameplay-configuration.json"),
                       "--gameplay-config-budget", "131072,64,8192,16384,8192"))
        for lane, code in KEYS.items():
            result.extend(("--gameplay-key", f"{code}:{lane}:domain.binding.one"))
        result.extend(("--execution-tail-input-deadline", "35100000"))
    return [*result, "--show-fps"]


def parse_log(text, *, kind, events, inventory, exit_code=0, affinity=None):
    """Validate a completed native trace offline, without inferring performance acceptance."""
    if kind not in KINDS:
        raise ValueError("unknown Player functional kind")
    gameplay = kind == "dfjk"
    if (len(events) != (39 if gameplay else 1) or
            events[-1] != ((DFJK_CLOSE_US if gameplay else MUSIC_CLOSE_US), 3, 0, 0)):
        raise ValueError("Player script does not describe the fixed complete trajectory")
    if not gameplay and events != [(MUSIC_CLOSE_US, 3, 0, 0)]:
        raise ValueError("music trajectory may only contain its final Close")
    parsed = measurement.parse_probe(text, exit_code=exit_code)
    identity = parsed["identity"]
    expected = dict(schema="pd5.probe.v1", harness="player", mode="script",
                    metrics_compile_enabled=False, allocator="normal", smoke=False,
                    gpu=True, audio=True, gameplay=gameplay, closed=True,
                    replay_equal=True if gameplay else None)
    if any(type(identity.get(key)) is not type(value) or identity[key] != value
           for key, value in expected.items()):
        raise ValueError("native trace differs from the Player/audio/renderer trajectory")
    rows = measurement.records(text, "PD5_PLAYER ")
    if len(rows) != 1 or not isinstance(rows[0], dict):
        raise ValueError("Player trace requires one complete trajectory summary")
    player = rows[0]
    expected_player = dict(script_events=len(events), dispatched=len(events),
                           actions=19 if gameplay else 0, captured=38 if gameplay else 0,
                           underruns=0, foreground_checked=True,
                           audio_boundary="AudioPublication" if gameplay else "IAudioTransport.snapshot",
                           endpoint_id=endpoint(inventory),
                           endpoint_method="CoreAudio active render session matching native PID",
                           judgement_seen=gameplay, score=46 if gameplay else 0,
                           hits=21 if gameplay else 0, misses=0)
    for key, expected_value in expected_player.items():
        if type(player.get(key)) is not type(expected_value) or player[key] != expected_value:
            raise ValueError("Player trajectory summary differs: " + key)
    frames = integer(player.get("frames"), "frames", 2)
    if identity["counts"] != {"close": 1, "request_to_ready": frames,
                              "frame_interval": frames - 1}:
        raise ValueError("Player frame counts changed or extra controls were inserted")
    duration = integer(player.get("trajectory_duration_us"), "trajectory duration")
    active = integer(player.get("active_trajectory_duration_us"), "active trajectory", 1)
    wait = integer(player.get("terminal_wait_us"), "terminal wait")
    if not events[-1][0] <= duration <= TIMEOUT * 1000000 or active + wait != duration:
        raise ValueError("Player did not complete its declared finite trajectory")
    integer(player.get("audio_reads"), "audio reads", 1)
    integer(player.get("audio_sequence_max"), "audio sequence", int(gameplay))
    position = measurement.positive(player.get("maximum_source_position_ms"), True)
    if type(player.get("audio_ended")) is not bool:
        raise ValueError("missing Player audio EOF observation")
    if gameplay:
        if player.get("terminal_executed") is not True:
            raise ValueError("DFJK requires the completed terminal execution")
    elif player["audio_ended"] is not True or position < 129031:
        raise ValueError("music requires the complete 129.03195-second source and audio EOF")
    starts = measurement.records(text, "PD5_PROCESS_START ")
    native = measurement.records(text, "PD_ENV ")
    if ("load" not in parsed["statistics"] or len(starts) != 1 or len(native) != 2 or
            type(starts[0].get("pid")) is not int or starts[0]["pid"] <= 0 or
            any(row.get("pid") != starts[0]["pid"] for row in native)):
        raise ValueError("Player startup and native process anchors are missing or unrelated")
    verify_native(text, native[0].get("process_affinity") if affinity is None else affinity)
    return {"schema": "rpa.execution.player-functional.v1", "kind": kind, "valid": True,
            "evidence_class": "single-process scripted Player functional evidence",
            "formal_registration": False, "acceptance": False, "formal_attempts_consumed": 0,
            "normal_timing_acceptance": False, "retention_600_seconds": False,
            "physical_input_observed": False, "listening_observed": False,
            "identity": identity, "player": player, "statistics": parsed["statistics"]}


class PlayerEnvironmentMonitor(RuntimeMonitor):
    """Sample AC/build conflicts; the native Player owns its changing foreground window."""
    def __init__(self, *, snapshot=None):
        super().__init__(None, timeout=TIMEOUT, snapshot=snapshot)

    def sample(self):
        try:
            row = self.snapshot()
            self.samples.append(row)
            if row["conflicts"] or row["ac_line_status"] != 1:
                self.errors.append("conflicting process or AC change")
            if row["power_scheme_bytes"] != self.samples[0]["power_scheme_bytes"]:
                self.errors.append("active power scheme changed")
        except Exception as error:
            self.errors.append(str(error))

    def result(self):
        result = super().result()
        result.update(schema="rpa.execution.player-environment.v1",
                      policy=dict(POLICY, id="sampled-player-functional-v1"),
                      foreground_policy="native-player-focus",
                      monitor_scope="sampled AC, power scheme and conflicts; native Player checks focus")
        return result


def run(args):
    root, bundle, output = args.source.resolve(), args.bundle.resolve(), args.output.resolve()
    if output.exists():
        raise FileExistsError("Player functional collection requires a new output directory")
    source = clean_source(root)
    metadata = read(args.build_binding)
    build_binding.verify(metadata, source, "player-trace")
    binary = verify_artifact(metadata["binary"])
    if metadata.get("config") != "Release" or binary.name != "cuexis_player.exe":
        raise ValueError("Player functional collection requires its Release player-trace executable")
    inventory_path = getattr(args, "inventory", None) or root / (
        "out/evidence/r4/post-o1/automatic-device-functional/inventory.json")
    inventory_binding = artifact(inventory_path)
    inventory = read(inventory_path)
    expected_endpoint = endpoint(inventory)
    inputs = bundle_bindings(bundle, args.kind, inventory)
    historical = read(inputs["historical_binding"]["path"])
    if (source["sha"] == historical.get("head") or
            metadata["binary"]["sha256"] == historical.get("player_sha256")):
        raise ValueError("historical fixture Player cannot stand in for the new candidate")
    events = script_events(bundle, args.kind)
    supplied_script = getattr(args, "script", None)
    supplied_binding = artifact(supplied_script) if supplied_script is not None else None
    if supplied_script is not None and read_script(supplied_script) != events:
        raise ValueError("supplied Player script differs from the complete bound trajectory")
    mask = integer(process_control.affinity(), "affinity", 1)
    output.mkdir(parents=True, exist_ok=False)
    monitor, completed = None, None
    log = output / "process.log"
    try:
        appdata = output / "settings"
        case = bundle / KINDS[args.kind][0]
        shutil.copytree(case / "settings", appdata)
        copied = {name: artifact(appdata / name) for name in inputs["settings"]}
        if any(copied[name]["sha256"] != item["sha256"]
               for name, item in inputs["settings"].items()):
            raise ValueError("copied Player settings do not match the bound fixture")
        script = output / "script.txt"
        if supplied_script is None:
            script.write_text(script_text(events), encoding="ascii", newline="\n")
        else:
            shutil.copyfile(supplied_script, script)
        write_once(output / "source.json", source)
        environment = process_environment()
        overrides = {"APPDATA": str(appdata), "CUEXIS_PD5_PLAYER_SCRIPT": str(script),
                     "SDL_WINDOW_ACTIVATE_WHEN_SHOWN": "1", "SDL_WINDOW_ACTIVATE_WHEN_RAISED": "1"}
        environment.update(overrides)
        launch = command(binary, bundle, args.kind)
        bound = [artifact(args.build_binding), inventory_binding, metadata["binary"],
                 inputs["historical_binding"], artifact(script), *inputs["content"].values(),
                 *inputs["settings"].values(), *copied.values()]
        if supplied_binding is not None:
            bound.append(supplied_binding)
        declaration = {
            "schema": "rpa.execution.player-functional-declaration.v1", "kind": args.kind,
            "formal_registration": False, "acceptance": False, "formal_attempts_consumed": 0,
            "measurement_mode": "player-trace", "processes": 1, "timeout_seconds": TIMEOUT,
            "automatic_retry": False, "source_binding": artifact(output / "source.json"),
            "build_binding": artifact(args.build_binding), "binary": metadata["binary"],
            "inventory_binding": inventory_binding, "inputs": inputs,
            "script_binding": artifact(script), "script_events": events,
            "settings_copied": copied, "environment_overrides": overrides,
            "command": launch, "cwd": str(root), "affinity": mask,
            "tool_binding": artifact(__file__), "input_bindings": bound,
            "foreground_policy": "native-player-focus"}
        write_once(output / "declaration.json", declaration)
        for item in bound:
            verify_artifact(item)
        before = device_inventory.default_audio()
        write_once(output / "audio-defaults-before.json", before)
        if endpoint(dict(schema=inventory["schema"], audio_defaults=before)) != expected_endpoint:
            raise ValueError("system-default audio endpoint differs before Player launch")
        monitor = PlayerEnvironmentMonitor()
        started = time.monotonic()
        with monitor:
            if monitor.errors:
                raise ValueError("Player prelaunch AC/conflict check failed")
            with log.open("xb") as stream:
                completed = process_control.run(launch, mask, cwd=root, env=environment,
                                                output=stream, timeout=TIMEOUT)
        write_once(output / "environment.json", monitor.result())
        if not monitor.result()["passed"]:
            raise ValueError("Player environment sampling failed")
        after = device_inventory.default_audio()
        write_once(output / "audio-defaults-after.json", after)
        if endpoint(dict(schema=inventory["schema"], audio_defaults=after)) != expected_endpoint:
            raise ValueError("system-default audio endpoint differs after Player exit")
        parsed = parse_log(log.read_text(encoding="utf-8", errors="strict"), kind=args.kind,
                           events=events, inventory=inventory, exit_code=completed.returncode,
                           affinity=mask)
        for item in bound:
            verify_artifact(item)
        build_binding.verify(metadata, source, "player-trace")
        if clean_source(root) != source:
            raise ValueError("candidate source changed during Player functional collection")
        write_once(output / "parsed.json", parsed)
        write_once(output / "result.json", {
            "schema": "rpa.execution.player-functional-result.v1", "valid": True,
            "formal_registration": False, "acceptance": False, "formal_attempts_consumed": 0,
            "exit_code": completed.returncode, "elapsed_seconds": time.monotonic() - started,
            "completed_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "declaration_binding": artifact(output / "declaration.json"),
            "log_binding": artifact(log), "parsed_binding": artifact(output / "parsed.json")})
        return parsed
    except Exception as error:
        if monitor is not None and not (output / "environment.json").exists():
            write_once(output / "environment.json", monitor.result())
        write_once(output / "stopped.json", {
            "error": str(error), "valid": False, "formal_registration": False,
            "acceptance": False, "formal_attempts_consumed": 0, "automatic_retry": False,
            "exit_code": completed.returncode if completed is not None else None,
            "log_binding": artifact(log) if log.exists() else None})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("source", "build-binding", "bundle", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--kind", choices=tuple(KINDS), required=True)
    parser.add_argument("--script", type=Path)
    parser.add_argument("--inventory", type=Path)
    args = parser.parse_args()
    run(args)


if __name__ == "__main__":
    main()
