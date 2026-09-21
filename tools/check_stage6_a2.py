"""Read-only characterization checks for the S6-A2 contract artifacts."""

from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas"
FIXTURE_DIR = ROOT / "tests" / "fixtures" / "stage6_a2"


def fail(message: str) -> None:
    raise AssertionError(message)


def read_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        fail(f"{path.relative_to(ROOT)} is not valid JSON: {error}")


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u32(value: int) -> bytes:
    return struct.pack("<I", value)


def i64(value: int) -> bytes:
    return struct.pack("<q", value)


def ascii_string(value: str) -> bytes:
    encoded = value.encode("ascii")
    return u32(len(encoded)) + encoded


def load_schemas() -> None:
    expected = {
        "cuexis.chart-entry.v1.schema.json": "https://cuexis.dev/schemas/cuexis.chart-entry.v1.schema.json",
        "cuexis.player-preferences.v1.schema.json": "https://cuexis.dev/schemas/cuexis.player-preferences.v1.schema.json",
        "cuexis.audio-device-profile.v1.schema.json": "https://cuexis.dev/schemas/cuexis.audio-device-profile.v1.schema.json",
    }
    for name, schema_id in expected.items():
        value = read_json(SCHEMA_DIR / name)
        require(isinstance(value, dict), f"{name}: schema root is not an object")
        require(value.get("$id") == schema_id, f"{name}: unexpected $id")
        require(value.get("type") == "object", f"{name}: root type is not object")
        require(value.get("additionalProperties") is False, f"{name}: root allows unknown fields")


def check_entry_fixture(path: Path, valid: bool) -> None:
    value = read_json(path)
    require(isinstance(value, dict), f"{path.name}: root is not an object")
    entries = value.get("entries")
    require(isinstance(entries, list) and entries, f"{path.name}: entries is not nonempty")
    paths = [entry.get("path") for entry in entries if isinstance(entry, dict)]
    require(len(paths) == len(entries), f"{path.name}: every entry must be an object")
    require(all(isinstance(entry_path, str) for entry_path in paths), f"{path.name}: paths must be strings")
    if valid:
        allowed = {"path", "kind", "encoding", "playback", "sourcePath",
                   "sourceSemanticIdentity", "compiledSemanticIdentity", "artifactIdentity",
                   "compilerProfile", "expandedEntityCount", "expandedRequirementCount"}
        require(set(entries[0]).issubset(allowed), f"{path.name}: unknown entry field")
        require({"path", "kind", "encoding", "playback"}.issubset(entries[0]),
                f"{path.name}: required entry field missing")
        require(paths == sorted(paths), f"{path.name}: valid paths are not sorted")
        require(len(paths) == len(set(paths)), f"{path.name}: valid paths are duplicated")
        record = entries[0]
        require(record.get("kind") == "chart", f"{path.name}: candidate kind mismatch")
        require(record.get("encoding") == "packed-chart", f"{path.name}: candidate encoding mismatch")
        require(record.get("playback") is True, f"{path.name}: candidate is not playback")
        require(record.get("compilerProfile") == "candidate.static-tap-lanes4-v1",
                f"{path.name}: candidate profile mismatch")
        for field in ("compiledSemanticIdentity", "artifactIdentity"):
            require(isinstance(record.get(field), str) and len(record[field]) == 64,
                    f"{path.name}: missing {field}")
    else:
        require(len(paths) != len(set(paths)), f"{path.name}: invalid fixture lacks duplicate path")


def check_invalid_encoding_fixture(path: Path) -> None:
    value = read_json(path)
    record = value["entries"][0]
    require(record["encoding"] not in {"source-json", "source-cxt", "packed-chart"},
            f"{path.name}: invalid encoding fixture is accidentally supported")

def check_schema_contract_fields() -> None:
    chart = read_json(SCHEMA_DIR / "cuexis.chart-entry.v1.schema.json")
    entry = chart["$defs"]["entry"]
    require(set(entry["required"]) == {"path", "kind", "encoding", "playback"},
            "chart-entry schema required fields changed")
    require(entry["properties"]["encoding"]["enum"] == ["source-json", "source-cxt", "packed-chart"],
            "chart-entry schema encoding set changed")
    preferences = read_json(SCHEMA_DIR / "cuexis.player-preferences.v1.schema.json")
    require(preferences["properties"]["format"]["const"] == "cuexis.player-preferences",
            "preferences format identity changed")
    require(preferences["properties"]["gain"]["minimum"] == 0 and
            preferences["properties"]["gain"]["maximum"] == 1,
            "preferences gain range changed")
    device = read_json(SCHEMA_DIR / "cuexis.audio-device-profile.v1.schema.json")
    correction = device["properties"]["outputCorrectionUs"]
    require(correction["minimum"] == -500000 and correction["maximum"] == 500000,
            "audio correction range changed")
    require(len(device["properties"]["selector"]["oneOf"]) == 2,
            "audio selector must have system-default and exact forms")


def check_identity_golden() -> None:
    execution = read_json(FIXTURE_DIR / "golden" / "execution_identity.json")
    execution_preimage = execution["domain"].encode("ascii") + b"\x00" + bytes.fromhex(execution["identityBytesHex"])
    require(execution_preimage.hex() == execution["preimageHex"], "execution preimage is not canonical")
    require(sha256_hex(execution_preimage) == execution["sha256"], "execution identity SHA mismatch")
    require(execution["executionId"] == "v5g1:" + execution["sha256"], "execution ID mismatch")

    prepared = read_json(FIXTURE_DIR / "golden" / "prepared_identity.json")
    resources = prepared["resources"]
    require([item["assetId"] for item in resources] == sorted(item["assetId"] for item in resources),
            "prepared resources are not ASCII sorted")
    require(len({item["assetId"] for item in resources}) == len(resources),
            "prepared resources contain duplicate IDs")
    prepared_preimage = (
        prepared["domain"].encode("ascii") + b"\x00"
        + u32(prepared["encodingVersion"])
        + bytes.fromhex(prepared["compiledSemanticIdentity"])
        + u32(len(resources))
        + b"".join(
            ascii_string(item["assetId"]) + bytes.fromhex(item["contentIdentity"])
            for item in resources
        )
    )
    require(prepared_preimage.hex() == prepared["preimageHex"], "prepared preimage is not canonical")
    require(sha256_hex(prepared_preimage) == prepared["sha256"], "prepared identity SHA mismatch")

    session = read_json(FIXTURE_DIR / "golden" / "session_identity.json")
    config = session["config"]
    require(config["clockMode"] == 3 and config["clockModeName"] == "CuexisAudio",
            "config clock mode characterization mismatch")
    require(-500000 <= config["outputCorrectionUs"] <= 500000,
            "config correction is outside the contract range")
    config_preimage = (
        config["domain"].encode("ascii") + b"\x00"
        + u32(config["encodingVersion"])
        + bytes([config["clockMode"]])
        + i64(config["outputCorrectionUs"])
    )
    require(config_preimage.hex() == config["preimageHex"], "config preimage is not canonical")
    require(sha256_hex(config_preimage) == config["sha256"], "config identity SHA mismatch")
    composite = session["session"]
    require(composite["preparedSemanticIdentity"] == prepared["sha256"],
            "session does not reference prepared identity")
    require(composite["resolvedSessionConfigIdentity"] == config["sha256"],
            "session does not reference config identity")
    composite_preimage = (
        composite["domain"].encode("ascii") + b"\x00"
        + u32(composite["encodingVersion"])
        + bytes.fromhex(composite["preparedSemanticIdentity"])
        + bytes.fromhex(composite["resolvedSessionConfigIdentity"])
    )
    require(composite_preimage.hex() == composite["preimageHex"], "session preimage is not canonical")
    require(sha256_hex(composite_preimage) == composite["sha256"], "session identity SHA mismatch")


def check_media_golden() -> None:
    image = read_json(FIXTURE_DIR / "golden" / "portable_texture2d.json")
    pixels = bytes.fromhex(image["pixelsRgba8Hex"])
    image_bytes = (
        b"CXPRES01"
        + u32(image["resourceKind"])
        + u32(image["payloadVersion"])
        + struct.pack("<Q", image["totalByteCount"])
        + u32(image["width"])
        + u32(image["height"])
        + u32(2)
        + u32(0)
        + pixels
    )
    require(image_bytes.hex() == image["bytesHex"], "texture bytes are not canonical")
    require(len(image_bytes) == image["totalByteCount"] == 48, "texture byte count mismatch")
    require(image_bytes[:8] == b"CXPRES01", "texture magic mismatch")
    require(image_bytes[8:12] == struct.pack("<I", 2), "texture kind mismatch")
    require(image_bytes[12:16] == struct.pack("<I", 1), "texture payload version mismatch")
    require(sha256_hex(image_bytes) == image["sha256"], "texture SHA mismatch")
    require(image["pixelsRgba8Hex"] == "ff0000ff00ff0080", "texture pixels mismatch")

    wav = read_json(FIXTURE_DIR / "golden" / "canonical_wav.json")
    samples = struct.pack("<" + "h" * len(wav["samples"]), *wav["samples"])
    wav_bytes = (
        b"RIFF"
        + u32(42)
        + b"WAVEfmt "
        + u32(16)
        + struct.pack("<HHIIHH", 1, wav["channels"], wav["sampleRate"],
                      wav["sampleRate"] * wav["channels"] * 2, wav["channels"] * 2, 16)
        + b"data"
        + u32(len(samples))
        + samples
    )
    require(wav_bytes.hex() == wav["bytesHex"], "WAV bytes are not canonical")
    require(len(wav_bytes) == wav["byteCount"] == 50, "WAV byte count mismatch")
    require(wav_bytes[:4] == b"RIFF" and wav_bytes[8:12] == b"WAVE", "WAV container mismatch")
    require(wav_bytes[12:16] == b"fmt " and wav_bytes[36:40] == b"data", "WAV chunk order mismatch")
    require(struct.unpack_from("<HHIIHH", wav_bytes, 20) == (1, 1, 8000, 16000, 2, 16),
            "WAV format fields mismatch")
    require(struct.unpack_from("<hhh", wav_bytes, 44) == tuple(wav["samples"]), "WAV samples mismatch")
    require(sha256_hex(wav_bytes) == wav["sha256"], "WAV SHA mismatch")


def check_audio_correction_golden() -> None:
    value = read_json(FIXTURE_DIR / "golden" / "audio_correction.json")
    require(value["unit"] == "microseconds", "audio correction unit changed")
    cases = {case["name"]: case for case in value["cases"]}
    require(set(cases) == {"zero_saturation", "negative_correction", "reverse_seek"},
            "audio correction cases changed")
    for case in cases.values():
        require(-500000 <= case["correctionUs"] <= 500000,
                f"{case['name']}: correction is outside the contract range")
    zero = cases["zero_saturation"]
    require(max(0, zero["rawAudioPositionUs"] - zero["correctionUs"]) ==
            zero["correctedAudioPositionUs"],
            "zero-saturation correction arithmetic changed")
    negative = cases["negative_correction"]
    require(max(0, negative["rawAudioPositionUs"] - negative["correctionUs"]) ==
            negative["correctedAudioPositionUs"],
            "negative correction arithmetic changed")
    seek = cases["reverse_seek"]
    require(seek["targetChartTimeUs"] + seek["timingOffsetUs"] + seek["correctionUs"] ==
            seek["sourcePositionUs"],
            "reverse seek correction arithmetic changed")


def check_text_boundaries() -> None:
    api = (ROOT / "docs" / "proposals" / "STAGE6_API_AND_INSTALL_DRAFT.md").read_text(encoding="utf-8")
    architecture = (ROOT / "docs" / "architecture" / "STAGE6_PRODUCTIZATION_BOUNDARIES.md").read_text(encoding="utf-8")
    entry_spec = (ROOT / "docs" / "formats" / "CHART_ENTRY_V1_FORMAT.md").read_text(encoding="utf-8")
    config_spec = (ROOT / "docs" / "formats" / "STAGE6_CONFIG_AND_MEDIA.md").read_text(encoding="utf-8")
    for name in ("fromFilesystemProjectEntry", "fromCxcFileEntry", "fromCxcMemoryEntry"):
        require(name in api and name in entry_spec, f"missing candidate factory {name}")
    for token in ("CUEXIS_ENABLE_CHART_V5_CANDIDATE", "Cuexis_ALLOW_EXPERIMENTAL=ON",
                  "candidate.static-tap-lanes4-v1", "cxc.candidate.profile_unsupported",
                  "cuexis.prepared-semantic.v5.candidate.1", "cuexis.execution-config.v5.candidate.1",
                  "cuexis.session.execution.v5.candidate.1"):
        require(token in api + entry_spec + config_spec, f"missing contract token {token}")
    for forbidden_edge in ("cuexis_render -> cuexis_presentation_renderer",
                           "cuexis_playback -> cuexis_media_import",
                           "cuexis_playback -> cuexis_player_support"):
        require(forbidden_edge not in architecture, f"forbidden dependency edge recorded: {forbidden_edge}")
    for required_edge in ("cuexis_presentation_renderer", "cuexis_render_opengl",
                          "cuexis_player_support", "cuexis_media_import"):
        require(required_edge in architecture, f"missing planned target {required_edge}")


def main() -> int:
    try:
        load_schemas()
        check_schema_contract_fields()
        check_entry_fixture(FIXTURE_DIR / "golden" / "candidate_entry_valid.json", valid=True)
        check_entry_fixture(FIXTURE_DIR / "invalid" / "candidate_entry_duplicate_path.json", valid=False)
        check_invalid_encoding_fixture(FIXTURE_DIR / "invalid" / "candidate_entry_invalid_encoding.json")
        check_identity_golden()
        check_media_golden()
        check_audio_correction_golden()
        check_text_boundaries()
    except AssertionError as error:
        print(f"S6-A2 characterization failed: {error}")
        return 1
    print("S6-A2 characterization passed: schemas, entry fixtures, identity/media goldens and boundaries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
