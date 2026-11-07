// cuexis_media_importer: the offline media import CLI.
//
// The importer is a separate default-OFF media-tools artifact. Playback and Player never link it
// and never launch it for implicit runtime decoding. It converts one bounded encoded source into
// one content-identity addressed, immutable canonical artifact, and it can additionally
//
//   * serve an identical request from the identity revalidating media cache,
//   * write an author-side provenance sidecar that keeps the raw source, profile, artifact and
//     AssetId identities apart,
//   * publish the artifact and its provenance through the asset publication transaction, so a
//     generation either appears complete or does not appear at all.
//
// None of those are implicit: the cache is used only when --cache-dir is given, a stale or corrupt
// cache entry is refused unless --rebuild is given explicitly, and provenance and generation
// publication happen only when their options are given.

#include "publish.hpp"
#include "worker_process.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/media_import/media_cache.hpp>
#include <cuexis/media_import/media_import.hpp>
#include <cuexis/media_import/media_provenance.hpp>
#include <cuexis/tools/asset_publish.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace media = cuexis::media_import;
namespace publish = cuexis::tools;
using cuexis::media_importer::publishImmutable;
using cuexis::media_importer::readFileBounded;
using cuexis::media_importer::runBoundedWorker;
using cuexis::media_importer::WorkerRequest;

constexpr std::uint64_t workerProcessOverheadBytes = 64ULL * 1024ULL * 1024ULL;

// Decoder family tokens. The exact decoder builds are pinned by mediaProfileIdentity(), which
// hashes every decoder version and the build options, so the family token only has to be stable for
// a kind. It exists because a cache lookup happens before any import has selected a decoder.
constexpr std::string_view imageDecoderFamily = "libpng+libjpeg-turbo";
constexpr std::string_view audioDecoderFamily = "minimp3+libvorbis+libflac";

struct Options final {
    std::string kind;
    std::filesystem::path input;
    std::filesystem::path outputDirectory;
    std::filesystem::path workerOutput;
    // S6-E3: resource identity, cache and generation publication.
    std::string assetId;
    std::filesystem::path provenanceDirectory;
    std::filesystem::path cacheDirectory;
    std::filesystem::path generationDirectory;
    std::string generationId;
    bool rebuild{false};
    bool worker{false};
    bool inProcess{false};
    bool printInfo{false};
    bool help{false};
    std::uint64_t memoryLimit{0};
};

void printUsage(std::ostream& stream) {
    stream << "usage: cuexis_media_importer --kind <image|audio> --input <file> "
              "--output-dir <dir> [--print-info] [--in-process] [--memory-limit <bytes>]\n"
              "       [--asset-id <id>] [--provenance-dir <dir>] [--cache-dir <dir>] [--rebuild]\n"
              "       [--generation-dir <dir> --generation-id <id>]\n"
              "       cuexis_media_importer --worker --kind <image|audio> --input <file> "
              "--output <file>\n";
}

[[nodiscard]] auto parseArguments(int argc, char** argv, Options& options) -> bool {
    for (int index = 1; index < argc; ++index) {
        const std::string argument{argv[index]};
        const auto next = [&](std::string& target) {
            if (index + 1 >= argc) {
                return false;
            }
            target = argv[++index];
            return true;
        };
        if (argument == "--kind") {
            if (!next(options.kind)) {
                return false;
            }
        } else if (argument == "--input") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.input = value;
        } else if (argument == "--output-dir") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.outputDirectory = value;
        } else if (argument == "--output") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.workerOutput = value;
        } else if (argument == "--memory-limit") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            try {
                options.memoryLimit = std::stoull(value);
            } catch (const std::exception&) {
                return false;
            }
        } else if (argument == "--asset-id") {
            if (!next(options.assetId)) {
                return false;
            }
        } else if (argument == "--provenance-dir") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.provenanceDirectory = value;
        } else if (argument == "--cache-dir") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.cacheDirectory = value;
        } else if (argument == "--generation-dir") {
            std::string value;
            if (!next(value)) {
                return false;
            }
            options.generationDirectory = value;
        } else if (argument == "--generation-id") {
            if (!next(options.generationId)) {
                return false;
            }
        } else if (argument == "--rebuild") {
            options.rebuild = true;
        } else if (argument == "--worker") {
            options.worker = true;
        } else if (argument == "--in-process") {
            options.inProcess = true;
        } else if (argument == "--print-info") {
            options.printInfo = true;
        } else if (argument == "--help") {
            options.help = true;
        } else {
            return false;
        }
    }
    if (options.help) {
        return true;
    }
    if (options.kind != "image" && options.kind != "audio") {
        return false;
    }
    if (options.input.empty()) {
        return false;
    }
    if (options.worker) {
        return !options.workerOutput.empty();
    }
    if (options.outputDirectory.empty()) {
        return false;
    }
    // A generation needs a directory, a label and the AssetId its runtime path is derived from.
    if (options.generationDirectory.empty() != options.generationId.empty()) {
        return false;
    }
    if (!options.generationDirectory.empty() && options.assetId.empty()) {
        return false;
    }
    return true;
}

[[nodiscard]] auto decoderFamily(std::string_view kind) -> std::string_view {
    return kind == "image" ? imageDecoderFamily : audioDecoderFamily;
}

[[nodiscard]] auto artifactFileName(const Options& options, const std::string& identity)
    -> std::string {
    return options.kind == "image" ? identity + ".texture.bin" : identity + ".wav";
}

// Runtime entry path inside a published generation, derived from the resource AssetId.
[[nodiscard]] auto runtimeEntryPath(std::string_view kind, std::string_view encodedAssetId)
    -> std::string {
    std::string path = kind == "image" ? "textures/" : "audio/";
    path.append(encodedAssetId);
    path += kind == "image" ? ".texture.bin" : ".wav";
    return path;
}

[[nodiscard]] auto defaultMemoryLimit(std::uint64_t sourceBytes) -> std::uint64_t {
    // Measured conversion budget plus the source, the largest possible artifact and process
    // overhead. The limit is a backstop for decoder-internal allocations.
    return media::MediaBudget{}.maxWorkingBytes + sourceBytes + media::MediaBudget{}.maxWavBytes +
           workerProcessOverheadBytes;
}

[[nodiscard]] auto describeImage(const media::ImageImportResult& result) -> std::string {
    std::string info = "{\"kind\":\"image\"";
    info += ",\"decoder\":\"" + result.info.decoder + "\"";
    info += ",\"width\":" + std::to_string(result.info.width);
    info += ",\"height\":" + std::to_string(result.info.height);
    info += ",\"sourceWidth\":" + std::to_string(result.info.sourceWidth);
    info += ",\"sourceHeight\":" + std::to_string(result.info.sourceHeight);
    info += ",\"orientation\":" + std::to_string(result.info.orientation);
    info += ",\"sourceBytes\":" + std::to_string(result.usage.sourceBytes);
    info += ",\"decodedBytes\":" + std::to_string(result.usage.decodedBytes);
    info += ",\"peakWorkingBytes\":" + std::to_string(result.usage.peakWorkingBytes);
    info += ",\"profile\":\"" + media::mediaProfileIdentity() + "\"";
    info += "}";
    return info;
}

[[nodiscard]] auto describeAudio(const media::AudioImportResult& result) -> std::string {
    std::string info = "{\"kind\":\"audio\"";
    info += ",\"decoder\":\"" + result.info.decoder + "\"";
    info += ",\"sampleRate\":" + std::to_string(result.info.sampleRate);
    info += ",\"channels\":" + std::to_string(result.info.channels);
    info += ",\"frames\":" + std::to_string(result.info.frames);
    info += ",\"sourceBytes\":" + std::to_string(result.usage.sourceBytes);
    info += ",\"decodedBytes\":" + std::to_string(result.usage.decodedBytes);
    info += ",\"peakWorkingBytes\":" + std::to_string(result.usage.peakWorkingBytes);
    info += ",\"profile\":\"" + media::mediaProfileIdentity() + "\"";
    info += "}";
    return info;
}

struct ImportOutcome final {
    std::vector<std::byte> bytes;
    std::string identity;
    std::string decoder;
    std::string info;
    // True when the artifact came from the cache instead of a decoder run.
    bool fromCache{false};
    // True when the artifact is already published, so the caller must not publish it again. The
    // bounded worker path promotes its temporary file itself and only reads the bytes back when a
    // later stage needs them.
    bool published{false};
};

[[nodiscard]] auto importSource(const Options& options, std::span<const std::byte> source)
    -> cuexis::core::Result<ImportOutcome> {
    ImportOutcome outcome;
    if (options.kind == "image") {
        auto result = media::importImage(source);
        if (!result) {
            return cuexis::core::unexpected(result.error());
        }
        outcome.decoder = result->info.decoder;
        outcome.bytes = std::move(result->portableTexture);
        outcome.info = describeImage(*result);
    } else {
        auto result = media::importAudio(source);
        if (!result) {
            return cuexis::core::unexpected(result.error());
        }
        outcome.decoder = result->info.decoder;
        outcome.bytes = std::move(result->canonicalWav);
        outcome.info = describeAudio(*result);
    }
    outcome.identity = media::contentIdentity(outcome.bytes);
    return outcome;
}

// Reads one string field out of the worker's info line. The worker prints the same info object as
// the in-process path, so the parent reports the decoder that actually ran.
[[nodiscard]] auto infoField(std::string_view info, std::string_view key) -> std::string {
    const std::string needle = "\"" + std::string{key} + "\":\"";
    const auto at = info.find(needle);
    if (at == std::string_view::npos) {
        return {};
    }
    const auto begin = at + needle.size();
    const auto end = info.find('"', begin);
    if (end == std::string_view::npos) {
        return {};
    }
    return std::string{info.substr(begin, end - begin)};
}

int fail(const std::string& code, const std::string& message) {
    std::cerr << "error: " << code << ": " << message << "\n";
    return 2;
}

int fail(const cuexis::core::Error& error) {
    return fail(std::string{error.code()}, std::string{error.message()});
}

// Converts one bounded source into a canonical artifact, through the worker or in process.
[[nodiscard]] auto convert(const Options& options, std::span<const std::byte> source, char** argv)
    -> cuexis::core::Result<ImportOutcome> {
    if (options.worker || options.inProcess) {
        return importSource(options, source);
    }

    const auto limit =
        options.memoryLimit != 0 ? options.memoryLimit : defaultMemoryLimit(source.size());
    if (options.memoryLimit != 0 && !cuexis::media_importer::workerAddressSpaceCapSupported) {
        std::cerr << "note: this sanitized build cannot apply an address-space limit, so "
                     "--memory-limit is ignored\n";
    }
    const auto temporary =
        options.outputDirectory /
        ("import-" + std::to_string(cuexis::media_importer::currentProcessId()) + ".tmp");
    WorkerRequest request;
    request.executable = std::filesystem::absolute(argv[0]);
    request.memoryLimitBytes = limit;
    request.arguments = {
        "--worker", "--kind",           options.kind,  "--input", options.input.string(),
        "--output", temporary.string(), "--print-info"};

    auto outcome = runBoundedWorker(request);
    if (!outcome) {
        return cuexis::core::unexpected(outcome.error());
    }
    if (outcome->exitCode != 0) {
        std::cerr << outcome->output;
        return cuexis::core::unexpected(
            cuexis::core::Error{"media.worker.failed", "The bounded worker did not finish"}
                .withContext("exitCode", std::to_string(outcome->exitCode))
                .withContext("memoryLimitBytes", std::to_string(limit)));
    }
    auto identity = cuexis::media_importer::hashFile(temporary);
    if (!identity) {
        std::filesystem::remove(temporary);
        return cuexis::core::unexpected(identity.error());
    }
    auto published = cuexis::media_importer::promoteTemporary(
        temporary, options.outputDirectory / artifactFileName(options, *identity), *identity);
    if (!published) {
        return cuexis::core::unexpected(published.error());
    }

    ImportOutcome converted;
    converted.identity = *identity;
    converted.decoder = infoField(outcome->output, "decoder");
    converted.info = outcome->output;
    converted.published = true;
    // The artifact is read back only when the caller still needs the bytes: the cache and the
    // provenance record both do, a plain publication does not.
    if (!options.cacheDirectory.empty() || !options.assetId.empty() ||
        !options.generationDirectory.empty()) {
        auto bytes = readFileBounded(options.outputDirectory / artifactFileName(options, *identity),
                                     media::MediaBudget{}.maxWavBytes);
        if (!bytes) {
            return cuexis::core::unexpected(bytes.error());
        }
        converted.bytes = std::move(*bytes);
    }
    return converted;
}

[[nodiscard]] auto asBytes(std::string_view text) -> std::vector<std::byte> {
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return std::vector<std::byte>{data, data + text.size()};
}

// Publishes the artifact, the provenance sidecar and, when asked for, a complete generation.
[[nodiscard]] auto publishAll(const Options& options, const ImportOutcome& outcome,
                              const media::MediaProvenance& provenance)
    -> cuexis::core::Result<void> {
    auto artifact = publishImmutable(
        options.outputDirectory / artifactFileName(options, outcome.identity), outcome.bytes);
    if (!artifact) {
        return cuexis::core::unexpected(artifact.error());
    }

    auto provenanceName = media::provenanceFileName(provenance.assetId);
    if (!provenanceName) {
        return cuexis::core::unexpected(provenanceName.error());
    }
    const auto provenanceDirectory =
        options.provenanceDirectory.empty() ? options.outputDirectory : options.provenanceDirectory;
    const auto encoded = media::encodeProvenance(provenance);
    auto sidecar = publishImmutable(provenanceDirectory / *provenanceName, asBytes(encoded));
    if (!sidecar) {
        return cuexis::core::unexpected(sidecar.error());
    }

    if (options.generationDirectory.empty()) {
        return {};
    }
    auto encodedAssetId = media::encodedAssetIdName(provenance.assetId);
    if (!encodedAssetId) {
        return cuexis::core::unexpected(encodedAssetId.error());
    }
    publish::GenerationPublishRequest request;
    request.root = options.generationDirectory;
    request.generationId = options.generationId;
    // The runtime closure holds the artifact; the provenance sidecar is an audit record stored
    // beside the closure, never a runtime resource. Record paths are relative to the generation's
    // provenance directory, which the publication adds.
    request.entries = {
        publish::PublishEntry{runtimeEntryPath(options.kind, *encodedAssetId), outcome.bytes}};
    request.provenanceRecords = {publish::PublishEntry{*provenanceName, asBytes(encoded)}};
    auto generation = publish::publishGeneration(request);
    if (!generation) {
        return cuexis::core::unexpected(generation.error());
    }
    return {};
}

[[nodiscard]] auto makeProvenance(const Options& options, std::span<const std::byte> source,
                                  const ImportOutcome& outcome) -> media::MediaProvenance {
    media::MediaProvenance provenance;
    provenance.kind = options.kind == "image" ? media::MediaArtifactKind::Texture
                                              : media::MediaArtifactKind::Audio;
    provenance.rawSourceIdentity = media::contentIdentity(source);
    provenance.rawSourceBytes = static_cast<std::uint64_t>(source.size());
    provenance.rawSourceLocator = options.input.generic_string();
    provenance.profileIdentity = media::mediaProfileIdentity();
    provenance.decoderVersion = outcome.decoder;
    provenance.artifactIdentity = outcome.identity;
    provenance.artifactBytes = static_cast<std::uint64_t>(outcome.bytes.size());
    provenance.assetId = options.assetId;
    return provenance;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage(std::cerr);
        return 1;
    }
    if (options.help) {
        printUsage(std::cout);
        return 0;
    }

    auto source = readFileBounded(options.input, media::MediaBudget{}.maxEncodedBytes);
    if (!source) {
        return fail(source.error());
    }

    // A worker only ever converts; the parent owns identity, provenance and publication.
    if (options.worker) {
        auto outcome = importSource(options, *source);
        if (!outcome) {
            return fail(outcome.error());
        }
        if (auto published = publishImmutable(options.workerOutput, outcome->bytes); !published) {
            return fail(published.error());
        }
        if (options.printInfo) {
            std::cout << outcome->info << "\n";
        }
        return 0;
    }

    const bool cacheEnabled = !options.cacheDirectory.empty();
    media::MediaCacheKeyInput keyInput;
    keyInput.rawSourceIdentity = media::contentIdentity(*source);
    keyInput.profileIdentity = media::mediaProfileIdentity();
    keyInput.decoderVersion = std::string{decoderFamily(options.kind)};
    const media::MediaCacheStore cache{options.cacheDirectory};

    if (cacheEnabled && options.rebuild) {
        // Rebuilding is explicit and only clears this one request: the content addressed artifact
        // stays, because another request may reference the same bytes.
        auto discarded = cache.discard(keyInput);
        if (!discarded) {
            return fail(discarded.error());
        }
    }

    ImportOutcome outcome;
    if (cacheEnabled) {
        auto hit = cache.load(keyInput);
        if (hit) {
            outcome.bytes = std::move(hit->artifact);
            outcome.identity = hit->record.artifactIdentity;
            outcome.decoder = hit->record.decoder;
            outcome.fromCache = true;
            outcome.info = "{\"kind\":\"" + options.kind + "\",\"cached\":true,\"decoder\":\"" +
                           outcome.decoder + "\",\"artifactIdentity\":\"" + outcome.identity +
                           "\",\"artifactBytes\":" + std::to_string(outcome.bytes.size()) +
                           ",\"profile\":\"" + media::mediaProfileIdentity() + "\"}";
        } else if (std::string{hit.error().code()} != "media.cache.missing") {
            // A stale or damaged entry is refused with its own code. The importer never silently
            // re-imports on top of a cache entry it could not validate; --rebuild does that.
            return fail(hit.error());
        }
    }

    if (!outcome.fromCache) {
        auto converted = convert(options, *source, argv);
        if (!converted) {
            return fail(converted.error());
        }
        outcome = std::move(*converted);
    }

    if (options.assetId.empty()) {
        // Without an AssetId there is no resource identity to record, so publication stays the
        // identity addressed artifact only. The bounded worker path already promoted its artifact.
        if (!outcome.published) {
            if (auto published = publishImmutable(options.outputDirectory /
                                                      artifactFileName(options, outcome.identity),
                                                  outcome.bytes);
                !published) {
                return fail(published.error());
            }
        }
    } else {
        const auto provenance = makeProvenance(options, *source, outcome);
        if (auto validation = media::validateProvenance(provenance); !validation) {
            return fail(validation.error());
        }
        if (auto verified = media::verifyProvenanceArtifact(provenance, outcome.bytes); !verified) {
            return fail(verified.error());
        }
        if (auto published = publishAll(options, outcome, provenance); !published) {
            return fail(published.error());
        }
    }

    if (cacheEnabled && !outcome.fromCache) {
        media::MediaCacheRecord record;
        record.key = media::mediaCacheKey(keyInput);
        record.kind = options.kind == "image" ? media::MediaArtifactKind::Texture
                                              : media::MediaArtifactKind::Audio;
        record.input = keyInput;
        record.decoder = outcome.decoder;
        record.artifactIdentity = outcome.identity;
        record.artifactBytes = static_cast<std::uint64_t>(outcome.bytes.size());
        auto stored = cache.store(record, outcome.bytes);
        if (!stored) {
            return fail(stored.error());
        }
    }

    if (options.printInfo) {
        std::cout << outcome.info << "\n";
    }
    return 0;
}
