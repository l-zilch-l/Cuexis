#include <cuexis/presentation_renderer/presentation_renderer.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/core/thread_checker.hpp>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cuexis::presentation_renderer {
namespace {

[[nodiscard]] auto nextInstanceId() noexcept -> std::uint64_t {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] auto wrongThread(const char* operation) -> core::Error {
    return core::Error{"presentation.renderer.wrong_thread",
                       "The presentation renderer belongs to another thread"}
        .withContext("operation", operation);
}

[[nodiscard]] auto validationError(const playback::PresentationValidationResult& validation)
    -> core::Error {
    auto error = core::Error{"presentation.renderer.capability_failed",
                             "Presentation capability validation failed"};
    if (!validation.diagnostics.items().empty()) {
        const auto& diagnostic = validation.diagnostics.items().front();
        error.withContext("diagnostic_code", std::string{diagnostic.code()})
            .withContext("diagnostic_message", std::string{diagnostic.message()});
    }
    return error;
}

[[nodiscard]] auto resourceMatchesDeclaredType(const playback::PortableResource& resource) noexcept
    -> bool {
    switch (resource.reference.type) {
    case playback::PresentationResourceType::Mesh:
        return std::holds_alternative<playback::PortableMesh>(resource.value);
    case playback::PresentationResourceType::Texture2D:
        return std::holds_alternative<playback::PortableTexture2D>(resource.value);
    case playback::PresentationResourceType::UnlitMaterial:
        return std::holds_alternative<playback::PortableUnlitMaterial>(resource.value);
    case playback::PresentationResourceType::Shader:
        return std::holds_alternative<playback::PortableShader>(resource.value);
    case playback::PresentationResourceType::ParameterizedMaterial:
        return std::holds_alternative<playback::PortableParameterizedMaterial>(resource.value);
    }
    return false;
}

[[nodiscard]] auto finiteBounds(const playback::PortableMesh& mesh) noexcept -> bool {
    for (std::size_t component = 0; component < 3; ++component) {
        if (!std::isfinite(mesh.boundsMin[component]) ||
            !std::isfinite(mesh.boundsMax[component])) {
            return false;
        }
    }
    return true;
}

} // namespace

struct PreparedPresentation::State {
    IPresentationRenderer* renderer{};
    bool attached{};
    std::uint64_t instance{};
    std::uint64_t rendererGeneration{};
    std::uint64_t candidateGeneration{};
    playback::PresentationCandidateToken token{};
    playback::EffectivePresentationSettings settings{};
};

struct TestPresentationRenderer::Cache {
    playback::PresentationCandidateToken token{};
    playback::EffectivePresentationSettings settings{};
    std::vector<playback::PortableResourcePtr> resources;
    bool manifestEmpty{};
    std::uint64_t candidateGeneration{};
};

struct TestPresentationRenderer::State {
    core::ThreadChecker owner{};
    std::uint64_t instance{};
    std::uint64_t generation{1};
    std::uint64_t nextCandidate{1};
    bool closed{};
    bool deviceLost{};
    bool outstanding{};
    bool frameSubmitted{};
    std::uint32_t surfaceWidth{16};
    std::uint32_t surfaceHeight{16};
    std::optional<PresentationFailureClass> presentFailure;
    playback::PresentationCapabilities capabilities{};
    std::optional<Cache> pending;
    std::optional<Cache> active;
};

PreparedPresentation::PreparedPresentation() noexcept = default;

PreparedPresentation::PreparedPresentation(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

PreparedPresentation::~PreparedPresentation() {
    if (state_ == nullptr || !state_->attached || state_->renderer == nullptr) {
        return;
    }
    auto* renderer = state_->renderer;
    const auto rendererGeneration = state_->rendererGeneration;
    const auto candidateGeneration = state_->candidateGeneration;
    auto token = state_->token;
    state_->attached = false;
    state_->renderer = nullptr;
    renderer->detachCandidate(rendererGeneration, candidateGeneration, token);
}

PreparedPresentation::PreparedPresentation(PreparedPresentation&& other) noexcept
    : state_(std::move(other.state_)) {}

auto PreparedPresentation::operator=(PreparedPresentation&& other) noexcept
    -> PreparedPresentation& {
    if (this != &other) {
        if (state_ != nullptr && state_->attached && state_->renderer != nullptr) {
            auto* renderer = state_->renderer;
            const auto rendererGeneration = state_->rendererGeneration;
            const auto candidateGeneration = state_->candidateGeneration;
            auto token = state_->token;
            state_->attached = false;
            state_->renderer = nullptr;
            renderer->detachCandidate(rendererGeneration, candidateGeneration, token);
        }
        state_ = std::move(other.state_);
    }
    return *this;
}

bool PreparedPresentation::valid() const noexcept {
    return state_ != nullptr && state_->attached && state_->renderer != nullptr;
}

auto PreparedPresentation::playbackToken() const noexcept
    -> const playback::PresentationCandidateToken* {
    return state_ == nullptr ? nullptr : &state_->token;
}

auto PreparedPresentation::rendererIdentity() const noexcept -> RendererIdentity {
    if (state_ == nullptr) {
        return {};
    }
    return RendererIdentity{state_->instance, state_->rendererGeneration};
}

auto PreparedPresentation::settings() const noexcept
    -> const playback::EffectivePresentationSettings* {
    return state_ == nullptr ? nullptr : &state_->settings;
}

auto portableRendererCapabilities() noexcept -> playback::PresentationCapabilities {
    playback::PresentationCapabilities capabilities;
    capabilities.opaquePass = true;
    capabilities.transparentPass = true;
    capabilities.linearTexture = true;
    capabilities.srgbTexture = true;
    capabilities.straightAlphaBlend = true;
    capabilities.backFaceCulling = true;
    capabilities.doubleSided = true;
    capabilities.debugPass = true;
    capabilities.maxResourceBytes = 64ULL * 1024ULL * 1024ULL;
    capabilities.maxTotalDecodedBytes = 512ULL * 1024ULL * 1024ULL;
    capabilities.maxTextureDimension = 8192;
    capabilities.maxMeshVertices = 1'048'576;
    capabilities.maxMeshIndices = 3'145'728;
    capabilities.parameterizedMaterial = true;
    return capabilities;
}

TestPresentationRenderer::TestPresentationRenderer() : state_(std::make_unique<State>()) {
    state_->instance = nextInstanceId();
    state_->capabilities = portableRendererCapabilities();
}

TestPresentationRenderer::~TestPresentationRenderer() {
    if (state_ != nullptr && state_->outstanding) {
        std::terminate();
    }
}

auto TestPresentationRenderer::capabilities() const noexcept
    -> const playback::PresentationCapabilities& {
    return state_->capabilities;
}

auto TestPresentationRenderer::identity() const noexcept -> RendererIdentity {
    return RendererIdentity{state_->instance, state_->generation};
}

auto TestPresentationRenderer::requireOwner(const char* operation) const -> core::Result<void> {
    if (state_->owner.isCurrent()) {
        return {};
    }
    return core::unexpected(wrongThread(operation));
}

auto TestPresentationRenderer::prepare(playback::PreparedPlayback& prepared,
                                       const playback::PresentationRequest& request)
    -> core::Result<PreparedPresentation> {
    if (auto owner = requireOwner("prepare"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->closed) {
        return core::unexpected(
            core::Error{"presentation.renderer.closed", "The presentation renderer is closed"});
    }
    if (state_->deviceLost) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (state_->outstanding) {
        return core::unexpected(core::Error{"presentation.renderer.candidate.outstanding",
                                            "A presentation candidate is already outstanding"});
    }
    if (!prepared.valid()) {
        return core::unexpected(core::Error{"presentation.renderer.prepare.invalid",
                                            "Prepared playback is not a valid candidate"});
    }
    auto validation = prepared.validatePresentation(state_->capabilities, request);
    if (!validation.hasValue()) {
        return core::unexpected(validationError(validation));
    }
    const auto* manifest = prepared.presentationManifest();
    if (manifest == nullptr) {
        return core::unexpected(core::Error{"presentation.renderer.prepare.manifest_missing",
                                            "Prepared playback has no presentation manifest"});
    }
    if (state_->nextCandidate == 0) {
        return core::unexpected(core::Error{"presentation.renderer.generation_exhausted",
                                            "Presentation candidate generation is exhausted"});
    }
    auto token = prepared.presentationCandidateToken();
    if (!token) {
        return core::unexpected(std::move(token.error()));
    }

    Cache cache;
    cache.token = *token;
    cache.settings = *validation.settings;
    cache.manifestEmpty = manifest->entries.empty();
    cache.candidateGeneration = state_->nextCandidate;
    cache.resources.reserve(manifest->entries.size());
    for (const auto& entry : manifest->entries) {
        auto resource = prepared.acquirePresentationResource(entry.reference);
        if (!resource) {
            return core::unexpected(std::move(resource.error()));
        }
        if ((*resource)->reference != entry.reference || !resourceMatchesDeclaredType(**resource)) {
            return core::unexpected(
                core::Error{"presentation.renderer.resource_mismatch",
                            "Acquired portable resource does not match its manifest entry"}
                    .withContext("asset_id", entry.reference.assetId));
        }
        if (const auto* mesh = std::get_if<playback::PortableMesh>(&(*resource)->value);
            mesh != nullptr && !finiteBounds(*mesh)) {
            return core::unexpected(
                core::Error{"presentation.renderer.frame.non_finite", "Mesh bounds are not finite"}
                    .withContext("asset_id", entry.reference.assetId));
        }
        cache.resources.push_back(std::move(*resource));
    }

    auto candidateState = std::make_unique<PreparedPresentation::State>();
    candidateState->renderer = this;
    candidateState->attached = true;
    candidateState->instance = state_->instance;
    candidateState->rendererGeneration = state_->generation;
    candidateState->candidateGeneration = cache.candidateGeneration;
    candidateState->token = cache.token;
    candidateState->settings = cache.settings;
    state_->pending = std::move(cache);
    ++state_->nextCandidate;
    state_->outstanding = true;
    return PreparedPresentation{std::move(candidateState)};
}

auto TestPresentationRenderer::accepts(const PreparedPresentation& candidate) const
    -> core::Result<void> {
    if (auto owner = requireOwner("accepts"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (candidate.state_ == nullptr || !candidate.state_->attached ||
        candidate.state_->renderer != this) {
        return core::unexpected(core::Error{"presentation.renderer.token.invalid",
                                            "The presentation candidate is not outstanding"});
    }
    if (candidate.state_->instance != state_->instance ||
        candidate.state_->rendererGeneration != state_->generation ||
        !state_->pending.has_value() ||
        state_->pending->candidateGeneration != candidate.state_->candidateGeneration ||
        !(state_->pending->token == candidate.state_->token)) {
        return core::unexpected(core::Error{"presentation.renderer.token.stale",
                                            "The presentation candidate token is stale"});
    }
    return {};
}

void TestPresentationRenderer::activate(PreparedPresentation&& candidate) noexcept {
    if (!state_->owner.isCurrent() || !accepts(candidate).has_value()) {
        std::terminate();
    }
    state_->active = std::move(state_->pending);
    state_->pending.reset();
    state_->outstanding = false;
    state_->frameSubmitted = false;
    candidate.state_->attached = false;
    candidate.state_->renderer = nullptr;
}

void TestPresentationRenderer::discard(PreparedPresentation&& candidate) noexcept {
    if (!state_->owner.isCurrent() || candidate.state_ == nullptr || !candidate.state_->attached ||
        candidate.state_->renderer != this) {
        std::terminate();
    }
    if (candidate.state_->rendererGeneration != state_->generation ||
        !state_->pending.has_value() ||
        state_->pending->candidateGeneration != candidate.state_->candidateGeneration ||
        !(state_->pending->token == candidate.state_->token)) {
        state_->outstanding = false;
        state_->pending.reset();
        candidate.state_->attached = false;
        candidate.state_->renderer = nullptr;
        return;
    }
    state_->pending.reset();
    state_->outstanding = false;
    candidate.state_->attached = false;
    candidate.state_->renderer = nullptr;
}

bool TestPresentationRenderer::hasActivePresentation() const noexcept {
    state_->owner.assertCurrent();
    return state_->active.has_value();
}

auto TestPresentationRenderer::submit(const playback::FrameSnapshot& snapshot,
                                      const render::RenderScene* debugScene)
    -> core::Result<DrawSummary> {
    if (auto owner = requireOwner("submit"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->closed) {
        return core::unexpected(
            core::Error{"presentation.renderer.closed", "The presentation renderer is closed"});
    }
    if (state_->deviceLost) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (state_->surfaceWidth == 0 || state_->surfaceHeight == 0) {
        return core::unexpected(
            core::Error{"presentation.renderer.surface.zero_size",
                        "A zero-size surface suspends presentation submission"});
    }
    if (!state_->active) {
        return core::unexpected(core::Error{"presentation.renderer.inactive",
                                            "No active presentation cache is available"});
    }
    if (state_->frameSubmitted) {
        return core::unexpected(core::Error{"presentation.renderer.frame.already_submitted",
                                            "The current frame was already submitted"});
    }
    auto summary = buildPresentationCommands(snapshot, state_->active->resources,
                                             state_->active->manifestEmpty,
                                             state_->active->settings.debugPassEnabled, debugScene);
    if (!summary) {
        return core::unexpected(std::move(summary.error()));
    }
    state_->frameSubmitted = true;
    return summary;
}

auto TestPresentationRenderer::present() -> core::Result<void> {
    if (auto owner = requireOwner("present"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->closed) {
        return core::unexpected(
            core::Error{"presentation.renderer.closed", "The presentation renderer is closed"});
    }
    if (state_->deviceLost) {
        return core::unexpected(core::Error{"presentation.renderer.surface.lost",
                                            "The presentation surface is lost until rebuild"});
    }
    if (!state_->frameSubmitted) {
        return core::unexpected(core::Error{"presentation.renderer.frame.not_submitted",
                                            "Present requires a submitted frame"});
    }
    state_->frameSubmitted = false;
    if (!state_->presentFailure.has_value()) {
        return {};
    }
    const auto failure = *state_->presentFailure;
    state_->presentFailure.reset();
    if (failure == PresentationFailureClass::Unrecoverable) {
        state_->deviceLost = true;
        return core::unexpected(
            core::Error{"presentation.renderer.present.device_lost",
                        "Present failed because the presentation device was lost"});
    }
    return core::unexpected(
        core::Error{"presentation.renderer.present.failed",
                    "Present failed and the active presentation was left unchanged"});
}

auto TestPresentationRenderer::resize(std::uint32_t width, std::uint32_t height)
    -> core::Result<void> {
    if (auto owner = requireOwner("resize"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->closed) {
        return core::unexpected(
            core::Error{"presentation.renderer.closed", "The presentation renderer is closed"});
    }
    state_->surfaceWidth = width;
    state_->surfaceHeight = height;
    return {};
}

auto TestPresentationRenderer::rebuild() -> core::Result<void> {
    if (auto owner = requireOwner("rebuild"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->generation == std::numeric_limits<std::uint64_t>::max()) {
        return core::unexpected(core::Error{"presentation.renderer.generation_exhausted",
                                            "Presentation renderer generation is exhausted"});
    }
    state_->pending.reset();
    state_->active.reset();
    state_->frameSubmitted = false;
    state_->deviceLost = false;
    state_->closed = false;
    state_->presentFailure.reset();
    ++state_->generation;
    return {};
}

auto TestPresentationRenderer::close() -> core::Result<void> {
    if (auto owner = requireOwner("close"); !owner) {
        return core::unexpected(std::move(owner.error()));
    }
    if (state_->outstanding) {
        return core::unexpected(core::Error{"presentation.renderer.close.candidate_outstanding",
                                            "Close cannot run while a candidate is outstanding"});
    }
    state_->active.reset();
    state_->pending.reset();
    state_->frameSubmitted = false;
    state_->closed = true;
    return {};
}

auto TestPresentationRenderer::status() const noexcept -> PresentationRendererStatus {
    PresentationRendererStatus status;
    status.closed = state_->closed;
    status.deviceLost = state_->deviceLost;
    status.active = state_->active.has_value();
    status.candidateOutstanding = state_->outstanding;
    status.frameSubmitted = state_->frameSubmitted;
    status.surfaceWidth = state_->surfaceWidth;
    status.surfaceHeight = state_->surfaceHeight;
    status.generation = state_->generation;
    status.activeResourceCount = state_->active ? state_->active->resources.size() : 0;
    return status;
}

void TestPresentationRenderer::markDeviceLost() noexcept {
    state_->owner.assertCurrent();
    state_->deviceLost = true;
}

void TestPresentationRenderer::failNextPresent(PresentationFailureClass failureClass) noexcept {
    state_->owner.assertCurrent();
    state_->presentFailure = failureClass;
}

void TestPresentationRenderer::detachCandidate(
    std::uint64_t rendererGeneration, std::uint64_t candidateGeneration,
    const playback::PresentationCandidateToken& token) noexcept {
    if (!state_->outstanding) {
        return;
    }
    if (rendererGeneration != state_->generation || !state_->pending.has_value() ||
        state_->pending->candidateGeneration != candidateGeneration ||
        !(state_->pending->token == token)) {
        state_->pending.reset();
        state_->outstanding = false;
        return;
    }
    state_->pending.reset();
    state_->outstanding = false;
}

} // namespace cuexis::presentation_renderer
