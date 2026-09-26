#!/usr/bin/env python3
"""Derive the byte-level negative fixtures for S6-E1/S6-E2 from the encoded fixtures.

These fixtures are deliberately damaged copies: truncation, a corrupted metadata block, a
concatenated second Ogg logical stream and a forged STREAMINFO sample count. They exist so the
importer's fail-closed behaviour is exercised by real bytes rather than synthetic buffers.

Usage: python -B tests/fixtures/stage6_e/media/generate_negative_fixtures.py
"""

from __future__ import annotations

import pathlib
import struct

HERE = pathlib.Path(__file__).resolve().parent
IMAGE = HERE / "image"
AUDIO = HERE / "audio"


def read(name: str) -> bytes:
    return (AUDIO / name).read_bytes()


def write(name: str, payload: bytes) -> None:
    (AUDIO / name).write_bytes(payload)
    print(f"{name}: {len(payload)} bytes")


def truncate(payload: bytes, keep: float) -> bytes:
    return payload[: max(1, int(len(payload) * keep))]


def patch_flac_total_samples(payload: bytes, total: int) -> bytes:
    """Rewrite the STREAMINFO total-sample field (36 bits) without touching the audio frames."""
    if payload[:4] != b"fLaC":
        raise SystemExit("expected a native FLAC stream")
    # The STREAMINFO block header is 4 bytes, its body starts at 8 and the packed
    # rate/channels/bits/total field starts 10 bytes into the body.
    field = 8 + 10
    packed = int.from_bytes(payload[field : field + 8], "big")
    packed &= ~((1 << 36) - 1)
    packed |= total & ((1 << 36) - 1)
    return payload[:field] + packed.to_bytes(8, "big") + payload[field + 8 :]


def main() -> None:
    mono_mp3 = read("mono.mp3")
    mono_ogg = read("mono.ogg")
    mono_flac = read("mono.flac")

    write("truncated.mp3", truncate(mono_mp3, 0.6))
    write("truncated.ogg", truncate(mono_ogg, 0.6))
    write("truncated.flac", truncate(mono_flac, 0.6))

    # Native FLAC magic with an unusable metadata block header.
    write("bad_header.flac", b"fLaC" + b"\x7f" + struct.pack(">I", 0xFFFFFFFF)[1:] + mono_flac[8:])

    # Two complete logical bitstreams in one file: chained Ogg.
    write("chained.ogg", mono_ogg + mono_ogg)

    # A forged STREAMINFO sample count must not change the decoded sample count.
    write("forged_total_samples.flac", patch_flac_total_samples(mono_flac, (1 << 36) - 1))

    for name in ("truncated.mp3", "truncated.ogg", "truncated.flac", "bad_header.flac",
                 "chained.ogg", "forged_total_samples.flac"):
        assert (AUDIO / name).exists(), name
    assert not (IMAGE / "unused.png").exists()


if __name__ == "__main__":
    main()
