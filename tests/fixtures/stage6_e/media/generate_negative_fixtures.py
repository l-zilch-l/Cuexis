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
import zlib

HERE = pathlib.Path(__file__).resolve().parent
IMAGE = HERE / "image"
AUDIO = HERE / "audio"


def read(name: str) -> bytes:
    return (AUDIO / name).read_bytes()


def write(name: str, payload: bytes) -> None:
    (AUDIO / name).write_bytes(payload)
    print(f"{name}: {len(payload)} bytes")


def read_image(name: str) -> bytes:
    return (IMAGE / name).read_bytes()


def write_image(name: str, payload: bytes) -> None:
    (IMAGE / name).write_bytes(payload)
    print(f"{name}: {len(payload)} bytes")


def png_chunks(payload: bytes) -> list[tuple[bytes, bytes]]:
    """Split a PNG into (type, full chunk bytes) pairs, including the signature check."""
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("expected a PNG signature")
    chunks: list[tuple[bytes, bytes]] = []
    offset = 8
    while offset + 12 <= len(payload):
        length = int.from_bytes(payload[offset : offset + 4], "big")
        end = offset + 12 + length
        chunks.append((payload[offset + 4 : offset + 8], payload[offset:end]))
        offset = end
        if chunks[-1][0] == b"IEND":
            break
    return chunks


def replace_gamma(payload: bytes, gamma: int) -> bytes:
    """Rewrite the gAMA chunk value, keeping the length, type and CRC consistent."""
    out = bytearray(payload[:8])
    replaced = False
    for kind, chunk in png_chunks(payload):
        body = bytearray(chunk)
        if kind == b"gAMA":
            body[8:12] = gamma.to_bytes(4, "big")
            body[-4:] = zlib.crc32(bytes(body[4:-4])).to_bytes(4, "big")
            replaced = True
        out += body
    if not replaced:
        raise SystemExit("source fixture has no gAMA chunk")
    return bytes(out)


def set_interlace(payload: bytes) -> bytes:
    """Set the IHDR interlace method to 1 (Adam7) and refresh that chunk's CRC.

    The pixel data stays non-interlaced, which is irrelevant: the profile rejects the header before
    any decoding happens, so the fixture only has to carry a truthful IHDR for the profile parser.
    """
    out = bytearray(payload[:8])
    patched = False
    for kind, chunk in png_chunks(payload):
        body = bytearray(chunk)
        if kind == b"IHDR":
            body[8 + 12] = 1  # IHDR byte 12 is the interlace method
            body[-4:] = zlib.crc32(bytes(body[4:-4])).to_bytes(4, "big")
            patched = True
        out += body
    if not patched:
        raise SystemExit("source fixture has no IHDR chunk")
    return bytes(out)


def corrupt_jpeg_marker(payload: bytes) -> bytes:
    """Damage a marker length inside the entropy-coded segment of a baseline JPEG.

    The SOI and APP0/quantisation tables stay intact, so the failure comes from the scan data and
    not from a missing header; libjpeg reports a decode failure rather than an unsupported format.
    """
    marker = payload.find(b"\xff\xda")  # SOS
    if marker < 0:
        raise SystemExit("expected a JPEG SOS marker")
#Corrupt the first few entropy bytes after the SOS header so the Huffman decode fails.
    body = marker + 2 + int.from_bytes(payload[marker + 2 : marker + 4], "big") + 2
    if body + 8 >= len(payload):
        raise SystemExit("JPEG scan data is too short to damage")
    out = bytearray(payload)
    for index in range(body, body + 8):
        out[index] ^= 0xFF
    return bytes(out)


def truncate(payload: bytes, keep: float) -> bytes:
    return payload[: max(1, int(len(payload) * keep))]


def patch_flac_total_samples(payload: bytes, total: int) -> bytes:
    """Rewrite the STREAMINFO total-sample field (36 bits) without touching the audio frames."""
    if payload[:4] != b"fLaC":
        raise SystemExit("expected a native FLAC stream")
#The STREAMINFO block header is 4 bytes, its body starts at 8 and the packed
#rate / channels / bits / total field starts 10 bytes into the body.
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

#Native FLAC magic with an unusable metadata block header.
    write("bad_header.flac", b"fLaC" + b"\x7f" + struct.pack(">I", 0xFFFFFFFF)[1:] + mono_flac[8:])

#Two complete logical bitstreams in one file : chained Ogg.
    write("chained.ogg", mono_ogg + mono_ogg)

#A forged STREAMINFO sample count must not change the decoded sample count.
    write("forged_total_samples.flac", patch_flac_total_samples(mono_flac, (1 << 36) - 1))

#Image negatives, derived from the committed baselines so the only difference is the defect.
# `gamma_unsupported.png` is the source for all three gamma cases : it already carries a gAMA
#chunk, so each variant differs from it only in the gamma value.
    write_image("gamma_linear.png", replace_gamma(read_image("gamma_unsupported.png"), 100000))
#The positive counterpart : gAMA 45455 IS the sRGB transfer function, so this file must be
#accepted. Without it an implementation that rejected every gAMA chunk would pass the suite.
    write_image("gamma_srgb_value.png", replace_gamma(read_image("gamma_unsupported.png"), 45455))
    write_image("interlaced.png", set_interlace(read_image("rgb8.png")))
    write_image("corrupt_marker.jpg", corrupt_jpeg_marker(read_image("baseline.jpg")))

    for name in ("truncated.mp3", "truncated.ogg", "truncated.flac", "bad_header.flac",
                 "chained.ogg", "forged_total_samples.flac"):
        assert (AUDIO / name).exists(), name
    for name in ("gamma_linear.png", "gamma_srgb_value.png", "interlaced.png", "corrupt_marker.jpg"):
        assert (IMAGE / name).exists(), name


if __name__ == "__main__":
    main()
