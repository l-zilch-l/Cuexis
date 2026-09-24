# Stage 6 E1/E2 Media Fixtures

Frozen inputs and canonical outputs for the offline media importer (`cuexis_media_import` and the
`cuexis_media_importer` CLI). The fixtures are recorded once and never regenerated to make a test
pass: a byte difference between platforms is a blocked profile decision, not a golden update.

## Layout

- `image/` — encoded image sources.
- `audio/` — encoded audio sources and the PCM WAV sources they were encoded from.
- `golden/` — the canonical artifact of every accepted input, keyed by test stem.

## Provenance

`generate_image_fixtures.py` writes the PNG inputs with the Python standard library only:

| Fixture | Purpose |
|---|---|
| `rgb8.png`, `rgba8.png` | 8-bit truecolour with and without alpha |
| `palette_trns.png` | palette with a `tRNS` transparency chunk |
| `gray8.png`, `gray_alpha16.png` | 8-bit grayscale; the 16-bit case must be rejected |
| `srgb_chunk.png` | explicit `sRGB` chunk |
| `iccp_non_srgb.png`, `gamma_unsupported.png` | rejected colour metadata |
| `apng.png` | APNG animation must be rejected |
| `forged_dimensions.png` | declared size above the image dimension budget |
| `corrupt_chunk.png`, `truncated.png` | damaged container |
| `budget_1024.png` | 1024x1024 truecolour: decoding needs 4 MiB of RGBA8, so the CLI gate can prove that a 1 MiB process memory limit fails closed. It has no canonical golden. |

`jpeg_fixtures.cpp` writes the JPEG inputs with libjpeg-turbo itself: baseline, progressive,
grayscale, CMYK, YCCK, the four supported EXIF orientations, an out-of-range orientation and a
contradictory pair of orientation tags. It is compiled by hand against the **release** libjpeg-turbo
from the `debug-media-tools` vcpkg tree (`jpeg.lib` plus `jpeg62.dll`, `/MD`); mixing the debug
import library with the release runtime makes `jpeg_finish_compress` fail inside an invisible
assertion dialog. The JPEG fixtures are committed, so the writer is provenance only.

`generate_encoded_fixtures.py` writes the audio inputs with `ffmpeg` from two PCM WAV sources
(`source_mono.wav` at 8000 Hz mono, `source_stereo.wav` at 44100 Hz stereo): MP3 (`libmp3lame`),
Ogg Vorbis (`libvorbis`) and native FLAC for both, plus a three-channel and an out-of-range-rate
FLAC for the rejection paths. The PCM sources are the lossless anchors: the canonical WAV of a FLAC
import must equal `source_mono.wav` / `source_stereo.wav` byte for byte.

`generate_negative_fixtures.py` derives the damaged audio inputs: truncated MP3, Ogg and FLAC
copies, a FLAC with a destroyed metadata block header, a chained Ogg stream, and a FLAC whose
`STREAMINFO` total-sample field is forged.

`generate_goldens.py` records `golden/*.json` from a local importer build. Each golden holds the
fixture path, kind, media profile identity, decoder identity, SHA-256 and byte count of the
canonical artifact, the conversion usage, the image or audio geometry, and — for artifacts of at
most 4096 bytes — the complete canonical byte sequence.

```powershell
python -B tests/fixtures/stage6_e/media/generate_goldens.py `
    out/build/debug-media-tools/bin/cuexis_media_importer.exe
```

Regenerating a golden is only legitimate when the frozen profile decision itself changes, which
requires an ADR update. The image goldens also cross-check the profile: `baseline.jpg`,
`progressive.jpg` and `orientation1.jpg` must produce the same canonical bytes, because entropy
coding and the identity orientation cannot change decoded pixels.
