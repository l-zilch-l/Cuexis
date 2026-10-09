"""Check device packages using independent ZIP/hash checks and manual host goldens."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import wave
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    args = parser.parse_args()
    directory = args.directory.resolve()
    ordinal = 0
    for name in ["keyboard", "audio"]:
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
                assert row["expandedRequirementCount"] == 4
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
                # Manual phase arithmetic: D/F Tap +2 each, J Miss -1,
                # K Hold head/body/tail +2/+3/+5 => 13, five Hits, one Miss.
                # No observations: all six phases Miss => -6, zero Combo.
                expected = "score=13 combo=3 hits=5 misses=1" if hit else "score=-6 combo=0 hits=0 misses=6"
                assert expected + " completeReplay=same" in output, str(log)
                assert "completeReplay=different" not in output, str(log)
                assert "host.summary outcome=ok" in output, str(log)
    print("Device fixtures: independent ZIP/hash/PCM checks passed; four public Host keyboard goldens/complete Replay runs and two audio/ChartClock rejection runs passed. Physical devices not exercised.")


if __name__ == "__main__":
    main()
