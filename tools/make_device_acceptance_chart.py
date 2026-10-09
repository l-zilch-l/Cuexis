"""Build an explicit test-only device fixture with the real candidate assembler.

All audio is synthesized here. Mesh/material bytes reuse the repository's demo20s
portable resources; a solid RGBA texture is generated locally. No network assets.
"""
import argparse
import copy
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import wave
import zipfile


ROOT = Path(__file__).resolve().parents[1]
CONFIG_BUDGET = "131072,64,8192,16384,8192"
GRAPH_BUDGET = "131072,64,8192,16384,8192,8192,65536"
EVENTS = [("tap.d", "lane.d", 120, 240, 180),
          ("tap.f", "lane.f", 300, 420, 360),
          ("miss.j", "lane.j", 480, 600, 540),
          ("hold.k", "lane.k", 720, 840, 780)]


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=True, sort_keys=True, indent=2) + "\n").encode()


def click_audio():
    # Sixty seconds, mono PCM16/48 kHz. Accented 120 BPM beats and alternating
    # quiet left/right-independent tones make output and pause/resume audible.
    rate, seconds = 48000, 60
    samples = bytearray(rate * seconds * 2)
    for beat in range(seconds * 2):
        start = beat * (rate // 2)
        frequency = 1320 if beat % 4 == 0 else 880
        for i in range(rate // 20):
            envelope = (1 - i / (rate / 20)) ** 2
            value = round(8000 * envelope * math.sin(2 * math.pi * frequency * i / rate))
            struct.pack_into("<h", samples, (start + i) * 2, value)
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(samples)
    return output.getvalue()


def write_base(path, music):
    with zipfile.ZipFile(ROOT / "tests/fixtures/chart_format_update/golden/cxc_v1_v4_static.cxc") as z:
        entries = {name: z.read(name) for name in z.namelist() if name != "cuexis.cxc.json"}
    assets = []
    for asset_id, kind, logical, data, dependencies in [
        ("mesh.quad", "mesh", "meshes/quad.mesh.bin",
         (ROOT / "assets/projects/demo20s/assets/meshes/quad.mesh.bin").read_bytes(), []),
        ("material.test", "material", "materials/test.material.bin",
         (ROOT / "assets/projects/demo20s/assets/materials/test.material.bin").read_bytes(), ["texture.test"]),
        ("texture.test", "texture", "textures/white.texture.bin",
         struct.pack("<8sIIQIIII", b"CXPRES01", 2, 1, 44, 1, 1, 2, 0) + b"\xff" * 4, []),
        ("audio.main", "audio", "audio/clicks.wav", click_audio(), [])]:
        if kind == "audio" and not music:
            continue
        entries["assets/" + logical] = data
        assets.append(dict(id=asset_id, type=kind, source=logical, dependencies=dependencies))
    entries["assets/cuexis.asset-index.json"] = json_bytes(
        dict(format="cuexis.asset-index", version=2, assets=assets, extensions={}))
    if music:
        chart = json.loads(entries["assets/charts/main.cuexis.chart.json"])
        chart["audio"] = dict(version=1, mainMusic=dict(domain="asset", id="audio.main"))
        entries["assets/charts/main.cuexis.chart.json"] = json_bytes(chart)
    manifest = dict(format="cuexis.cxc", version=1, project="cuexis.project.json",
                    entries=[dict(path=name, byteCount=len(data), sha256=hashlib.sha256(data).hexdigest())
                             for name, data in sorted(entries.items())],
                    requiredExtensions=[], extensions={})
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as z:
        for name, data in [("cuexis.cxc.json", json_bytes(manifest)), *sorted(entries.items())]:
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            z.writestr(info, data)


def sources(configuration, music):
    source = json.loads((ROOT / "tests/fixtures/gameplay_author/inline_prototype.json").read_bytes())
    gameplay = source["gameplay"]
    common = gameplay["common"]
    template = copy.deepcopy(gameplay["requirements"][0])
    grace = copy.deepcopy(common["graceInputs"][0]["inputs"])
    common["graceInputs"] = []
    common["declarations"] = [row for row in common["declarations"] if row["kind"] != "requirement"]
    common["factBindings"] = []
    gameplay["requirements"] = []
    gameplay["rankAssignment"]["rows"] = []
    source["entities"] = []
    source["resourceClosure"] = [dict(assetId="mesh.quad", use="renderableMesh"),
                                 dict(assetId="material.test", use="renderableMaterial")]
    if music:
        source["mainMusic"] = "audio.main"
        source["resourceClosure"].append(dict(assetId="audio.main", use="mainMusic"))
    source["defaultCamera"]["defaultTransform"] = dict(
        position=[0, 0, 5], rotation=[0, 0, 0, 1], scale=[1, 1, 1])
    config = copy.deepcopy(configuration)
    c = config["configuration"]
    c["calibration"] = "calibration.device-test.frame-bridge.uncalibrated"
    c["domains"][0]["maximum"] = 2000
    c["bindings"] = []
    c["scoreRules"] = [dict(phase=phase, outcome=outcome, grade=None,
                            delta=delta if outcome == "hit" else -1,
                            incrementsCombo=outcome == "hit")
                       for phase, delta in [("tap", 2), ("head", 2), ("body", 3), ("tail", 5)]
                       for outcome in ["hit", "miss"]]
    marker = 0
    for ordinal, (name, channel, start, end, target) in enumerate(EVENTS, 4):
        r = copy.deepcopy(template)
        r["stableId"]["declarationOrdinal"] = ordinal
        r["identity"]["requirementLocalId"] = name
        r["identity"]["emissionPath"][0]["nodeId"] = name
        r["factBindingRefs"] = []
        r["atomBindings"][0]["channelToken"] = channel
        r["timing"] = dict(end=str(end), successWindows=[dict(phase=r["phases"][0],
                            start=str(start), end=str(end))],
                            phaseTargets=[dict(phase="tap", chartTick=str(target))])
        if name == "hold.k":
            r["phases"] = [dict(kind=phase, declarationOrdinal=i)
                           for i, phase in enumerate(["head", "body", "tail"], 1)]
            r["requiresReleaseTailSemantics"] = True
            r["measure"]["components"] = [dict(phase=phase, categoryToken="hold_"+phase,
                                                   gradeTokens=[])
                                              for phase in ["head", "body", "tail"]]
            r["timing"] = dict(end="1200", successWindows=[
                dict(phase=r["phases"][0], start="720", end="840"),
                dict(phase=r["phases"][1], start="840", end="1080"),
                dict(phase=r["phases"][2], start="1080", end="1200")],
                body=dict(start="840", end="1080"),
                phaseTargets=[dict(phase=phase, chartTick=str(tick))
                              for phase, tick in [("head", 780), ("body", 1080), ("tail", 1140)]])
            tail = copy.deepcopy(r["atomBindings"][0])
            tail.update(atomRef="tail.release", action="release", tailOnly=True)
            r["atomBindings"].append(tail)
        common["declarations"].append(dict(stableId=r["stableId"], kind="requirement",
                                           localName=name, references=[], requiredRefs=r["requiredRefs"]))
        common["graceInputs"].append(dict(requirement=r["stableId"], inputs=copy.deepcopy(grace)))
        gameplay["requirements"].append(r)
        gameplay["rankAssignment"]["rows"].append(dict(identity=r["identity"], priority="-1",
                                                       tieRank=str(ordinal), namespace="@independent"))
        source["entities"].append(dict(identity=dict(kind="generated", chartId=source["chartId"],
            bindingId="invocation.one", moduleId="module.one", exportId="export.one",
            path=[dict(nodeId=name, iterationIndexPlusOne=0)]), components=[], requirements=[]))
        for phase in r["phases"]:
            marker += 1
            object_id = f"019a0000-0000-7000-8000-{marker:012d}"
            binding = f"device.feedback.{marker}"
            common["factBindings"].append(binding)
            r["factBindingRefs"].append(binding)
            source["entities"].append(dict(identity=dict(kind="explicit", objectId=object_id),
                components=[dict(kind="transform", position=[(marker-3.5)*0.6, 0, 0],
                                 rotation=[0, 0, 0, 1], scale=[0.3, 0.3, 0.3]),
                            dict(kind="renderable", mesh="mesh.quad", material="material.test", alpha=255)],
                requirements=[]))
            reference = copy.deepcopy(r["identity"])
            for step in reference["emissionPath"]:
                step["repeatIndex"] = int(step["repeatIndex"])
            c["bindings"].append(dict(bindingId=binding, source=reference, phase=phase["kind"],
                outcome="hit", timing="any", target=object_id, visible=False,
                start=0, end=None, aggregation="any", groupMembers=[]))
    return source, config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True,
                        help="Existing public fixture directory containing foundation.packed/configuration.json")
    parser.add_argument("--output", type=Path, default=ROOT / "out/device-acceptance")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    configuration = json.loads((args.fixture / "configuration.json").read_bytes())
    for music, name in [(False, "keyboard"), (True, "audio")]:
        base, author, config = [output / f"{name}-{suffix}" for suffix in ["base.cxc", "author.json", "configuration.json"]]
        write_base(base, music)
        source, settings = sources(configuration, music)
        author.write_bytes(json_bytes(source))
        config.write_bytes(json_bytes(settings))
        command = [str(args.tool.resolve()), "--gameplay", "--base", str(base),
                   "--foundation", str((args.fixture / "foundation.packed").resolve()),
                   "--configuration", str(config), "--entry-id", "chart.entry.one",
                   "--test-only", "true", "--configuration-budget", CONFIG_BUDGET,
                   "--graph-budget", GRAPH_BUDGET, "--source", str(author),
                   "--source-kind", "inline", "--publish", "cxc", "--output", str(output / f"{name}.cxc")]
        result = subprocess.run(command, capture_output=True, timeout=120)
        (output / f"{name}-assemble.log").write_bytes(
            ("command=" + json.dumps(command) + "\nexit=" + str(result.returncode) + "\n").encode()
            + result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"Assembler rejected fixture; see {output / (name+'-assemble.log')}")
        report = json.loads(result.stdout)
        if not report["actualPrepareValidated"] or report["productionBudgetAccepted"]:
            raise RuntimeError("Fixture must pass actual prepare without accepting production budgets")
    (output / "hit-observations.txt").write_bytes(
        b"180 1 press lane.d domain.binding.one keyboard 0 0 0 0 0 0 0\n"
        b"181 2 release lane.d domain.binding.one keyboard 0 0 0 0 0 0 0\n"
        b"360 3 press lane.f domain.binding.one keyboard 0 0 0 0 0 0 0\n"
        b"361 4 release lane.f domain.binding.one keyboard 0 0 0 0 0 0 0\n"
        b"780 5 press lane.k domain.binding.one keyboard 0 0 0 0 0 0 0\n"
        b"1140 6 release lane.k domain.binding.one keyboard 0 0 0 0 0 0 0\n")
    (output / "commands.txt").write_bytes(b"open\nplay\ntick 125\nquit\n")
    manifest = {p.name: dict(bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                for p in sorted(output.iterdir()) if p.is_file() and p.name != "manifest.json"}
    (output / "manifest.json").write_bytes(json_bytes(manifest))
    print(f"Prepared test-only keyboard/audio packages: {output}")


if __name__ == "__main__":
    main()
