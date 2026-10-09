"""Check device packages using independent ZIP/hash checks and manual host goldens."""
import argparse
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import wave
import zipfile


def check_guide(directory, name):
    """Inspect author fields and bindings without invoking a production compiler."""
    author = json.loads((directory / (name + "-author.json")).read_bytes())
    config = json.loads((directory / (name + "-configuration.json")).read_bytes())["configuration"]
    rows = (directory / (name + "-guide.txt")).read_text(encoding="ascii").splitlines()
    assert rows[:2] == ["cuexis-player-guide-v1", "5"] and len(rows) == 7
    requirements = {r["identity"]["requirementLocalId"]: r for r in author["gameplay"]["requirements"]}
    objects = {e["identity"].get("objectId") for e in author["entities"]}
    bindings = {b["target"]: b for b in config["bindings"]}
    assert len(bindings) == len(config["bindings"]) == 14
    seen = set()
    # Hand-authored course expectation; do not import the generator's EVENTS.
    for row, (key, head, tail) in zip(rows[2:], [("D", 60, 0), ("F", 120, 0),
                                               ("J", 180, 0), ("K", 240, 0), ("K", 300, 440)]):
        fields = row.split()
        assert len(fields) == 10
        kind = "hold" if tail else "tap"
        assert fields[:4] == [kind, key, str(head), str(tail)]
        requirement = requirements[kind + "." + key.lower()]
        assert requirement["atomBindings"][0]["channelToken"] == "lane." + key.lower()
        targets = {t["phase"]: int(t["chartTick"]) for t in requirement["timing"]["phaseTargets"]}
        assert targets == ({"head": head, "body": 420, "tail": tail} if tail else {"tap": head})
        assert requirement["requiresReleaseTailSemantics"] == bool(tail)
        if tail:
            assert requirement["atomBindings"][1]["action"] == "release"
        phases = ["head", "body", "tail"] if tail else ["tap"]
        for cue, (phase, outcome) in zip(fields[4:], [(p, o) for p in phases for o in ["hit", "miss"]]):
            assert cue in objects and cue not in seen
            seen.add(cue)
            binding = bindings[cue]
            assert binding["phase"] == phase and binding["outcome"] == outcome
            identity = copy.deepcopy(requirement["identity"])
            for step in identity["emissionPath"]:
                step["repeatIndex"] = int(step["repeatIndex"])
            assert binding["source"] == identity
            assert binding["bindingId"] in requirement["factBindingRefs"]
            assert binding["visible"] is False and binding["timing"] == "any"
        if not tail:
            assert fields[6:] == ["-"] * 4
    assert seen == set(bindings)
    for phase, delta in [("tap", 2), ("head", 2), ("body", 3), ("tail", 5)]:
        for outcome in ["hit", "miss"]:
            matches = [r for r in config["scoreRules"] if r["phase"] == phase and r["outcome"] == outcome]
            assert len(matches) == 1
            assert matches[0]["delta"] == (delta if outcome == "hit" else -1)
            assert matches[0]["incrementsCombo"] == (outcome == "hit")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    args = parser.parse_args()
    directory = args.directory.resolve()
    ordinal = 0
    for name in ["keyboard", "audio"]:
        check_guide(directory, name)
        with zipfile.ZipFile(directory / (name + ".cxc")) as archive:
            manifest = json.loads(archive.read("cuexis.cxc.json"))
            for row in manifest["entries"]:
                data = archive.read(row["path"])
                assert row["byteCount"] == len(data), row["path"]
                assert row["sha256"] == hashlib.sha256(data).hexdigest(), row["path"]
            entries = json.loads(archive.read("cuexis.project.json"))["extensions"]["cuexis.gameplay-entry.v1"]["entries"]
            assert len(entries) == 2
            assert entries[0]["compiledSemanticIdentity"] == entries[1]["compiledSemanticIdentity"]
            for row in entries:
                assert row["artifactIdentity"] == hashlib.sha256(archive.read(row["path"])).hexdigest()
                assert row["expandedRequirementCount"] == 5
            if name == "audio":
                with wave.open(io.BytesIO(archive.read("assets/audio/clicks.wav")), "rb") as wav:
                    assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getnframes()) == (1, 2, 48000, 2880000)
        for encoding in ["packed", "graph.json"]:
            for hit in [False, True]:
                if name == "audio" and hit:
                    continue
                ordinal += 1
                command = [str(args.host.resolve()), "--content", str(directory / (name + ".cxc")),
                           "--candidate-entry", "compiled/gameplay." + encoding,
                           "--command-file", str(directory / "commands.txt"),
                           "--gameplay-configuration", str(directory / (name + "-configuration.json")),
                           "--gameplay-config-budget", "131072,64,8192,16384,8192",
                           "--gameplay-h-step", "10", "--gameplay-presentation-step", "10"]
                if hit:
                    command += ["--gameplay-observations", str(directory / "hit-observations.txt")]
                result = subprocess.run(command, capture_output=True, timeout=120)
                log = directory / f"check-{ordinal:02d}.log"
                log.write_bytes(("command="+json.dumps(command)+"\nexit="+str(result.returncode)+"\n").encode()
                                + result.stdout + result.stderr)
                output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
                if name == "audio":
                    # Reference Host explicitly uses ChartClock. It must reject
                    # mainMusic rather than silently drop it; real audio uses Player.
                    assert result.returncode != 0 and "playback.mode.content_mismatch" in output, str(log)
                    continue
                assert result.returncode == 0, str(log)
                # Independent manual golden: four Taps +2 each and one Hold
                # head/body/tail +2/+3/+5 => 18, seven phase Combo increments,
                # seven Hits. No input: seven phases Miss => -7, zero Combo.
                expected = "score=18 combo=7 hits=7 misses=0" if hit else "score=-7 combo=0 hits=0 misses=7"
                assert expected + " completeReplay=same" in output, str(log)
                assert "completeReplay=different" not in output, str(log)
                assert "host.summary outcome=ok" in output, str(log)
    print("Device fixtures: independent ZIP/hash/PCM checks passed; four public Host keyboard goldens/complete Replay runs and two audio/ChartClock rejection runs passed. Physical devices not exercised.")


if __name__ == "__main__":
    main()
