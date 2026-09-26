#!/usr/bin/env python3
"""Record the canonical S6-E1/S6-E2 goldens from the importer.

The goldens are the frozen canonical output of the fixed media profile. They are recorded once from
a local build and then verified byte for byte on every platform; a platform difference is a blocked
profile decision, never a golden update.

Usage:
  python -B tests/fixtures/stage6_e/media/generate_goldens.py <path-to-cuexis_media_importer>
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
GOLDEN = HERE.parent / "golden"

# (golden stem, fixture path relative to the media directory, kind)
CASES = [
    ("image_rgb8", "image/rgb8.png", "image"),
    ("image_rgba8", "image/rgba8.png", "image"),
    ("image_palette_trns", "image/palette_trns.png", "image"),
    ("image_gray8", "image/gray8.png", "image"),
    ("image_srgb_chunk", "image/srgb_chunk.png", "image"),
    ("image_baseline", "image/baseline.jpg", "image"),
    ("image_progressive", "image/progressive.jpg", "image"),
    ("image_gray", "image/gray.jpg", "image"),
    ("image_orientation1", "image/orientation1.jpg", "image"),
    ("image_orientation3", "image/orientation3.jpg", "image"),
    ("image_orientation6", "image/orientation6.jpg", "image"),
    ("image_orientation8", "image/orientation8.jpg", "image"),
    ("audio_mono_mp3", "audio/mono.mp3", "audio"),
    ("audio_mono_ogg", "audio/mono.ogg", "audio"),
    ("audio_mono_flac", "audio/mono.flac", "audio"),
    ("audio_stereo_mp3", "audio/stereo.mp3", "audio"),
    ("audio_stereo_ogg", "audio/stereo.ogg", "audio"),
    ("audio_stereo_flac", "audio/stereo.flac", "audio"),
]

# The whole canonical artifact is recorded when it stays readable in review.
MAX_RECORDED_BYTES = 4096


def import_fixture(importer: str, fixture: pathlib.Path, kind: str, work: pathlib.Path) -> dict:
    result = subprocess.run(
        [importer, "--kind", kind, "--input", str(fixture), "--output-dir", str(work),
         "--in-process", "--print-info"],
        capture_output=True, text=True, check=False,
    )
    if result.returncode != 0:
        raise SystemExit(f"{fixture.name}: importer exited {result.returncode}: {result.stderr}")
    info = json.loads(result.stdout.strip().splitlines()[-1])
    artifacts = [p for p in work.iterdir() if p.is_file()]
    if len(artifacts) != 1:
        raise SystemExit(f"{fixture.name}: expected one artifact, found {artifacts}")
    return info, artifacts[0]


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    importer = str(pathlib.Path(sys.argv[1]).resolve())
    GOLDEN.mkdir(parents=True, exist_ok=True)

    for stem, relative, kind in CASES:
        fixture = HERE / relative
        with tempfile.TemporaryDirectory() as directory:
            info, artifact = import_fixture(importer, fixture, kind, pathlib.Path(directory))
            payload = artifact.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if artifact.name.split(".")[0] != digest:
            raise SystemExit(f"{relative}: artifact name {artifact.name} is not its content hash")
        document = {
            "fixture": relative,
            "kind": kind,
            "profile": info["profile"],
            "decoder": info["decoder"],
            "sha256": digest,
            "byteCount": len(payload),
            "sourceBytes": info["sourceBytes"],
            "decodedBytes": info["decodedBytes"],
            "peakWorkingBytes": info["peakWorkingBytes"],
        }
        if kind == "image":
            document["width"] = info["width"]
            document["height"] = info["height"]
            document["sourceWidth"] = info["sourceWidth"]
            document["sourceHeight"] = info["sourceHeight"]
            document["orientation"] = info["orientation"]
        else:
            document["sampleRate"] = info["sampleRate"]
            document["channels"] = info["channels"]
            document["frames"] = info["frames"]
        if len(payload) <= MAX_RECORDED_BYTES:
            document["bytesHex"] = payload.hex()
        (GOLDEN / f"{stem}.json").write_text(
            json.dumps(document, indent=2, sort_keys=False) + "\n", encoding="utf-8")
        print(f"{stem}.json: {len(payload)} bytes sha256={digest}")


if __name__ == "__main__":
    main()
