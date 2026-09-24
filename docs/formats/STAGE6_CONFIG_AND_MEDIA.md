# Stage 6 Configuration And Media Contract

Status: S6-A2 contract record. This document freezes the fields needed by the Stage 6
implementation batches; it does not claim that the Player support library or media importer
exists. The owning decision is ADR 0042.

## 1. Configuration Ownership

The application layer owns file discovery, JSON validation, migration dispatch, source diagnostics,
atomic persistence and immutable typed snapshots. Playback does not read a preferences path, an SDL
device list or mutable global configuration.

| Value | Owner | Persisted fields | Lifetime | Apply class |
| --- | --- | --- | --- | --- |
| ProjectConfig v1 | Player/Studio project layer | Existing project identity, roots and entry.chart; candidate extension only | Project | Content input; never overridden by preferences |
| UserPreferences v1 | Player support | Window size, fullscreen, vsync, gain, AudioDeviceProfile ID | User config directory | Window/backend fields rebuild; gain is explicit dynamic apply; profile is explicit audio rebuild |
| AudioDeviceProfile v1 | Player support | Profile ID, selector and output correction | User config directory | Audio device rebuild; no implicit fallback for explicit profiles |
| LaunchOptions | Player front end | Explicitly named command options | Current process | Only fields explicitly allowed by the command contract |
| ResolvedAppConfig | Player support | Requested values and source ownership | Immutable application snapshot | Input to subsystem construction |
| ResolvedSessionConfig | Player support | Consumed clock mode and output correction only | Frozen before Session/prepare | Candidate execution identity |
| EffectiveSettings | Subsystems | Actual negotiated values and diagnostics | Active bundle | Observation only; never written back automatically |

UserPreferences does not contain selector fields, correction, chart offsets, timing maps, resource
paths, semantic identities, input calibration, judgement rules or arbitrary key/value extensions.
AudioDeviceProfile does not contain input latency or subjective judgement calibration.

The only v1 UserPreferences defaults are:

| Field | Default | Source | Apply class |
| --- | --- | --- | --- |
| windowWidth | 1280 | Player code default | Window/backend rebuild |
| windowHeight | 720 | Player code default | Window/backend rebuild |
| fullscreen | false | Player code default | Window/backend rebuild |
| vsync | true | Player code default | Window/backend rebuild |
| gain | 1.0 | Player code default | Dynamic audio apply through a separately named API |
| audioDeviceProfileId | system-default | Player code default | Explicit audio rebuild |

There is one code source for these defaults. A missing or malformed UserPreferences file records a
diagnostic and uses these defaults. A future or unsupported UserPreferences version also uses the
documented defaults; the implementation must not invent a v0 migration. An explicitly selected
profile that is absent, malformed, too new or unmatched is an error, not a silent switch to another
profile. Normalization is idempotent and must not add arbitrary keys.

UserPreferences writes use a same-directory temporary file, complete UTF-8 serialization and
validation, an inter-process write lock and one atomic replacement. A failed write leaves the last
valid file unchanged. Two writers targeting the same file do not use last-writer-wins; the later
writer returns a busy diagnostic. File changes after an active bundle is published do not mutate
that bundle.

## 2. AudioDeviceProfile v1

The file identity is cuexis.audio-device-profile, version 1. The id is a portable ASCII profile
identifier. selector has exactly one of these forms:

~~~json
{ "kind": "system-default" }
~~~

or:

~~~json
{
  "kind": "exact",
  "driver": "SDL audio driver UTF-8 bytes",
  "deviceName": "exact SDL UTF-8 device name bytes"
}
~~~

The exact selector compares the pair (SDL audio driver, exact UTF-8 device name) byte-for-byte for
one output device. It does not use an ordinal, SDL process ID, fuzzy match, first-item fallback, or
an unverified OS hardware identifier. Enumeration binds an instance ID only for the current open;
the pair is checked again immediately before opening and must match exactly one output device.
Hot removal requires an explicit re-match. This is a reproducible selection condition, not hardware
serial-number authentication; same-name replacement hardware is outside the guarantee.

outputCorrectionUs is a signed integer microsecond value in [-500000, 500000]. It defaults to zero.
system-default must use zero; nonzero correction requires an exact selector. Positive values mean the
device output is later than the existing estimate. The correction is not put into music bytes, Chart
offset, or the raw audio snapshot.

For CuexisAudio, the adapter applies the correction exactly once before RuntimeTimeline:

~~~text
positionMs = max(0, rawAudioPositionMs - correctionUs / 1000)
~~~

For reverse seek conversion:

~~~text
sourcePositionMs = targetChartTimeMs + timingOffsetMs + correctionUs / 1000
~~~

The user target is rejected when the resulting source position is outside [0, duration]; it is not
silently clamped. ChartClock ignores audio correction. HostClock calibration remains a host
responsibility.
For deterministic implementation and testing, the same equations may be evaluated as checked signed
integer microseconds: `max(0, rawAudioPositionUs - correctionUs)` and
`targetChartTimeUs + timingOffsetUs + correctionUs`. The A2 characterization covers zero saturation,
negative correction and reverse seek in [audio_correction.json](../../tests/fixtures/stage6_a2/golden/audio_correction.json).

## 3. Session Identity Encoding

These are internal candidate execution observations, not public structs in SDK 0.7.0. The fields
and bytes are recorded so C1/C2 can implement one deterministic encoder and so a configuration
change cannot alter chart semantic identity.

### 3.1 Prepared semantic identity

The candidate domain is cuexis.prepared-semantic.v5.candidate.1 followed by one NUL byte. All
integers are fixed-width little-endian. The preimage is:

~~~text
domain + NUL
encodingVersion: u32 = 1
compiledSemanticIdentity: 32 bytes
resourceCount: u32
repeat resourceCount times, AssetId ASCII order:
  assetIdByteCount: u32
  assetId bytes
  actualContentIdentity: 32 bytes
~~~

Asset IDs use raw portable ASCII byte ordering. Each ID occurs once. The resource list is the actual
validated transitive closure, not only a declared requirement list. A missing resource, duplicate
ID, or one ID with two identities fails before publication. Paths, archive layout, package hash,
provider revision, device selector, window, clock mode, gain and correction are excluded.

For main music content, the resource identity is SHA-256 of cuexis.prepared-audio.v1 plus a NUL
byte and the exact fetched bytes. Presentation content uses its existing Portable Presentation
semantic identity. Existing v4 PreparedSemanticIdentity is not changed by this candidate contract.

### 3.2 Resolved session configuration identity

The candidate domain is cuexis.execution-config.v5.candidate.1 followed by one NUL byte. The
preimage is:

~~~text
domain + NUL
encodingVersion: u32 = 1
clockMode: u8 (ChartClock=1, HostClock=2, CuexisAudio=3)
outputCorrectionUs: i64 little-endian
~~~

Only values actually consumed by the playback clock belong here. Window state, vsync, gain, device
name and source path are application/backend state and do not enter this identity. The output
correction is range-checked as above; only CuexisAudio consumes nonzero correction.

### 3.3 Session execution identity

The composite domain is cuexis.session.execution.v5.candidate.1 followed by one NUL byte. Its
preimage is:

~~~text
domain + NUL
encodingVersion: u32 = 1
preparedSemanticIdentity: 32 bytes
resolvedSessionConfigIdentity: 32 bytes
~~~

This composite is an application/session observation. It does not replace either component
identity, the CXC package identity, artifact identity or FrameDigest.

## 4. Transaction And State Boundary

All Player commands are typed commands handled serially on one owner thread. Stage 6 does not add
background prepare, cancellation, automatic retry or file-watch reload. A UI or command-line event
uses the same command layer as smoke and reference-host test drivers.

The transaction order is fixed:

1. Validate the command and current state; capture target time and freeze configuration.
2. Prepare Playback, all renderer candidates and any audio replacement.
3. Validate owner, generation and transaction tokens; prohibit re-entry and state changes.
4. Activate the audio replacement. This is the last step allowed to fail for a physical device.
5. Commit Playback state, then activate the renderer without failure.
6. Publish the active application bundle, EffectiveSettings and one timeline discontinuity.

Software preparation failure keeps the old active bundle, active identity, content generation and
transport state. It does not reset the old timeline or pair old audio with new presentation. An
audio activation failure cancels every uncommitted candidate and enters a diagnostic Failed state;
it does not claim that old audio remains safely active. A post-commit renderer/present failure does
not roll content back; it enters suspended/failed and recovers only through explicit Rebuild.

Reload keeps PlaybackMode. Switching audio presence or device is an explicit Open/Rebuild. There is
at most one load/reload/rebuild at a time. No public no-fail commit claim substitutes for the private
token/guard check required before step 4.

## 5. Media Import Profile

The media profile is selected but not yet verified for release. The fixed vcpkg baseline records:

| Input | Decoder | Baseline record | Output |
| --- | --- | --- | --- |
| PNG | libpng | 1.6.58 | CXPRES01 Texture2D RGBA8 sRGB |
| JPEG | libjpeg-turbo | 3.2.0 | CXPRES01 Texture2D RGBA8 sRGB |
| MP3 | minimp3 | 2021-11-30 | RIFF/WAVE PCM S16LE |
| Ogg Vorbis | libvorbis 1.3.7#4 + libogg 1.3.6#1 | Fixed baseline ports | RIFF/WAVE PCM S16LE |
| FLAC | libFLAC | 1.5.0 | RIFF/WAVE PCM S16LE |

The baseline record is not license or cross-platform release evidence. Before E1/E2 close, the
implementation must verify installed license files, build options, decoder error behavior and
Windows MSVC, Windows MinGW, Linux GCC and Linux Clang byte equality. A failure is a blocked
profile decision and requires owner re-ruling; it must not silently change decoders or golden data.

The importer is a separate default-OFF media-tools feature. cuexis_media_import and its CLI own
decoding; Playback and Player do not link it and do not launch it for implicit runtime decoding.
The adapters are thin and fixed rather than a dynamic codec plugin registry. No ffmpeg, system codec
probing, self-written codec or failed-decoder fallback is allowed.

### 5.1 Image output

The output is the existing Portable Presentation Texture2D v1 envelope: CXPRES01, kind 2, payload
version 1, 24-byte envelope, followed by width, height, colorSpace=sRGB, reserved zero and tightly
packed top-left RGBA8 pixels. Straight alpha, no row padding, mip 0 only. Missing color metadata
defaults to sRGB. Palette, gray and tRNS are expanded to RGBA. 16-bit PNG, APNG, CMYK/YCCK,
unsupported ICC/gamma and malformed or contradictory EXIF orientation are rejected. PNG/JPEG
orientation 1-8 is normalized once at import.

### 5.2 Audio output

The output is a canonical RIFF/WAVE PCM S16LE file with only the normative fmt and data chunks.
Input sample rate and mono/stereo channel count are preserved within the profile; no resampling,
mixing, loudness normalization, dithering, timestamps or source paths are written. MP3 delay/padding
is trimmed once only when valid metadata is present. Vorbis uses the granule end for truncation and
rejects chained or multiplexed streams. FLAC preserves sample count. Float Vorbis conversion uses a
specified integer rounding/saturation rule independent of the process rounding mode; NaN/Inf is
rejected. FLAC high-depth conversion uses fixed integer rounding and saturation. Fast-math and
FMA-changing options are forbidden for conversion code.

Truncated data, bad frames, holes, checksum errors and decoder exceptions fail closed. They are not
recovered by zero-fill, frame skipping, resynchronization or silent end-of-stream acceptance.

### 5.3 Implemented Rejection Taxonomy

The importer reports one stable code per rejected input. `media.source.empty` and
`media.source.limit` cover the encoded source, `media.budget.working_limit` and
`media.image.pixel_limit`, `media.image.byte_limit`, `media.audio.duration_limit`,
`media.audio.wav_limit` cover the budgets, `media.publish.immutable_conflict` covers publication,
and `media.io.*` covers input and output failures.

Images report `media.image.format_unsupported` for a container that is neither PNG nor JPEG or for
APNG animation, `media.image.truncated` for a source that ends inside the container,
`media.image.header_invalid` for inconsistent headers, `media.image.dimension_invalid` for an
oversized or overflowing declared size, `media.image.bit_depth_unsupported` for 16-bit PNG,
`media.image.color_type_unsupported` for unexpandable PNG colour types and for CMYK/YCCK JPEG,
`media.image.icc_unsupported` and `media.image.gamma_unsupported` for unsupported colour metadata,
`media.image.exif_invalid` and `media.image.orientation_invalid` for malformed, out-of-range or
contradictory EXIF orientation, and `media.image.decode_failed` or `media.image.decode_incomplete`
for a decoder-level failure.

Audio reports `media.audio.format_unsupported` for a container that is not MP3, Ogg Vorbis or native
FLAC or for a non-Layer-III MPEG stream, `media.audio.container_invalid` for unusable container
metadata, `media.audio.truncated` for a stream that ends before the length its own metadata
declares, `media.audio.chained_stream` for a chained or multiplexed Ogg stream,
`media.audio.channels_unsupported` and `media.audio.rate_unsupported` outside the profile,
`media.audio.sample_non_finite` for a non-finite float Vorbis sample, and
`media.audio.decode_failed` for a decoder-level or checksum failure. A source that decodes to zero
frames is never accepted.

### 5.4 Evidence State

E1/E2 implementation and local Windows/MSVC verification are recorded in
[the S6-E1/E2 report](../stage_reports/stages/stage-06/2026-09-25-s6-e1-e2-media-importer.md).
Installed license files, build options and four-platform byte equality are hosted evidence: until a
same-SHA hosted matrix passes, the profile is implemented but not release-verified.

## 6. Budgets And Atomic Publication

The importer uses the stricter of these limits and any existing resource limit:

| Resource | Hard limit |
| --- | ---: |
| Encoded source | 64 MiB |
| Image width or height | 8192 |
| Image pixels | 8,388,608 |
| Decoded RGBA8 image | 32 MiB |
| Audio sample rate | 8,000 through 192,000 Hz |
| Audio channels | 1 or 2 |
| Audio duration | 1,800 seconds |
| Final WAV including header | 64 MiB |
| One import extra conversion memory | 128 MiB, excluding source and final output |
| Import concurrency | 1 |

All counts and products use checked arithmetic before reserve/resize and each row/frame is checked
while decoding. Metadata lengths are untrusted. If a library allocation cannot be measured with a
hard limit, import runs in a bounded worker process; catching bad_alloc at the outer boundary is not
a budget implementation.

Cache keys contain the original bytes hash, conversion profile, exact decoder version and build
options. Cache output is revalidated. Output paths are content-identity addressed and immutable.
Project assets are written into a new generation directory. CXC publication writes a same-directory
temporary package, closes and validates it, then performs one atomic replacement while holding a
process-shared publication lock. A concurrent writer returns busy. Individual file renames do not
claim project-level atomicity. Import does not rewrite the user project entry automatically;
adoption is explicit through generation selection or repack.

## 7. A2 Evidence State

The identity and canonical media fixtures under tests/fixtures/stage6_a2/golden/ are deterministic
preimage/byte characterizations. They are not decoder interoperability, hardware, hosted or
release-license evidence. Those checks belong to C2, E1/E2, C4 and F1.

The E1/E2 canonical outputs under tests/fixtures/stage6_e/golden/ are recorded from the fixed
profile implementation. They pin the decoder versions and build options that produced them through
the media profile identity, and the library-level suite verifies the profile, the canonical layout,
the rejection taxonomy and the budgets. Cross-platform byte equality, installed license files and
release build options remain hosted evidence.
