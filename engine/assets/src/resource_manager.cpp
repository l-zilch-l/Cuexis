//  ResourceManager 实现 — 同步 CPU blob 资源加载与管理
//  类型化槽位：index + generation + managerToken 防止 ABA 问题和跨 Manager 别名
//  ResourceLease: move-only 强引用；ResourceScope: 批量持有 + 依赖去重 + 事务回滚
//  contentRevision: 热重载扩展点；当前无文件监听或正式热重载 API

#include <cuexis/assets/resource_manager.hpp>

#include <cuexis/core/error.hpp>

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

namespace cuexis::assets {
namespace detail {
namespace {

std::atomic<std::uint64_t> nextManagerToken{1};

template <typename Tag> struct ResourceTraits;

template <> struct ResourceTraits<MeshTag> final {
    static constexpr AssetType type = AssetType::Mesh;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.mesh";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_MESH_V1";
};

template <> struct ResourceTraits<MaterialTag> final {
    static constexpr AssetType type = AssetType::Material;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.material";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_MATERIAL_V1";
};

template <> struct ResourceTraits<TextureTag> final {
    static constexpr AssetType type = AssetType::Texture;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.texture";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_TEXTURE_V1";
};

template <> struct ResourceTraits<AudioSourceTag> final {
    static constexpr AssetType type = AssetType::Audio;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.audio";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_AUDIO_SOURCE_V1";
};

template <> struct ResourceTraits<ShaderTag> final {
    static constexpr AssetType type = AssetType::Shader;
    static constexpr std::string_view fallbackId = "cuexis.builtin.fallback.shader";
    static constexpr std::string_view fallbackPayload = "CUEXIS_FALLBACK_SHADER_V1";
};

template <typename Tag> struct Slot final {
    AssetId id;
    ResourceState state{ResourceState::Unloaded};
    std::uint32_t generation{1};
    std::uint64_t contentRevision{};
    std::size_t strongReferences{};
    std::shared_ptr<const CpuResource<Tag>> resource;
    bool pinned{};
    bool retired{};
};

template <typename Tag> struct Pool final {
    std::vector<Slot<Tag>> slots;
    std::map<std::string, std::uint32_t, std::less<>> byId;
    std::uint32_t fallbackIndex{};
};

auto makeBlob(std::string_view payload, std::string source) -> std::shared_ptr<const AssetBlob> {
    auto blob = std::make_shared<AssetBlob>();
    blob->rootId = "builtin";
    blob->source = std::move(source);
    blob->bytes.reserve(payload.size());
    for (const char character : payload) {
        blob->bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return blob;
}

auto makeDiagnostics(std::size_t capacity) -> core::Diagnostics {
    return core::Diagnostics{capacity,
                             core::Diagnostic{core::DiagnosticSeverity::Error,
                                              "assets.resource.diagnostic_limit",
                                              "Resource diagnostic limit was reached", "$"}};
}

} // namespace

struct ResourceManagerState;

struct ResourceLeaseControl final {
    std::weak_ptr<ResourceManagerState> state;
    AssetType type{AssetType::Mesh};
    std::uint32_t index{};
    std::uint32_t generation{};

    ~ResourceLeaseControl();
};

struct ResourceManagerState final : std::enable_shared_from_this<ResourceManagerState> {
    explicit ResourceManagerState(AssetDatabase assetDatabase,
                                  std::shared_ptr<content::IContentProvider> contentProvider,
                                  ResourceManagerLimits resourceLimits)
        : database(std::move(assetDatabase)), provider(std::move(contentProvider)),
          limits(resourceLimits), token(allocateToken()), ownerThread(std::this_thread::get_id()) {
        initializeFallback<MeshTag>();
        initializeFallback<MaterialTag>();
        initializeFallback<TextureTag>();
        initializeFallback<AudioSourceTag>();
        initializeFallback<ShaderTag>();
    }

    static auto allocateToken() noexcept -> std::uint64_t {
        auto token = nextManagerToken.fetch_add(1, std::memory_order_relaxed);
        if (token == 0) {
            token = nextManagerToken.fetch_add(1, std::memory_order_relaxed);
        }
        return token;
    }

    [[nodiscard]] bool isOwnerThread() const noexcept {
        return ownerThread == std::this_thread::get_id();
    }

    template <typename Tag> auto pool() -> Pool<Tag>& {
        if constexpr (std::is_same_v<Tag, MeshTag>) {
            return meshes;
        } else if constexpr (std::is_same_v<Tag, MaterialTag>) {
            return materials;
        } else if constexpr (std::is_same_v<Tag, TextureTag>) {
            return textures;
        } else if constexpr (std::is_same_v<Tag, ShaderTag>) {
            return shaders;
        } else {
            static_assert(std::is_same_v<Tag, AudioSourceTag>);
            return audioSources;
        }
    }

    template <typename Tag> auto pool() const -> const Pool<Tag>& {
        if constexpr (std::is_same_v<Tag, MeshTag>) {
            return meshes;
        } else if constexpr (std::is_same_v<Tag, MaterialTag>) {
            return materials;
        } else if constexpr (std::is_same_v<Tag, TextureTag>) {
            return textures;
        } else if constexpr (std::is_same_v<Tag, ShaderTag>) {
            return shaders;
        } else {
            static_assert(std::is_same_v<Tag, AudioSourceTag>);
            return audioSources;
        }
    }

    template <typename Tag> void initializeFallback() {
        auto& typedPool = pool<Tag>();
        const auto id = AssetId{std::string{ResourceTraits<Tag>::fallbackId}};
        auto resource = std::make_shared<CpuResource<Tag>>(CpuResource<Tag>{
            id,
            makeBlob(ResourceTraits<Tag>::fallbackPayload,
                     "builtin/" + std::string{assetTypeName(ResourceTraits<Tag>::type)}),
        });
        typedPool.slots.push_back(Slot<Tag>{
            .id = id,
            .state = ResourceState::Ready,
            .generation = 1,
            .contentRevision = 1,
            .strongReferences = 0,
            .resource = std::move(resource),
            .pinned = true,
            .retired = false,
        });
        typedPool.byId.emplace(id.value, 0);
        typedPool.fallbackIndex = 0;
    }

    template <typename Tag> auto makeLease(std::uint32_t index) -> ResourceLease<Tag> {
        auto& slot = pool<Tag>().slots[index];
        ++slot.strongReferences;
        auto control = std::make_shared<ResourceLeaseControl>();
        control->state = weak_from_this();
        control->type = ResourceTraits<Tag>::type;
        control->index = index;
        control->generation = slot.generation;
        return ResourceLease<Tag>{ResourceHandle<Tag>{index, slot.generation, token}, slot.resource,
                                  std::move(control)};
    }

    template <typename Tag> auto load(const AssetId& id) -> core::Result<ResourceLease<Tag>> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        const auto* record = database.find(id);
        if (record == nullptr) {
            return core::unexpected(
                core::Error{"assets.asset.not_found", "AssetId does not exist in AssetDatabase"}
                    .withContext("assetId", id.value));
        }
        if (record->type != ResourceTraits<Tag>::type) {
            return core::unexpected(
                core::Error{"assets.resource.type_mismatch",
                            "Asset type does not match the requested resource type"}
                    .withContext("assetId", id.value)
                    .withContext("expected", std::string{assetTypeName(ResourceTraits<Tag>::type)})
                    .withContext("actual", std::string{assetTypeName(record->type)}));
        }

        std::uint32_t index{};
        {
            std::scoped_lock lock{mutex};
            auto& typedPool = pool<Tag>();
            const auto existing = typedPool.byId.find(id.value);
            if (existing != typedPool.byId.end()) {
                index = existing->second;
                auto& slot = typedPool.slots[index];
                if (slot.state == ResourceState::Ready && slot.resource != nullptr) {
                    return makeLease<Tag>(index);
                }
                slot.state = ResourceState::Loading;
            } else {
                if (typedPool.slots.size() >= ResourceHandle<Tag>::invalidIndex) {
                    return core::unexpected(core::Error{"assets.resource.slot_limit",
                                                        "Resource slot index space is exhausted"});
                }
                index = static_cast<std::uint32_t>(typedPool.slots.size());
                typedPool.slots.push_back(Slot<Tag>{
                    .id = id,
                    .state = ResourceState::Loading,
                    .generation = 1,
                    .contentRevision = 0,
                    .strongReferences = 0,
                    .resource = {},
                    .pinned = false,
                    .retired = false,
                });
                typedPool.byId.emplace(id.value, index);
            }
        }

        if (!provider) {
            std::scoped_lock lock{mutex};
            auto& slot = pool<Tag>().slots[index];
            slot.state = ResourceState::Failed;
            slot.resource.reset();
            return core::unexpected(core::Error{"assets.resource.provider_missing",
                                                "ResourceManager has no content provider"}
                                        .withContext("assetId", id.value));
        }

        const auto rootId = database.rootIdOf(id);
        auto providerBlob = [&]() -> core::Result<content::ContentBlob> {
            try {
                return provider->readBlob(
                    {.rootId = rootId, .source = record->source, .maxBytes = limits.maxBlobBytes});
            } catch (const std::exception& exception) {
                return core::unexpected(core::Error{"assets.resource.provider_exception",
                                                    "Content provider threw an exception"}
                                            .withContext("exception", exception.what()));
            } catch (...) {
                return core::unexpected(core::Error{"assets.resource.provider_exception",
                                                    "Content provider threw an exception"});
            }
        }();
        if (!providerBlob || providerBlob->bytes.size() > limits.maxBlobBytes) {
            std::scoped_lock lock{mutex};
            auto& slot = pool<Tag>().slots[index];
            slot.state = ResourceState::Failed;
            slot.resource.reset();
            auto cause = providerBlob
                             ? core::Error{"assets.resource.provider_too_large",
                                           "Content provider exceeded the resource byte limit"}
                             : std::move(providerBlob.error());
            return core::unexpected(
                core::Error{"assets.resource.load_failed", "CPU resource blob could not be loaded"}
                    .withContext("assetId", id.value)
                    .withContext("rootId", std::string{rootId})
                    .withContext("source", record->source)
                    .withCause(std::move(cause)));
        }

        auto sharedBlob = std::make_shared<AssetBlob>();
        sharedBlob->rootId = std::string{rootId};
        sharedBlob->source = record->source;
        sharedBlob->providerRevision = providerBlob->revision;
        sharedBlob->bytes = std::move(providerBlob->bytes);
        auto resource = std::make_shared<CpuResource<Tag>>(CpuResource<Tag>{id, sharedBlob});
        std::scoped_lock lock{mutex};
        auto& slot = pool<Tag>().slots[index];
        slot.resource = std::move(resource);
        slot.state = ResourceState::Ready;
        ++slot.contentRevision;
        if (slot.contentRevision == 0) {
            slot.contentRevision = 1;
        }
        return makeLease<Tag>(index);
    }

    template <typename Tag> auto fallback() -> core::Result<ResourceLease<Tag>> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        return makeLease<Tag>(pool<Tag>().fallbackIndex);
    }

    template <typename Tag>
    auto get(ResourceHandle<Tag> handle) const -> core::Result<const CpuResource<Tag>*> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        if ((*slot)->state != ResourceState::Ready || (*slot)->resource == nullptr) {
            return core::unexpected(core::Error{
                "assets.resource.not_ready", "Resource handle does not refer to a ready resource"});
        }
        return (*slot)->resource.get();
    }

    template <typename Tag>
    auto stateOf(ResourceHandle<Tag> handle) const -> core::Result<ResourceState> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        return (*slot)->state;
    }

    template <typename Tag>
    auto revisionOf(ResourceHandle<Tag> handle) const -> core::Result<std::uint64_t> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slot = checkedSlot<Tag>(handle);
        if (!slot) {
            return core::unexpected(std::move(slot.error()));
        }
        return (*slot)->contentRevision;
    }

    template <typename Tag> auto unload(ResourceHandle<Tag> handle) -> core::Result<void> {
        if (!isOwnerThread()) {
            return core::unexpected(core::Error{"assets.resource.not_owner_thread",
                                                "ResourceManager belongs to another thread"});
        }
        std::scoped_lock lock{mutex};
        auto slotResult = checkedSlot<Tag>(handle);
        if (!slotResult) {
            return core::unexpected(std::move(slotResult.error()));
        }
        auto& slot = **slotResult;
        if (slot.pinned) {
            return core::unexpected(core::Error{"assets.resource.fallback_pinned",
                                                "Built-in fallback resources cannot be unloaded"});
        }
        if (slot.strongReferences != 0) {
            return core::unexpected(
                core::Error{"assets.resource.in_use", "Resource still has strong references"}
                    .withContext("assetId", slot.id.value));
        }
        releaseSlot<Tag>(handle.index, slot);
        return {};
    }

    template <typename Tag>
    auto checkedSlot(ResourceHandle<Tag> handle) const -> core::Result<const Slot<Tag>*> {
        if (!handle.valid()) {
            return core::unexpected(core::Error{"assets.resource.invalid_handle",
                                                "Resource handle is structurally invalid"});
        }
        if (handle.managerToken == 0 || handle.managerToken != token) {
            return core::unexpected(core::Error{"assets.resource.manager_mismatch",
                                                "Resource handle belongs to another manager"});
        }
        const auto& slots = pool<Tag>().slots;
        if (handle.index >= slots.size()) {
            return core::unexpected(core::Error{"assets.resource.invalid_index",
                                                "Resource handle index is out of range"});
        }
        const auto& slot = slots[handle.index];
        if (slot.retired || handle.generation != slot.generation) {
            return core::unexpected(
                core::Error{"assets.resource.stale_handle", "Resource handle generation is stale"});
        }
        return &slot;
    }

    template <typename Tag>
    auto checkedSlot(ResourceHandle<Tag> handle) -> core::Result<Slot<Tag>*> {
        auto result = std::as_const(*this).checkedSlot<Tag>(handle);
        if (!result) {
            return core::unexpected(std::move(result.error()));
        }
        return const_cast<Slot<Tag>*>(*result);
    }

    template <typename Tag> void releaseSlot(std::uint32_t index, Slot<Tag>& slot) noexcept {
        slot.resource.reset();
        slot.state = ResourceState::Unloaded;
        if (slot.generation == std::numeric_limits<std::uint32_t>::max()) {
            slot.retired = true;
            pool<Tag>().byId.erase(slot.id.value);
            return;
        }
        ++slot.generation;
        (void)index;
    }

    template <typename Tag>
    void releaseLease(std::uint32_t index, std::uint32_t generation) noexcept {
        std::scoped_lock lock{mutex};
        auto& slots = pool<Tag>().slots;
        if (index >= slots.size()) {
            return;
        }
        auto& slot = slots[index];
        if (slot.generation != generation || slot.strongReferences == 0) {
            return;
        }
        --slot.strongReferences;
        if (slot.strongReferences == 0 && !slot.pinned) {
            releaseSlot<Tag>(index, slot);
        }
    }

    void releaseLease(AssetType type, std::uint32_t index, std::uint32_t generation) noexcept {
        switch (type) {
        case AssetType::Mesh:
            releaseLease<MeshTag>(index, generation);
            break;
        case AssetType::Material:
            releaseLease<MaterialTag>(index, generation);
            break;
        case AssetType::Texture:
            releaseLease<TextureTag>(index, generation);
            break;
        case AssetType::Audio:
            releaseLease<AudioSourceTag>(index, generation);
            break;
        case AssetType::Shader:
            releaseLease<ShaderTag>(index, generation);
            break;
        }
    }

    template <typename Tag> void unloadUnused() noexcept {
        auto& typedPool = pool<Tag>();
        for (std::uint32_t index = 0; index < typedPool.slots.size(); ++index) {
            auto& slot = typedPool.slots[index];
            if (!slot.pinned && !slot.retired && slot.strongReferences == 0 &&
                slot.state != ResourceState::Unloaded) {
                releaseSlot<Tag>(index, slot);
            }
        }
    }

    AssetDatabase database;
    std::shared_ptr<content::IContentProvider> provider;
    ResourceManagerLimits limits;
    std::uint64_t token{};
    std::thread::id ownerThread;
    mutable std::mutex mutex;
    Pool<MeshTag> meshes;
    Pool<MaterialTag> materials;
    Pool<TextureTag> textures;
    Pool<AudioSourceTag> audioSources;
    Pool<ShaderTag> shaders;
};

ResourceLeaseControl::~ResourceLeaseControl() {
    if (const auto locked = state.lock()) {
        locked->releaseLease(type, index, generation);
    }
}

} // namespace detail
namespace {

void appendErrorDiagnostic(core::Diagnostics& diagnostics, const core::Error& error,
                           core::DiagnosticSeverity severity, std::string code, std::string message,
                           const AssetId& id) {
    auto diagnostic = core::Diagnostic{severity, std::move(code), std::move(message), "$"};
    diagnostic.withContext("assetId", id.value).withContext("cause", std::string{error.code()});
    for (const auto& context : error.context()) {
        diagnostic.withContext(context.key, context.value);
    }
    diagnostics.add(std::move(diagnostic));
}

template <typename Tag>
auto requestDirect(const std::shared_ptr<detail::ResourceManagerState>& state, const AssetId& id,
                   ResourcePolicy policy) -> ResourceLoadResult<ResourceLease<Tag>> {
    auto diagnostics = detail::makeDiagnostics(state->limits.maxDiagnostics);
    auto loaded = state->load<Tag>(id);
    if (loaded) {
        return {std::optional<ResourceLease<Tag>>{std::move(*loaded)}, std::move(diagnostics)};
    }

    switch (policy) {
    case ResourcePolicy::Required:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Error,
                              "assets.resource.required_failed",
                              "Required resource could not be loaded", id);
        break;
    case ResourcePolicy::Fallback: {
        auto fallback = state->fallback<Tag>();
        if (!fallback) {
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.fallback_used",
                              "Built-in fallback resource was used", id);
        diagnostics.sortDeterministically();
        return {std::optional<ResourceLease<Tag>>{std::move(*fallback)}, std::move(diagnostics)};
    }
    case ResourcePolicy::Optional:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.optional_skipped", "Optional resource was skipped",
                              id);
        break;
    }
    diagnostics.sortDeterministically();
    return {std::nullopt, std::move(diagnostics)};
}

template <typename Tag>
auto handleFromEntry(const std::variant<MeshLease, MaterialLease, TextureLease, AudioSourceLease,
                                        ShaderLease>& lease) -> ResourceHandle<Tag> {
    return std::get<ResourceLease<Tag>>(lease).handle();
}

auto handleFromEntry(AssetType type, const std::variant<MeshLease, MaterialLease, TextureLease,
                                                        AudioSourceLease, ShaderLease>& lease)
    -> std::variant<MeshHandle, MaterialHandle, TextureHandle, AudioSourceHandle, ShaderHandle> {
    switch (type) {
    case AssetType::Mesh:
        return handleFromEntry<MeshTag>(lease);
    case AssetType::Material:
        return handleFromEntry<MaterialTag>(lease);
    case AssetType::Texture:
        return handleFromEntry<TextureTag>(lease);
    case AssetType::Audio:
        return handleFromEntry<AudioSourceTag>(lease);
    case AssetType::Shader:
        return handleFromEntry<ShaderTag>(lease);
    }
    return MeshHandle{};
}

} // namespace

ResourceManager::ResourceManager(AssetDatabase database, ResourceManagerLimits limits)
    : ResourceManager(database, database.defaultContentProvider(), limits) {}

ResourceManager::ResourceManager(AssetDatabase database,
                                 std::shared_ptr<content::IContentProvider> provider,
                                 ResourceManagerLimits limits)
    : state_(std::make_shared<detail::ResourceManagerState>(std::move(database),
                                                            std::move(provider), limits)) {}

ResourceManager::~ResourceManager() = default;

auto ResourceManager::loadMesh(const AssetId& id) -> core::Result<MeshLease> {
    return state_->load<MeshTag>(id);
}

auto ResourceManager::loadMaterial(const AssetId& id) -> core::Result<MaterialLease> {
    return state_->load<MaterialTag>(id);
}

auto ResourceManager::loadTexture(const AssetId& id) -> core::Result<TextureLease> {
    return state_->load<TextureTag>(id);
}

auto ResourceManager::loadAudioSource(const AssetId& id) -> core::Result<AudioSourceLease> {
    return state_->load<AudioSourceTag>(id);
}

auto ResourceManager::loadShader(const AssetId& id) -> core::Result<ShaderLease> {
    return state_->load<ShaderTag>(id);
}

auto ResourceManager::requestMesh(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<MeshLease> {
    return requestDirect<MeshTag>(state_, id, policy);
}

auto ResourceManager::requestMaterial(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<MaterialLease> {
    return requestDirect<MaterialTag>(state_, id, policy);
}

auto ResourceManager::requestTexture(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<TextureLease> {
    return requestDirect<TextureTag>(state_, id, policy);
}

auto ResourceManager::requestAudioSource(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<AudioSourceLease> {
    return requestDirect<AudioSourceTag>(state_, id, policy);
}

auto ResourceManager::requestShader(const AssetId& id, ResourcePolicy policy)
    -> ResourceLoadResult<ShaderLease> {
    return requestDirect<ShaderTag>(state_, id, policy);
}

auto ResourceManager::get(MeshHandle handle) const -> core::Result<const MeshResource*> {
    return state_->get<MeshTag>(handle);
}

auto ResourceManager::get(MaterialHandle handle) const -> core::Result<const MaterialResource*> {
    return state_->get<MaterialTag>(handle);
}

auto ResourceManager::get(TextureHandle handle) const -> core::Result<const TextureResource*> {
    return state_->get<TextureTag>(handle);
}

auto ResourceManager::get(AudioSourceHandle handle) const
    -> core::Result<const AudioSourceResource*> {
    return state_->get<AudioSourceTag>(handle);
}

auto ResourceManager::get(ShaderHandle handle) const -> core::Result<const ShaderResource*> {
    return state_->get<ShaderTag>(handle);
}

auto ResourceManager::state(MeshHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<MeshTag>(handle);
}

auto ResourceManager::state(MaterialHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<MaterialTag>(handle);
}

auto ResourceManager::state(TextureHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<TextureTag>(handle);
}

auto ResourceManager::state(AudioSourceHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<AudioSourceTag>(handle);
}

auto ResourceManager::state(ShaderHandle handle) const -> core::Result<ResourceState> {
    return state_->stateOf<ShaderTag>(handle);
}

auto ResourceManager::contentRevision(MeshHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<MeshTag>(handle);
}

auto ResourceManager::contentRevision(MaterialHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<MaterialTag>(handle);
}

auto ResourceManager::contentRevision(TextureHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<TextureTag>(handle);
}

auto ResourceManager::contentRevision(AudioSourceHandle handle) const
    -> core::Result<std::uint64_t> {
    return state_->revisionOf<AudioSourceTag>(handle);
}

auto ResourceManager::contentRevision(ShaderHandle handle) const -> core::Result<std::uint64_t> {
    return state_->revisionOf<ShaderTag>(handle);
}

std::uint64_t ResourceManager::managerToken() const noexcept {
    return state_ ? state_->token : 0;
}

ResourceManagerMetrics ResourceManager::metrics() const noexcept {
    ResourceManagerMetrics result;
    if (!state_) {
        return result;
    }
    std::scoped_lock lock{state_->mutex};
    const auto accumulate = [&result](const auto& pool) {
        for (const auto& slot : pool.slots) {
            if (slot.pinned || slot.retired) {
                continue;
            }
            ++result.slots;
            result.strongReferences += slot.strongReferences;
            if (slot.state == ResourceState::Ready && slot.resource != nullptr) {
                ++result.ready;
                result.loadedBytes += slot.resource->bytes().size();
            } else if (slot.state == ResourceState::Failed) {
                ++result.failed;
            }
        }
    };
    accumulate(state_->meshes);
    accumulate(state_->materials);
    accumulate(state_->textures);
    accumulate(state_->audioSources);
    accumulate(state_->shaders);
    return result;
}

const AssetDatabase& ResourceManager::database() const noexcept {
    static const AssetDatabase emptyDatabase;
    return state_ ? state_->database : emptyDatabase;
}

auto ResourceManager::unload(MeshHandle handle) -> core::Result<void> {
    return state_->unload<MeshTag>(handle);
}

auto ResourceManager::unload(MaterialHandle handle) -> core::Result<void> {
    return state_->unload<MaterialTag>(handle);
}

auto ResourceManager::unload(TextureHandle handle) -> core::Result<void> {
    return state_->unload<TextureTag>(handle);
}

auto ResourceManager::unload(AudioSourceHandle handle) -> core::Result<void> {
    return state_->unload<AudioSourceTag>(handle);
}

auto ResourceManager::unload(ShaderHandle handle) -> core::Result<void> {
    return state_->unload<ShaderTag>(handle);
}

void ResourceManager::unloadUnused() noexcept {
    if (!state_) {
        return;
    }
    std::scoped_lock lock{state_->mutex};
    state_->unloadUnused<MeshTag>();
    state_->unloadUnused<MaterialTag>();
    state_->unloadUnused<TextureTag>();
    state_->unloadUnused<AudioSourceTag>();
    state_->unloadUnused<ShaderTag>();
}

ResourceScope ResourceManager::createScope() {
    return ResourceScope{*this};
}

bool ResourceScope::EntryKeyLess::operator()(const EntryKey& left,
                                             const EntryKey& right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id.value < right.id.value;
}

bool ResourceScope::EntryKeyLess::operator()(const EntryKey& left,
                                             EntryKeyView right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id.value < right.id;
}

bool ResourceScope::EntryKeyLess::operator()(EntryKeyView left,
                                             const EntryKey& right) const noexcept {
    if (left.type != right.type) {
        return left.type < right.type;
    }
    return left.id < right.id.value;
}

ResourceScope::ResourceScope(ResourceManager& manager) noexcept : state_(manager.state_) {}

ResourceScope::ResourceScope(ResourceScope&& other) noexcept
    : state_(std::move(other.state_)), entries_(std::move(other.entries_)),
      entryIndex_(std::move(other.entryIndex_)) {
    other.entryIndex_.clear();
    other.entries_.clear();
    other.state_.reset();
}

auto ResourceScope::operator=(ResourceScope&& other) noexcept -> ResourceScope& {
    if (this == &other) {
        return *this;
    }
    clear();
    state_ = std::move(other.state_);
    entries_ = std::move(other.entries_);
    entryIndex_ = std::move(other.entryIndex_);
    other.entryIndex_.clear();
    other.entries_.clear();
    other.state_.reset();
    return *this;
}

auto ResourceScope::request(AssetType type, const AssetId& id, ResourcePolicy policy)
    -> UntypedRequestResult {
    const auto state = state_.lock();
    auto diagnostics = detail::makeDiagnostics(state ? state->limits.maxDiagnostics : 1);
    if (!state) {
        diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                         "assets.resource.manager_expired",
                                         "ResourceScope manager no longer exists", "$"});
        return {std::nullopt, std::move(diagnostics)};
    }
    if (!state->isOwnerThread()) {
        diagnostics.add(core::Diagnostic{core::DiagnosticSeverity::Error,
                                         "assets.resource.not_owner_thread",
                                         "ResourceScope belongs to another thread", "$"});
        return {std::nullopt, std::move(diagnostics)};
    }

    const auto existing = entryIndex_.find(EntryKeyView{type, id.value});
    if (existing != entryIndex_.end()) {
        const auto& entry = entries_[existing->second];
        if (entry.resolution == EntryResolution::Loaded) {
            return {handleFromEntry(entry.type, entry.lease), std::move(diagnostics)};
        }

        const auto cause = entry.failureCause.value_or(core::Error{
            "assets.resource.cached_fallback", "A previous request resolved to a fallback"});
        switch (policy) {
        case ResourcePolicy::Required:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Error,
                                  "assets.resource.required_failed",
                                  "Required resource cannot reuse a cached fallback", id);
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        case ResourcePolicy::Fallback:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Warning,
                                  "assets.resource.fallback_used",
                                  "Built-in fallback resource was used", id);
            diagnostics.sortDeterministically();
            return {handleFromEntry(entry.type, entry.lease), std::move(diagnostics)};
        case ResourcePolicy::Optional:
            appendErrorDiagnostic(diagnostics, cause, core::DiagnosticSeverity::Warning,
                                  "assets.resource.optional_skipped",
                                  "Optional resource was skipped instead of reusing a fallback",
                                  id);
            diagnostics.sortDeterministically();
            return {std::nullopt, std::move(diagnostics)};
        }
    }

    const auto originalSize = entries_.size();
    const auto addEntry = [&](Entry entry) {
        const auto keyView = EntryKeyView{entry.type, entry.requestedId.value};
        if (entryIndex_.contains(keyView)) {
            return false;
        }
        auto key = EntryKey{entry.type, entry.requestedId};
        const auto index = entries_.size();
        entries_.push_back(std::move(entry));
        entryIndex_.emplace(std::move(key), index);
        return true;
    };
    std::vector<std::pair<AssetType, AssetId>> visiting;
    const auto acquire = [&](auto&& self, AssetType requestedType, const AssetId& requestedId,
                             std::size_t depth)
        -> core::Result<std::variant<MeshHandle, MaterialHandle, TextureHandle, AudioSourceHandle,
                                     ShaderHandle>> {
        const auto held = entryIndex_.find(EntryKeyView{requestedType, requestedId.value});
        if (held != entryIndex_.end()) {
            const auto& entry = entries_[held->second];
            if (entry.resolution == EntryResolution::Loaded) {
                return handleFromEntry(entry.type, entry.lease);
            }
            auto error = core::Error{
                "assets.resource.cached_fallback_not_required",
                "A required dependency cannot reuse a fallback cached by an earlier request"};
            error.withContext("assetId", requestedId.value);
            if (entry.failureCause) {
                error.withCause(*entry.failureCause);
            }
            return core::unexpected(std::move(error));
        }
        if (depth > state->limits.maxDependencyDepth) {
            return core::unexpected(core::Error{"assets.resource.dependency_depth",
                                                "Resource dependency depth limit was exceeded"}
                                        .withContext("assetId", requestedId.value));
        }
        if (std::find(visiting.begin(), visiting.end(), std::pair{requestedType, requestedId}) !=
            visiting.end()) {
            std::string cycle;
            for (const auto& [cycleType, cycleId] : visiting) {
                if (!cycle.empty()) {
                    cycle += " -> ";
                }
                cycle += std::string{assetTypeName(cycleType)} + ":" + cycleId.value;
            }
            cycle += " -> " + std::string{assetTypeName(requestedType)} + ":" + requestedId.value;
            return core::unexpected(core::Error{"assets.resource.dependency_cycle",
                                                "Resource dependency graph contains a cycle"}
                                        .withContext("cycle", std::move(cycle)));
        }

        const auto* record = state->database.find(requestedId);
        if (record == nullptr) {
            return core::unexpected(
                core::Error{"assets.asset.not_found", "AssetId does not exist in AssetDatabase"}
                    .withContext("assetId", requestedId.value));
        }
        if (record->type != requestedType) {
            return core::unexpected(
                core::Error{"assets.resource.type_mismatch",
                            "Asset type does not match the requested resource type"}
                    .withContext("assetId", requestedId.value)
                    .withContext("expected", std::string{assetTypeName(requestedType)})
                    .withContext("actual", std::string{assetTypeName(record->type)}));
        }

        visiting.emplace_back(requestedType, requestedId);
        for (const auto& dependency : record->dependencies) {
            const auto* dependencyRecord = state->database.find(dependency);
            if (dependencyRecord == nullptr) {
                visiting.pop_back();
                return core::unexpected(core::Error{"assets.resource.dependency_missing",
                                                    "Resource dependency does not exist"}
                                            .withContext("assetId", requestedId.value)
                                            .withContext("dependency", dependency.value));
            }
            auto dependencyHandle = self(self, dependencyRecord->type, dependency, depth + 1);
            if (!dependencyHandle) {
                visiting.pop_back();
                return core::unexpected(core::Error{"assets.resource.dependency_failed",
                                                    "Resource dependency could not be loaded"}
                                            .withContext("assetId", requestedId.value)
                                            .withContext("dependency", dependency.value)
                                            .withCause(std::move(dependencyHandle.error())));
            }
        }
        visiting.pop_back();

        switch (requestedType) {
        case AssetType::Mesh: {
            auto lease = state->load<MeshTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Material: {
            auto lease = state->load<MaterialTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Texture: {
            auto lease = state->load<TextureTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Audio: {
            auto lease = state->load<AudioSourceTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        case AssetType::Shader: {
            auto lease = state->load<ShaderTag>(requestedId);
            if (!lease) {
                return core::unexpected(std::move(lease.error()));
            }
            const auto handle = lease->handle();
            if (!addEntry(Entry{.type = requestedType,
                                .requestedId = requestedId,
                                .resolution = EntryResolution::Loaded,
                                .acquisitionPolicy = ResourcePolicy::Required,
                                .failureCause = std::nullopt,
                                .lease = std::move(*lease)})) {
                return core::unexpected(
                    core::Error{"assets.resource.scope_index_conflict",
                                "ResourceScope already contains the requested resource key"});
            }
            return handle;
        }
        }
        return core::unexpected(
            core::Error{"assets.resource.type_unknown", "Resource type is unknown"});
    };

    auto loaded = acquire(acquire, type, id, 1);
    if (loaded) {
        return {std::move(*loaded), std::move(diagnostics)};
    }

    rollbackTo(originalSize);
    switch (policy) {
    case ResourcePolicy::Required:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Error,
                              "assets.resource.required_failed",
                              "Required resource dependency closure could not be loaded", id);
        break;
    case ResourcePolicy::Optional:
        appendErrorDiagnostic(diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                              "assets.resource.optional_skipped",
                              "Optional resource dependency closure was skipped", id);
        break;
    case ResourcePolicy::Fallback:
        switch (type) {
        case AssetType::Mesh: {
            auto fallback = state->fallback<MeshTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Material: {
            auto fallback = state->fallback<MaterialTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Texture: {
            auto fallback = state->fallback<TextureTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Audio: {
            auto fallback = state->fallback<AudioSourceTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        case AssetType::Shader: {
            auto fallback = state->fallback<ShaderTag>();
            if (fallback) {
                const auto handle = fallback->handle();
                if (!addEntry(Entry{.type = type,
                                    .requestedId = id,
                                    .resolution = EntryResolution::Fallback,
                                    .acquisitionPolicy = ResourcePolicy::Fallback,
                                    .failureCause = loaded.error(),
                                    .lease = std::move(*fallback)})) {
                    appendErrorDiagnostic(diagnostics, loaded.error(),
                                          core::DiagnosticSeverity::Error,
                                          "assets.resource.scope_index_conflict",
                                          "ResourceScope fallback key conflicts with an entry", id);
                    break;
                }
                appendErrorDiagnostic(
                    diagnostics, loaded.error(), core::DiagnosticSeverity::Warning,
                    "assets.resource.fallback_used", "Built-in fallback resource was used", id);
                diagnostics.sortDeterministically();
                return {handle, std::move(diagnostics)};
            }
            appendErrorDiagnostic(diagnostics, fallback.error(), core::DiagnosticSeverity::Error,
                                  "assets.resource.fallback_failed",
                                  "Built-in fallback resource could not be acquired", id);
            break;
        }
        }
        break;
    }
    diagnostics.sortDeterministically();
    return {std::nullopt, std::move(diagnostics)};
}

auto ResourceScope::requestMesh(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<MeshHandle> {
    auto result = request(AssetType::Mesh, id, policy);
    std::optional<MeshHandle> handle;
    if (result.handle && std::holds_alternative<MeshHandle>(*result.handle)) {
        handle = std::get<MeshHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestMaterial(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<MaterialHandle> {
    auto result = request(AssetType::Material, id, policy);
    std::optional<MaterialHandle> handle;
    if (result.handle && std::holds_alternative<MaterialHandle>(*result.handle)) {
        handle = std::get<MaterialHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestTexture(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<TextureHandle> {
    auto result = request(AssetType::Texture, id, policy);
    std::optional<TextureHandle> handle;
    if (result.handle && std::holds_alternative<TextureHandle>(*result.handle)) {
        handle = std::get<TextureHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestAudioSource(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<AudioSourceHandle> {
    auto result = request(AssetType::Audio, id, policy);
    std::optional<AudioSourceHandle> handle;
    if (result.handle && std::holds_alternative<AudioSourceHandle>(*result.handle)) {
        handle = std::get<AudioSourceHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

auto ResourceScope::requestShader(const AssetId& id, ResourcePolicy policy)
    -> ResourceRequestResult<ShaderHandle> {
    auto result = request(AssetType::Shader, id, policy);
    std::optional<ShaderHandle> handle;
    if (result.handle && std::holds_alternative<ShaderHandle>(*result.handle)) {
        handle = std::get<ShaderHandle>(*result.handle);
    }
    return {std::move(handle), std::move(result.diagnostics)};
}

void ResourceScope::rollbackTo(std::size_t size) noexcept {
    if (size >= entries_.size()) {
        return;
    }
    for (std::size_t index = size; index < entries_.size(); ++index) {
        const auto& entry = entries_[index];
        const auto found = entryIndex_.find(EntryKeyView{entry.type, entry.requestedId.value});
        if (found != entryIndex_.end() && found->second == index) {
            entryIndex_.erase(found);
        }
    }
    entries_.resize(size);
}

std::size_t ResourceScope::size() const noexcept {
    return entries_.size();
}

bool ResourceScope::empty() const noexcept {
    return entries_.empty();
}

bool ResourceScope::contains(AssetType type, const AssetId& id) const noexcept {
    return entryIndex_.contains(EntryKeyView{type, id.value});
}

bool ResourceScope::contains(MeshHandle handle) const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return std::holds_alternative<MeshLease>(entry.lease) &&
               std::get<MeshLease>(entry.lease).handle() == handle;
    });
}

bool ResourceScope::contains(MaterialHandle handle) const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return std::holds_alternative<MaterialLease>(entry.lease) &&
               std::get<MaterialLease>(entry.lease).handle() == handle;
    });
}

bool ResourceScope::contains(TextureHandle handle) const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return std::holds_alternative<TextureLease>(entry.lease) &&
               std::get<TextureLease>(entry.lease).handle() == handle;
    });
}

bool ResourceScope::contains(AudioSourceHandle handle) const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return std::holds_alternative<AudioSourceLease>(entry.lease) &&
               std::get<AudioSourceLease>(entry.lease).handle() == handle;
    });
}

bool ResourceScope::contains(ShaderHandle handle) const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return std::holds_alternative<ShaderLease>(entry.lease) &&
               std::get<ShaderLease>(entry.lease).handle() == handle;
    });
}

std::uint64_t ResourceScope::managerToken() const noexcept {
    const auto state = state_.lock();
    return state ? state->token : 0;
}

void ResourceScope::clear() noexcept {
    entryIndex_.clear();
    entries_.clear();
}

} // namespace cuexis::assets
