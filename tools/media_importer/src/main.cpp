// cuexis_media_importer: the offline media import CLI.
//
// The importer is a separate default-OFF media-tools artifact. Playback and Player never link it
// and never launch it for implicit runtime decoding. It converts one bounded encoded source into
// one content-identity addressed, immutable canonical artifact.

#include "publish.hpp"
#include "worker_process.hpp"

#include <cuexis/core/error.hpp>
#include <cuexis/media_import/media_import.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

namespace media = cuexis::media_import;
using cuexis::media_importer::publishImmutable;
using cuexis::media_importer::readFileBounded;
using cuexis::media_importer::runBoundedWorker;
using cuexis::media_importer::WorkerRequest;

constexpr std::uint64_t workerProcessOverheadBytes = 64ULL * 1024ULL * 1024ULL;

struct Options final {
    std::string kind;
    std::filesystem::path input;
    std::filesystem::path outputDirectory;
    std::filesystem::path workerOutput;
    bool worker{false};
    bool inProcess{false};
    bool printInfo{false};
    bool help{false};
    std::uint64_t memoryLimit{0};
};

void printUsage(std::ostream& stream) {
    stream << "usage: cuexis_media_importer --kind <image|audio> --input <file> "
              "--output-dir <dir> [--print-info] [--in-process] [--memory-limit <bytes>]\n"
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
    return !options.outputDirectory.empty();
}

struct ImportOutcome final {
    std::vector<std::byte> bytes;
    std::string identity;
    std::string info;
};

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

[[nodiscard]] auto importSource(const Options& options, std::span<const std::byte> source)
    -> cuexis::core::Result<ImportOutcome> {
    ImportOutcome outcome;
    if (options.kind == "image") {
        auto result = media::importImage(source);
        if (!result) {
            return cuexis::core::unexpected(result.error());
        }
        outcome.bytes = std::move(result->portableTexture);
        outcome.info = describeImage(*result);
    } else {
        auto result = media::importAudio(source);
        if (!result) {
            return cuexis::core::unexpected(result.error());
        }
        outcome.bytes = std::move(result->canonicalWav);
        outcome.info = describeAudio(*result);
    }
    outcome.identity = media::contentIdentity(outcome.bytes);
    return outcome;
}

[[nodiscard]] auto artifactFileName(const Options& options, const std::string& identity)
    -> std::string {
    return options.kind == "image" ? identity + ".texture.bin" : identity + ".wav";
}

[[nodiscard]] auto defaultMemoryLimit(std::uint64_t sourceBytes) -> std::uint64_t {
    // Measured conversion budget plus the source, the largest possible artifact and process
    // overhead. The limit is a backstop for decoder-internal allocations.
    return media::MediaBudget{}.maxWorkingBytes + sourceBytes + media::MediaBudget{}.maxWavBytes +
           workerProcessOverheadBytes;
}

int fail(const std::string& code, const std::string& message) {
    std::cerr << "error: " << code << ": " << message << "\n";
    return 2;
}

int fail(const cuexis::core::Error& error) {
    return fail(std::string{error.code()}, std::string{error.message()});
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

    if (options.inProcess) {
        auto outcome = importSource(options, *source);
        if (!outcome) {
            return fail(outcome.error());
        }
        const auto target = options.outputDirectory / artifactFileName(options, outcome->identity);
        if (auto published = publishImmutable(target, outcome->bytes); !published) {
            return fail(published.error());
        }
        if (options.printInfo) {
            std::cout << outcome->info << "\n";
        }
        return 0;
    }

    const auto limit =
        options.memoryLimit != 0 ? options.memoryLimit : defaultMemoryLimit(source->size());
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
        return fail(outcome.error());
    }
    if (outcome->exitCode != 0) {
        std::cerr << outcome->output;
        return fail(cuexis::core::Error{"media.worker.failed", "The bounded worker did not finish"}
                        .withContext("exitCode", std::to_string(outcome->exitCode))
                        .withContext("memoryLimitBytes", std::to_string(limit)));
    }

    auto identity = cuexis::media_importer::hashFile(temporary);
    if (!identity) {
        std::filesystem::remove(temporary);
        return fail(identity.error());
    }
    auto published = cuexis::media_importer::promoteTemporary(
        temporary, options.outputDirectory / artifactFileName(options, *identity), *identity);
    if (!published) {
        return fail(published.error());
    }
    if (options.printInfo) {
        std::cout << outcome->output;
    }
    return 0;
}
