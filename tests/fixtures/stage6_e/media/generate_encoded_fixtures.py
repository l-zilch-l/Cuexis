#!/usr/bin/env python3
"""Generate the encoded audio fixtures for S6-E1/S6-E2.

The audio fixtures come from a deterministic PCM WAV written by this script and encoded by
ffmpeg with fixed options. The JPEG fixtures are written by jpeg_fixtures.cpp instead, because
ffmpeg's mjpeg encoder does not expose the encoder options the profile pins.

Provenance is recorded in the neighbouring README.md. The committed bytes are the
fixtures; regenerating them with a different ffmpeg build can produce different
encoded bytes, which would require new canonical goldens.

Usage: python -B tests/fixtures/stage6_e/media/generate_encoded_fixtures.py [--ffmpeg PATH]
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
IMAGE = HERE / "image"
AUDIO = HERE / "audio"


def pcm_wav(path: pathlib.Path, sample_rate: int, channels: int, frames: int) -> bytes:
    samples = bytearray()
    for frame in range(frames):
        for channel in range(channels):
            # A deterministic two-tone pattern that stays inside the S16 range.
            value = ((frame * (64 + channel * 96)) % 20000) - 10000
            samples += struct.pack("<h", value)
    block_align = channels * 2
    data_bytes = len(samples)
    header = b"RIFF" + struct.pack("<I", 36 + data_bytes) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, channels, sample_rate, sample_rate * block_align,
                                    block_align, 16)
    header += b"data" + struct.pack("<I", data_bytes)
    payload = header + bytes(samples)
    path.write_bytes(payload)
    return payload


def run(ffmpeg: str, arguments: list[str]) -> None:
    completed = subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", *arguments],
                               check=False, capture_output=True)
    if completed.returncode != 0:
        raise SystemExit(f"ffmpeg failed: {' '.join(arguments)}\n{completed.stderr.decode()}")


def app1_orientation(orientation: int) -> bytes:
    tiff = bytearray()
    tiff += b"MM" + struct.pack(">HI", 42, 8)
    tiff += struct.pack(">H", 1)
    tiff += struct.pack(">HHI", 0x0112, 3, 1) + struct.pack(">HH", orientation, 0)
    tiff += struct.pack(">I", 0)
    payload = b"Exif\x00\x00" + bytes(tiff)
    return b"\xff\xe1" + struct.pack(">H", len(payload) + 2) + payload


def app1_conflicting_orientation() -> bytes:
    tiff = bytearray()
    tiff += b"MM" + struct.pack(">HI", 42, 8)
    tiff += struct.pack(">H", 2)
    tiff += struct.pack(">HHI", 0x0112, 3, 1) + struct.pack(">HH", 6, 0)
    tiff += struct.pack(">HHI", 0x0112, 3, 1) + struct.pack(">HH", 1, 0)
    tiff += struct.pack(">I", 0)
    payload = b"Exif\x00\x00" + bytes(tiff)
    return b"\xff\xe1" + struct.pack(">H", len(payload) + 2) + payload


def insert_app1(jpeg: bytes, segment: bytes) -> bytes:
    """Insert an APP1 segment directly after SOI."""
    if jpeg[:2] != b"\xff\xd8":
        raise SystemExit("expected a JPEG SOI marker")
    return jpeg[:2] + segment + jpeg[2:]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ffmpeg", default="ffmpeg")
    options = parser.parse_args()

    IMAGE.mkdir(parents=True, exist_ok=True)
    AUDIO.mkdir(parents=True, exist_ok=True)

    # The JPEG fixtures are written by tools/jpeg_fixture_writer.c, which pins the
    # libjpeg-turbo encoder options (including progressive and CMYK) that ffmpeg's
    # mjpeg encoder does not expose. This script owns the audio fixtures.
    mono = AUDIO / "source_mono.wav"
    stereo = AUDIO / "source_stereo.wav"
    pcm_wav(mono, 8000, 1, 640)
    pcm_wav(stereo, 44100, 2, 4410)

    run(options.ffmpeg, ["-i", str(mono), "-c:a", "libmp3lame", "-b:a", "64k", "-write_xing", "1",
                         str(AUDIO / "mono.mp3")])
    run(options.ffmpeg, ["-i", str(mono), "-c:a", "libvorbis", "-q:a", "3",
                         str(AUDIO / "mono.ogg")])
    run(options.ffmpeg, ["-i", str(mono), "-c:a", "flac", "-compression_level", "5",
                         str(AUDIO / "mono.flac")])
    run(options.ffmpeg, ["-i", str(stereo), "-c:a", "libmp3lame", "-b:a", "128k",
                         str(AUDIO / "stereo.mp3")])
    run(options.ffmpeg, ["-i", str(stereo), "-c:a", "libvorbis", "-q:a", "3",
                         str(AUDIO / "stereo.ogg")])
    run(options.ffmpeg, ["-i", str(stereo), "-c:a", "flac", "-compression_level", "5",
                         str(AUDIO / "stereo.flac")])

    run(options.ffmpeg, ["-i", str(mono), "-ac", "3", "-c:a", "flac",
                         str(AUDIO / "channels3.flac")])
    run(options.ffmpeg, ["-i", str(mono), "-ar", "384000", "-c:a", "flac",
                         str(AUDIO / "rate_out_of_range.flac")])

    for name in ("mono.mp3", "mono.ogg", "mono.flac", "stereo.mp3", "stereo.ogg", "stereo.flac",
                 "channels3.flac", "rate_out_of_range.flac", "source_mono.wav",
                 "source_stereo.wav"):
        path = AUDIO / name
        print(f"{name}: {path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
