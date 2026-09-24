#!/usr/bin/env python3
"""Generate the deterministic PNG fixtures for S6-E1.

The script writes real PNG streams built from the published chunk layout so that
each fixture is reproducible from this file alone. It never writes a golden: the
canonical import bytes are produced by the importer and recorded separately.

Usage: python -B tests/fixtures/stage6_e/media/generate_image_fixtures.py
"""

from __future__ import annotations

import pathlib
import struct
import zlib

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "image"

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def ihdr(width: int, height: int, depth: int, color_type: int) -> bytes:
    return chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color_type, 0, 0, 0))


def raw_rows(row: bytes, height: int) -> bytes:
    """Rows with filter type 0, repeating the same unfiltered row."""
    return (b"\x00" + row) * height


def write(name: str, payload: bytes) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_bytes(payload)
    print(f"{name}: {len(payload)} bytes")


def rgb8() -> bytes:
    """3x2 truecolor 8-bit, no alpha."""
    pixels = b"\x10\x20\x30" + b"\x40\x50\x60" + b"\x70\x80\x90"
    return (
        PNG_SIGNATURE
        + ihdr(3, 2, 8, 2)
        + chunk(b"IDAT", zlib.compress(raw_rows(pixels, 2), 9))
        + chunk(b"IEND", b"")
    )


def rgba8() -> bytes:
    """2x2 truecolor with alpha, including a zero-alpha pixel."""
    rows = (
        b"\x00" + b"\xff\x00\x00\xff" + b"\x00\xff\x00\x80"
        + b"\x00" + b"\x00\x00\xff\x00" + b"\xff\xff\xff\x01"
    )
    return (
        PNG_SIGNATURE
        + ihdr(2, 2, 8, 6)
        + chunk(b"IDAT", zlib.compress(rows, 9))
        + chunk(b"IEND", b"")
    )


def palette_trns() -> bytes:
    """2x1 palette image with a tRNS alpha table."""
    palette = b"\xff\x00\x00" + b"\x00\x00\xff"
    trns = b"\xff\x40"
    rows = b"\x00" + b"\x00" + b"\x01"
    return (
        PNG_SIGNATURE
        + ihdr(2, 1, 8, 3)
        + chunk(b"PLTE", palette)
        + chunk(b"tRNS", trns)
        + chunk(b"IDAT", zlib.compress(rows, 9))
        + chunk(b"IEND", b"")
    )


def gray8() -> bytes:
    """2x1 grayscale 8-bit."""
    return (
        PNG_SIGNATURE
        + ihdr(2, 1, 8, 0)
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x7f\x3f", 1), 9))
        + chunk(b"IEND", b"")
    )


def gray_alpha16() -> bytes:
    """1x1 grayscale+alpha 16-bit, which the v1 profile rejects."""
    return (
        PNG_SIGNATURE
        + ihdr(1, 1, 16, 4)
        + chunk(b"IDAT", zlib.compress(b"\x00" + b"\x12\x34\x56\x78", 9))
        + chunk(b"IEND", b"")
    )


def srgb_chunk() -> bytes:
    """1x1 truecolor with an explicit sRGB rendering intent."""
    return (
        PNG_SIGNATURE
        + ihdr(1, 1, 8, 2)
        + chunk(b"sRGB", b"\x00")
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x00\x00\x00", 1), 9))
        + chunk(b"IEND", b"")
    )


def icc_profile(description: bytes) -> bytes:
    """A 132-byte ICC profile header that is not sRGB."""
    header = bytearray(132)
    header[0:4] = struct.pack(">I", 132)
    header[4:8] = b"cuex"
    header[8:12] = struct.pack(">I", 0x04300000)
    header[12:16] = b"mntr"
    header[16:20] = b"RGB "
    header[20:24] = b"XYZ "
    header[36:40] = b"acsp"
    header[40:44] = b"APPL"
    header[64:68] = struct.pack(">I", 0)  # rendering intent: perceptual
    header[80:84] = description.ljust(4, b"\x00")[:4]
    return bytes(header)


def iccp_non_srgb() -> bytes:
    """1x1 truecolor with an iCCP chunk holding a non-sRGB profile."""
    profile = icc_profile(b"Adobe RGB (1998)")
    compressed = zlib.compress(profile, 9)
    return (
        PNG_SIGNATURE
        + ihdr(1, 1, 8, 2)
        + chunk(b"iCCP", b"Adobe RGB (1998)\x00\x00" + compressed)
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33", 1), 9))
        + chunk(b"IEND", b"")
    )


def gamma_unsupported() -> bytes:
    """1x1 truecolor with a gAMA chunk that is neither 1/2.2 nor linear."""
    return (
        PNG_SIGNATURE
        + ihdr(1, 1, 8, 2)
        + chunk(b"gAMA", struct.pack(">I", 25000))
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33", 1), 9))
        + chunk(b"IEND", b"")
    )


def apng() -> bytes:
    """1x1 truecolor with an acTL chunk, which the v1 profile rejects."""
    return (
        PNG_SIGNATURE
        + ihdr(1, 1, 8, 2)
        + chunk(b"acTL", struct.pack(">II", 2, 0))
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33", 1), 9))
        + chunk(b"IEND", b"")
    )


def forged_dimensions() -> bytes:
    """A valid 1x1 stream whose IHDR declares 20000x20000."""
    body = (
        PNG_SIGNATURE
        + ihdr(20000, 20000, 8, 2)
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33", 1), 9))
        + chunk(b"IEND", b"")
    )
    return body


def corrupt_chunk() -> bytes:
    """1x1 truecolor whose IDAT chunk carries a broken CRC."""
    payload = chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33", 1), 9))
    mutated = bytearray(payload)
    mutated[-1] ^= 0xFF  # IDAT CRC
    return PNG_SIGNATURE + ihdr(1, 1, 8, 2) + bytes(mutated) + chunk(b"IEND", b"")


def truncated() -> bytes:
    """A stream cut inside its IDAT payload."""
    good = (
        PNG_SIGNATURE
        + ihdr(2, 2, 8, 2)
        + chunk(b"IDAT", zlib.compress(raw_rows(b"\x11\x22\x33\x44\x55\x66", 2), 9))
        + chunk(b"IEND", b"")
    )
    return good[: len(good) - 14]


def budget_1024() -> bytes:
    """1024x1024 truecolor 8-bit. Decoding it needs at least 4 MiB of RGBA8 output, so the CLI gate
    can prove that a 1 MiB process memory limit fails closed on every platform."""
    row = bytearray()
    for x in range(1024):
        row += bytes(((x * 7) & 0xFF, (x * 11 + 3) & 0xFF, (x * 13 + 5) & 0xFF))
    return (
        PNG_SIGNATURE
        + ihdr(1024, 1024, 8, 2)
        + chunk(b"IDAT", zlib.compress(raw_rows(bytes(row), 1024), 9))
        + chunk(b"IEND", b"")
    )


def main() -> None:
    write("rgb8.png", rgb8())
    write("rgba8.png", rgba8())
    write("palette_trns.png", palette_trns())
    write("gray8.png", gray8())
    write("gray_alpha16.png", gray_alpha16())
    write("srgb_chunk.png", srgb_chunk())
    write("iccp_non_srgb.png", iccp_non_srgb())
    write("gamma_unsupported.png", gamma_unsupported())
    write("apng.png", apng())
    write("forged_dimensions.png", forged_dimensions())
    write("corrupt_chunk.png", corrupt_chunk())
    write("truncated.png", truncated())
    write("budget_1024.png", budget_1024())


if __name__ == "__main__":
    main()
