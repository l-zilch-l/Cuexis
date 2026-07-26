#pragma once

// Bounded logical content sources used by ResourceManager and external hosts.

#include <cuexis/core/result.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::content {

struct ContentRequest final {
    std::string_view rootId;
    std::string_view source;
    std::size_t maxBytes{64U * 1024U * 1024U};
};

struct ContentBlob final {
    std::vector<std::byte> bytes;
    // Provider-defined, process-local source revision. Zero means unspecified.
    std::uint64_t revision{};

    [[nodiscard]] auto span() const noexcept -> std::span<const std::byte> {
        return bytes;
    }
};

class IContentProvider {
  public:
    virtual ~IContentProvider() = default;

    IContentProvider(const IContentProvider&) = delete;
    auto operator=(const IContentProvider&) -> IContentProvider& = delete;
    IContentProvider(IContentProvider&&) = delete;
    auto operator=(IContentProvider&&) -> IContentProvider& = delete;

    // Synchronous call on the requesting ResourceManager owner thread. Implementations must not
    // retain request string_views and must not allow exceptions to cross this boundary.
    [[nodiscard]] virtual auto readBlob(const ContentRequest& request)
        -> core::Result<ContentBlob> = 0;

  protected:
    IContentProvider() = default;
};

struct FilesystemContentRoot final {
    std::string id;
    std::filesystem::path path;
};

class FilesystemContentProvider final : public IContentProvider {
  public:
    ~FilesystemContentProvider() override;

    [[nodiscard]] static auto create(std::vector<FilesystemContentRoot> roots)
        -> core::Result<std::shared_ptr<FilesystemContentProvider>>;

    [[nodiscard]] auto readBlob(const ContentRequest& request)
        -> core::Result<ContentBlob> override;

  private:
    struct Root;
    explicit FilesystemContentProvider(std::vector<Root> roots) noexcept;

    std::vector<Root> roots_;
};

struct MemoryContentEntry final {
    std::string rootId;
    std::string source;
    std::vector<std::byte> bytes;
    std::uint64_t revision{1};
};

class MemoryContentProvider final : public IContentProvider {
  public:
    ~MemoryContentProvider() override;

    [[nodiscard]] static auto create(std::vector<MemoryContentEntry> entries)
        -> core::Result<std::shared_ptr<MemoryContentProvider>>;

    [[nodiscard]] auto readBlob(const ContentRequest& request)
        -> core::Result<ContentBlob> override;

  private:
    struct Data;
    explicit MemoryContentProvider(std::shared_ptr<const Data> data) noexcept;

    std::shared_ptr<const Data> data_;
};

using HostContentCallback = std::function<core::Result<ContentBlob>(const ContentRequest& request)>;

class HostContentProvider final : public IContentProvider {
  public:
    [[nodiscard]] static auto create(HostContentCallback callback)
        -> core::Result<std::shared_ptr<HostContentProvider>>;

    [[nodiscard]] auto readBlob(const ContentRequest& request)
        -> core::Result<ContentBlob> override;

  private:
    explicit HostContentProvider(HostContentCallback callback) noexcept;

    HostContentCallback callback_;
};

} // namespace cuexis::content
