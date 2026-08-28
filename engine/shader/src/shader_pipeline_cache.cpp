#include <cuexis/shader/shader_cache.hpp>
#include <cuexis/shader/shader_compiler.hpp>

#include <cuexis/core/error.hpp>

#include <exception>
#include <future>
#include <string>
#include <thread>
#include <utility>

namespace cuexis::shader {
namespace {

[[nodiscard]] auto cacheError(std::string_view code, std::string message) -> core::Error {
    return core::Error{std::string{code}, std::move(message)};
}

void addErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error) {
    auto diagnostic = core::Diagnostic{core::DiagnosticSeverity::Error, std::string{error.code()},
                                       std::string{error.message()}, "$"};
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

[[nodiscard]] auto compileOnWorker(const ShaderCompileRequest& request)
    -> core::Result<ShaderCompileArtifact> {
    std::promise<core::Result<ShaderCompileArtifact>> promise;
    std::thread worker{[&]() {
        try {
            promise.set_value(ShaderCompiler::compile(request));
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }};
    worker.join();
    return promise.get_future().get();
}

} // namespace

ShaderPipelineCache::ShaderPipelineCache(std::filesystem::path directory)
    : store_(std::move(directory)) {}

auto ShaderPipelineCache::prepareCandidate(const ShaderCompileRequest& request,
                                           const ShaderCacheKeyInput& key, bool compileEnabled)
    -> core::Result<void> {
    diagnostics_ = makeShaderDiagnostics();
    auto loaded = store_.load(key);
    if (loaded) {
        candidate_ = std::make_unique<ShaderCacheRecord>(std::move(*loaded));
        return {};
    }
    addErrorDiagnostic(diagnostics_, loaded.error());
    const bool cacheAbsent = loaded.error().code() == diagnosticCacheMissing;
    const bool notReusable = loaded.error().code() == diagnosticCacheToolMismatch ||
                             loaded.error().code() == diagnosticCacheKeyInvalid;
    if (!compileEnabled) {
        return core::unexpected(std::move(loaded.error()));
    }
    if (!cacheAbsent && !notReusable) {
        return core::unexpected(std::move(loaded.error()));
    }

    core::Result<ShaderCompileArtifact> compiled;
    try {
        compiled = compileOnWorker(request);
    } catch (const std::exception& exception) {
        compiled = core::unexpected(
            cacheError(diagnosticCompileFailed, exception.what()).withContext("tool", "worker"));
    }
    if (!compiled) {
        addErrorDiagnostic(diagnostics_, compiled.error());
        if (active_ != nullptr) {
            auto error = cacheError(diagnosticHotReloadFailed,
                                    "Candidate shader compile failed; the active pipeline was kept")
                             .withCause(std::move(compiled.error()));
            addErrorDiagnostic(diagnostics_, error);
            return core::unexpected(std::move(error));
        }
        return core::unexpected(std::move(compiled.error()));
    }

    ShaderCacheRecord record;
    record.sourceIdentity = key.sourceIdentity;
    record.importerProfile =
        std::string{key.importerProfile.empty() ? importerProfileShaderV1 : key.importerProfile};
    if (key.targetProfiles.empty()) {
        for (const auto profile : defaultTargetProfiles()) {
            record.targetProfiles.emplace_back(profile);
        }
    } else {
        for (const auto profile : key.targetProfiles) {
            record.targetProfiles.emplace_back(profile);
        }
    }
    if (key.selectedKeywords.empty()) {
        for (const auto keyword : request.selectedKeywords) {
            record.selectedKeywords.emplace_back(keyword);
        }
    } else {
        for (const auto keyword : key.selectedKeywords) {
            record.selectedKeywords.emplace_back(keyword);
        }
    }
    record.vertexEntry =
        std::string{key.vertexEntry.empty() ? request.vertexEntry : key.vertexEntry};
    record.fragmentEntry =
        std::string{key.fragmentEntry.empty() ? request.fragmentEntry : key.fragmentEntry};
    record.tools = key.tools.empty()
                       ? currentToolVersions()
                       : std::vector<ShaderToolVersion>{key.tools.begin(), key.tools.end()};
    record.artifact = std::move(*compiled);
    auto stored = store_.store(record);
    if (!stored) {
        addErrorDiagnostic(diagnostics_, stored.error());
        if (active_ != nullptr) {
            auto error =
                cacheError(diagnosticHotReloadFailed,
                           "Candidate shader cache store failed; the active pipeline was kept")
                    .withCause(std::move(stored.error()));
            addErrorDiagnostic(diagnostics_, error);
            return core::unexpected(std::move(error));
        }
        return core::unexpected(std::move(stored.error()));
    }
    auto written = encodeCache(record);
    if (!written) {
        return core::unexpected(std::move(written.error()));
    }
    auto decoded = decodeCache(*written);
    if (!decoded) {
        return core::unexpected(std::move(decoded.error()));
    }
    candidate_ = std::make_unique<ShaderCacheRecord>(std::move(*decoded));
    diagnostics_.clear();
    return {};
}

void ShaderPipelineCache::activate() noexcept {
    if (candidate_ == nullptr) {
        return;
    }
    active_ = std::move(candidate_);
}

auto ShaderPipelineCache::active() const noexcept -> const ShaderCacheRecord* {
    return active_.get();
}

auto ShaderPipelineCache::candidate() const noexcept -> const ShaderCacheRecord* {
    return candidate_.get();
}

auto ShaderPipelineCache::diagnostics() const noexcept -> const core::Diagnostics& {
    return diagnostics_;
}

} // namespace cuexis::shader
